#include "FirstPersonCameraPawn.h"

#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "AmmoSupplyComponent.h"
#include "BalloonFlightComponent.h"
#include "GameFramework/PlayerController.h"
#include "Grabbable.h"
#include "GunAnimInstance.h"
#include "Interactable.h"
#include "RevolverAnimInstance.h"
#include "RevolverComponent.h"
#include "RifleComponent.h"
#include "ShotgunComponent.h"
#include "WeaponRackComponent.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	/** Hand-less viewmodel mesh hanging off the eyes: no collision, no shadow, drawn as first-person. */
	void SetupViewmodelMesh(USkeletalMeshComponent* Mesh, USceneComponent* Parent)
	{
		Mesh->SetupAttachment(Parent);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetGenerateOverlapEvents(false);
		Mesh->CastShadow = false;
		Mesh->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::FirstPerson;
	}
}

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

	// --- Revolver (key 1) ---
	WeaponMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("WeaponMesh"));
	SetupViewmodelMesh(WeaponMesh, Camera);

	static ConstructorHelpers::FObjectFinder<USkeletalMesh> RevolverMesh(TEXT("/Game/Assets/Revolver/SK_Pax.SK_Pax"));
	if (RevolverMesh.Succeeded())
	{
		WeaponMesh->SetSkeletalMeshAsset(RevolverMesh.Object);
	}

	// Pose comes from code (URevolverComponent), not an Animation Blueprint.
	WeaponMesh->AnimClass = URevolverAnimInstance::StaticClass();

	Revolver = CreateDefaultSubobject<URevolverComponent>(TEXT("Revolver"));

	// --- Rifle (key 2) ---
	RifleMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("RifleMesh"));
	SetupViewmodelMesh(RifleMesh, Camera);

	static ConstructorHelpers::FObjectFinder<USkeletalMesh> MartiniHenryMesh(TEXT("/Game/Assets/Rifle_MartiniHenry/SK_MartiniHenry.SK_MartiniHenry"));
	if (MartiniHenryMesh.Succeeded())
	{
		RifleMesh->SetSkeletalMeshAsset(MartiniHenryMesh.Object);
	}

	// Pose comes from code (URifleComponent), not an Animation Blueprint.
	RifleMesh->AnimClass = UGunAnimInstance::StaticClass();

	Rifle = CreateDefaultSubobject<URifleComponent>(TEXT("Rifle"));

	// --- Shotgun (key 3) ---
	ShotgunMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("ShotgunMesh"));
	SetupViewmodelMesh(ShotgunMesh, Camera);

	static ConstructorHelpers::FObjectFinder<USkeletalMesh> HomesteadMesh(TEXT("/Game/Assets/Homestead_78/SK_Homestead78.SK_Homestead78"));
	if (HomesteadMesh.Succeeded())
	{
		ShotgunMesh->SetSkeletalMeshAsset(HomesteadMesh.Object);
	}

	// Pose comes from code (UShotgunComponent), not an Animation Blueprint.
	ShotgunMesh->AnimClass = UGunAnimInstance::StaticClass();

	Shotgun = CreateDefaultSubobject<UShotgunComponent>(TEXT("Shotgun"));

	WeaponMesh->SetRelativeLocationAndRotation(Revolver->GetViewOffset(), Revolver->GetViewRotation());
	RifleMesh->SetRelativeLocationAndRotation(Rifle->GetViewOffset(), Rifle->GetViewRotation());
	ShotgunMesh->SetRelativeLocationAndRotation(Shotgun->GetViewOffset(), Shotgun->GetViewRotation());
	RifleMesh->SetVisibility(false); // the revolver is in hand by default
	ShotgunMesh->SetVisibility(false);

	// The actor stays exactly where it was placed: it never takes the controller's rotation.
	bUseControllerRotationYaw = false;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;

	SetActorEnableCollision(false);

	// Possess automatically when placed in a level (works with any GameMode).
	AutoPossessPlayer = EAutoReceiveInput::Player0;
}

