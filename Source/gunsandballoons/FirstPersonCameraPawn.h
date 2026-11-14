#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "FirstPersonCameraPawn.generated.h"

class UCameraComponent;
class UGunComponent;
class URevolverComponent;
class URifleComponent;
class UShotgunComponent;
class USkeletalMeshComponent;
class UWeaponRackComponent;

/** What the player's hand holds: a gun, or nothing (the revolver hangs on a weapon rack), and the moves between. */
UENUM(BlueprintType)
enum class EPlayerHandState : uint8 { Armed, Holstering, Empty, Drawing };

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnHandStateChanged, EPlayerHandState, HandState);

/**
 * A bodiless, stationary first-person camera.
 * - No body, no collision, no movement component: WASD/any move input does nothing.
 * - Mouse X/Y rotates the view (yaw/pitch). The actor itself never moves or rotates.
 * - Needs no Input Action / Mapping Context assets: it reads the mouse delta directly.
 * - Holds a hand-less weapon viewmodel (Dusk style): the equipped gun floats bottom-right in front
 *   of the eyes, swings up into view on possession, lags behind fast look movement, breathes slightly
 *   while idle, kicks when fired and turns its loading port toward the eye while reloading.
 *   Where each gun sits and how hard it kicks is set on the gun component (UGunComponent).
 * - Three guns: 1 = Pax revolver (SK_Pax), 2 = Martini-Henry rifle (SK_MartiniHenry),
 *   3 = Homestead 78 double-barrel shotgun (SK_Homestead78). A swap lets the
 *   current gun finish and close up, lowers it out of view, then raises the other one.
 * - LMB interacts with the IInteractable under the crosshair, otherwise fires.
 *   There is no reload key: reloading is RMB on an ammo supply (UAmmoSupplyComponent), one round per click.
 * - The revolver can be hung on a weapon rack (UWeaponRackComponent) to free the hand; with an empty
 *   hand LMB grabs and drags IGrabbable controls (the balloon's levers) instead of firing.
 * - Riding on the balloon, the view turns along with it.
 */
UCLASS(Blueprintable)
class GUNSANDBALLOONS_API AFirstPersonCameraPawn : public APawn
{
	GENERATED_BODY()

public:
	AFirstPersonCameraPawn();

	virtual void Tick(float DeltaSeconds) override;
	virtual void NotifyControllerChanged() override;
	virtual void UnPossessed() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UCameraComponent* GetCamera() const { return Camera; }
	USkeletalMeshComponent* GetWeaponMesh() const { return WeaponMesh; }
	USkeletalMeshComponent* GetRifleMesh() const { return RifleMesh; }
	USkeletalMeshComponent* GetShotgunMesh() const { return ShotgunMesh; }
	URevolverComponent* GetRevolver() const { return Revolver; }
	URifleComponent* GetRifle() const { return Rifle; }
	UShotgunComponent* GetShotgun() const { return Shotgun; }

	/** The gun in hand (the one still being put away during a swap). */
	UFUNCTION(BlueprintPure, Category = "Weapon")
	UGunComponent* GetActiveGun() const { return ActiveGun; }

	/**
	 * Swap to this gun: the current one finishes what it is doing and closes, is lowered out of view,
	 * then this one is raised. Picking the gun in hand again mid-swap cancels the swap.
	 */
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void EquipGun(UGunComponent* Gun);

	/** The IInteractable actor or ammo supply under the crosshair within InteractRange, or null. */
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	AActor* GetAimedInteractable() const;

