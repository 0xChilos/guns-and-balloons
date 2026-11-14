#include "FirstPersonCameraPawn.h"

#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "AmmoSupplyComponent.h"
#include "BalloonFlightComponent.h"
#include "GameFramework/PlayerController.h"
#include "Interactable.h"
#include "RevolverAnimInstance.h"
#include "RevolverComponent.h"
#include "UObject/ConstructorHelpers.h"

AFirstPersonCameraPawn::AFirstPersonCameraPawn()
{
	PrimaryActorTick.bCanEverTick = true;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	SetRootComponent(Camera);
	Camera->bUsePawnControlRotation = true; // camera follows the controller's look rotation

	// First-person primitives (the weapon) get their own FOV and are squashed toward the eye,
	// so the gun keeps its look regardless of the world FOV and never pokes through walls.
	Camera->bEnableFirstPersonFieldOfView = true;
	Camera->FirstPersonFieldOfView = 80.f;
	Camera->bEnableFirstPersonScale = true;
	Camera->FirstPersonScale = 0.7f;

	// Hand-less viewmodel: the gun itself hangs off the eyes.
	WeaponMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("WeaponMesh"));
	WeaponMesh->SetupAttachment(Camera);
	WeaponMesh->SetRelativeLocationAndRotation(WeaponOffset, WeaponRotation);
	WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WeaponMesh->SetGenerateOverlapEvents(false);
	WeaponMesh->CastShadow = false;
	WeaponMesh->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::FirstPerson;

	static ConstructorHelpers::FObjectFinder<USkeletalMesh> RevolverMesh(TEXT("/Game/Assets/Revolver/SK_Pax.SK_Pax"));
	if (RevolverMesh.Succeeded())
	{
		WeaponMesh->SetSkeletalMeshAsset(RevolverMesh.Object);
	}

	// Pose comes from code (URevolverComponent), not an Animation Blueprint.
	WeaponMesh->AnimClass = URevolverAnimInstance::StaticClass();

	Revolver = CreateDefaultSubobject<URevolverComponent>(TEXT("Revolver"));

	// The actor stays exactly where it was placed: it never takes the controller's rotation.
	bUseControllerRotationYaw = false;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;

	SetActorEnableCollision(false);

	// Possess automatically when placed in a level (works with any GameMode).
	AutoPossessPlayer = EAutoReceiveInput::Player0;
}

void AFirstPersonCameraPawn::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// Keep the editor preview in sync with the tweakable rest pose.
	WeaponMesh->SetRelativeLocationAndRotation(WeaponOffset, WeaponRotation);
}

void AFirstPersonCameraPawn::BeginPlay()
{
	// Before Super, which runs the components' BeginPlay.
	Revolver->SetupWeapon(WeaponMesh, Camera);
	Revolver->OnFired.AddDynamic(this, &AFirstPersonCameraPawn::HandleWeaponFired);

	Super::BeginPlay();

	// Riding on the balloon: its flight must move first, so this pawn sees the same frame's
	// position (the parent yaw carried below stays in sync with the basket).
	if (const AActor* Parent = GetAttachParentActor())
	{
		if (UBalloonFlightComponent* Flight = Parent->FindComponentByClass<UBalloonFlightComponent>())
		{
			AddTickPrerequisiteComponent(Flight);
		}
	}
}

void AFirstPersonCameraPawn::HandleWeaponFired(bool bHit, const FHitResult& Hit)
{
	RecoilAlpha = 1.f;
}

void AFirstPersonCameraPawn::NotifyControllerChanged()
{
	Super::NotifyControllerChanged();

	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC || !PC->IsLocalController())
	{
		return;
	}

	// Block movement input entirely, start looking where the actor faces.
	PC->SetIgnoreMoveInput(true);
	PC->SetControlRotation(GetActorRotation());

	// Seed the carried parent yaw so the first frame on the balloon is not a jump.
	if (const AActor* Parent = GetAttachParentActor())
	{
		LastParentYaw = Parent->GetActorRotation().Yaw;
		bHasParentYaw = true;
	}
	else
	{
		bHasParentYaw = false;
	}

	// Read the mouse after the controller has processed this frame's input.
	AddTickPrerequisiteActor(PC);

	// The snap to the actor's facing above must not read as a huge flick of the gun.
	bHasLastViewRotation = false;

	// Swing the gun up into view.
	RaiseAlpha = RaiseTime > 0.f ? 1.f : 0.f;

	// Capture the mouse for look.
	PC->SetInputMode(FInputModeGameOnly());
	PC->bShowMouseCursor = false;
}

void AFirstPersonCameraPawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC || !PC->IsLocalController())
	{
		return;
	}

	// Aiming at ammo opens the loading gate, ready for the click that hands over a round.
	const AActor* Aimed = TraceAimedTarget(/*bDebugDraw=*/false);
	const UAmmoSupplyComponent* AimedSupply = Aimed ? Aimed->FindComponentByClass<UAmmoSupplyComponent>() : nullptr;
	Revolver->SetHoldGateOpen(AimedSupply && AimedSupply->HasRounds());

	// The only input besides the mouse look. There is no reload key: rounds come from ammo supplies.
	if (PC->WasInputKeyJustPressed(EKeys::LeftMouseButton) && !TryInteract())
	{
		Revolver->PullTrigger();
	}

	// The view rides along with the parent's turning: carry the parent yaw delta into the
	// control rotation before the mouse look is applied (UpdateLook does not run without
	// mouse movement, so this must not depend on it).
	if (const AActor* Parent = GetAttachParentActor())
	{
		const float ParentYaw = Parent->GetActorRotation().Yaw;
		if (bHasParentYaw)
		{
			const float Delta = FMath::FindDeltaAngleDegrees(LastParentYaw, ParentYaw);
			if (!FMath::IsNearlyZero(Delta))
			{
				const FRotator Rot = PC->GetControlRotation();
				PC->SetControlRotation(FRotator(Rot.Pitch, Rot.Yaw + Delta, 0.f));
			}
		}
		LastParentYaw = ParentYaw;
		bHasParentYaw = true;
	}
	else
	{
		bHasParentYaw = false;
	}

	UpdateLook(PC);
	UpdateWeaponSway(DeltaSeconds);
}

void AFirstPersonCameraPawn::UpdateLook(APlayerController* PC)
{
	if (PC->IsLookInputIgnored())
	{
		return;
	}

	float DX = 0.f, DY = 0.f;
	PC->GetInputMouseDelta(DX, DY);
	if (FMath::IsNearlyZero(DX) && FMath::IsNearlyZero(DY))
	{
		return;
	}

	FRotator Rot = PC->GetControlRotation();
	Rot.Yaw += DX * LookSensitivity;
	Rot.Pitch = FMath::ClampAngle(Rot.Pitch + DY * LookSensitivity * (bInvertY ? -1.f : 1.f), MinPitch, MaxPitch);
	Rot.Roll = 0.f;

	if (bLimitYaw)
	{
		const float StartYaw = GetActorRotation().Yaw;
		const float Offset = FMath::Clamp(FMath::FindDeltaAngleDegrees(StartYaw, Rot.Yaw), -MaxYawFromStart, MaxYawFromStart);
		Rot.Yaw = StartYaw + Offset;
	}

	PC->SetControlRotation(Rot);
}

