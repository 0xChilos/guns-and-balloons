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

	const FName ChamberBones[URevolverComponent::NumChambers] = {
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
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;

	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> Idle(TEXT("/Game/Assets/SK_RevolverArmature_Idle.SK_RevolverArmature_Idle"));
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> Fire(TEXT("/Game/Assets/SK_RevolverArmature_Fire.SK_RevolverArmature_Fire"));
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> Open(TEXT("/Game/Assets/SK_RevolverArmature_Reload_Open.SK_RevolverArmature_Reload_Open"));
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> OpenKeepLive(TEXT("/Game/Assets/SK_RevolverArmature_Reload_Open_KeepLive.SK_RevolverArmature_Reload_Open_KeepLive"));
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> Insert(TEXT("/Game/Assets/SK_RevolverArmature_Reload_InsertRound.SK_RevolverArmature_Reload_InsertRound"));
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> Close(TEXT("/Game/Assets/SK_RevolverArmature_Reload_Close.SK_RevolverArmature_Reload_Close"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Casing(TEXT("/Game/Assets/SM_RevolverCasing.SM_RevolverCasing"));

	IdleAnim = Idle.Object;
	FireAnim = Fire.Object;
	OpenAnim = Open.Object;
	OpenKeepLiveAnim = OpenKeepLive.Object;
	InsertRoundAnim = Insert.Object;
	CloseAnim = Close.Object;
	CasingMesh = Casing.Object;

	Chambers.Init(ERevolverChamber::Live, NumChambers);
	Ejecting.Init(false, NumChambers);
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
		CylinderStep = (CylinderStep + 1) % NumChambers;
		bShotDone = false;
		SetState(ERevolverState::Firing, FireAnim, FirePlayRate);
		break;

	case ERevolverState::Opening:
	case ERevolverState::Advancing:
	case ERevolverState::Inserting:
		bCancelReload = true;
		break;

	default:
		break;
	}
}

bool URevolverComponent::StartReload()
{
	if (State != ERevolverState::Idle)
	{
		return false;
	}

	bool bAnyLive = false;
	bool bAnyNotLive = false;
	for (int32 i = 0; i < NumChambers; ++i)
	{
		bAnyLive |= Chambers[i] == ERevolverChamber::Live;
		bAnyNotLive |= Chambers[i] != ERevolverChamber::Live;
		Ejecting[i] = Chambers[i] == ERevolverChamber::Spent;
	}

	if (!bAnyNotLive)
	{
		return false;
	}

	bCasingsEjected = false;
	bCancelReload = false;
	SetState(ERevolverState::Opening, bAnyLive ? OpenKeepLiveAnim : OpenAnim, ReloadPlayRate);
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

int32 URevolverComponent::FindNextEmptyOffset() const
{
	for (int32 Offset = 0; Offset < NumChambers; ++Offset)
	{
		if (Chambers[(TopChamber() + Offset) % NumChambers] == ERevolverChamber::Empty)
		{
			return Offset;
		}
	}
	return INDEX_NONE;
}

void URevolverComponent::BeginAdvanceOrClose()
{
	const int32 Offset = bCancelReload ? INDEX_NONE : FindNextEmptyOffset();
	if (Offset == INDEX_NONE || !InsertRoundAnim)
	{
		SetState(ERevolverState::Closing, CloseAnim, ReloadPlayRate);
		return;
	}

	CylinderFromAngle = CylinderStep * DegreesPerChamber;
	CylinderToAngle = CylinderFromAngle + Offset * DegreesPerChamber;
	CylinderStep = (CylinderStep + Offset) % NumChambers;
	AdvanceDuration = FMath::Max(MinAdvanceTime, Offset * DegreesPerChamber / AdvanceDegreesPerSecond);
	SetState(ERevolverState::Advancing, InsertRoundAnim, ReloadPlayRate);
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

	case ERevolverState::Opening:
		if (!bCasingsEjected && StateTime >= EjectStartTime)
		{
			bCasingsEjected = true;
			EjectSpentCasings();
		}
		if (StateTime >= LengthOf(StateAnim))
		{
			if (!bCasingsEjected)
			{
				bCasingsEjected = true;
				EjectSpentCasings();
			}
			for (bool& bEjecting : Ejecting)
			{
				bEjecting = false;
			}
			BeginAdvanceOrClose();
		}
		break;

	case ERevolverState::Advancing:
		if (StateTime >= AdvanceDuration)
		{
			SetState(ERevolverState::Inserting, InsertRoundAnim, ReloadPlayRate);
		}
		break;

	case ERevolverState::Inserting:
		if (StateTime >= LengthOf(InsertRoundAnim))
		{
			Chambers[TopChamber()] = ERevolverChamber::Live;
			OnRoundInserted.Broadcast();
			BeginAdvanceOrClose();
		}
		break;

	case ERevolverState::Closing:
		if (StateTime >= LengthOf(CloseAnim))
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
	Anim->RoundLoadScale = 0.f;

	switch (State)
	{
	case ERevolverState::Firing:
	{
		// Turn from the previous chamber to the current one while the hammer cocks.
		const float Turn = FMath::SmoothStep(0.f, CylinderTurnTime, StateTime);
		Anim->CylinderAngle = (CylinderStep - 1 + Turn) * DegreesPerChamber;
		break;
	}

	case ERevolverState::Advancing:
	{
		// Hold the insert clip's first frame (round outside the gate) while it fades in.
		const float Alpha = FMath::Clamp(StateTime / FMath::Max(AdvanceDuration, KINDA_SMALL_NUMBER), 0.f, 1.f);
		Anim->Time = 0.f;
		Anim->CylinderAngle = FMath::Lerp(CylinderFromAngle, CylinderToAngle, FMath::SmoothStep(0.f, 1.f, Alpha));
		Anim->RoundLoadScale = Alpha;
		break;
	}

	case ERevolverState::Inserting:
		Anim->RoundLoadScale = 1.f;
		break;

	default:
		break;
	}

	const float EjectAlpha = State == ERevolverState::Opening
		? FMath::Clamp((StateTime - EjectStartTime) / FMath::Max(EjectEndTime - EjectStartTime, KINDA_SMALL_NUMBER), 0.f, 1.f)
		: 1.f;

	for (int32 i = 0; i < NumChambers; ++i)
	{
		Anim->ChamberScale[i] = Ejecting[i] ? 1.f - EjectAlpha : (Chambers[i] == ERevolverChamber::Empty ? 0.f : 1.f);
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

void URevolverComponent::EjectSpentCasings()
{
	for (int32 i = 0; i < NumChambers; ++i)
	{
		if (!Ejecting[i])
		{
			continue;
		}
		Chambers[i] = ERevolverChamber::Empty;

		if (!CasingMesh || !Mesh)
		{
			continue;
		}

		const FRotator GunRotation = Mesh->GetComponentRotation();
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = GetOwner();
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		AStaticMeshActor* Casing = GetWorld()->SpawnActor<AStaticMeshActor>(Mesh->GetSocketLocation(ChamberBones[i]), GunRotation, SpawnParams);
		if (!Casing)
		{
			continue;
		}

		Casing->SetMobility(EComponentMobility::Movable);
		Casing->SetLifeSpan(CasingLifeSpan);

		UStaticMeshComponent* CasingComp = Casing->GetStaticMeshComponent();
		CasingComp->SetStaticMesh(CasingMesh);
		CasingComp->SetCollisionProfileName(UCollisionProfile::PhysicsActor_ProfileName);
		CasingComp->SetSimulatePhysics(true);

		// Out of the back of the swung-out cylinder, with a little scatter and tumble.
		const FVector Velocity = -GunRotation.Vector() * CasingEjectSpeed + FMath::VRand() * (CasingEjectSpeed * 0.25f) + FVector(0.f, 0.f, -20.f);
		CasingComp->SetPhysicsLinearVelocity(Velocity);
		CasingComp->SetPhysicsAngularVelocityInDegrees(FMath::VRand() * 360.f);
	}
}
