#include "FirstPersonCameraPawn.h"

#include "Camera/CameraComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Interactable.h"

AFirstPersonCameraPawn::AFirstPersonCameraPawn()
{
	PrimaryActorTick.bCanEverTick = true;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	SetRootComponent(Camera);
	Camera->bUsePawnControlRotation = true; // camera follows the controller's look rotation

	// The actor stays exactly where it was placed: it never takes the controller's rotation.
	bUseControllerRotationYaw = false;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;

	SetActorEnableCollision(false);

	// Possess automatically when placed in a level (works with any GameMode).
	AutoPossessPlayer = EAutoReceiveInput::Player0;
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

	// Read the mouse after the controller has processed this frame's input.
	AddTickPrerequisiteActor(PC);

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

	if (PC->WasInputKeyJustPressed(EKeys::LeftMouseButton))
	{
		TryInteract();
	}

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

AActor* AFirstPersonCameraPawn::GetAimedInteractable() const
{
	const FVector Start = Camera->GetComponentLocation();
	// Use the control rotation: the camera component only picks it up during the camera update,
	// so its own forward vector can be one frame behind a fresh mouse move.
	const FVector End = Start + GetViewRotation().Vector() * InteractRange;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(FirstPersonInteract), /*bTraceComplex=*/false, this);
	FHitResult Hit;
	const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, Start, End, InteractTraceChannel, Params);

	if (bDebugInteractTrace)
	{
		DrawDebugLine(GetWorld(), Start, bHit ? Hit.ImpactPoint : End, bHit ? FColor::Green : FColor::Red, false, 1.f, 0, 1.f);
	}

	// The hit actor itself, or whoever owns it (e.g. an AInteractSwap owns the meshes it swaps).
	for (AActor* Actor = bHit ? Hit.GetActor() : nullptr; Actor; Actor = Actor->GetOwner())
	{
		if (Actor->Implements<UInteractable>())
		{
			return Actor;
		}
	}
	return nullptr;
}

bool AFirstPersonCameraPawn::TryInteract()
{
	AActor* Target = GetAimedInteractable();
	if (!Target || !IInteractable::Execute_CanInteract(Target, this))
	{
		return false;
	}

	IInteractable::Execute_Interact(Target, this);
	return true;
}
