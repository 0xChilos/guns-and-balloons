#include "HookComponent.h"

#include "Animation/AnimSequenceBase.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GunAnimInstance.h"
#include "HookPickup.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	/** SK_Hook bones (SK_Hook_Skeleton). */
	const FName BodyBone(TEXT("body"));
	const FName HookHeadBone(TEXT("hook_head"));
	const FName NeedleBone(TEXT("needle"));
	const FName MuzzleBone(TEXT("muzzle"));

	constexpr float FullCharge = 100.f;

	/**
	 * The flying head is a whole SK_Hook whose body bone is shrunk by this much (taking the pump, the
	 * needle and the gun's shell with it), while the head bone is grown back by the inverse, so only
	 * the head and its claws show, at full size.
	 */
	constexpr float HiddenBodyScale = 1.e-3f;

	AHookPickup* FindPickup(AActor* Actor)
	{
		for (; Actor; Actor = Actor->GetOwner())
		{
			if (AHookPickup* Pickup = Cast<AHookPickup>(Actor))
			{
				return Pickup;
			}
		}
		return nullptr;
	}
}

UHookComponent::UHookComponent()
{
	// Imported from SK_Hook.fbx: every take is bound to SK_Hook_Skeleton.
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> Idle(TEXT("/Game/Assets/Hook/SK_HookSK_Hook_Idle.SK_HookSK_Hook_Idle"));
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> Throw(TEXT("/Game/Assets/Hook/SK_HookSK_Hook_Throw.SK_HookSK_Hook_Throw"));
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> Grab(TEXT("/Game/Assets/Hook/SK_HookSK_Hook_Grab.SK_HookSK_Hook_Grab"));
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> Pump(TEXT("/Game/Assets/Hook/SK_HookSK_Hook_Pump.SK_HookSK_Hook_Pump"));
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> Charged(TEXT("/Game/Assets/Hook/SK_HookSK_Hook_Charged.SK_HookSK_Hook_Charged"));
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> NeedleZero(TEXT("/Game/Assets/Hook/SK_HookSK_Hook_Needle_Ref_0.SK_HookSK_Hook_Needle_Ref_0"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> RopeAsset(TEXT("/Game/Assets/Hook/SM_HK_Rope.SM_HK_Rope"));
	static ConstructorHelpers::FObjectFinder<UTexture> Texture(TEXT("/Game/Textures/Hook/T_Hook_256.T_Hook_256"));
	static ConstructorHelpers::FObjectFinder<UTexture> RopeTex(TEXT("/Game/Textures/Hook/T_Hook_Rope_64.T_Hook_Rope_64"));

	IdleAnim = Idle.Object;
	ThrowAnim = Throw.Object;
	GrabAnim = Grab.Object;
	PumpAnim = Pump.Object;
	ChargedAnim = Charged.Object;
	NeedleZeroAnim = NeedleZero.Object;
	RopeMesh = RopeAsset.Object;
	WeaponTexture = Texture.Object;
	RopeTexture = RopeTex.Object;

	// Pneumatic: no powder smoke, no casings, no bullets.
	SmokeClass = nullptr;
	CasingMesh = nullptr;

	// SK_Hook's root is the back of the gun; the barrel runs 43cm forward (+X) to the muzzle and the
	// gauge sits on top at the back, facing the eye. Held where the rifle and shotgun are.
	ViewOffset = FVector(35.f, 18.f, -9.f);
	ViewRotation = FRotator(0.f, -2.f, 0.f);
	ReloadViewOffset = ViewOffset;
	ReloadViewRotation = ViewRotation;

	// A soft pneumatic kick. First-pass values, need tuning.
	RecoilKickBack = 3.f;
	RecoilKickPitch = 4.f;
	AimRecoilPitch = 1.5f;
	AimRecoilPitchVariance = 0.2f;
	AimRecoilYaw = 0.5f;
	AimRecoilRecovery = 0.5f;
	AimRecoilRecoveryTime = 0.4f;
	HandSwayPitch = 0.5f;
	HandSwayYaw = 0.6f;
	HandSwaySpeed = 0.6f;
}

void UHookComponent::BeginPlay()
{
	Super::BeginPlay();

	// The pawn places the gun in front of the eyes first, so the head and the rope start from where it is now.
	if (AActor* Owner = GetOwner())
	{
		PrimaryComponentTick.AddPrerequisite(Owner, Owner->PrimaryActorTick);
	}

	// The base forced T_Hook_256 onto every slot; the rope slot takes the rope's own texture.
	if (Mesh)
	{
		TextureRope(Mesh, Mesh->GetMaterialIndex(RopeMaterialSlot));
	}
	CreateWorldParts();

	Charge = FMath::Clamp(InitialCharge, 0.f, FullCharge);
	DisplayedCharge = Charge;
	State = EHookState::Stowed;
	SetAction(EHookAction::Idle, IdleAnim, 1.f);
	UpdatePose();
	UpdateHeadAndRope();
}

void UHookComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	DeliverPickup();
	if (HeadMesh)
	{
		HeadMesh->DestroyComponent();
		HeadMesh = nullptr;
	}
	if (Rope)
	{
		Rope->DestroyComponent();
		Rope = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}

void UHookComponent::TextureRope(UMeshComponent* Target, int32 MaterialIndex)
{
	if (!Target || !RopeTexture || MaterialIndex == INDEX_NONE)
	{
		return;
	}
	UMaterialInstanceDynamic* Material = Cast<UMaterialInstanceDynamic>(Target->GetMaterial(MaterialIndex));
	if (!Material)
	{
		Material = Target->CreateAndSetMaterialInstanceDynamic(MaterialIndex);
	}
	if (Material)
	{
		Material->SetTextureParameterValue(WeaponTextureParameter, RopeTexture);
		Material->SetScalarParameterValue(FName(*(WeaponTextureParameter.ToString() + TEXT("Weight"))), 1.f);
	}
}

void UHookComponent::CreateWorldParts()
{
	AActor* Owner = GetOwner();
	if (!Owner || !Mesh)
	{
		return;
	}

	// The flying head: a world-space SK_Hook (not first-person, so it sits among the world objects it
	// grabs), not attached to the camera.
	HeadMesh = NewObject<USkeletalMeshComponent>(Owner, TEXT("HookHeadWorld"), RF_Transient);
	HeadMesh->SetUsingAbsoluteLocation(true);
	HeadMesh->SetUsingAbsoluteRotation(true);
	HeadMesh->SetUsingAbsoluteScale(true);
	HeadMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HeadMesh->SetGenerateOverlapEvents(false);
	// The pose shrinks the body to a point at the head, so the imported bounds around the origin cover it.
	HeadMesh->bComponentUseFixedSkelBounds = true;
	HeadMesh->SetSkeletalMeshAsset(Mesh->GetSkeletalMeshAsset());
	HeadMesh->SetAnimInstanceClass(UGunAnimInstance::StaticClass());
	HeadMesh->SetVisibility(false);
	HeadMesh->RegisterComponent();
	for (int32 i = 0; i < Mesh->GetNumMaterials(); ++i)
	{
		HeadMesh->SetMaterial(i, Mesh->GetMaterial(i));
	}
	HeadMesh->PrimaryComponentTick.AddPrerequisite(this, PrimaryComponentTick);

	// The rope: a straight line from the muzzle to the head, no physics.
	Rope = NewObject<UStaticMeshComponent>(Owner, TEXT("HookRopeWorld"), RF_Transient);
	Rope->SetUsingAbsoluteLocation(true);
	Rope->SetUsingAbsoluteRotation(true);
	Rope->SetUsingAbsoluteScale(true);
	Rope->SetMobility(EComponentMobility::Movable);
	Rope->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Rope->SetGenerateOverlapEvents(false);
	Rope->CastShadow = false;
	Rope->SetStaticMesh(RopeMesh);
	Rope->SetVisibility(false);
	Rope->RegisterComponent();
	const int32 RopeSlot = Mesh->GetMaterialIndex(RopeMaterialSlot);
	if (RopeSlot != INDEX_NONE)
	{
		Rope->SetMaterial(0, Mesh->GetMaterial(RopeSlot));
	}
	else
	{
		TextureRope(Rope, 0);
	}
}

// --- Input -----------------------------------------------------------------------------

bool UHookComponent::IsFullyCharged() const
{
	return Charge >= FullCharge - KINDA_SMALL_NUMBER;
}

void UHookComponent::SetCharge(float NewCharge)
{
	Charge = FMath::Clamp(NewCharge, 0.f, FullCharge);
}

void UHookComponent::PullTrigger()
{
	if (State != EHookState::Stowed || Action == EHookAction::Pump)
	{
		return;
	}
	if (!IsFullyCharged())
	{
		// The hook does nothing until the gauge shows 100.
		OnDryFire.Broadcast();
		return;
	}

	Charge = 0.f;
	SetAction(EHookAction::Throw, ThrowAnim, 1.f);
	Launch();
	OnFired.Broadcast();
}

void UHookComponent::SetSecondaryHeld(bool bHeld)
{
	bPumpHeld = bHeld;
}

float UHookComponent::GetPumpStrokeRate(float FromCharge) const
{
	// Judged by the pressure the stroke will reach, so the first stroke past the middle is already heavy.
	const float After = FMath::Min(FullCharge, FromCharge + FullCharge / FMath::Max(PumpsToFullCharge, 1));
	const float Span = FullCharge - HardPumpFrom;
	const float Effort = Span > KINDA_SMALL_NUMBER ? FMath::Clamp((After - HardPumpFrom) / Span, 0.f, 1.f) : (After >= FullCharge ? 1.f : 0.f);
	return PumpPlayRate * FMath::Lerp(1.f, HardPumpPlayRate, Effort);
}

bool UHookComponent::CanPump() const
{
	return State == EHookState::Stowed && !IsFullyCharged() && (Action == EHookAction::Idle || Action == EHookAction::Pump);
}

void UHookComponent::SetEquipped(bool bInEquipped)
{
	Super::SetEquipped(bInEquipped);
	if (!bInEquipped)
	{
		bPumpHeld = false;
		if (State != EHookState::Stowed)
		{
			StowHead();
		}
		UpdateHeadAndRope();
	}
}

void UHookComponent::PrepareToHolster()
{
	bPumpHeld = false;
}

bool UHookComponent::IsReadyToHolster() const
{
	// The head has to be back on the gun, and a pump stroke finishes first.
	return State == EHookState::Stowed && Action != EHookAction::Pump;
}

// --- Tick --------------------------------------------------------------------------------

void UHookComponent::SetAction(EHookAction NewAction, UAnimSequenceBase* Anim, float Rate)
{
	Action = NewAction;
	ActionAnim = Anim;
	ActionRate = Rate;
	ActionTime = 0.f;
}

void UHookComponent::SetHeadClip(UAnimSequenceBase* Anim)
{
	HeadAnim = Anim;
	HeadAnimTime = 0.f;
}

void UHookComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	ActionTime += DeltaTime * ActionRate;
	HeadAnimTime += DeltaTime;

	switch (Action)
	{
	case EHookAction::Idle:
		ActionTime = 0.f;
		if (bPumpHeld && CanPump())
		{
			SetAction(EHookAction::Pump, PumpAnim, GetPumpStrokeRate(Charge));
		}
		break;

	case EHookAction::Throw:
	case EHookAction::Charged:
		if (ActionTime >= AnimLength(ActionAnim))
		{
			SetAction(EHookAction::Idle, IdleAnim, 1.f);
		}
		break;

	case EHookAction::Pump:
		if (ActionTime >= AnimLength(PumpAnim))
		{
			// The stroke is done: its share of pressure goes in.
			Charge = FMath::Min(FullCharge, Charge + FullCharge / FMath::Max(PumpsToFullCharge, 1));
			if (Charge >= FullCharge - 0.01f)
			{
				Charge = FullCharge;
			}
			OnPumped.Broadcast();

			if (IsFullyCharged())
			{
				SetAction(EHookAction::Charged, ChargedAnim, 1.f);
				OnFullyCharged.Broadcast();
			}
			else if (bPumpHeld)
			{
				SetAction(EHookAction::Pump, PumpAnim, GetPumpStrokeRate(Charge));
			}
			else
			{
				SetAction(EHookAction::Idle, IdleAnim, 1.f);
			}
		}
		break;
	}

	UpdateFlight(DeltaTime);

	DisplayedCharge = FMath::FInterpConstantTo(DisplayedCharge, Charge, DeltaTime, NeedleSpeed);

	UpdatePose();
	UpdateHeadAndRope();
}

// --- Flight ------------------------------------------------------------------------------

FVector UHookComponent::GetGunPoint(FName Bone) const
{
	if (!Mesh)
	{
		return Camera ? Camera->GetComponentLocation() : GetOwner()->GetActorLocation();
	}
	return FirstPersonToWorld(Mesh->GetSocketLocation(Bone));
}

FVector UHookComponent::GetRestHeadLocation() const
{
	// The head bone keeps its place while hidden (scaled to 0), so this is where it goes back to.
	return GetGunPoint(HookHeadBone);
}

FCollisionQueryParams UHookComponent::MakeQueryParams() const
{
	FCollisionQueryParams Params(SCENE_QUERY_STAT(HookFlight), /*bTraceComplex=*/false, GetOwner());

	// Never the player, nor what they ride (the balloon and its basket) or anything attached to it.
	if (AActor* Ride = GetOwner())
	{
		while (AActor* Parent = Ride->GetAttachParentActor())
		{
			Ride = Parent;
		}
		TArray<AActor*> Attached;
		Ride->GetAttachedActors(Attached, /*bResetArray=*/true, /*bRecursivelyIncludeAttachedActors=*/true);
		Params.AddIgnoredActor(Ride);
		Params.AddIgnoredActors(Attached);
	}
	if (AHookPickup* Carried = CarriedPickup.Get())
	{
		Params.AddIgnoredActor(Carried);
	}
	return Params;
}

void UHookComponent::Launch()
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	const FVector Eye = Camera ? Camera->GetComponentLocation() : GetOwner()->GetActorLocation();
	const FVector Aim = (Pawn ? Pawn->GetViewRotation() : GetOwner()->GetActorRotation()).Vector();

	// The head leaves the gun and heads for what is under the crosshair (or the crosshair at full range).
	const FCollisionQueryParams Params = MakeQueryParams();
	FHitResult AimHit;
	const FVector AimEnd = Eye + Aim * MaxRange;
	const FVector Target = GetWorld()->LineTraceSingleByChannel(AimHit, Eye, AimEnd, TraceChannel, Params) ? AimHit.ImpactPoint : AimEnd;

	LaunchLocation = GetRestHeadLocation();
	HeadLocation = LaunchLocation;
	FlightDirection = (Target - LaunchLocation).GetSafeNormal(UE_KINDA_SMALL_NUMBER, Aim);
	HeadVelocity = FlightDirection * ThrowSpeed;
	State = EHookState::Out;
	SetHeadClip(ThrowAnim);

	if (bDebugHook)
	{
		DrawDebugLine(GetWorld(), Eye, Target, FColor::Yellow, false, 2.f, 0, 0.5f);
	}
}

