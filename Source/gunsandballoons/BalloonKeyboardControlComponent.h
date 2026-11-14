#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BalloonKeyboardControlComponent.generated.h"

class UBalloonFlightComponent;

/**
 * Temporary keyboard steering for the balloon prototype: W/S step the target speed up/down
 * while held, A/D hold the turn input. Polls raw IsInputKeyDown on the player controller, so
 * it needs no Input Action or Mapping Context assets. This is the only control source that
 * bypasses the final diegetic design (speed panel + side levers, balloon-controls.md); it
 * talks exclusively to UBalloonFlightComponent and is meant to be removed once those exist.
 */
UCLASS(ClassGroup = (Balloon), meta = (BlueprintSpawnableComponent))
class GUNSANDBALLOONS_API UBalloonKeyboardControlComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBalloonKeyboardControlComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	/** Enable/disable keyboard control at runtime or in the editor. // TBD: prototype value */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Balloon|Keyboard")
	bool bEnabled = true;

	/** Key held to increase the target speed. // TBD: prototype value */
	UPROPERTY(EditAnywhere, Category = "Balloon|Keyboard")
	FKey SpeedUpKey = EKeys::W;

	/** Key held to decrease the target speed. // TBD: prototype value */
	UPROPERTY(EditAnywhere, Category = "Balloon|Keyboard")
	FKey SpeedDownKey = EKeys::S;

	/** Key held to turn left. // TBD: prototype value */
	UPROPERTY(EditAnywhere, Category = "Balloon|Keyboard")
	FKey TurnLeftKey = EKeys::A;

	/** Key held to turn right. // TBD: prototype value */
	UPROPERTY(EditAnywhere, Category = "Balloon|Keyboard")
	FKey TurnRightKey = EKeys::D;

	/** Speed change per key press (click), cm/s. // TBD: prototype value */
	UPROPERTY(EditAnywhere, Category = "Balloon|Keyboard", meta = (ClampMin = "0"))
	float SpeedStep = 50.f;

	/** How long a turn key press rotates the balloon, seconds. // TBD: prototype value */
	UPROPERTY(EditAnywhere, Category = "Balloon|Keyboard", meta = (ClampMin = "0"))
	float TurnDuration = 0.5f;

private:
	/** Flight component found on the same actor at BeginPlay; null when absent (tick disabled). */
	UPROPERTY()
	TObjectPtr<UBalloonFlightComponent> Flight = nullptr;

	// Edge detection / timing state.
	bool bPrevSpeedUp = false;
	bool bPrevSpeedDown = false;
	bool bPrevTurnLeft = false;
	bool bPrevTurnRight = false;
	float TurnTimer = 0.f;
};
