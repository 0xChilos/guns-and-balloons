#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "FirstPersonCameraPawn.generated.h"

class UCameraComponent;

/**
 * A bodiless, stationary first-person camera.
 * - No mesh, no collision, no movement component: WASD/any move input does nothing.
 * - Mouse X/Y rotates the view (yaw/pitch). The actor itself never moves or rotates.
 * - Needs no Input Action / Mapping Context assets: it reads the mouse delta directly.
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

	/** The IInteractable actor under the crosshair within InteractRange, or null. */
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	AActor* GetAimedInteractable() const;

	/** Interact with whatever is under the crosshair. Called on LMB. Returns true if something was interacted with. */
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	bool TryInteract();

protected:
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
};