void UHookComponent::UpdateFlight(float DeltaTime)
{
	if (State == EHookState::Stowed || DeltaTime <= 0.f)
	{
		return;
	}

	const FVector Previous = HeadLocation;

	switch (State)
	{
	case EHookState::Out:
	{
		const float Travelled = FVector::Dist(LaunchLocation, HeadLocation);
		const float StepLength = FMath::Min(ThrowSpeed * DeltaTime, FMath::Max(MaxRange - Travelled, 0.f));
		const FVector Next = HeadLocation + FlightDirection * StepLength;

		FHitResult Hit;
		const bool bHit = StepLength > 0.f && GetWorld()->SweepSingleByChannel(Hit, HeadLocation, Next, FQuat::Identity, TraceChannel,
			FCollisionShape::MakeSphere(HookRadius), MakeQueryParams());

		if (bDebugHook)
		{
			DrawDebugLine(GetWorld(), HeadLocation, bHit ? Hit.Location : Next, bHit ? FColor::Green : FColor::Red, false, 2.f, 0, 1.f);
		}

		if (bHit)
		{
			HeadLocation = Hit.Location;
			AHookPickup* Pickup = FindPickup(Hit.GetActor());
			if (Pickup && Pickup->CanBeHooked())
			{
				// Grab: the claws close on it and it rides on the head from here.
				State = EHookState::Grabbing;
				GrabTimeLeft = AnimLength(GrabAnim);
				SetHeadClip(GrabAnim);
				UpdateHeadAndRope();
				Pickup->OnHooked();
				Pickup->AttachToComponent(HeadMesh, FAttachmentTransformRules::KeepWorldTransform);
				CarriedPickup = Pickup;
				OnPickupGrabbed.Broadcast(Pickup);
			}
			else
			{
				StartReturning();
			}
		}
		else
		{
			HeadLocation = Next;
			if (Travelled + StepLength >= MaxRange - 1.f)
			{
				StartReturning();
			}
		}
		break;
	}

	case EHookState::Grabbing:
		GrabTimeLeft -= DeltaTime;
		if (GrabTimeLeft <= 0.f)
		{
			StartReturning();
		}
		break;

	case EHookState::Returning:
	{
		const FVector Target = GetRestHeadLocation();
		const FVector ToTarget = Target - HeadLocation;
		const float Distance = ToTarget.Size();
		const bool bCarrying = CarriedPickup.IsValid();

		// Reel in, but with a pickup never past the catch point in one step (a long frame must not
		// teleport it into the player's face, nor let it go where it was hooked).
		const float Stop = bCarrying ? FMath::Min(CatchDistance, Distance) : 0.f;
		const float NewDistance = FMath::Max(Distance - ReturnSpeed * DeltaTime, Stop);
		HeadLocation = Distance > KINDA_SMALL_NUMBER ? Target - ToTarget / Distance * NewDistance : Target;
		HeadVelocity = (HeadLocation - Previous) / DeltaTime;

		// Move the head (and the pickup riding on it) before letting go or stowing.
		UpdateHeadAndRope();
		if (bCarrying && NewDistance <= CatchDistance + KINDA_SMALL_NUMBER)
		{
			DeliverPickup();
		}
		if (NewDistance <= KINDA_SMALL_NUMBER)
		{
			StowHead();
		}
		break;
	}

	default:
		break;
	}

	if (State != EHookState::Stowed && State != EHookState::Returning)
	{
		HeadVelocity = (HeadLocation - Previous) / DeltaTime;
	}
}

