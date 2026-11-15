#include "FirstPersonCameraPawn.h"

#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "ExtinguisherComponent.h"
#include "AmmoSupplyComponent.h"
#include "BalloonFlightComponent.h"
#include "GameFramework/PlayerController.h"
#include "Grabbable.h"
#include "GunAnimInstance.h"
#include "HookComponent.h"
#include "HookDebugMenu.h"
#include "Engine/GameViewportClient.h"
#include "Interactable.h"
#include "RevolverAnimInstance.h"
#include "RevolverComponent.h"
#include "RifleComponent.h"
#include "ShotgunComponent.h"
#include "WeaponRackComponent.h"
#include "WrenchComponent.h"
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

	// --- Revolver ---
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

	// --- Rifle ---
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

	// --- Shotgun ---
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

	// --- Grappling hook ---
	HookMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("HookMesh"));
	SetupViewmodelMesh(HookMesh, Camera);

	static ConstructorHelpers::FObjectFinder<USkeletalMesh> HookAsset(TEXT("/Game/Assets/Hook/SK_Hook.SK_Hook"));
	if (HookAsset.Succeeded())
	{
		HookMesh->SetSkeletalMeshAsset(HookAsset.Object);
	}

	// Pose comes from code (UHookComponent), not an Animation Blueprint.
	HookMesh->AnimClass = UGunAnimInstance::StaticClass();

	Hook = CreateDefaultSubobject<UHookComponent>(TEXT("Hook"));

	// --- Fire extinguisher ---
	ExtinguisherMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("ExtinguisherMesh"));
	SetupViewmodelMesh(ExtinguisherMesh, Camera);

	static ConstructorHelpers::FObjectFinder<USkeletalMesh> ExtinguisherAsset(TEXT("/Game/Assets/Fire_extinguisher/SK_Extinguisher.SK_Extinguisher"));
	if (ExtinguisherAsset.Succeeded())
	{
		ExtinguisherMesh->SetSkeletalMeshAsset(ExtinguisherAsset.Object);
	}

	// Pose comes from code (UExtinguisherComponent), not an Animation Blueprint.
	ExtinguisherMesh->AnimClass = UGunAnimInstance::StaticClass();

	Extinguisher = CreateDefaultSubobject<UExtinguisherComponent>(TEXT("Extinguisher"));

	// --- Wrench ---
	WrenchMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("WrenchMesh"));
	SetupViewmodelMesh(WrenchMesh, Camera);

	static ConstructorHelpers::FObjectFinder<USkeletalMesh> WrenchAsset(TEXT("/Game/Assets/Wrench/SK_Wrench.SK_Wrench"));
	if (WrenchAsset.Succeeded())
	{
		WrenchMesh->SetSkeletalMeshAsset(WrenchAsset.Object);
	}

	// Pose comes from code (UWrenchComponent), not an Animation Blueprint.
	WrenchMesh->AnimClass = UGunAnimInstance::StaticClass();

	Wrench = CreateDefaultSubobject<UWrenchComponent>(TEXT("Wrench"));

	// The hand is empty by default: every item is hidden until it is taken (OnConstruction, BeginPlay).
	const TArray<UGunComponent*, TInlineAllocator<6>> Guns = GetGuns();
	const TArray<USkeletalMeshComponent*, TInlineAllocator<6>> Meshes = GetGunMeshes();
	for (int32 i = 0; i < Guns.Num(); ++i)
	{
		Meshes[i]->SetRelativeLocationAndRotation(Guns[i]->GetViewOffset(), Guns[i]->GetViewRotation());
		Meshes[i]->SetVisibility(false);
	}

	// The actor stays exactly where it was placed: it never takes the controller's rotation.
	bUseControllerRotationYaw = false;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;

	SetActorEnableCollision(false);

	// Possess automatically when placed in a level (works with any GameMode).
	AutoPossessPlayer = EAutoReceiveInput::Player0;
}

