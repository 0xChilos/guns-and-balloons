#include "ShotgunComponent.h"

#include "Animation/AnimSequenceBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture.h"
#include "GunAnimInstance.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	constexpr int32 RightBarrel = static_cast<int32>(EShotgunBarrel::Right);

	/** Each bore sits 1.5cm from the rib (SK_Homestead78 shell_R / shell_L). */
	constexpr float MuzzleSideOffset = 1.5f;

	/** SK_Homestead78 bones that carry the shells, cases and hammers, indexed by barrel (0 = right, 1 = left). */
	const FName ShellBones[] = { TEXT("shell_R"), TEXT("shell_L") };
	const FName SpentShellBones[] = { TEXT("shell_R_spent"), TEXT("shell_L_spent") };
	const FName CaseEjectBones[] = { TEXT("case_eject_R"), TEXT("case_eject_L") };
	const FName HammerBones[] = { TEXT("hammer_R"), TEXT("hammer_L") };
	/** The shell going in rides on this bone in both insert clips. */
	const FName ShellLoadBone(TEXT("shell_load"));
}

UShotgunComponent::UShotgunComponent()
{
	// Imported from SK_Homestead78.fbx: every take is bound to SK_Homestead78_Skeleton.
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> Idle(TEXT("/Game/Assets/Homestead_78/SK_Homestead78Armature_Idle.SK_Homestead78Armature_Idle"));
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> FireRight(TEXT("/Game/Assets/Homestead_78/SK_Homestead78Armature_Fire_Right.SK_Homestead78Armature_Fire_Right"));
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> FireLeft(TEXT("/Game/Assets/Homestead_78/SK_Homestead78Armature_Fire_Left.SK_Homestead78Armature_Fire_Left"));
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> DryRight(TEXT("/Game/Assets/Homestead_78/SK_Homestead78Armature_Trigger_Dry_Right.SK_Homestead78Armature_Trigger_Dry_Right"));
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> DryLeft(TEXT("/Game/Assets/Homestead_78/SK_Homestead78Armature_Trigger_Dry_Left.SK_Homestead78Armature_Trigger_Dry_Left"));
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> Open(TEXT("/Game/Assets/Homestead_78/SK_Homestead78Armature_Reload_Open.SK_Homestead78Armature_Reload_Open"));
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> EjectRight(TEXT("/Game/Assets/Homestead_78/SK_Homestead78Armature_Reload_EjectRight.SK_Homestead78Armature_Reload_EjectRight"));
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> EjectLeft(TEXT("/Game/Assets/Homestead_78/SK_Homestead78Armature_Reload_EjectLeft.SK_Homestead78Armature_Reload_EjectLeft"));
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> InsertRight(TEXT("/Game/Assets/Homestead_78/SK_Homestead78Armature_Reload_InsertRight.SK_Homestead78Armature_Reload_InsertRight"));
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> InsertLeft(TEXT("/Game/Assets/Homestead_78/SK_Homestead78Armature_Reload_InsertLeft.SK_Homestead78Armature_Reload_InsertLeft"));
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> Close(TEXT("/Game/Assets/Homestead_78/SK_Homestead78Armature_Reload_Close.SK_Homestead78Armature_Reload_Close"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Casing(TEXT("/Game/Assets/Homestead_78/SM_Homestead78_ShellSpent.SM_Homestead78_ShellSpent"));
	static ConstructorHelpers::FObjectFinder<UTexture> Texture(TEXT("/Game/Textures/Homestead_78/T_Homestead78_256.T_Homestead78_256"));

	IdleAnim = Idle.Object;
	FireRightAnim = FireRight.Object;
	FireLeftAnim = FireLeft.Object;
	DryFireRightAnim = DryRight.Object;
	DryFireLeftAnim = DryLeft.Object;
	OpenAnim = Open.Object;
	EjectRightAnim = EjectRight.Object;
	EjectLeftAnim = EjectLeft.Object;
	InsertRightAnim = InsertRight.Object;
	InsertLeftAnim = InsertLeft.Object;
	CloseAnim = Close.Object;
	CasingMesh = Casing.Object;
	WeaponTexture = Texture.Object;

	// Each barrel has its own case bone (case_eject_R / case_eject_L), picked per eject.
	CaseEjectBone = NAME_None;

	// Held where the revolver and rifle are: low on the right, muzzle toward the crosshair.
	// First-pass values (the rifle's), need tuning in game.
	ViewOffset = FVector(35.f, 18.f, -9.f);
	ViewRotation = FRotator(0.f, -2.f, 0.f);
	ReloadViewOffset = FVector(35.f, 11.f, -9.f);
	ReloadViewRotation = FRotator(12.f, 20.f, -30.f);
	RecoilKickBack = 8.f;
	RecoilKickPitch = 10.f;
	// Aimed: the eye sits a little behind and above the top strap between the hammers (the strap runs from 10cm
	// behind the root to the breech, top at Z 0.8; hammer spurs at Y +-3.7 reach Z 3.2), at about (-14, 0, 4) in
	// mesh space, looking down onto the action and along the rib, as over a real side-by-side. Level with the
	// strap, the strap, hammers and the two barrels flatten into one wide band across the screen. The muzzle is
	// tipped up 1.5 deg so the bead (X 76, Z 1.6) still lands on the centre of the screen. The stock is behind
	// and well below the eye, out of view; the near plane is pulled in so the action is not sliced. TBD, tune in game.
	AimViewOffset = FVector(14.f, 0.f, -3.6f);
	AimViewRotation = FRotator(1.5f, 0.f, 0.f);
	AimTime = 0.2f;
	AimFieldOfView = 82.f;
	AimNearClipPlane = 1.f;

	// --- Homestead 78 handling (design, 2026-11-14) ---
	// Buckshot, not a bullet: 9 pellets per barrel in a pattern ~2.4m wide at 20m.
	ProjectilesPerShot = 9;
	PelletSpreadAngle = 3.5f;
	Damage = 220.f / 9.f;       // 220 per shot when every pellet hits (24.4 per pellet)
	HitImpulse = 1500.f;        // per pellet
	Range = 5000.f;
	MuzzleVelocity = 30000.f;   // 300 m/s
	// "Drop range 30m": the air halves the pellets' speed by 30m, so they fly nearly flat to there,
	// then slow and drop steeply (~30cm low at 30m, ~1.4m at 50m).
	SpeedHalvingDistance = 30.f;
	GravityScale = 3.f;
	ProjectileVisualScale = 0.55f;
	CycleTime = 0.3f;           // the second barrel can follow 0.3s after the first
	ReloadTime = 4.7f;          // full reload of both barrels (animation time; the clicks come on top)
	// Rate of fire 24 rpm is the sustained pace two barrels plus a 4.7s reload already give (~2.5s per
	// shot), so there is no trigger lock: a lock would kill the quick second barrel.

	// Recommended starting values (tweak in the editor). The heaviest kick of the three: the second
	// barrel 0.3s later goes off ~10 degrees higher unless the player drags it down, so a double is a
	// close-range move. The pattern forgives aim, so the hand sway can be the largest.
	AimRecoilPitch = 10.f;
	AimRecoilPitchVariance = 0.2f;
	AimRecoilYaw = 2.5f;
	AimRecoilRecovery = 0.3f;
	AimRecoilRecoveryTime = 0.7f;
	SpreadAngle = 0.75f;        // the whole pattern's aim error, on top of the pellet pattern
	HandSwayPitch = 0.9f;
	HandSwayYaw = 1.f;
	HandSwaySpeed = 0.8f;

	// Muzzles 76cm along the bore, barrels 1.5cm either side (set per shot).
	MuzzleOffset = FVector(76.f, MuzzleSideOffset, 0.f);

	// A shotgun charge: the heaviest, widest cloud, out of whichever barrel fired. First-pass values, tune in game.
	MuzzleBlastSmoke.Amount = 16.f;
	MuzzleBlastSmoke.SpeedMax = 800.f;
	MuzzleBlastSmoke.ConeAngle = 18.f;
	MuzzleBlastSmoke.EndRadius = 42.f;
}

void UShotgunComponent::BeginPlay()
{
	Super::BeginPlay();

	for (int32 i = 0; i < NumBarrels; ++i)
	{
		Barrels[i] = i < InitialRounds ? EGunChamber::Live : EGunChamber::Empty;
	}
	QueuedRounds = 0;
	QueuedEjects = 0;

	SetState(EShotgunState::Idle, IdleAnim, 1.f);
	UpdatePose();
}

UAnimSequenceBase* UShotgunComponent::FireAnimFor(int32 Barrel) const
{
	return Barrel == RightBarrel ? FireRightAnim : FireLeftAnim;
}

UAnimSequenceBase* UShotgunComponent::DryFireAnimFor(int32 Barrel) const
{
	return Barrel == RightBarrel ? DryFireRightAnim : DryFireLeftAnim;
}

UAnimSequenceBase* UShotgunComponent::EjectAnimFor(int32 Barrel) const
{
	return Barrel == RightBarrel ? EjectRightAnim : EjectLeftAnim;
}

UAnimSequenceBase* UShotgunComponent::InsertAnimFor(int32 Barrel) const
{
	return Barrel == RightBarrel ? InsertRightAnim : InsertLeftAnim;
}

int32 UShotgunComponent::FindBarrel(EGunChamber Content) const
{
	for (int32 i = 0; i < NumBarrels; ++i)
	{
		if (Barrels[i] == Content)
		{
			return i;
		}
	}
	return INDEX_NONE;
}

int32 UShotgunComponent::GetLiveRounds() const
{
	int32 Live = 0;
	for (const EGunChamber Barrel : Barrels)
	{
		Live += Barrel == EGunChamber::Live ? 1 : 0;
	}
	return Live;
}

bool UShotgunComponent::IsReloading() const
{
	return State == EShotgunState::Opening || State == EShotgunState::Ejecting || State == EShotgunState::Open
		|| State == EShotgunState::Inserting || State == EShotgunState::Closing;
}

bool UShotgunComponent::CanLoadRound() const
{
	return GetLiveRounds() + QueuedRounds < NumBarrels;
}

bool UShotgunComponent::IsAimingAtAmmo() const
{
	return bAimRequested || (bCloseWhenAimLeavesAmmo && AimReleasedTime < GateHoldGraceTime);
}

bool UShotgunComponent::ShouldStayOpen() const
{
	if (!CanLoadRound() || bCloseRequested)
	{
		return false;
	}
	return bCloseWhenAimLeavesAmmo ? IsAimingAtAmmo() : true;
}

void UShotgunComponent::PullTrigger()
{
	switch (State)
	{
	case EShotgunState::Idle:
		if (ConsumeRateOfFire())
		{
			StartFiring();
		}
		break;

	case EShotgunState::Opening:
	case EShotgunState::Ejecting:
	case EShotgunState::Inserting:
		// The step in hand finishes and a shell in hand still goes in; the rest of the queue is dropped,
		// then the gun closes.
		bCloseRequested = true;
		QueuedRounds = FMath::Min(QueuedRounds, 1);
		QueuedEjects = 0;
		break;

	case EShotgunState::Open:
		bCloseRequested = true;
		ContinueReload();
		break;

	default:
		break;
	}
}

bool UShotgunComponent::LoadRound()
{
	if (!CanLoadRound())
	{
		return false;
	}

	++QueuedRounds;

	// While opening, ejecting or inserting the shell waits its turn; while firing or closing, EnterIdle reopens for it.
	if (State == EShotgunState::Idle)
	{
		StartOpening();
	}
	else if (State == EShotgunState::Open)
	{
		ContinueReload();
	}
	return true;
}

bool UShotgunComponent::EjectCase()
{
	int32 SpentBarrels = 0;
	for (const EGunChamber Barrel : Barrels)
	{
		SpentBarrels += Barrel == EGunChamber::Spent ? 1 : 0;
	}
	if (SpentBarrels <= QueuedEjects)
	{
		// Nothing left to pull out: the click goes on to take a shell.
		return false;
	}

	++QueuedEjects;

	// While opening, ejecting or inserting it waits its turn; while firing or closing, EnterIdle reopens for it.
	if (State == EShotgunState::Idle)
	{
		StartOpening();
	}
	else if (State == EShotgunState::Open)
	{
		ContinueReload();
	}
	return true;
}

void UShotgunComponent::SetAimingAtAmmo(bool bAiming)
{
	// Stays 0 while aiming; TickComponent counts it up once the aim leaves.
	if (bAiming)
	{
		AimReleasedTime = 0.f;
	}
	bAimRequested = bAiming;

	if (State == EShotgunState::Idle && IsAimingAtAmmo() && CanLoadRound())
	{
		StartOpening();
	}
}

void UShotgunComponent::PrepareToHolster()
{
	bAimRequested = false;
	AimReleasedTime = TNumericLimits<float>::Max();
	// Same as a trigger pull mid-reload: the shell in hand goes in, then the gun closes.
	bCloseRequested = true;
	QueuedRounds = FMath::Min(QueuedRounds, 1);
	QueuedEjects = 0;
	if (State == EShotgunState::Open)
	{
		ContinueReload();
	}
}

bool UShotgunComponent::IsReadyToHolster() const
{
	return State == EShotgunState::Idle && QueuedRounds == 0 && QueuedEjects == 0;
}

void UShotgunComponent::SetState(EShotgunState NewState, UAnimSequenceBase* Anim, float Rate)
{
	State = NewState;
	StateAnim = Anim;
	StateRate = Rate;
	StateTime = 0.f;
}

void UShotgunComponent::EnterIdle()
{
	ActiveBarrel = INDEX_NONE;
	if (QueuedRounds > 0 || QueuedEjects > 0 || (IsAimingAtAmmo() && CanLoadRound()))
	{
		StartOpening();
		return;
	}
	SetState(EShotgunState::Idle, IdleAnim, 1.f);
}

void UShotgunComponent::StartFiring()
{
	// The front trigger fires the right barrel; once that is spent the back trigger fires the left.
	// With nothing live, the front trigger just clicks.
	const int32 LiveBarrel = FindBarrel(EGunChamber::Live);
	bDryFire = LiveBarrel == INDEX_NONE;
	ActiveBarrel = bDryFire ? RightBarrel : LiveBarrel;
	bShotDone = false;
	FiringElapsed = 0.f;
	SetState(EShotgunState::Firing, bDryFire ? DryFireAnimFor(ActiveBarrel) : FireAnimFor(ActiveBarrel), FirePlayRate);
}

float UShotgunComponent::ReloadRate() const
{
	if (ReloadTime <= 0.f)
	{
		return ReloadPlayRate;
	}
	const float Clips = AnimLength(OpenAnim) + AnimLength(EjectRightAnim) + AnimLength(EjectLeftAnim)
		+ AnimLength(InsertRightAnim) + AnimLength(InsertLeftAnim) + AnimLength(CloseAnim);
	return Clips > 0.f ? Clips / ReloadTime : ReloadPlayRate;
}

void UShotgunComponent::StartOpening()
{
	bCloseRequested = false;
	ActiveBarrel = INDEX_NONE;
	SetState(EShotgunState::Opening, OpenAnim, ReloadRate());
	OnReloadStarted.Broadcast();
}

void UShotgunComponent::ContinueReload()
{
	const int32 SpentBarrel = FindBarrel(EGunChamber::Spent);
	const int32 EmptyBarrel = FindBarrel(EGunChamber::Empty);

	// Pull out a fired case when a click asked for it, or when the shell in hand has nowhere else to go.
	if (SpentBarrel != INDEX_NONE && (QueuedEjects > 0 || (QueuedRounds > 0 && EmptyBarrel == INDEX_NONE)))
	{
		QueuedEjects = FMath::Max(QueuedEjects - 1, 0);

		// The case now rides on that barrel's case_eject bone; the barrel itself is empty.
		Barrels[SpentBarrel] = EGunChamber::Empty;
		ActiveBarrel = SpentBarrel;
		bCasingReleased = false;
		ResetCaseSamples();
		SetState(EShotgunState::Ejecting, EjectAnimFor(SpentBarrel), ReloadRate());
		return;
	}

	if (QueuedRounds > 0 && EmptyBarrel != INDEX_NONE)
	{
		ActiveBarrel = EmptyBarrel;
		SetState(EShotgunState::Inserting, InsertAnimFor(EmptyBarrel), ReloadRate());
		return;
	}

	ActiveBarrel = INDEX_NONE;
	if (ShouldStayOpen())
	{
		// Hold the open gun (last frame of the opening clip) until the next click.
		SetState(EShotgunState::Open, OpenAnim, ReloadRate());
		return;
	}

	SetState(EShotgunState::Closing, CloseAnim, ReloadRate());
}

void UShotgunComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	StateTime += DeltaTime * StateRate;
	if (!bAimRequested && AimReleasedTime < GateHoldGraceTime)
	{
		AimReleasedTime += DeltaTime;
	}

	switch (State)
	{
	case EShotgunState::Idle:
		StateTime = 0.f;
		break;

	case EShotgunState::Firing:
		FiringElapsed += DeltaTime;
		if (!bShotDone && StateTime >= (bDryFire ? DryClickTime : ShotTime))
		{
			bShotDone = true;
			if (bDryFire)
			{
				OnDryFire.Broadcast();
			}
			else
			{
				Barrels[ActiveBarrel] = EGunChamber::Spent;
				// The tracer leaves from the barrel that fired (mesh +Y is the gun's right).
				MuzzleOffset.Y = ActiveBarrel == RightBarrel ? MuzzleSideOffset : -MuzzleSideOffset;
				FireShot();
			}
			// The rest of the clip (trigger coming back) fits what is left of CycleTime: the other
			// trigger can be pulled CycleTime after this one.
			StateRate = RateForRestOfCycle(AnimLength(StateAnim) - StateTime, FiringElapsed, StateRate);
		}
		if (StateTime >= AnimLength(StateAnim))
		{
			EnterIdle();
		}
		break;

	case EShotgunState::Opening:
		if (StateTime >= AnimLength(OpenAnim))
		{
			// Broken open: what is left of the shot's smoke drifts out of each fired barrel's breech.
			TArray<FName, TInlineAllocator<NumBarrels>> SmokingBarrels;
			for (int32 i = 0; i < NumBarrels; ++i)
			{
				if (Barrels[i] == EGunChamber::Spent)
				{
					SmokingBarrels.Add(SpentShellBones[i]);
				}
			}
			if (!SmokingBarrels.IsEmpty())
			{
				EmitBreechSmoke(SmokingBarrels);
			}
			ContinueReload();
		}
		break;

	case EShotgunState::Ejecting:
		if (!bCasingReleased)
		{
			SampleCase(CaseEjectBones[ActiveBarrel]);
			if (StateTime >= CaseReleaseTime)
			{
				bCasingReleased = true;
				ReleaseCasing();
				OnCaseEjected.Broadcast();
			}
		}
		if (StateTime >= AnimLength(StateAnim))
		{
			ContinueReload();
		}
		break;

	case EShotgunState::Open:
		if (QueuedRounds > 0 || QueuedEjects > 0 || !ShouldStayOpen())
		{
			ContinueReload();
		}
		break;

	case EShotgunState::Inserting:
		if (StateTime >= AnimLength(StateAnim))
		{
			Barrels[ActiveBarrel] = EGunChamber::Live;
			--QueuedRounds;
			OnRoundInserted.Broadcast();
			ContinueReload();
		}
		break;

	case EShotgunState::Closing:
		if (StateTime >= AnimLength(CloseAnim))
		{
			OnReloadFinished.Broadcast();
			EnterIdle();
		}
		break;
	}

	UpdatePose();
}

void UShotgunComponent::UpdatePose()
{
	UGunAnimInstance* Anim = Mesh ? Cast<UGunAnimInstance>(Mesh->GetAnimInstance()) : nullptr;
	if (!Anim)
	{
		return;
	}

	Anim->Sequence = StateAnim;
	Anim->Time = State == EShotgunState::Open ? AnimLength(OpenAnim) : StateTime;

	for (int32 i = 0; i < NumBarrels; ++i)
	{
		// Each barrel shows either its live shell or its fired case, never both. The case being pulled
		// out rides on case_eject until the physics casing takes over; the shell going in rides on
		// shell_load, so the barrel stays empty until the insert clip ends.
		Anim->SetBoneScale(ShellBones[i], Barrels[i] == EGunChamber::Live ? 1.f : 0.f);
		Anim->SetBoneScale(SpentShellBones[i], Barrels[i] == EGunChamber::Spent ? 1.f : 0.f);
		const bool bEjectingThis = State == EShotgunState::Ejecting && ActiveBarrel == i;
		Anim->SetBoneScale(CaseEjectBones[i], bEjectingThis && !(bCasingReleased && CasingMesh) ? 1.f : 0.f);

		// The clips key both hammers down unless they animate one: the fire clip drops it, the insert
		// clip cocks it. Any other time a live barrel's hammer is held cocked (first frame of its fire clip).
		const bool bClipDrivesHammer = ActiveBarrel == i && (State == EShotgunState::Inserting || (State == EShotgunState::Firing && !bDryFire));
		const bool bHoldCocked = Barrels[i] == EGunChamber::Live && !bClipDrivesHammer;
		Anim->SetBonePose(HammerBones[i], bHoldCocked ? FireAnimFor(i) : nullptr, 0.f);
	}
	Anim->SetBoneScale(ShellLoadBone, State == EShotgunState::Inserting ? 1.f : 0.f);
}