TArray<UGunComponent*, TInlineAllocator<3>> AFirstPersonCameraPawn::GetGuns() const
{
	return { Revolver.Get(), Rifle.Get(), Shotgun.Get() };
}

TArray<USkeletalMeshComponent*, TInlineAllocator<3>> AFirstPersonCameraPawn::GetGunMeshes() const
{
	return { WeaponMesh.Get(), RifleMesh.Get(), ShotgunMesh.Get() };
}

void AFirstPersonCameraPawn::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// Keep the editor preview in sync with each gun's tweakable rest pose; only the starting gun shows.
	const TArray<UGunComponent*, TInlineAllocator<3>> Guns = GetGuns();
	const TArray<USkeletalMeshComponent*, TInlineAllocator<3>> Meshes = GetGunMeshes();
	for (int32 i = 0; i < Guns.Num(); ++i)
	{
		if (USkeletalMeshComponent* Mesh = Meshes[i])
		{
			Mesh->SetRelativeLocationAndRotation(Guns[i]->GetViewOffset(), Guns[i]->GetViewRotation());
			Mesh->SetVisibility(i == FMath::Clamp(StartingGunIndex, 0, Guns.Num() - 1));
		}
	}
}

void AFirstPersonCameraPawn::BeginPlay()
{
	// Before Super, which runs the components' BeginPlay.
	Revolver->SetupWeapon(WeaponMesh, Camera);
	Rifle->SetupWeapon(RifleMesh, Camera);
	Shotgun->SetupWeapon(ShotgunMesh, Camera);

	const TArray<UGunComponent*, TInlineAllocator<3>> Guns = GetGuns();
	for (UGunComponent* Gun : Guns)
	{
		Gun->OnFired.AddDynamic(this, &AFirstPersonCameraPawn::HandleWeaponFired);
	}

	Super::BeginPlay();

	// Starting with an empty hand means the revolver is on a rack: that is the gun to draw back.
	const bool bStartEmpty = InitialHandState == EPlayerHandState::Empty;
	ActiveGun = bStartEmpty ? Revolver.Get() : Guns[FMath::Clamp(StartingGunIndex, 0, Guns.Num() - 1)];
	PendingGun = nullptr;
	for (UGunComponent* Gun : Guns)
	{
		Gun->SetEquipped(!bStartEmpty && Gun == ActiveGun);
	}

	HandLowerAlpha = bStartEmpty ? 1.f : 0.f;
	SetHandState(bStartEmpty ? EPlayerHandState::Empty : EPlayerHandState::Armed);
	SetupBalloonTickPrerequisite();
}