	/**
	 * Interact with the IInteractable under the crosshair, unless it is an ammo supply (those are RMB).
	 * Called on LMB. Returns true if the click was used, so the gun does not fire.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	bool TryInteract();

	/**
	 * Take one round for the gun in hand from the ammo supply under the crosshair (interacting with it
	 * too, e.g. opening a crate's lid). Called on RMB. If the gun has a fired case to pull out by hand
	 * (UGunComponent::EjectCase), the click does that instead and takes nothing. Returns true if the gun took the round.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	bool TryTakeRound();

	/**
	 * Hang the gun in hand on the rack: lowered over HolsterTime, then hidden and shown on the rack.
	 * Only the revolver fits a rack, and only while it is at rest and no swap is running.
	 */
	UFUNCTION(BlueprintCallable, Category = "Weapon|Hand")
	bool HolsterWeapon(UWeaponRackComponent* Rack);

	/** Take the revolver back from the rack (its rounds kept) and raise it. Needs an empty hand. */
	UFUNCTION(BlueprintCallable, Category = "Weapon|Hand")
	bool DrawWeapon(UWeaponRackComponent* Rack);

	UFUNCTION(BlueprintPure, Category = "Weapon|Hand")
	EPlayerHandState GetHandState() const { return HandState; }

	UFUNCTION(BlueprintPure, Category = "Weapon|Hand")
	bool IsHandEmpty() const { return HandState == EPlayerHandState::Empty; }

	UPROPERTY(BlueprintAssignable, Category = "Weapon|Hand")
	FOnHandStateChanged OnHandStateChanged;

protected:
	/** Only Armed and Empty are valid initial states. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Hand")
	EPlayerHandState InitialHandState = EPlayerHandState::Armed;

	/** TBD: prototype duration, needs tuning. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Hand", meta = (ClampMin = "0.0"))
	float HolsterTime = 0.35f;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Weapon|Hand")
	EPlayerHandState HandState = EPlayerHandState::Armed;

	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;

	/** GetAimedInteractable, with the debug line optional (it runs every frame for the loading gate). */
	AActor* TraceAimedTarget(bool bDebugDraw) const;

	/** The interaction trace from the eye along the view. */
	bool TraceAimed(FHitResult& OutHit, bool bDebugDraw) const;

	/** Applies mouse look to the control rotation. */
	void UpdateLook(APlayerController* PC);

	/**
	 * Moves the aim itself (not just the viewmodel): the gun's recoil kick and its partial settle, the
	 * hand sway of the gun in hand and the balloon's sway. The player's mouse fights all of it.
	 */
	void UpdateAimMotion(APlayerController* PC, float DeltaSeconds);

	/** Adds to the control rotation within the look limits (pitch clamp, optional yaw limit). */
	void AddAimRotation(APlayerController* PC, const FRotator& Delta);

	/** Places the weapon in front of the eyes (rest or reload pose), plus raise, look lag, idle breathing and recoil. */
	void UpdateWeaponSway(float DeltaSeconds);

	/** Lowers the gun being put away, and once it is out of view hides it and brings out the next one. */
	void UpdateWeaponSwap(float DeltaSeconds);

	/** The guns in key order: 1 = revolver, 2 = rifle, 3 = shotgun. */
	TArray<UGunComponent*, TInlineAllocator<3>> GetGuns() const;

	/** Each gun's mesh, in the same order as GetGuns. */
	TArray<USkeletalMeshComponent*, TInlineAllocator<3>> GetGunMeshes() const;

	UFUNCTION()
	void HandleWeaponFired();