void UHookComponent::StartReturning()
{
	State = EHookState::Returning;
	// Coming back empty: the claws close now. With a pickup they already have.
	if (HeadAnim != GrabAnim)
	{
		SetHeadClip(GrabAnim);
	}
}

void UHookComponent::StowHead()
{
	DeliverPickup();
	State = EHookState::Stowed;
	HeadVelocity = FVector::ZeroVector;
	OnHookReturned.Broadcast();
}

void UHookComponent::DeliverPickup()
{
	AHookPickup* Pickup = CarriedPickup.Get();
	CarriedPickup.Reset();
	if (!Pickup)
	{
		return;
	}
	// Let go in front of the player: it keeps a little of the pull toward them, and drops.
	const FVector Toward = HeadVelocity.GetSafeNormal();
	Pickup->OnDelivered(Toward * 150.f + FVector(0.f, 0.f, 100.f));
	OnPickupDelivered.Broadcast(Pickup);
}

// --- Pose --------------------------------------------------------------------------------

void UHookComponent::UpdatePose()
{
	if (UGunAnimInstance* Anim = Mesh ? Cast<UGunAnimInstance>(Mesh->GetAnimInstance()) : nullptr)
	{
		Anim->Sequence = ActionAnim ? ActionAnim.Get() : IdleAnim.Get();
		Anim->Time = Action == EHookAction::Idle ? 0.f : ActionTime;

		// Gauge: from the needle's 0 pose, turned about its own +Y by the share of the full sweep.
		// (A blend between the 0 and 100 poses would take the short way round, under the dial.)
		Anim->SetBonePose(NeedleBone, NeedleZeroAnim, 0.f);
		const float Angle = FMath::DegreesToRadians(NeedleSweep * FMath::Clamp(DisplayedCharge / FullCharge, 0.f, 1.f));
		Anim->SetBoneRotationOffset(NeedleBone, FQuat(FVector::YAxisVector, Angle));

		// The head (and its claws) is out in the world while the hook flies.
		Anim->SetBoneScale(HookHeadBone, State == EHookState::Stowed ? 1.f : 0.f);
	}

	if (UGunAnimInstance* HeadPose = HeadMesh ? Cast<UGunAnimInstance>(HeadMesh->GetAnimInstance()) : nullptr)
	{
		HeadPose->Sequence = HeadAnim ? HeadAnim.Get() : IdleAnim.Get();
		HeadPose->Time = HeadAnimTime;
		// Only the head: the body (and everything else on it) shrinks to a point, the head is grown back
		// and moved onto the component's origin.
		HeadPose->SetBoneScale(BodyBone, HiddenBodyScale);
		HeadPose->SetBoneScale(HookHeadBone, 1.f / HiddenBodyScale);
		HeadPose->SetBoneTranslation(HookHeadBone, FVector::ZeroVector);
	}
}