void AFirstPersonCameraPawn::SetupBalloonTickPrerequisite()
{
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

void AFirstPersonCameraPawn::HandleWeaponFired()
{
	RecoilAlpha = 1.f;

	// Only the gun in hand fires. Its kick throws the aim; part of it settles back by itself afterwards.
	if (ActiveGun)
	{
		const FRotator Kick = ActiveGun->RollAimRecoil();
		PendingAimKick += Kick;
		PendingAimSettle -= Kick * ActiveGun->GetAimRecoilRecovery();
		AimSettleTime = ActiveGun->GetAimRecoilRecoveryTime();
	}
}

void AFirstPersonCameraPawn::NotifyControllerChanged()
{
	Super::NotifyControllerChanged();
	SetupBalloonTickPrerequisite();

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

	// Nor carry over aim motion from before.
	PendingAimKick = FRotator::ZeroRotator;
	PendingAimSettle = FRotator::ZeroRotator;
	bHasAppliedAimSway = false;

	// Swing the gun up into view.
	RaiseAlpha = RaiseTime > 0.f ? 1.f : 0.f;

	// Capture the mouse for look.
	PC->SetInputMode(FInputModeGameOnly());
	PC->bShowMouseCursor = false;
}

void AFirstPersonCameraPawn::EquipGun(UGunComponent* Gun)
{
	if (!Gun || !ActiveGun)
	{
		return;
	}

	if (PendingGun)
	{
		// Mid-swap: picking the gun in hand cancels the swap, picking another redirects it.
		PendingGun = Gun == ActiveGun ? nullptr : Gun;
		return;
	}

	if (Gun != ActiveGun)
	{
		PendingGun = Gun;
		ActiveGun->PrepareToHolster();
	}
}

void AFirstPersonCameraPawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UpdateHandState(DeltaSeconds);

	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC || !PC->IsLocalController() || !ActiveGun)
	{
		return;
	}

	const bool bArmed = HandState == EPlayerHandState::Armed;

	// Number keys pick a gun (not while the hand is empty or busy with the rack).
	if (bArmed)
	{
		if (PC->WasInputKeyJustPressed(EKeys::One))
		{
			EquipGun(Revolver.Get());
		}
		if (PC->WasInputKeyJustPressed(EKeys::Two))
		{
			EquipGun(Rifle.Get());
		}
		if (PC->WasInputKeyJustPressed(EKeys::Three))
		{
			EquipGun(Shotgun.Get());
		}
	}

	if (PendingGun)
	{
		// Putting the gun away: it stops waiting for ammo and takes no clicks.
		ActiveGun->SetAimingAtAmmo(false);
	}
	else
	{
		// Aiming at ammo opens the gun, ready for the RMB click that hands over a round.
		const AActor* Aimed = TraceAimedTarget(/*bDebugDraw=*/false);
		const UAmmoSupplyComponent* AimedSupply = Aimed ? Aimed->FindComponentByClass<UAmmoSupplyComponent>() : nullptr;
		ActiveGun->SetAimingAtAmmo(bArmed && AimedSupply && AimedSupply->HasRounds());

		// There is no reload key: RMB on an ammo supply takes one round from it.
		if (bArmed && PC->WasInputKeyJustPressed(EKeys::RightMouseButton))
		{
			TryTakeRound();
		}

		// LMB grabs a control (empty hand) or uses other interactables, otherwise pulls the trigger
		// (an open gun closes without firing).
		if (PC->WasInputKeyJustPressed(EKeys::LeftMouseButton) && !TryBeginGrab() && !TryInteract() && bArmed)
		{
			ActiveGun->PullTrigger();
		}
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
				AddAimRotation(PC, FRotator(0.f, Delta, 0.f));
			}
		}
		LastParentYaw = ParentYaw;
		bHasParentYaw = true;
	}
	else
	{
		bHasParentYaw = false;
	}

	// While a control is held, the mouse moves it instead of the view.
	const bool bHadGrab = GrabTarget.IsValid();
	UpdateHeldGrab(PC);
	if (!bHadGrab)
	{
		UpdateLook(PC);
	}
	UpdateAimMotion(PC, DeltaSeconds);
	UpdateWeaponSwap(DeltaSeconds);
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

	AddAimRotation(PC, FRotator(DY * LookSensitivity * (bInvertY ? -1.f : 1.f), DX * LookSensitivity, 0.f));
}

void AFirstPersonCameraPawn::AddAimRotation(APlayerController* PC, const FRotator& Delta)
{
	FRotator Rot = PC->GetControlRotation();
	Rot.Yaw += Delta.Yaw;
	Rot.Pitch = FMath::ClampAngle(Rot.Pitch + Delta.Pitch, MinPitch, MaxPitch);
	// Roll belongs to the balloon sway alone (the mouse never rolls).
	Rot.Roll = FRotator::NormalizeAxis(Rot.Roll + Delta.Roll);

	if (bLimitYaw)
	{
		const float StartYaw = GetActorRotation().Yaw;
		const float Offset = FMath::Clamp(FMath::FindDeltaAngleDegrees(StartYaw, Rot.Yaw), -MaxYawFromStart, MaxYawFromStart);
		Rot.Yaw = StartYaw + Offset;
	}

	PC->SetControlRotation(Rot);
}