	/** How far from the camera (cm) the player can interact. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction", meta = (ClampMin = "0.0"))
	float InteractRange = 400.f;

	/** TBD: prototype centimeters per mouse delta unit, needs tuning. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Grab", meta = (ClampMin = "0.0"))
	float GrabDragScale = 0.5f;

	/** TBD: break distance in cm; 0 uses InteractRange * 1.25 (needs tuning). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Grab", meta = (ClampMin = "0.0"))
	float GrabBreakDistance = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	TEnumAsByte<ECollisionChannel> InteractTraceChannel = ECC_Visibility;

	/** Draw the interaction trace for 1s on each click. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	bool bDebugInteractTrace = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UCameraComponent> Camera;

	/** Degrees of rotation per unit of processed mouse delta. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Look", meta = (ClampMin = "0.0"))
	float LookSensitivity = 2.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Look")
	bool bInvertY = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Look", meta = (ClampMin = "-89.9", ClampMax = "0.0"))
	float MinPitch = -89.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Look", meta = (ClampMin = "0.0", ClampMax = "89.9"))
	float MaxPitch = 89.f;

	/** If true, yaw is limited to +/- MaxYawFromStart around the actor's placed facing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Look")
	bool bLimitYaw = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Look", meta = (EditCondition = "bLimitYaw", ClampMin = "0.0", ClampMax = "180.0"))
	float MaxYawFromStart = 90.f;

	// --- Weapon viewmodel ---------------------------------------------------------------

	/** The revolver, attached to the camera. Rendered as a first-person primitive so it never clips into walls. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<USkeletalMeshComponent> WeaponMesh;

	/** Revolver chambers, firing, reloading; drives WeaponMesh's animation. Key 1. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<URevolverComponent> Revolver;

	/** The rifle, attached to the camera like WeaponMesh. Hidden until equipped. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<USkeletalMeshComponent> RifleMesh;

	/** Rifle chamber, firing, reloading; drives RifleMesh's animation. Key 2. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<URifleComponent> Rifle;

	/** The double-barrel shotgun, attached to the camera like WeaponMesh. Hidden until equipped. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<USkeletalMeshComponent> ShotgunMesh;

	/** Shotgun barrels, firing, reloading; drives ShotgunMesh's animation. Key 3. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<UShotgunComponent> Shotgun;

	/** Gun in hand at BeginPlay: 0 = revolver, 1 = rifle, 2 = shotgun. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon", meta = (ClampMin = "0", ClampMax = "2"))
	int32 StartingGunIndex = 0;

	/** Seconds to lower a gun out of view when swapping. Raising the next one takes RaiseTime. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Raise", meta = (ClampMin = "0.0"))
	float LowerTime = 0.25f;

	/** Seconds to move into or out of the reload pose. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Reload Pose", meta = (ClampMin = "0.01"))
	float ReloadPoseBlendTime = 0.25f;

	/** Seconds for the weapon to swing up into view when the pawn is possessed. 0 disables it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Raise", meta = (ClampMin = "0.0"))
	float RaiseTime = 0.4f;

	/** How far below the rest position (cm) the weapon starts its raise. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Raise", meta = (ClampMin = "0.0"))
	float RaiseDrop = 20.f;

	/** How far (degrees) the muzzle is tipped down at the start of the raise. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Raise", meta = (ClampMin = "0.0"))
	float RaisePitch = 35.f;

	/** Degrees the weapon lags per degree/second of look speed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Sway", meta = (ClampMin = "0.0"))
	float SwayAmount = 0.012f;

	/** Largest lag angle (degrees). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Sway", meta = (ClampMin = "0.0"))
	float MaxSwayAngle = 5.f;

	/** How fast the weapon catches up with the view. Higher = snappier. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Sway", meta = (ClampMin = "0.0"))
	float SwayReturnSpeed = 9.f;

	/** How far (cm) the weapon slides sideways per degree of lag. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Sway", meta = (ClampMin = "0.0"))
	float SwayShift = 0.15f;

	/** Idle breathing height (cm). 0 disables it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Sway", meta = (ClampMin = "0.0"))
	float IdleBobAmount = 0.25f;

	/** Idle breathing speed (radians/second). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Sway", meta = (ClampMin = "0.0"))
	float IdleBobSpeed = 1.6f;

	/** How fast the gun settles after a shot (the kick itself is set per gun). Higher = snappier. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Recoil", meta = (ClampMin = "0.0"))
	float RecoilRecoverySpeed = 12.f;

	/** How fast a gun's aim kick (AimRecoilPitch/Yaw) is applied. Higher = sharper snap. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Recoil", meta = (ClampMin = "1.0"))
	float AimKickSpeed = 30.f;

	// --- Balloon sway ---------------------------------------------------------------------
	// The basket rocking under the player, added to the aim on top of the gun's hand sway. Placeholder
	// motion for now: once the balloon flies, drive it from the basket's real movement.

	/** How far (degrees) the basket rocks the aim up/down. 0 turns that axis off. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aim|Balloon Sway", meta = (ClampMin = "0.0", Units = "Degrees"))
	float BalloonSwayPitch = 1.f;

	/** How far (degrees) the basket turns the aim left/right. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aim|Balloon Sway", meta = (ClampMin = "0.0", Units = "Degrees"))
	float BalloonSwayYaw = 0.8f;

	/** How far (degrees) the basket tilts the horizon. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aim|Balloon Sway", meta = (ClampMin = "0.0", Units = "Degrees"))
	float BalloonSwayRoll = 1.5f;

	/** Balloon sway speed: roughly how many direction changes per second (slow and heavy). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aim|Balloon Sway", meta = (ClampMin = "0.0"))
	float BalloonSwaySpeed = 0.12f;

	/** How fast the hand sway blends to a new gun's amount after a swap. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aim", meta = (ClampMin = "0.0"))
	float HandSwayBlendSpeed = 2.f;

private:
	friend class FPlayerGrabTest;

	/** Starts dragging the IGrabbable under the crosshair (empty hand only). True if the click was used. */
	bool TryBeginGrab();
	/** Moves the held control with the mouse; lets go when LMB is released or the player is too far. */
	void UpdateHeldGrab(APlayerController* PC);
	void EndGrab();
	TWeakObjectPtr<UObject> GrabTarget;

