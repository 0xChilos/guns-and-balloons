#include "RevolverComponent.h"

#include "Animation/AnimSequenceBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture.h"
#include "RevolverAnimInstance.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	constexpr float DegreesPerChamber = 360.f / URevolverComponent::NumChambers;

	/** SK_Pax chamber bones by chamber index (they turn with the cylinder), for smoke out of the open gate. */
	const FName GateChamberBones[URevolverComponent::NumChambers] = {
		TEXT("chamber_1"), TEXT("chamber_2"), TEXT("chamber_3"),
		TEXT("chamber_4"), TEXT("chamber_5"), TEXT("chamber_6"),
	};

	float LengthOf(const UAnimSequenceBase* Anim)
	{
		return Anim ? Anim->GetPlayLength() : 0.f;
	}
}

URevolverComponent::URevolverComponent()
{
	// Imported from SK_Pax.fbx: every take is bound to SK_Pax_Skeleton.
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> Idle(TEXT("/Game/Assets/Revolver/SK_PaxArmature_Idle.SK_PaxArmature_Idle"));
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> Fire(TEXT("/Game/Assets/Revolver/SK_PaxArmature_Fire.SK_PaxArmature_Fire"));
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> Start(TEXT("/Game/Assets/Revolver/SK_PaxArmature_Reload_Start.SK_PaxArmature_Reload_Start"));
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> Eject(TEXT("/Game/Assets/Revolver/SK_PaxArmature_Reload_EjectCase.SK_PaxArmature_Reload_EjectCase"));
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> Insert(TEXT("/Game/Assets/Revolver/SK_PaxArmature_Reload_InsertRound.SK_PaxArmature_Reload_InsertRound"));
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> End(TEXT("/Game/Assets/Revolver/SK_PaxArmature_Reload_End.SK_PaxArmature_Reload_End"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Casing(TEXT("/Game/Assets/Revolver/SM_PaxCasing.SM_PaxCasing"));
	static ConstructorHelpers::FObjectFinder<UTexture> Texture(TEXT("/Game/Textures/Pax_Revolver_Textures/T_Pax_256.T_Pax_256"));

	IdleAnim = Idle.Object;
	FireAnim = Fire.Object;
	ReloadStartAnim = Start.Object;
	EjectCaseAnim = Eject.Object;
	InsertRoundAnim = Insert.Object;
	ReloadEndAnim = End.Object;
	CasingMesh = Casing.Object;
	WeaponTexture = Texture.Object;

	// SK_Pax's root sits on the bore axis just ahead of the cylinder; the gun reaches 27cm forward and 13cm back.
	// Held bottom-right (Dusk style); while reloading, muzzle up and out, rolled so the gate (right) side faces the eye.
	ViewOffset = FVector(36.f, 18.f, -7.5f);
	ViewRotation = FRotator(0.f, -3.f, 0.f);
	ReloadViewOffset = FVector(30.f, 11.f, -3.f);
	ReloadViewRotation = FRotator(15.f, 35.f, -45.f);
	RecoilKickBack = 4.f;
	RecoilKickPitch = 12.f;

	// --- Pax handling (design, 2026-11-14) ---
	Damage = 5.f;
	RateOfFire = 60.f;          // rpm: at most one trigger pull per second
	CycleTime = 0.8f;           // seconds from the pull until the gun is ready again
	MuzzleVelocity = 33000.f;   // 330 m/s
	GravityScale = 4.f;         // bullet drop: ~10cm low at 25m, ~45cm at 50m, ~1.8m at 100m (needs tuning)

	// Recommended starting values (tweak in the editor): hard to hold in mid-air, but fair.
	// Each shot throws the aim up ~6 degrees (4.5..7.5) and up to 1.5 to a side; only a quarter of it
	// settles back by itself, so the player has to pull the rest down before the next shot.
	AimRecoilPitch = 6.f;
	AimRecoilPitchVariance = 0.25f;
	AimRecoilYaw = 1.5f;
	AimRecoilRecovery = 0.25f;
	AimRecoilRecoveryTime = 0.6f;
	// At most 1.2 degrees off the crosshair, center-weighted: about 30cm on average at 30m, 60cm at worst.
	SpreadAngle = 1.2f;
	// The hand wanders about half a degree; the balloon's sway (on the pawn) comes on top.
	HandSwayPitch = 0.5f;
	HandSwayYaw = 0.6f;
	HandSwaySpeed = 0.7f;

	// The muzzle is 27cm along the bore from the root.
	MuzzleOffset = FVector(27.f, 0.f, 0.f);

	Chambers.Init(EGunChamber::Empty, NumChambers);
}

void URevolverComponent::BeginPlay()
{
	Super::BeginPlay();

	// Fill from the chamber that fires next (the trigger turns the cylinder before the hammer drops).
	Chambers.Init(EGunChamber::Empty, NumChambers);
	for (int32 i = 0; i < FMath::Clamp(InitialRounds, 0, NumChambers); ++i)
	{
		Chambers[(TopChamber() + 1 + i) % NumChambers] = EGunChamber::Live;
	}
	QueuedRounds = 0;

	SetState(ERevolverState::Idle, IdleAnim, 1.f);
	UpdatePose();
}

int32 URevolverComponent::GetLiveRounds() const
{
	int32 Count = 0;
	for (EGunChamber Chamber : Chambers)
	{
		Count += Chamber == EGunChamber::Live ? 1 : 0;
	}
	return Count;
}

bool URevolverComponent::IsReloading() const
{
	return State != ERevolverState::Idle && State != ERevolverState::Firing;
}

void URevolverComponent::PullTrigger()
{
	switch (State)
	{
	case ERevolverState::Idle:
		if (!ConsumeRateOfFire())
		{
			break;
		}
		// Double action: the trigger turns the cylinder to the next chamber while cocking the hammer.
		++CylinderStep;
		bShotDone = false;
		FiringElapsed = 0.f;
		SetState(ERevolverState::Firing, FireAnim, FirePlayRate);
		break;

	case ERevolverState::ReloadStart:
	case ERevolverState::Turning:
	case ERevolverState::Ejecting:
	case ERevolverState::Inserting:
		// A reload only runs with a round in hand: load that one, drop the rest, close the gate.
		QueuedRounds = FMath::Min(QueuedRounds, 1);
		break;

	case ERevolverState::GateOpen:
		SetState(ERevolverState::ReloadEnd, ReloadEndAnim, ReloadPlayRate);
		break;

	default:
		break;
	}
}

void URevolverComponent::PrepareToHolster()
{
	// No more holding the gate for ammo, not even for the grace time.
	bHoldGateRequested = false;
	HoldReleasedTime = TNumericLimits<float>::Max();

	// Like the trigger mid-reload: finish the round in hand, drop the rest, close.
	QueuedRounds = FMath::Min(QueuedRounds, 1);
	if (State == ERevolverState::GateOpen)
	{
		SetState(ERevolverState::ReloadEnd, ReloadEndAnim, ReloadPlayRate);
	}
}

bool URevolverComponent::IsReadyToHolster() const
{
	return State == ERevolverState::Idle && QueuedRounds == 0;
}

bool URevolverComponent::CanLoadRound() const
{
	return GetLiveRounds() + QueuedRounds < NumChambers;
}

void URevolverComponent::SetAimingAtAmmo(bool bAiming)
{
	// Stays 0 while aiming; TickComponent counts it up once the aim leaves.
	if (bAiming)
	{
		HoldReleasedTime = 0.f;
	}
	bHoldGateRequested = bAiming;

	if (State == ERevolverState::Idle && ShouldHoldGateOpen())
	{
		OpenGate();
	}
}

bool URevolverComponent::ShouldHoldGateOpen() const
{
	const bool bAiming = bHoldGateRequested || HoldReleasedTime < GateHoldGraceTime;
	return bAiming && CanLoadRound();
}

bool URevolverComponent::LoadRound()
{
	if (!CanLoadRound())
	{
		return false;
	}

	++QueuedRounds;

	// While firing or closing the gate the round waits; EnterIdle reopens the gate for it.
	// While the gate is open it is picked up by ContinueReload after the current chamber.
	if (State == ERevolverState::Idle)
	{
		OpenGate();
	}
	return true;
}

void URevolverComponent::OpenGate()
{
	SetState(ERevolverState::ReloadStart, ReloadStartAnim, ReloadPlayRate);
	OnReloadStarted.Broadcast();
}

void URevolverComponent::EnterIdle()
{
	if (QueuedRounds > 0 || ShouldHoldGateOpen())
	{
		OpenGate();
		return;
	}
	SetState(ERevolverState::Idle, IdleAnim, 1.f);
}

void URevolverComponent::SetState(ERevolverState NewState, UAnimSequenceBase* Anim, float Rate)
{
	State = NewState;
	StateAnim = Anim;
	StateRate = Rate;
	StateTime = 0.f;
}

void URevolverComponent::ContinueReload()
{
	// Steps forward (the firing direction) until a chamber that is not live sits at the gate.
	int32 Offset = INDEX_NONE;
	for (int32 i = 0; i < NumChambers; ++i)
	{
		if (Chambers[(GateChamber() + i) % NumChambers] != EGunChamber::Live)
		{
			Offset = i;
			break;
		}
	}

	if (Offset == INDEX_NONE || QueuedRounds == 0)
	{
		if (Offset == INDEX_NONE)
		{
			// Cylinder full (CanLoadRound keeps the queue within the free chambers).
			QueuedRounds = 0;
		}
		else if (ShouldHoldGateOpen())
		{
			// Still aiming at the ammo: keep the gate open for the next click.
			SetState(ERevolverState::GateOpen, ReloadStartAnim, ReloadPlayRate);
			return;
		}
		SetState(ERevolverState::ReloadEnd, ReloadEndAnim, ReloadPlayRate);
		return;
	}

	if (Offset == 0)
	{
		ServiceGateChamber();
		return;
	}

	TurnFromStep = CylinderStep;
	CylinderStep += Offset;
	TurnDuration = FMath::Max(MinTurnTime, Offset * DegreesPerChamber / TurnDegreesPerSecond);
	// The gate stays open while turning: hold the last frame of the opening clip.
	SetState(ERevolverState::Turning, ReloadStartAnim, ReloadPlayRate);
}

void URevolverComponent::ServiceGateChamber()
{
	const int32 Gate = GateChamber();
	if (Chambers[Gate] == EGunChamber::Spent && EjectCaseAnim)
	{
		// The case now rides on the case_eject bone; the chamber itself is empty.
		Chambers[Gate] = EGunChamber::Empty;
		bCasingReleased = false;
		ResetCaseSamples();
		SetState(ERevolverState::Ejecting, EjectCaseAnim, ReloadPlayRate);
		return;
	}

	Chambers[Gate] = EGunChamber::Empty;
	SetState(ERevolverState::Inserting, InsertRoundAnim, ReloadPlayRate);
}

void URevolverComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	StateTime += DeltaTime * StateRate;
	if (!bHoldGateRequested && HoldReleasedTime < GateHoldGraceTime)
	{
		HoldReleasedTime += DeltaTime;
	}

	switch (State)
	{
	case ERevolverState::Idle:
		StateTime = 0.f;
		break;

	case ERevolverState::GateOpen:
		if (QueuedRounds > 0)
		{
			ContinueReload();
		}
		else if (!ShouldHoldGateOpen())
		{
			SetState(ERevolverState::ReloadEnd, ReloadEndAnim, ReloadPlayRate);
		}
		break;

	case ERevolverState::Firing:
		FiringElapsed += DeltaTime;
		if (!bShotDone && StateTime >= HammerDropTime)
		{
			bShotDone = true;
			Shoot();

			// The shot itself stays quick; the rest of the clip (the trigger coming back) is spread over
			// what is left of CycleTime, and the gun does nothing else until then.
			StateRate = RateForRestOfCycle(LengthOf(FireAnim) - StateTime, FiringElapsed, StateRate);
		}
		if (StateTime >= LengthOf(FireAnim))
		{
			EnterIdle();
		}
		break;

	case ERevolverState::ReloadStart:
		if (StateTime >= LengthOf(ReloadStartAnim))
		{
			// The gate is open: what is left of the shots' smoke drifts out of the chamber behind it.
			if (Chambers.Contains(EGunChamber::Spent))
			{
				EmitBreechSmoke(GateChamberBones[GateChamber()]);
			}
			ContinueReload();
		}
		break;

	case ERevolverState::Turning:
		if (StateTime >= TurnDuration)
		{
			ServiceGateChamber();
		}
		break;

	case ERevolverState::Ejecting:
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
		if (StateTime >= LengthOf(EjectCaseAnim))
		{
			SetState(ERevolverState::Inserting, InsertRoundAnim, ReloadPlayRate);
		}
		break;

	case ERevolverState::Inserting:
		if (StateTime >= LengthOf(InsertRoundAnim))
		{
			Chambers[GateChamber()] = EGunChamber::Live;
			QueuedRounds = FMath::Max(QueuedRounds - 1, 0);
			OnRoundInserted.Broadcast();
			ContinueReload();
		}
		break;

	case ERevolverState::ReloadEnd:
		if (StateTime >= LengthOf(ReloadEndAnim))
		{
			OnReloadFinished.Broadcast();
			EnterIdle();
		}
		break;
	}

	UpdatePose();
}

void URevolverComponent::UpdatePose()
{
	URevolverAnimInstance* Anim = Mesh ? Cast<URevolverAnimInstance>(Mesh->GetAnimInstance()) : nullptr;
	if (!Anim)
	{
		return;
	}

	Anim->Sequence = StateAnim;
	Anim->Time = StateTime;
	Anim->CylinderAngle = CylinderStep * DegreesPerChamber;
	// The ejected case only exists while it is being pushed out; the new rig's rest pose shows it,
	// so it is hidden everywhere else, and once the physics casing has taken over.
	Anim->CaseEjectScale = State == ERevolverState::Ejecting && !(bCasingReleased && CasingMesh) ? 1.f : 0.f;

	switch (State)
	{
	case ERevolverState::Firing:
	{
		// Turn from the previous chamber to the current one while the hammer cocks.
		const float Turn = FMath::SmoothStep(0.f, CylinderTurnTime, StateTime);
		Anim->CylinderAngle = (CylinderStep - 1 + Turn) * DegreesPerChamber;
		break;
	}

	case ERevolverState::GateOpen:
		// Hold the last frame of the opening clip.
		Anim->Time = LengthOf(ReloadStartAnim);
		break;

	case ERevolverState::Turning:
	{
		const float Alpha = FMath::Clamp(StateTime / FMath::Max(TurnDuration, KINDA_SMALL_NUMBER), 0.f, 1.f);
		Anim->Time = LengthOf(ReloadStartAnim);
		Anim->CylinderAngle = FMath::Lerp(float(TurnFromStep), float(CylinderStep), FMath::SmoothStep(0.f, 1.f, Alpha)) * DegreesPerChamber;
		break;
	}

	default:
		break;
	}

	// Each chamber shows either its live round (chamber_N) or its fired case (chamber_N_spent), never
	// both: they occupy the same space. Spent cases stay until ejected (case_eject carries the one
	// leaving the gate); the round sliding in rides on round_load, so the chamber being loaded stays
	// empty until the clip ends and nothing is left in it for the round to clip through.
	for (int32 i = 0; i < NumChambers; ++i)
	{
		Anim->ChamberScale[i] = Chambers[i] == EGunChamber::Live ? 1.f : 0.f;
		Anim->SpentScale[i] = Chambers[i] == EGunChamber::Spent ? 1.f : 0.f;
	}
}

void URevolverComponent::Shoot()
{
	const int32 Chamber = TopChamber();
	if (Chambers[Chamber] != EGunChamber::Live)
	{
		OnDryFire.Broadcast();
		return;
	}
	Chambers[Chamber] = EGunChamber::Spent;

	FireShot();
}