void AFirstPersonCameraPawn::UpdateAimMotion(APlayerController* PC, float DeltaSeconds)
{
	if (PC->IsLookInputIgnored())
	{
		return;
	}

	FRotator Delta = FRotator::ZeroRotator;

	// Recoil: the kick snaps in fast; once it is in, the settling share eases back (~95% in AimSettleTime).
	const FRotator KickStep = PendingAimKick * FMath::Min(1.f, DeltaSeconds * AimKickSpeed);
	PendingAimKick -= KickStep;
	Delta += KickStep;
	if (PendingAimKick.IsNearlyZero(0.05f))
	{
		const FRotator SettleStep = PendingAimSettle * FMath::Min(1.f, DeltaSeconds * 3.f / FMath::Max(AimSettleTime, 0.05f));
		PendingAimSettle -= SettleStep;
		Delta += SettleStep;
	}

	// Sway: the hand (per gun, blended across swaps) plus the balloon rocking. Smooth noise, each axis on
	// its own stretch of it. Only the change since last tick is applied, so it never drifts the aim away
	// and the mouse stays in charge.
	// An empty hand holds no gun to waver.
	const UGunComponent* HeldGun = HandState != EPlayerHandState::Empty ? ActiveGun.Get() : nullptr;
	const FVector2D TargetHandSway = HeldGun ? HeldGun->GetHandSway() : FVector2D::ZeroVector;
	const float TargetHandSwaySpeed = HeldGun ? HeldGun->GetHandSwaySpeed() : 0.f;
	CurrentHandSway = FMath::Vector2DInterpTo(CurrentHandSway, TargetHandSway, DeltaSeconds, HandSwayBlendSpeed);
	CurrentHandSwaySpeed = FMath::FInterpTo(CurrentHandSwaySpeed, TargetHandSwaySpeed, DeltaSeconds, HandSwayBlendSpeed);
	HandSwayPhase += DeltaSeconds * CurrentHandSwaySpeed;
	BalloonSwayPhase += DeltaSeconds * BalloonSwaySpeed;

	auto Noise = [](float Phase, int32 Axis) { return FMath::PerlinNoise1D(Phase + Axis * 37.31f); };
	const FRotator Sway(
		Noise(HandSwayPhase, 0) * CurrentHandSway.X + Noise(BalloonSwayPhase, 1) * BalloonSwayPitch,
		Noise(HandSwayPhase, 2) * CurrentHandSway.Y + Noise(BalloonSwayPhase, 3) * BalloonSwayYaw,
		Noise(BalloonSwayPhase, 4) * BalloonSwayRoll);

	if (bHasAppliedAimSway)
	{
		Delta += Sway - AppliedAimSway;
	}
	AppliedAimSway = Sway;
	bHasAppliedAimSway = true;

	if (!Delta.IsNearlyZero(1.e-4f))
	{
		AddAimRotation(PC, Delta);
	}
}

void AFirstPersonCameraPawn::UpdateWeaponSwap(float DeltaSeconds)
{
	// Lowering starts only once the gun has finished firing/reloading and closed up.
	if (PendingGun && ActiveGun->IsReadyToHolster())
	{
		RaiseAlpha = LowerTime > 0.f ? FMath::FInterpConstantTo(RaiseAlpha, 1.f, DeltaSeconds, 1.f / LowerTime) : 1.f;
		if (RaiseAlpha >= 1.f)
		{
			// Out of view: swap, then the normal raise brings the new gun up.
			ActiveGun->SetEquipped(false);
			PendingGun->SetEquipped(true);
			ActiveGun = PendingGun;
			PendingGun = nullptr;
			RecoilAlpha = 0.f;
			ReloadPoseAlpha = 0.f;
			if (RaiseTime <= 0.f)
			{
				RaiseAlpha = 0.f;
			}
		}
		return;
	}

	// Swing up from below (on possession, after a swap, or when a swap was cancelled).
	if (RaiseTime > 0.f)
	{
		RaiseAlpha = FMath::FInterpConstantTo(RaiseAlpha, 0.f, DeltaSeconds, 1.f / RaiseTime);
	}
	else
	{
		RaiseAlpha = 0.f;
	}
}