void AFirstPersonCameraPawn::UpdateWeaponSway(float DeltaSeconds)
{
	const FRotator View = GetViewRotation();
	if (!bHasLastViewRotation)
	{
		LastViewRotation = View;
		bHasLastViewRotation = true;
	}

	// Look speed in degrees/second, so the lag feels the same at any frame rate.
	const float SafeDelta = FMath::Max(DeltaSeconds, KINDA_SMALL_NUMBER);
	const float YawSpeed = FMath::FindDeltaAngleDegrees(LastViewRotation.Yaw, View.Yaw) / SafeDelta;
	const float PitchSpeed = FMath::FindDeltaAngleDegrees(LastViewRotation.Pitch, View.Pitch) / SafeDelta;
	LastViewRotation = View;

	// The gun trails the view: turning right swings it left, looking up dips it down.
	FRotator TargetSway;
	TargetSway.Pitch = FMath::Clamp(-PitchSpeed * SwayAmount, -MaxSwayAngle, MaxSwayAngle);
	TargetSway.Yaw = FMath::Clamp(-YawSpeed * SwayAmount, -MaxSwayAngle, MaxSwayAngle);
	TargetSway.Roll = TargetSway.Yaw * 0.5f; // a little tilt into the drag
	CurrentSway = FMath::RInterpTo(CurrentSway, TargetSway, DeltaSeconds, SwayReturnSpeed);

	// Slow breathing figure-eight while idle.
	const float Time = GetWorld()->GetTimeSeconds() * IdleBobSpeed;
	const FVector Bob(0.f, FMath::Sin(Time * 0.5f) * IdleBobAmount * 0.5f, FMath::Sin(Time) * IdleBobAmount);

	const FVector Lag(0.f, CurrentSway.Yaw * SwayShift, CurrentSway.Pitch * SwayShift);

	// Shot kick: snaps back and up, then eases home.
	RecoilAlpha = FMath::FInterpTo(RecoilAlpha, 0.f, DeltaSeconds, RecoilRecoverySpeed);
	const FVector Kick(-RecoilAlpha * RecoilKickBack, 0.f, 0.f);
	const FRotator KickRotation(RecoilAlpha * RecoilKickPitch, 0.f, 0.f);

	// Tilt the loading gate into view for the whole reload, then bring the gun back.
	const bool bReloading = Revolver && Revolver->IsReloading();
	ReloadPoseAlpha = FMath::FInterpConstantTo(ReloadPoseAlpha, bReloading ? 1.f : 0.f, DeltaSeconds, 1.f / ReloadPoseBlendTime);
	const float PoseBlend = FMath::SmoothStep(0.f, 1.f, ReloadPoseAlpha);
	const FVector PoseOffset = FMath::Lerp(WeaponOffset, ReloadOffset, PoseBlend);
	const FRotator PoseRotation = FQuat::Slerp(WeaponRotation.Quaternion(), ReloadRotation.Quaternion(), PoseBlend).Rotator();

	// Swing up from below on possession.
	if (RaiseTime > 0.f)
	{
		RaiseAlpha = FMath::FInterpConstantTo(RaiseAlpha, 0.f, DeltaSeconds, 1.f / RaiseTime);
	}
	const float Lowered = FMath::Square(RaiseAlpha); // fast at first, settling softly
	const FVector Raise(0.f, 0.f, -Lowered * RaiseDrop);
	const FRotator RaiseRotation(-Lowered * RaisePitch, 0.f, 0.f);

	WeaponMesh->SetRelativeLocationAndRotation(PoseOffset + Raise + Lag + Bob + Kick, PoseRotation + RaiseRotation + CurrentSway + KickRotation);
}

AActor* AFirstPersonCameraPawn::GetAimedInteractable() const
{
	return TraceAimedTarget(bDebugInteractTrace);
}

AActor* AFirstPersonCameraPawn::TraceAimedTarget(bool bDebugDraw) const
{
	const FVector Start = Camera->GetComponentLocation();
	// Use the control rotation: the camera component only picks it up during the camera update,
	// so its own forward vector can be one frame behind a fresh mouse move.
	const FVector End = Start + GetViewRotation().Vector() * InteractRange;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(FirstPersonInteract), /*bTraceComplex=*/false, this);
	FHitResult Hit;
	const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, Start, End, InteractTraceChannel, Params);

	if (bDebugDraw)
	{
		DrawDebugLine(GetWorld(), Start, bHit ? Hit.ImpactPoint : End, bHit ? FColor::Green : FColor::Red, false, 1.f, 0, 1.f);
	}

	// The hit actor itself, or whoever owns it (e.g. an AInteractSwap owns the meshes it swaps).
	for (AActor* Actor = bHit ? Hit.GetActor() : nullptr; Actor; Actor = Actor->GetOwner())
	{
		if (Actor->Implements<UInteractable>() || Actor->FindComponentByClass<UAmmoSupplyComponent>())
		{
			return Actor;
		}
	}
	return nullptr;
}

bool AFirstPersonCameraPawn::TryInteract()
{
	AActor* Target = GetAimedInteractable();
	if (!Target)
	{
		return false;
	}

	bool bHandled = false;

	if (Target->Implements<UInteractable>() && IInteractable::Execute_CanInteract(Target, this))
	{
		IInteractable::Execute_Interact(Target, this);
		bHandled = true;
	}

	// One click, one round. Clicking a supply with a full gun does nothing rather than shooting it.
	if (UAmmoSupplyComponent* Supply = Target->FindComponentByClass<UAmmoSupplyComponent>(); Supply && Supply->HasRounds())
	{
		Supply->GiveRound(this);
		bHandled = true;
	}

	return bHandled;
}