	void SetHandState(EPlayerHandState NewState);
	/** Lowers the gun being holstered and finishes holstering/drawing. */
	void UpdateHandState(float DeltaSeconds);
	TWeakObjectPtr<UWeaponRackComponent> TransitionRack;
	/** 0 = in hand, 1 = lowered out of view for the rack (linear; eased when applied). */
	float HandLowerAlpha = 0.f;

	/** Ensure the balloon moves before the rider reads its transform. */
	void SetupBalloonTickPrerequisite();
	/** Parent actor's yaw last frame; the view carries along with the balloon's turning. */
	float LastParentYaw = 0.f;
	bool bHasParentYaw = false;

	/** The gun in hand. */
	UPROPERTY(Transient)
	TObjectPtr<UGunComponent> ActiveGun;

	/** The gun a swap is bringing out; null when not swapping. */
	UPROPERTY(Transient)
	TObjectPtr<UGunComponent> PendingGun;

	float RecoilAlpha = 0.f;
	/** 0 = rest pose, 1 = reload pose (linear; eased when applied). */
	float ReloadPoseAlpha = 0.f;
	/** 1 = fully lowered, 0 = raised (linear; eased when applied). */
	float RaiseAlpha = 0.f;
	FRotator CurrentSway = FRotator::ZeroRotator;
	FRotator LastViewRotation = FRotator::ZeroRotator;
	bool bHasLastViewRotation = false;

	/** Aim kick still to apply, and the share of past kicks still to settle back. */
	FRotator PendingAimKick = FRotator::ZeroRotator;
	FRotator PendingAimSettle = FRotator::ZeroRotator;
	float AimSettleTime = 0.5f;

	/**
	 * Aim sway: position along each noise (advanced by its speed, so a speed change never jumps), the
	 * offset applied so far (only its change is applied each tick), the hand sway amount in use.
	 */
	float HandSwayPhase = 0.f;
	float BalloonSwayPhase = 0.f;
	FRotator AppliedAimSway = FRotator::ZeroRotator;
	bool bHasAppliedAimSway = false;
	FVector2D CurrentHandSway = FVector2D::ZeroVector;
	float CurrentHandSwaySpeed = 0.7f;
};