void AFirstPersonCameraPawn::UpdateWeaponSway(float DeltaSeconds)
{
	USkeletalMeshComponent* GunMesh = ActiveGun ? ActiveGun->GetMesh() : nullptr;
	if (!GunMesh)
	{
		return;
	}

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
	const FVector Kick(-RecoilAlpha * ActiveGun->GetRecoilKickBack(), 0.f, 0.f);
	const FRotator KickRotation(RecoilAlpha * ActiveGun->GetRecoilKickPitch(), 0.f, 0.f);

	// Turn the loading port into view for the whole reload, then bring the gun back.
	ReloadPoseAlpha = FMath::FInterpConstantTo(ReloadPoseAlpha, ActiveGun->IsReloading() ? 1.f : 0.f, DeltaSeconds, 1.f / ReloadPoseBlendTime);
	const float PoseBlend = FMath::SmoothStep(0.f, 1.f, ReloadPoseAlpha);
	const FVector PoseOffset = FMath::Lerp(ActiveGun->GetViewOffset(), ActiveGun->GetReloadViewOffset(), PoseBlend);
	const FRotator PoseRotation = FQuat::Slerp(ActiveGun->GetViewRotation().Quaternion(), ActiveGun->GetReloadViewRotation().Quaternion(), PoseBlend).Rotator();

	// Lowered below the view on possession, during a swap and when hung on a rack.
	const float Lowered = FMath::Max(FMath::Square(RaiseAlpha), FMath::Square(HandLowerAlpha)); // fast at first, settling softly
	const FVector Raise(0.f, 0.f, -Lowered * RaiseDrop);
	const FRotator RaiseRotation(-Lowered * RaisePitch, 0.f, 0.f);

	GunMesh->SetRelativeLocationAndRotation(PoseOffset + Raise + Lag + Bob + Kick, PoseRotation + RaiseRotation + CurrentSway + KickRotation);
}

AActor* AFirstPersonCameraPawn::GetAimedInteractable() const
{
	return TraceAimedTarget(bDebugInteractTrace);
}

bool AFirstPersonCameraPawn::TraceAimed(FHitResult& Hit, bool bDebugDraw) const
{
	const FVector Start = Camera->GetComponentLocation();
	// Use the control rotation: the camera component only picks it up during the camera update,
	// so its own forward vector can be one frame behind a fresh mouse move.
	const FVector End = Start + GetViewRotation().Vector() * InteractRange;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(FirstPersonInteract), /*bTraceComplex=*/false, this);
	const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, Start, End, InteractTraceChannel, Params);

	if (bDebugDraw)
	{
		DrawDebugLine(GetWorld(), Start, bHit ? Hit.ImpactPoint : End, bHit ? FColor::Green : FColor::Red, false, 1.f, 0, 1.f);
	}

	return bHit;
}

