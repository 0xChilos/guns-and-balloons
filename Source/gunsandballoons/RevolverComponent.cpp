#include "RevolverComponent.h"

#include "Animation/AnimSequenceBase.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "RevolverAnimInstance.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	constexpr float DegreesPerChamber = 360.f / URevolverComponent::NumChambers;

	/** Carries the ejected case in the reload clips. */
	const FName CaseEjectBone(TEXT("case_eject"));

	/** A casing never leaves faster than this (cm/s), whatever the frame timing. */
	constexpr float MaxCasingSpeed = 600.f;

	float LengthOf(const UAnimSequenceBase* Anim)
	{
		return Anim ? Anim->GetPlayLength() : 0.f;
	}
}

URevolverComponent::URevolverComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;

	// Imported from SK_Pax.fbx: every take is bound to SK_Pax_Skeleton.
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> Idle(TEXT("/Game/Assets/Revolver/SK_PaxArmature_Idle.SK_PaxArmature_Idle"));
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> Fire(TEXT("/Game/Assets/Revolver/SK_PaxArmature_Fire.SK_PaxArmature_Fire"));
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> Start(TEXT("/Game/Assets/Revolver/SK_PaxArmature_Reload_Start.SK_PaxArmature_Reload_Start"));
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> Eject(TEXT("/Game/Assets/Revolver/SK_PaxArmature_Reload_EjectCase.SK_PaxArmature_Reload_EjectCase"));
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> Insert(TEXT("/Game/Assets/Revolver/SK_PaxArmature_Reload_InsertRound.SK_PaxArmature_Reload_InsertRound"));
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> End(TEXT("/Game/Assets/Revolver/SK_PaxArmature_Reload_End.SK_PaxArmature_Reload_End"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Casing(TEXT("/Game/Assets/Revolver/SM_PaxCasing.SM_PaxCasing"));

	IdleAnim = Idle.Object;
	FireAnim = Fire.Object;
	ReloadStartAnim = Start.Object;
	EjectCaseAnim = Eject.Object;
	InsertRoundAnim = Insert.Object;
	ReloadEndAnim = End.Object;
	CasingMesh = Casing.Object;

	Chambers.Init(ERevolverChamber::Live, NumChambers);
}

void URevolverComponent::SetupWeapon(USkeletalMeshComponent* InMesh, UCameraComponent* InCamera)
{
	Mesh = InMesh;
	Camera = InCamera;
}

void URevolverComponent::BeginPlay()
{
	Super::BeginPlay();

	if (Mesh)
	{
		// Pose values are written here, so the mesh evaluates after this component each frame.
		Mesh->PrimaryComponentTick.AddPrerequisite(this, PrimaryComponentTick);
	}

	SetState(ERevolverState::Idle, IdleAnim, 1.f);
	UpdatePose();
}

int32 URevolverComponent::GetLiveRounds() const
{
	int32 Count = 0;
	for (ERevolverChamber Chamber : Chambers)
	{
		Count += Chamber == ERevolverChamber::Live ? 1 : 0;
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
		if (bAutoReloadWhenEmpty && GetLiveRounds() == 0 && StartReload())
		{
			return;
		}
		// Double action: the trigger turns the cylinder to the next chamber while cocking the hammer.
		++CylinderStep;
		bShotDone = false;
		SetState(ERevolverState::Firing, FireAnim, FirePlayRate);
		break;

	case ERevolverState::ReloadStart:
	case ERevolverState::Turning:
	case ERevolverState::Ejecting:
	case ERevolverState::Inserting:
		bCancelReload = true;
		break;

	default:
		break;
	}
}

bool URevolverComponent::StartReload()
{
	if (State != ERevolverState::Idle || GetLiveRounds() == NumChambers)
	{
		return false;
	}

	bCancelReload = false;
	SetState(ERevolverState::ReloadStart, ReloadStartAnim, ReloadPlayRate);
	OnReloadStarted.Broadcast();
	return true;
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
	if (!bCancelReload)
	{
		for (int32 i = 0; i < NumChambers; ++i)
		{
			if (Chambers[(GateChamber() + i) % NumChambers] != ERevolverChamber::Live)
			{
				Offset = i;
				break;
			}
		}
	}

	if (Offset == INDEX_NONE)
	{
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
	if (Chambers[Gate] == ERevolverChamber::Spent && EjectCaseAnim)
	{
		// The case now rides on the case_eject bone; the chamber itself is empty.
		Chambers[Gate] = ERevolverChamber::Empty;
		bCasingReleased = false;
		bHasLastCaseLocation = false;
		SetState(ERevolverState::Ejecting, EjectCaseAnim, ReloadPlayRate);
		return;
	}

	Chambers[Gate] = ERevolverChamber::Empty;
	SetState(ERevolverState::Inserting, InsertRoundAnim, ReloadPlayRate);
}

void URevolverComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	StateTime += DeltaTime * StateRate;

	switch (State)
	{
	case ERevolverState::Idle:
		StateTime = 0.f;
		break;

	case ERevolverState::Firing:
		if (!bShotDone && StateTime >= HammerDropTime)
		{
			bShotDone = true;
			Shoot();
		}
		if (StateTime >= LengthOf(FireAnim))
		{
			SetState(ERevolverState::Idle, IdleAnim, 1.f);
		}
		break;

	case ERevolverState::ReloadStart:
		if (StateTime >= LengthOf(ReloadStartAnim))
		{
			ContinueReload();
		}
		break;

	case ERevolverState::Turning:
		if (StateTime >= TurnDuration)
		{
			if (bCancelReload)
			{
				ContinueReload();
			}
			else
			{
				ServiceGateChamber();
			}
		}
		break;

	case ERevolverState::Ejecting:
		if (!bCasingReleased && StateTime >= CaseReleaseTime)
		{
			bCasingReleased = true;
			ReleaseCasing();
			OnCaseEjected.Broadcast();
		}
		else if (!bCasingReleased && Mesh)
		{
			// Mesh poses are a frame behind this tick, which is fine for a velocity estimate.
			LastCaseLocation = Mesh->GetSocketLocation(CaseEjectBone);
			bHasLastCaseLocation = true;
		}
		if (StateTime >= LengthOf(EjectCaseAnim))
		{
			if (bCancelReload)
			{
				ContinueReload();
			}
			else
			{
				SetState(ERevolverState::Inserting, InsertRoundAnim, ReloadPlayRate);
			}
		}
		break;

	case ERevolverState::Inserting:
		if (StateTime >= LengthOf(InsertRoundAnim))
		{
			Chambers[GateChamber()] = ERevolverChamber::Live;
			OnRoundInserted.Broadcast();
			ContinueReload();
		}
		break;

	case ERevolverState::ReloadEnd:
		if (StateTime >= LengthOf(ReloadEndAnim))
		{
			SetState(ERevolverState::Idle, IdleAnim, 1.f);
			OnReloadFinished.Broadcast();
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
	Anim->CaseEjectScale = State == ERevolverState::Ejecting && bCasingReleased && CasingMesh ? 0.f : 1.f;

	switch (State)
	{
	case ERevolverState::Firing:
	{
		// Turn from the previous chamber to the current one while the hammer cocks.
		const float Turn = FMath::SmoothStep(0.f, CylinderTurnTime, StateTime);
		Anim->CylinderAngle = (CylinderStep - 1 + Turn) * DegreesPerChamber;
		break;
	}

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

	// Spent cases stay in their chambers until ejected. The round sliding in rides on round_load,
	// so the chamber being loaded stays hidden until the clip ends.
	for (int32 i = 0; i < NumChambers; ++i)
	{
		Anim->ChamberScale[i] = Chambers[i] == ERevolverChamber::Empty ? 0.f : 1.f;
	}
}

void URevolverComponent::Shoot()
{
	const int32 Chamber = TopChamber();
	if (Chambers[Chamber] != ERevolverChamber::Live)
	{
		OnDryFire.Broadcast();
		return;
	}
	Chambers[Chamber] = ERevolverChamber::Spent;

	AActor* Owner = GetOwner();
	APawn* Pawn = Cast<APawn>(Owner);

	// Shoot from the eye along the view so the shot lands on the crosshair.
	const FVector Start = Camera ? Camera->GetComponentLocation() : Owner->GetActorLocation();
	const FVector Direction = (Pawn ? Pawn->GetViewRotation() : Owner->GetActorRotation()).Vector();
	const FVector End = Start + Direction * Range;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(RevolverShot), /*bTraceComplex=*/true, Owner);
	FHitResult Hit;
	const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, Start, End, TraceChannel, Params);

	if (bDebugShots)
	{
		DrawDebugLine(GetWorld(), Start, bHit ? Hit.ImpactPoint : End, bHit ? FColor::Green : FColor::Red, false, 2.f, 0, 0.5f);
		if (bHit)
		{
			DrawDebugPoint(GetWorld(), Hit.ImpactPoint, 8.f, FColor::Yellow, false, 2.f);
		}
	}

	if (bHit)
	{
		if (AActor* HitActor = Hit.GetActor())
		{
			UGameplayStatics::ApplyPointDamage(HitActor, Damage, Direction, Hit, Pawn ? Pawn->GetController() : nullptr, Owner, UDamageType::StaticClass());
		}

		UPrimitiveComponent* HitComponent = Hit.GetComponent();
		if (HitComponent && HitComponent->IsSimulatingPhysics())
		{
			HitComponent->AddImpulseAtLocation(Direction * HitImpulse, Hit.ImpactPoint, Hit.BoneName);
		}
	}

	OnFired.Broadcast(bHit, Hit);
}

void URevolverComponent::ReleaseCasing()
{
	if (!CasingMesh || !Mesh)
	{
		return;
	}

	// Take over from the animated case exactly where it is, carrying on with its motion.
	const FVector Location = Mesh->GetSocketLocation(CaseEjectBone);
	const FRotator Rotation = Mesh->GetSocketRotation(CaseEjectBone);
	const float DeltaSeconds = GetWorld()->GetDeltaSeconds();
	FVector Velocity = bHasLastCaseLocation && DeltaSeconds > KINDA_SMALL_NUMBER
		? (Location - LastCaseLocation) / DeltaSeconds
		: -Mesh->GetForwardVector() * 60.f;
	Velocity = Velocity.GetClampedToMaxSize(MaxCasingSpeed) + FMath::VRand() * CasingScatter;

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = GetOwner();
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AStaticMeshActor* Casing = GetWorld()->SpawnActor<AStaticMeshActor>(Location, Rotation, SpawnParams);
	if (!Casing)
	{
		return;
	}

	Casing->SetMobility(EComponentMobility::Movable);
	Casing->SetLifeSpan(CasingLifeSpan);

	UStaticMeshComponent* CasingComp = Casing->GetStaticMeshComponent();
	CasingComp->SetStaticMesh(CasingMesh);
	CasingComp->SetCollisionProfileName(UCollisionProfile::PhysicsActor_ProfileName);
	CasingComp->SetSimulatePhysics(true);
	CasingComp->SetPhysicsLinearVelocity(Velocity);
	CasingComp->SetPhysicsAngularVelocityInDegrees(FMath::VRand() * 360.f);
}
