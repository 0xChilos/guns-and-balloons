#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "FirstPersonCameraPawn.generated.h"

class UCameraComponent;
class URevolverComponent;
class USkeletalMeshComponent;

/**
 * A bodiless, stationary first-person camera.
 * - No body, no collision, no movement component: WASD/any move input does nothing.
 * - Mouse X/Y rotates the view (yaw/pitch). The actor itself never moves or rotates.
 * - Needs no Input Action / Mapping Context assets: it reads the mouse delta directly.
 * - Holds a hand-less weapon viewmodel (Dusk style): the gun (SK_Pax) floats bottom-right in front
 *   of the eyes, swings up into view on possession, lags behind fast look movement, breathes slightly
 *   while idle, kicks when fired and tilts its loading gate toward the eye while reloading.
 * - LMB interacts with the IInteractable / UAmmoSupplyComponent under the crosshair, otherwise fires.
 *   No keyboard: reloading is clicking an ammo supply, one round per click.
 */
UCLASS(Blueprintable)
class GUNSANDBALLOONS_API AFirstPersonCameraPawn : public APawn
{
	GENERATED_BODY()

public:
	AFirstPersonCameraPawn();

	virtual void Tick(float DeltaSeconds) override;
	virtual void NotifyControllerChanged() override;

	UCameraComponent* GetCamera() const { return Camera; }
	USkeletalMeshComponent* GetWeaponMesh() const { return WeaponMesh; }
	URevolverComponent* GetRevolver() const { return Revolver; }

	/** The IInteractable actor or ammo supply under the crosshair within InteractRange, or null. */
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	AActor* GetAimedInteractable() const;

	/**
	 * Interact with whatever is under the crosshair (and take a round if it is an ammo supply).
	 * Called on LMB. Returns true if the click was used, so the gun does not fire.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	bool TryInteract();

protected:
	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;

	/** GetAimedInteractable, with the debug line optional (it runs every frame for the loading gate). */
	AActor* TraceAimedTarget(bool bDebugDraw) const;

	/** Applies mouse look to the control rotation. */
	void UpdateLook(APlayerController* PC);

	/** Places the weapon in front of the eyes (rest or reload pose), plus raise, look lag, idle breathing and recoil. */
	void UpdateWeaponSway(float DeltaSeconds);

	UFUNCTION()
	void HandleWeaponFired(bool bHit, const FHitResult& Hit);

	/** How far from the camera (cm) the player can interact. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction", meta = (ClampMin = "0.0"))
	float InteractRange = 400.f;

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

	/** The gun, attached to the camera. Rendered as a first-person primitive so it never clips into walls. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<USkeletalMeshComponent> WeaponMesh;

	/** Chambers, firing, reloading; drives the weapon mesh's animation. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<URevolverComponent> Revolver;

	/**
	 * Rest position of the weapon's root bone relative to the eyes (cm): X forward, Y right, Z up.
	 * SK_Pax's root sits on the bore axis just ahead of the cylinder; the gun reaches 27cm forward and 13cm back.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	FVector WeaponOffset = FVector(32.f, 12.f, -7.5f);

	/** Rest rotation of the weapon relative to the view. A little inward yaw points the muzzle at the crosshair. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	FRotator WeaponRotation = FRotator(0.f, -3.f, 0.f);

	/** Position while reloading: closer and more central, so the loading gate is in view. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Reload Pose")
	FVector ReloadOffset = FVector(26.f, 5.f, -3.f);

	/** Rotation while reloading: muzzle up and out, rolled so the gate (right) side faces the eye. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Reload Pose")
	FRotator ReloadRotation = FRotator(15.f, 35.f, -45.f);

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

	/** How far (cm) the gun jumps back toward the eye on each shot. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Recoil", meta = (ClampMin = "0.0"))
	float RecoilKickBack = 4.f;

	/** How far (degrees) the muzzle flips up on each shot. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Recoil", meta = (ClampMin = "0.0"))
	float RecoilKickPitch = 12.f;

	/** How fast the gun settles after a shot. Higher = snappier. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Recoil", meta = (ClampMin = "0.0"))
	float RecoilRecoverySpeed = 12.f;

private:
	float RecoilAlpha = 0.f;
	/** 0 = rest pose, 1 = reload pose (linear; eased when applied). */
	float ReloadPoseAlpha = 0.f;
	/** 1 = fully lowered, 0 = raised (linear; eased when applied). */
	float RaiseAlpha = 0.f;
	FRotator CurrentSway = FRotator::ZeroRotator;
	FRotator LastViewRotation = FRotator::ZeroRotator;
	bool bHasLastViewRotation = false;
	/** Parent actor's yaw last frame; the view carries along with the balloon's turning. */
	float LastParentYaw = 0.f;
	bool bHasParentYaw = false;
};