AActor* AFirstPersonCameraPawn::TraceAimedTarget(bool bDebugDraw) const
{
	FHitResult Hit;
	const bool bHit = TraceAimed(Hit, bDebugDraw);

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
	FHitResult Hit;
	if (!TraceAimed(Hit, bDebugInteractTrace))
	{
		return false;
	}

	// The hit actor itself, or whoever owns it (e.g. an AInteractSwap owns the meshes it swaps).
	AActor* Target = Hit.GetActor();
	while (Target && !Target->Implements<UInteractable>() && !Target->FindComponentByClass<UAmmoSupplyComponent>())
	{
		Target = Target->GetOwner();
	}

	// Ammo supplies (and whatever they sit on, e.g. the crate's lid) belong to RMB: see TryTakeRound.
	if (Target && Target->FindComponentByClass<UAmmoSupplyComponent>())
	{
		return false;
	}

	// Interactable components first (e.g. a weapon rack on the balloon), each checking the part that was hit.
	if (AActor* HitActor = Hit.GetActor())
	{
		TInlineComponentArray<UActorComponent*> Components(HitActor);
		for (UActorComponent* Component : Components)
		{
			if (Component->Implements<UInteractable>()
				&& IInteractable::Execute_AcceptsHit(Component, Hit)
				&& IInteractable::Execute_CanInteract(Component, this))
			{
				IInteractable::Execute_Interact(Component, this);
				return true;
			}
		}
	}

	if (!Target || !Target->Implements<UInteractable>()
		|| !IInteractable::Execute_AcceptsHit(Target, Hit)
		|| !IInteractable::Execute_CanInteract(Target, this))
	{
		return false;
	}

	IInteractable::Execute_Interact(Target, this);
	return true;
}

bool AFirstPersonCameraPawn::TryTakeRound()
{
	AActor* Target = GetAimedInteractable();
	UAmmoSupplyComponent* Supply = Target ? Target->FindComponentByClass<UAmmoSupplyComponent>() : nullptr;
	if (!Supply)
	{
		return false;
	}

	// The same click opens the supply (a crate's lid) and already takes a round.
	if (Target->Implements<UInteractable>() && IInteractable::Execute_CanInteract(Target, this))
	{
		IInteractable::Execute_Interact(Target, this);
	}

	// A gun with a fired case waiting to come out (the shotgun) spends this click pulling it out.
	if (ActiveGun && ActiveGun->EjectCase())
	{
		return false;
	}

	// One click, one round.
	return Supply->GiveRound(this);
}

// --- Grabbing controls (empty hand) -------------------------------------------------------

bool AFirstPersonCameraPawn::TryBeginGrab()
{
	if (!IsHandEmpty())
	{
		return false;
	}
	FHitResult Hit;
	if (!TraceAimed(Hit, bDebugInteractTrace))
	{
		return false;
	}
	AActor* Actor = Hit.GetActor();
	if (!Actor)
	{
		return false;
	}

	auto TryTarget = [this, &Hit](UObject* Target)
	{
		if (!IsValid(Target) || !Target->Implements<UGrabbable>() || !IGrabbable::Execute_CanGrab(Target, this, Hit))
		{
			return false;
		}
		GrabTarget = Target;
		IGrabbable::Execute_BeginGrab(Target, this, Hit);
		// A valid grab attempt consumes the click, even if the target immediately releases it.
		return true;
	};
	TInlineComponentArray<UActorComponent*> Components(Actor);
	for (UActorComponent* Component : Components)
	{
		if (TryTarget(Component))
		{
			return true;
		}
	}
	return TryTarget(Actor);
}

void AFirstPersonCameraPawn::UpdateHeldGrab(APlayerController* PC)
{
	UObject* Target = GrabTarget.Get();
	if (!Target || !IGrabbable::Execute_IsGrabbed(Target))
	{
		GrabTarget.Reset();
		return;
	}
	const float BreakDistance = GrabBreakDistance > 0.f ? GrabBreakDistance : InteractRange * 1.25f;
	if (!PC->IsInputKeyDown(EKeys::LeftMouseButton) || !IsHandEmpty()
		|| FVector::DistSquared(Camera->GetComponentLocation(), IGrabbable::Execute_GetGrabLocation(Target)) > FMath::Square(BreakDistance))
	{
		EndGrab();
		return;
	}
	float DX = 0.f, DY = 0.f;
	PC->GetInputMouseDelta(DX, DY);
	const FQuat View = GetViewRotation().Quaternion();
	const FVector Delta = (View.GetRightVector() * DX + View.GetUpVector() * DY) * GrabDragScale;
	IGrabbable::Execute_UpdateGrab(Target, this, Delta);
	if (!GrabTarget.IsValid() || !IGrabbable::Execute_IsGrabbed(GrabTarget.Get()))
	{
		GrabTarget.Reset();
	}
}