TArray<UGunComponent*, TInlineAllocator<6>> AFirstPersonCameraPawn::GetGuns() const
{
	return { Revolver.Get(), Rifle.Get(), Shotgun.Get(), Hook.Get(), Extinguisher.Get(), Wrench.Get() };
}

TArray<USkeletalMeshComponent*, TInlineAllocator<6>> AFirstPersonCameraPawn::GetGunMeshes() const
{
	return { WeaponMesh.Get(), RifleMesh.Get(), ShotgunMesh.Get(), HookMesh.Get(), ExtinguisherMesh.Get(), WrenchMesh.Get() };
}

UGunComponent* AFirstPersonCameraPawn::FindGun(TSubclassOf<UGunComponent> GunClass) const
{
	if (GunClass)
	{
		for (UGunComponent* Gun : GetGuns())
		{
			if (Gun && Gun->IsA(GunClass))
			{
				return Gun;
			}
		}
	}
	return nullptr;
}

USkeletalMeshComponent* AFirstPersonCameraPawn::GetGunMesh(const UGunComponent* Gun) const
{
	const int32 Index = Gun ? GetGuns().IndexOfByKey(Gun) : INDEX_NONE;
	return Index != INDEX_NONE ? GetGunMeshes()[Index] : nullptr;
}

void AFirstPersonCameraPawn::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// Keep the editor preview in sync with each gun's tweakable rest pose; only the starting gun shows
	// (none with an empty hand).
	const TArray<UGunComponent*, TInlineAllocator<6>> Guns = GetGuns();
	const TArray<USkeletalMeshComponent*, TInlineAllocator<6>> Meshes = GetGunMeshes();
	const int32 ShownIndex = InitialHandState == EPlayerHandState::Empty ? INDEX_NONE : FMath::Clamp(StartingGunIndex, 0, Guns.Num() - 1);
	for (int32 i = 0; i < Guns.Num(); ++i)
	{
		if (USkeletalMeshComponent* Mesh = Meshes[i])
		{
			Mesh->SetRelativeLocationAndRotation(Guns[i]->GetViewOffset(), Guns[i]->GetViewRotation());
			Mesh->SetVisibility(i == ShownIndex);
		}
	}
}

