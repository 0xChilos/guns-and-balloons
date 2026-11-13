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
 * - Holds a hand-less weapon viewmodel (Dusk style): the gun floats in front of the eyes,
 *   lags behind fast look movement, breathes slightly while idle and kicks when fired.
 * - LMB interacts with the IInteractable under the crosshair, otherwise fires. R reloads.
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

	/** The IInteractable actor under the crosshair within InteractRange, or null. */
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	AActor* GetAimedInteractable() const;

	/** Interact with whatever is under the crosshair. Called on LMB. Returns true if something was interacted with. */
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	bool TryInteract();

protected:
	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;

	/** Applies mouse look to the control rotation. */
	void UpdateLook(APlayerController* PC);

	/** Places the weapon in front of the eyes, plus look lag, idle breathing and recoil. */
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

	/** Rest position of the weapon relative to the eyes (cm): X forward, Y right, Z up. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	FVector WeaponOffset = FVector(28.f, 10.f, -6.f);

	/** Rest rotation of the weapon relative to the view. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	FRotator WeaponRotation = FRotator::ZeroRotator;

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
	FRotator CurrentSway = FRotator::ZeroRotator;
	FRotator LastViewRotation = FRotator::ZeroRotator;
	bool bHasLastViewRotation = false;
};