void AFirstPersonCameraPawn::EndGrab()
{
	UObject* Target = GrabTarget.Get();
	GrabTarget.Reset(); // Clear before dispatching events, which may re-enter the pawn.
	if (Target)
	{
		IGrabbable::Execute_EndGrab(Target, this);
	}
}

void AFirstPersonCameraPawn::UnPossessed()
{
	EndGrab();
	Super::UnPossessed();
}

void AFirstPersonCameraPawn::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	EndGrab();
	Super::EndPlay(EndPlayReason);
}

// --- Weapon rack (holstering the revolver) ---------------------------------------------------

void AFirstPersonCameraPawn::SetHandState(EPlayerHandState NewState)
{
	if (HandState == NewState)
	{
		return;
	}
	HandState = NewState;
	if (HandState != EPlayerHandState::Armed && ActiveGun)
	{
		ActiveGun->SetAimingAtAmmo(false);
	}
	OnHandStateChanged.Broadcast(HandState);
}

bool AFirstPersonCameraPawn::HolsterWeapon(UWeaponRackComponent* Rack)
{
	// The rack's hooks take the revolver only; it must be at rest, with no swap under way.
	if (HandState != EPlayerHandState::Armed || !IsValid(Rack) || Rack->HasWeapon()
		|| !ActiveGun || ActiveGun != Revolver || PendingGun
		|| ActiveGun->IsReloading() || !ActiveGun->IsReadyToHolster())
	{
		return false;
	}
	TransitionRack = Rack;
	HandLowerAlpha = 0.f;
	SetHandState(EPlayerHandState::Holstering);
	return true;
}

bool AFirstPersonCameraPawn::DrawWeapon(UWeaponRackComponent* Rack)
{
	if (!IsHandEmpty() || !IsValid(Rack) || !Rack->HasWeapon() || !ActiveGun)
	{
		return false;
	}
	TransitionRack.Reset();
	Rack->SetHasWeapon(false);
	HandLowerAlpha = 0.f;
	RaiseAlpha = RaiseTime > 0.f ? 1.f : 0.f;
	// Place it below the camera before it shows.
	UpdateWeaponSway(0.f);
	ActiveGun->SetEquipped(true);
	SetHandState(EPlayerHandState::Drawing);
	return true;
}

void AFirstPersonCameraPawn::UpdateHandState(float DeltaSeconds)
{
	if (HandState == EPlayerHandState::Holstering)
	{
		if (!TransitionRack.IsValid() || TransitionRack->HasWeapon())
		{
			// The rack disappeared or became occupied: keep the player's gun.
			HandLowerAlpha = 0.f;
			RaiseAlpha = RaiseTime > 0.f ? 1.f : 0.f;
			TransitionRack.Reset();
			SetHandState(EPlayerHandState::Drawing);
			return;
		}
		HandLowerAlpha = HolsterTime > 0.f ? FMath::Min(1.f, HandLowerAlpha + DeltaSeconds / HolsterTime) : 1.f;
		if (HandLowerAlpha >= 1.f)
		{
			// Out of view: it hangs on the rack now (rounds and spent cases kept).
			if (ActiveGun)
			{
				ActiveGun->SetEquipped(false);
			}
			TransitionRack->SetHasWeapon(true);
			TransitionRack.Reset();
			SetHandState(EPlayerHandState::Empty);
		}
	}
	else if (HandState == EPlayerHandState::Drawing && RaiseAlpha <= 0.f)
	{
		SetHandState(EPlayerHandState::Armed);
	}
}