void UHookComponent::UpdateHeadAndRope()
{
	const bool bOut = State != EHookState::Stowed && IsEquipped();
	if (HeadMesh)
	{
		HeadMesh->SetVisibility(bOut);
	}
	if (Rope)
	{
		Rope->SetVisibility(bOut);
	}
	if (!bOut || !HeadMesh)
	{
		return;
	}

	// Claws pointing away from the gun, upright with the view.
	FVector Direction = (HeadLocation - GetGunPoint(BodyBone)).GetSafeNormal();
	if (Direction.IsNearlyZero())
	{
		Direction = FlightDirection;
	}
	const FVector Up = Camera ? Camera->GetUpVector() : FVector::UpVector;
	HeadMesh->SetWorldLocationAndRotation(HeadLocation, FRotationMatrix::MakeFromXZ(Direction, Up).ToQuat());

	// The rope runs straight from the muzzle to the head (the hook_rope bone sits on the head's pivot).
	if (Rope)
	{
		const FVector Start = GetGunPoint(MuzzleBone);
		const FVector Span = HeadLocation - Start;
		const float Length = Span.Size();
		Rope->SetVisibility(Length > 1.f);
		if (Length > 1.f)
		{
			Rope->SetWorldTransform(FTransform(Span.Rotation(), Start, FVector(Length, 1.f, 1.f)));
		}
	}
}
