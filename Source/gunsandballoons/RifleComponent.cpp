#include "RifleComponent.h"

#include "Animation/AnimSequenceBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture.h"
#include "GunAnimInstance.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	/** SK_MartiniHenry bones that carry the rounds and cases; the clips key them all visible. */
	const FName ChamberBone(TEXT("chamber"));
	const FName ChamberSpentBone(TEXT("chamber_spent"));
	const FName RoundLoadBone(TEXT("round_load"));

	float LengthOf(const UAnimSequenceBase* Anim)
	{
		return Anim ? Anim->GetPlayLength() : 0.f;
	}
}

URifleComponent::URifleComponent()
{
	// Imported from SK_MartiniHenry.fbx: every take is bound to SK_MartiniHenry_Skeleton.
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> Idle(TEXT("/Game/Assets/Rifle_MartiniHenry/SK_MartiniHenryArmature_Idle.SK_MartiniHenryArmature_Idle"));
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> Fire(TEXT("/Game/Assets/Rifle_MartiniHenry/SK_MartiniHenryArmature_Fire.SK_MartiniHenryArmature_Fire"));
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> Open(TEXT("/Game/Assets/Rifle_MartiniHenry/SK_MartiniHenryArmature_Reload_Open.SK_MartiniHenryArmature_Reload_Open"));
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> Insert(TEXT("/Game/Assets/Rifle_MartiniHenry/SK_MartiniHenryArmature_Reload_Insert.SK_MartiniHenryArmature_Reload_Insert"));
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> Close(TEXT("/Game/Assets/Rifle_MartiniHenry/SK_MartiniHenryArmature_Reload_Close.SK_MartiniHenryArmature_Reload_Close"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Casing(TEXT("/Game/Assets/Rifle_MartiniHenry/SM_MartiniHenry_Casing.SM_MartiniHenry_Casing"));
	static ConstructorHelpers::FObjectFinder<UTexture> Texture(TEXT("/Game/Textures/Rifle_MartiniHenry/T_MartiniHenry_256.T_MartiniHenry_256"));

	IdleAnim = Idle.Object;
	FireAnim = Fire.Object;
	OpenAnim = Open.Object;
	InsertAnim = Insert.Object;
	CloseAnim = Close.Object;
	CasingMesh = Casing.Object;
	WeaponTexture = Texture.Object;

	// SK_MartiniHenry's root sits on the bore axis at the breech face; the gun reaches 80cm forward
	// (muzzle) and 50cm back (butt), with the lever and stock hanging up to 15cm below the bore.
	// Held low on the right with the butt passing beside the cheek; while reloading the receiver comes
	// in toward the middle, muzzle up and rolled so the open breech (top) faces the eye.
	// First-pass values, need tuning in game.
	ViewOffset = FVector(35.f, 18.f, -9.f);
	ViewRotation = FRotator(0.f, -2.f, 0.f);
	ReloadViewOffset = FVector(35.f, 11.f, -9.f);
	ReloadViewRotation = FRotator(12.f, 20.f, -30.f);
	RecoilKickBack = 7.f;
	RecoilKickPitch = 8.f;

	// --- Martini-Henry handling (design, 2026-11-14) ---
	Damage = 143.f;
	Range = 30000.f;
	HitImpulse = 6000.f;
	MuzzleVelocity = 40000.f;   // 400 m/s
	ReloadTime = 2.7f;          // case out + round in, lever down and up (animation time; the clicks come on top)
	// Rate of fire 18 rpm and cycle time 2.7s: on a single-shot rifle the cycle is the reload, so a shot
	// plus the 2.7s reload already gives ~18 rpm. No extra trigger lock or wait.
	// Bullet drop: twice real gravity - flat up close, ~60cm low at 100m, ~2.5m at 200m. Needs tuning.
	GravityScale = 2.f;

	// Recommended starting values (tweak in the editor). The precise gun: almost no spread, so the
	// shot goes where the crosshair is - and the crosshair is what the balloon fights. Its hand sway is
	// small but slow and long (a long barrel), so a good shot is timed, not snapped. The kick is big,
	// but with ~3s between shots it costs re-acquiring the target, not the next shot.
	SpreadAngle = 0.15f;        // ~25cm at 100m at worst
	AimRecoilPitch = 8.f;
	AimRecoilPitchVariance = 0.2f;
	AimRecoilYaw = 1.f;
	AimRecoilRecovery = 0.35f;
	AimRecoilRecoveryTime = 0.9f;
	HandSwayPitch = 0.45f;
	HandSwayYaw = 0.55f;
	HandSwaySpeed = 0.45f;

	// The muzzle is 80cm along the bore from the root.
	MuzzleOffset = FVector(80.f, 0.f, 0.f);

	// A big black-powder cartridge: the cloud is thrown further and the long barrel smokes for longer.
	// First-pass values, tune in game.
	MuzzleBlastSmoke.Amount = 14.f;
	MuzzleBlastSmoke.SpeedMax = 900.f;
	MuzzleBlastSmoke.EndRadius = 38.f;
	MuzzleWispSmoke.Duration = 2.f;
}

void URifleComponent::BeginPlay()
{
	Super::BeginPlay();

	Chamber = bStartLoaded ? EGunChamber::Live : EGunChamber::Empty;
	bRoundQueued = false;
	bEjectQueued = false;

	SetState(ERifleState::Idle, IdleAnim, 1.f);
	UpdatePose();
}

bool URifleComponent::IsReloading() const
{
	return State == ERifleState::Opening || State == ERifleState::Ejecting || State == ERifleState::Open
		|| State == ERifleState::Inserting || State == ERifleState::Closing;
}

bool URifleComponent::CanLoadRound() const
{
	return Chamber != EGunChamber::Live && !bRoundQueued;
}

bool URifleComponent::IsAimingAtAmmo() const
{
	return bAimRequested || (bCloseWhenAimLeavesAmmo && AimReleasedTime < GateHoldGraceTime);
}

bool URifleComponent::ShouldStayOpen() const
{
	if (!CanLoadRound() || bCloseRequested)
	{
		return false;
	}
	return bCloseWhenAimLeavesAmmo ? IsAimingAtAmmo() : true;
}

void URifleComponent::PullTrigger()
{
	switch (State)
	{
	case ERifleState::Idle:
		if (ConsumeRateOfFire())
		{
			bShotDone = false;
			SetState(ERifleState::Firing, FireAnim, FirePlayRate);
		}
		break;

	case ERifleState::Opening:
	case ERifleState::Ejecting:
		// Close as soon as the lever is down / the case is out (or load first, if a round is already in hand).
		bCloseRequested = true;
		bEjectQueued = false;
		break;

	case ERifleState::Open:
		bCloseRequested = true;
		ContinueReload();
		break;

	default:
		break;
	}
}

bool URifleComponent::LoadRound()
{
	if (!CanLoadRound())
	{
		return false;
	}

	bRoundQueued = true;

	// While opening the round waits for the lever; while firing or closing, EnterIdle reopens for it.
	if (State == ERifleState::Idle)
	{
		StartOpening();
	}
	else if (State == ERifleState::Open)
	{
		ContinueReload();
	}
	return true;
}

bool URifleComponent::EjectCase()
{
	if (Chamber != EGunChamber::Spent || bEjectQueued)
	{
		// Nothing left to pull out: the click goes on to take a round.
		return false;
	}

	bEjectQueued = true;

	// While opening it waits for the lever; while firing or closing, EnterIdle reopens for it.
	if (State == ERifleState::Idle)
	{
		StartOpening();
	}
	else if (State == ERifleState::Open)
	{
		ContinueReload();
	}
	return true;
}

void URifleComponent::SetAimingAtAmmo(bool bAiming)
{
	// Stays 0 while aiming; TickComponent counts it up once the aim leaves.
	if (bAiming)
	{
		AimReleasedTime = 0.f;
	}
	bAimRequested = bAiming;

	if (State == ERifleState::Idle && IsAimingAtAmmo() && CanLoadRound())
	{
		StartOpening();
	}
}

void URifleComponent::PrepareToHolster()
{
	bAimRequested = false;
	AimReleasedTime = TNumericLimits<float>::Max();
	// A round already handed over is still loaded (nothing is lost); otherwise the breech just closes.
	bCloseRequested = true;
	bEjectQueued = false;
	if (State == ERifleState::Open)
	{
		ContinueReload();
	}
}

bool URifleComponent::IsReadyToHolster() const
{
	return State == ERifleState::Idle && !bRoundQueued && !bEjectQueued;
}

float URifleComponent::ReloadRate() const
{
	if (ReloadTime <= 0.f)
	{
		return ReloadPlayRate;
	}
	// Opening and ejecting together are the whole opening clip.
	const float Clips = LengthOf(OpenAnim) + LengthOf(InsertAnim) + LengthOf(CloseAnim);
	return Clips > 0.f ? Clips / ReloadTime : ReloadPlayRate;
}

void URifleComponent::SetState(ERifleState NewState, UAnimSequenceBase* Anim, float Rate, float StartTime)
{
	State = NewState;
	StateAnim = Anim;
	StateRate = Rate;
	StateTime = StartTime;
}

void URifleComponent::EnterIdle()
{
	if (bRoundQueued || bEjectQueued || (IsAimingAtAmmo() && CanLoadRound()))
	{
		StartOpening();
		return;
	}
	SetState(ERifleState::Idle, IdleAnim, 1.f);
}

void URifleComponent::StartOpening()
{
	bCloseRequested = false;

	// Only the lever and block: a fired case stays in until a click pulls it out.
	SetState(ERifleState::Opening, OpenAnim, ReloadRate());
	OnReloadStarted.Broadcast();
}

void URifleComponent::ContinueReload()
{
	// Pull the fired case out when a click asked for it, or when the round in hand needs the chamber.
	if (Chamber == EGunChamber::Spent && (bEjectQueued || bRoundQueued))
	{
		// The case now rides on the case_eject bone; the chamber itself is empty. The extraction is the
		// rest of the opening clip.
		bEjectQueued = false;
		Chamber = EGunChamber::Empty;
		bCasingReleased = false;
		ResetCaseSamples();
		SetState(ERifleState::Ejecting, OpenAnim, ReloadRate(), ExtractStartTime);
		return;
	}

	if (bRoundQueued && Chamber != EGunChamber::Live)
	{
		SetState(ERifleState::Inserting, InsertAnim, ReloadRate());
		return;
	}

	if (ShouldStayOpen())
	{
		// Hold the open breech (lever down, at ExtractStartTime) until the next click.
		SetState(ERifleState::Open, OpenAnim, ReloadRate());
		return;
	}

	SetState(ERifleState::Closing, CloseAnim, ReloadRate());
}

void URifleComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	StateTime += DeltaTime * StateRate;
	if (!bAimRequested && AimReleasedTime < GateHoldGraceTime)
	{
		AimReleasedTime += DeltaTime;
	}

	switch (State)
	{
	case ERifleState::Idle:
		StateTime = 0.f;
		break;

	case ERifleState::Firing:
		if (!bShotDone && StateTime >= ShotTime)
		{
			bShotDone = true;
			Shoot();
		}
		if (StateTime >= LengthOf(FireAnim))
		{
			EnterIdle();
		}
		break;

	case ERifleState::Opening:
		if (StateTime >= FMath::Min(ExtractStartTime, LengthOf(OpenAnim)))
		{
			// The block is down: what is left of the shot's smoke drifts out past the fired case.
			if (Chamber == EGunChamber::Spent)
			{
				EmitBreechSmoke(ChamberSpentBone);
			}
			ContinueReload();
		}
		break;

	case ERifleState::Ejecting:
		if (!bCasingReleased)
		{
			SampleCase();
			if (StateTime >= CaseReleaseTime)
			{
				bCasingReleased = true;
				ReleaseCasing();
				OnCaseEjected.Broadcast();
			}
		}
		if (StateTime >= LengthOf(OpenAnim))
		{
			ContinueReload();
		}
		break;

	case ERifleState::Open:
		if (bRoundQueued || bEjectQueued || !ShouldStayOpen())
		{
			ContinueReload();
		}
		break;

	case ERifleState::Inserting:
		if (StateTime >= LengthOf(InsertAnim))
		{
			Chamber = EGunChamber::Live;
			bRoundQueued = false;
			OnRoundInserted.Broadcast();
			ContinueReload();
		}
		break;

	case ERifleState::Closing:
		if (StateTime >= LengthOf(CloseAnim))
		{
			OnReloadFinished.Broadcast();
			EnterIdle();
		}
		break;
	}

	UpdatePose();
}

void URifleComponent::UpdatePose()
{
	UGunAnimInstance* Anim = Mesh ? Cast<UGunAnimInstance>(Mesh->GetAnimInstance()) : nullptr;
	if (!Anim)
	{
		return;
	}

	Anim->Sequence = StateAnim;
	// Open holds the pose where the lever is down and the case not yet moving (the same lever and block
	// pose as the clip's end), so it fits both before and after an eject.
	Anim->Time = State == ERifleState::Open ? ExtractStartTime : StateTime;

	// The chamber shows either the live round (chamber) or the fired case (chamber_spent), never both.
	// The case being extracted rides on case_eject until the physics casing takes over; the round
	// sliding in rides on round_load, so the chamber stays empty until the insert clip ends.
	Anim->SetBoneScale(ChamberBone, Chamber == EGunChamber::Live ? 1.f : 0.f);
	Anim->SetBoneScale(ChamberSpentBone, Chamber == EGunChamber::Spent ? 1.f : 0.f);
	Anim->SetBoneScale(CaseEjectBone, State == ERifleState::Ejecting && !(bCasingReleased && CasingMesh) ? 1.f : 0.f);
	Anim->SetBoneScale(RoundLoadBone, State == ERifleState::Inserting ? 1.f : 0.f);
}

void URifleComponent::Shoot()
{
	if (Chamber != EGunChamber::Live)
	{
		OnDryFire.Broadcast();
		return;
	}
	Chamber = EGunChamber::Spent;

	FireShot();
}