void AFirstPersonCameraPawn::BeginPlay()
{
	// Before Super, which runs the components' BeginPlay.
	Revolver->SetupWeapon(WeaponMesh, Camera);
	Rifle->SetupWeapon(RifleMesh, Camera);
	Shotgun->SetupWeapon(ShotgunMesh, Camera);
	Hook->SetupWeapon(HookMesh, Camera);
	Extinguisher->SetupWeapon(ExtinguisherMesh, Camera);
	Wrench->SetupWeapon(WrenchMesh, Camera);

	const TArray<UGunComponent*, TInlineAllocator<6>> Guns = GetGuns();
	for (UGunComponent* Gun : Guns)
	{
		Gun->OnFired.AddDynamic(this, &AFirstPersonCameraPawn::HandleWeaponFired);
	}

	Super::BeginPlay();

	// Aiming down the sights zooms from here and back.
	BaseFieldOfView = Camera->FieldOfView;
	BaseFirstPersonFieldOfView = Camera->FirstPersonFieldOfView;
	bWantsAimDownSights = false;
	AimAlpha = 0.f;

	// An empty hand still keeps a hidden item as ActiveGun, until the first one is taken from its rack.
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

	UpdateHookDebugMenu(PC);

	if (PendingGun)
	{
		// Putting the gun away: it stops waiting for ammo and takes no clicks.
		ActiveGun->SetAimingAtAmmo(false);
		ActiveGun->SetSecondaryHeld(false);
		bTriggerHeld = false;
		ActiveGun->SetTriggerHeld(false);
		bWantsAimDownSights = false;
	}
	else
	{
		// Aiming at ammo opens the gun, ready for the RMB click that hands over a round.
		const AActor* Aimed = TraceAimedTarget(/*bDebugDraw=*/false);
		const UAmmoSupplyComponent* AimedSupply = Aimed ? Aimed->FindComponentByClass<UAmmoSupplyComponent>() : nullptr;
		ActiveGun->SetAimingAtAmmo(bArmed && AimedSupply && AimedSupply->HasRounds());

		// RMB held anywhere but on ammo (the hook pumps with it; a gun is raised to the eye, unless it is open).
		const bool bSecondaryHeld = bArmed && !AimedSupply && PC->IsInputKeyDown(EKeys::RightMouseButton);
		ActiveGun->SetSecondaryHeld(bSecondaryHeld);
		bWantsAimDownSights = bSecondaryHeld && ActiveGun->CanAimDownSights() && !ActiveGun->IsReloading();

		// There is no reload key: RMB on an ammo supply takes one round from it.
		if (bArmed && PC->WasInputKeyJustPressed(EKeys::RightMouseButton))
		{
			TryTakeRound();
		}

		// LMB grabs a control (empty hand) or uses other interactables (a weapon rack: take, put back or
		// swap the item), otherwise pulls the trigger (an open gun closes without firing).
		if (PC->WasInputKeyJustPressed(EKeys::LeftMouseButton) && !TryBeginGrab() && !TryInteract() && bArmed)
		{
			ActiveGun->PullTrigger();
			bTriggerHeld = true;
		}

		// Held on after that click (the extinguisher sprays while it is).
		bTriggerHeld = bTriggerHeld && bArmed && PC->IsInputKeyDown(EKeys::LeftMouseButton);
		ActiveGun->SetTriggerHeld(bTriggerHeld);
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

void AFirstPersonCameraPawn::CalcCamera(float DeltaTime, FMinimalViewInfo& OutResult)
{
	Super::CalcCamera(DeltaTime, OutResult);

	// A long gun is aimed with the eye on the back of its action: pull the near plane in for the aim, so the
	// action right in front of the eye is not sliced off (the stock behind it is behind the camera anyway).
	// Only near things change, so switching it as the raise starts and after the lowering ends does not show.
	const float AimNear = ActiveGun ? ActiveGun->GetAimNearClipPlane() : 0.f;
	if (AimAlpha > 0.f && AimNear > 0.f)
	{
		OutResult.PerspectiveNearClipPlane = AimNear;
	}
}

void AFirstPersonCameraPawn::UpdateHookDebugMenu(APlayerController* PC)
{
	if (PC->WasInputKeyJustPressed(EKeys::H))
	{
		if (!HookDebugMenu.IsValid())
		{
			HookDebugMenu = MakeShared<FHookDebugMenu>();
		}
		if (HookDebugMenu->IsOpen())
		{
			HookDebugMenu->Close();
		}
		else
		{
			HookDebugMenu->Open(Hook, GetWorld()->GetGameViewport());
		}
	}
	if (HookDebugMenu.IsValid())
	{
		HookDebugMenu->HandleInput(PC);
	}
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

	// Zoomed in, the same mouse move turns the view less, so the world moves the same under the cursor.
	float Sensitivity = LookSensitivity;
	if (bScaleLookWithAimZoom && Camera->FieldOfView < BaseFieldOfView)
	{
		const float BaseTan = FMath::Tan(FMath::DegreesToRadians(BaseFieldOfView * 0.5f));
		const float ZoomTan = FMath::Tan(FMath::DegreesToRadians(Camera->FieldOfView * 0.5f));
		Sensitivity *= BaseTan > KINDA_SMALL_NUMBER ? ZoomTan / BaseTan : 1.f;
	}

	AddAimRotation(PC, FRotator(DY * Sensitivity * (bInvertY ? -1.f : 1.f), DX * Sensitivity, 0.f));
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
	UWeaponRackComponent* NextRack = TransitionRack.Get();
	UWeaponRackComponent* HomeRack = HeldItemRack.Get();
	if (PendingGun && (!NextRack || !NextRack->HasWeapon() || !HomeRack || HomeRack->HasWeapon()))
	{
		// A rack of the swap is gone or changed in the meantime: keep the item in hand.
		PendingGun = nullptr;
		TransitionRack.Reset();
		ActiveGun->CancelHolster();
	}

	// Lowering starts only once the gun has finished firing/reloading and closed up.
	if (PendingGun && ActiveGun->IsReadyToHolster())
	{
		// A gun whose own unequip clip has already taken it out of view is swapped at once.
		const bool bOwnSwap = ActiveGun->AnimatesOwnSwap();
		RaiseAlpha = LowerTime > 0.f && !bOwnSwap ? FMath::FInterpConstantTo(RaiseAlpha, 1.f, DeltaSeconds, 1.f / LowerTime) : 1.f;
		if (RaiseAlpha >= 1.f)
		{
			// Out of view: the old item is back on its rack and the next one leaves its own, then the
			// normal raise brings it up.
			HomeRack->SetHasWeapon(true);
			NextRack->SetHasWeapon(false);
			HeldItemRack = NextRack;
			TransitionRack.Reset();
			ActiveGun->SetEquipped(false);
			PendingGun->SetEquipped(true);
			ActiveGun = PendingGun;
			PendingGun = nullptr;
			RecoilAlpha = 0.f;
			ReloadPoseAlpha = 0.f;
			AimAlpha = 0.f;
			if (RaiseTime <= 0.f || ActiveGun->AnimatesOwnSwap())
			{
				// (Its equip clip brings it up by itself.)
				RaiseAlpha = 0.f;
			}
		}
		return;
	}

	// Swing up from below (on possession, after a swap, or when putting the item away was called off).
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

	// Aim down the sights: lift the gun from the hip onto the eye line and zoom the view in with it.
	const bool bAim = bWantsAimDownSights && ActiveGun->CanAimDownSights();
	AimAlpha = FMath::FInterpConstantTo(AimAlpha, bAim ? 1.f : 0.f, DeltaSeconds, 1.f / FMath::Max(ActiveGun->GetAimTime(), 0.01f));
	const float AimBlend = FMath::SmoothStep(0.f, 1.f, AimAlpha);
	const float AimFov = ActiveGun->GetAimFieldOfView() > 0.f ? ActiveGun->GetAimFieldOfView() : BaseFieldOfView;
	const float Fov = FMath::Lerp(BaseFieldOfView, AimFov, AimBlend);
	if (!FMath::IsNearlyEqual(Camera->FieldOfView, Fov))
	{
		Camera->SetFieldOfView(Fov);
	}

	// The move toward or away from the eye (X) and the viewmodel FOV lead (ease-out), so a long gun is already
	// at its aimed depth before it swings in front of the eye: its stock passes the camera off to the side,
	// never across the middle of the screen.
	const float ForwardBlend = 1.f - FMath::Cube(1.f - AimBlend);
	const float AimFirstPersonFov = ActiveGun->GetAimFirstPersonFieldOfView() > 0.f ? ActiveGun->GetAimFirstPersonFieldOfView() : BaseFirstPersonFieldOfView;
	const float FirstPersonFov = FMath::Lerp(BaseFirstPersonFieldOfView, AimFirstPersonFov, ForwardBlend);
	if (!FMath::IsNearlyEqual(Camera->FirstPersonFieldOfView, FirstPersonFov))
	{
		Camera->SetFirstPersonFieldOfView(FirstPersonFov);
	}

	// Aimed, the hand steadies the gun: less trailing, breathing and muzzle flip on screen.
	const float SwayScale = FMath::Lerp(1.f, AimSwayScale, AimBlend);
	const FRotator AppliedSway = CurrentSway * SwayScale;

	// Slow breathing figure-eight while idle.
	const float Time = GetWorld()->GetTimeSeconds() * IdleBobSpeed;
	const float BobAmount = IdleBobAmount * FMath::Lerp(1.f, AimBobScale, AimBlend);
	const FVector Bob(0.f, FMath::Sin(Time * 0.5f) * BobAmount * 0.5f, FMath::Sin(Time) * BobAmount);

	const FVector Lag(0.f, AppliedSway.Yaw * SwayShift, AppliedSway.Pitch * SwayShift);

	// Shot kick: snaps back and up, then eases home.
	RecoilAlpha = FMath::FInterpTo(RecoilAlpha, 0.f, DeltaSeconds, RecoilRecoverySpeed);
	// (Aimed, less of it: the sights come back sooner, and a long gun's stock stays clear of the near plane.)
	const float KickScale = FMath::Lerp(1.f, AimKickScale, AimBlend);
	const FVector Kick(-RecoilAlpha * ActiveGun->GetRecoilKickBack() * KickScale, 0.f, 0.f);
	const FRotator KickRotation(RecoilAlpha * ActiveGun->GetRecoilKickPitch() * KickScale, 0.f, 0.f);

	// The hold: hip or aimed. On the way between, the gun dips and rolls in a little, like being brought up.
	const float AimArc = FMath::Sin(AimBlend * UE_PI);
	FVector HoldOffset = FMath::Lerp(ActiveGun->GetViewOffset(), ActiveGun->GetAimViewOffset(), AimBlend) + FVector(0.f, 0.f, -AimArc * AimTransitionDip);
	HoldOffset.X = FMath::Lerp(ActiveGun->GetViewOffset().X, ActiveGun->GetAimViewOffset().X, ForwardBlend);
	const FQuat HoldRotation = FQuat::Slerp(ActiveGun->GetViewRotation().Quaternion(), ActiveGun->GetAimViewRotation().Quaternion(), AimBlend)
		* FRotator(0.f, 0.f, AimArc * AimTransitionRoll).Quaternion();

	// Turn the loading port into view for the whole reload, then bring the gun back.
	ReloadPoseAlpha = FMath::FInterpConstantTo(ReloadPoseAlpha, ActiveGun->IsReloading() ? 1.f : 0.f, DeltaSeconds, 1.f / ReloadPoseBlendTime);
	const float PoseBlend = FMath::SmoothStep(0.f, 1.f, ReloadPoseAlpha);
	const FVector PoseOffset = FMath::Lerp(HoldOffset, ActiveGun->GetReloadViewOffset(), PoseBlend);
	const FRotator PoseRotation = FQuat::Slerp(HoldRotation, ActiveGun->GetReloadViewRotation().Quaternion(), PoseBlend).Rotator();

	// Lowered below the view on possession, during a swap and when hung on a rack.
	const float Lowered = FMath::Max(FMath::Square(RaiseAlpha), FMath::Square(HandLowerAlpha)); // fast at first, settling softly
	const FVector Raise(0.f, 0.f, -Lowered * RaiseDrop);
	const FRotator RaiseRotation(-Lowered * RaisePitch, 0.f, 0.f);

	GunMesh->SetRelativeLocationAndRotation(PoseOffset + Raise + Lag + Bob + Kick, PoseRotation + RaiseRotation + AppliedSway + KickRotation);
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

	// An interactable component on the hit actor (a weapon rack), if it takes this part of the actor.
	if (AActor* HitActor = bHit ? Hit.GetActor() : nullptr)
	{
		TInlineComponentArray<UActorComponent*> Components(HitActor);
		for (UActorComponent* Component : Components)
		{
			if (Component->Implements<UInteractable>() && IInteractable::Execute_AcceptsHit(Component, Hit))
			{
				return HitActor;
			}
		}
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
	if (HookDebugMenu.IsValid())
	{
		HookDebugMenu->Close();
		HookDebugMenu.Reset();
	}
	Super::EndPlay(EndPlayReason);
}

// --- Weapon racks (taking an item, putting it back, swapping) --------------------------------

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
		ActiveGun->SetSecondaryHeld(false);
		bTriggerHeld = false;
		ActiveGun->SetTriggerHeld(false);
		bWantsAimDownSights = false;
	}
	OnHandStateChanged.Broadcast(HandState);
}

bool AFirstPersonCameraPawn::HolsterWeapon(UWeaponRackComponent* Rack)
{
	// A rack takes its own kind of item only; nothing is put away while open for loading or mid-swap.
	if (HandState != EPlayerHandState::Armed || !IsValid(Rack) || Rack->HasWeapon()
		|| !ActiveGun || ActiveGun != FindGun(Rack->ItemClass) || PendingGun || ActiveGun->IsReloading())
	{
		return false;
	}
	TransitionRack = Rack;
	HandLowerAlpha = 0.f;
	// It finishes what it is doing first (a shot, the hook coming back, the extinguisher's own Unequip).
	ActiveGun->PrepareToHolster();
	SetHandState(EPlayerHandState::Holstering);
	return true;
}

bool AFirstPersonCameraPawn::DrawWeapon(UWeaponRackComponent* Rack)
{
	UGunComponent* Gun = IsValid(Rack) && Rack->HasWeapon() ? FindGun(Rack->ItemClass) : nullptr;
	if (!IsHandEmpty() || !Gun)
	{
		return false;
	}
	TransitionRack.Reset();
	Rack->SetHasWeapon(false);
	HeldItemRack = Rack;
	ActiveGun = Gun;
	RecoilAlpha = 0.f;
	ReloadPoseAlpha = 0.f;
	AimAlpha = 0.f;
	HandLowerAlpha = 0.f;
	// (An item with its own Equip clip comes up by itself.)
	RaiseAlpha = RaiseTime > 0.f && !Gun->AnimatesOwnSwap() ? 1.f : 0.f;
	// Place it below the camera before it shows.
	UpdateWeaponSway(0.f);
	ActiveGun->SetEquipped(true);
	SetHandState(EPlayerHandState::Drawing);
	return true;
}

bool AFirstPersonCameraPawn::SwapWeapon(UWeaponRackComponent* Rack)
{
	UGunComponent* Gun = IsValid(Rack) && Rack->HasWeapon() ? FindGun(Rack->ItemClass) : nullptr;
	const UWeaponRackComponent* HomeRack = HeldItemRack.Get();
	// The item in hand goes back where it was taken from, so that rack has to be there and free.
	if (HandState != EPlayerHandState::Armed || !Gun || !ActiveGun || Gun == ActiveGun || PendingGun
		|| ActiveGun->IsReloading() || !HomeRack || HomeRack->HasWeapon())
	{
		return false;
	}
	TransitionRack = Rack;
	PendingGun = Gun;
	ActiveGun->PrepareToHolster();
	return true;
}

void AFirstPersonCameraPawn::UpdateHandState(float DeltaSeconds)
{
	if (HandState == EPlayerHandState::Holstering)
	{
		if (!TransitionRack.IsValid() || TransitionRack->HasWeapon() || !ActiveGun)
		{
			// The rack disappeared or became occupied: keep the item, raising it back from where it got to.
			if (ActiveGun)
			{
				ActiveGun->CancelHolster();
			}
			RaiseAlpha = HandLowerAlpha;
			HandLowerAlpha = 0.f;
			TransitionRack.Reset();
			SetHandState(EPlayerHandState::Drawing);
			return;
		}
		if (!ActiveGun->IsReadyToHolster())
		{
			// Still finishing and closing up.
			return;
		}
		// An item whose own unequip clip has already taken it out of view is not lowered as well.
		HandLowerAlpha = HolsterTime > 0.f && !ActiveGun->AnimatesOwnSwap() ? FMath::Min(1.f, HandLowerAlpha + DeltaSeconds / HolsterTime) : 1.f;
		if (HandLowerAlpha >= 1.f)
		{
			// Out of view: it lies on the rack now (rounds, spent cases, pressure and contents kept).
			ActiveGun->SetEquipped(false);
			TransitionRack->SetHasWeapon(true);
			TransitionRack.Reset();
			HeldItemRack.Reset();
			SetHandState(EPlayerHandState::Empty);
		}
	}
	else if (HandState == EPlayerHandState::Drawing && RaiseAlpha <= 0.f)
	{
		SetHandState(EPlayerHandState::Armed);
	}
}
