#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BalloonFlightComponent.generated.h"

/**
 * Moves the balloon: the owner always flies forward along its horizontal forward axis, with
 * speed and turn rate easing in/out instead of changing instantly (heavy-aircraft feel,
 * balloon-controls.md). Every change comes through SetTargetSpeed / SetTurnInput — this
 * component is the single entry point for any control source (keyboard today, the speed
 * panel, side levers and the stove later). It knows nothing about input devices itself.
 *
 * Speed units are cm/s, accelerations cm/s^2, turn rates deg/s and deg/s^2.
 */
UCLASS(ClassGroup = (Balloon), meta = (BlueprintSpawnableComponent))
class GUNSANDBALLOONS_API UBalloonFlightComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBalloonFlightComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/**
	 * Ask for a new forward speed. The request is clamped to [MinSpeed, MaxSpeed]; speeds at or
	 * below zero mean "as slow as possible" and resolve to MinSpeed. The balloon cannot stop or
	 * reverse: the slowest it can go is MinSpeed.
	 */
	UFUNCTION(BlueprintCallable, Category = "Balloon|Flight")
	void SetTargetSpeed(float SpeedCmPerSecond);

	/** Ask for a turn, clamped to [-1, 1]; the value is kept until the next call. 0 = fly straight. */
	UFUNCTION(BlueprintCallable, Category = "Balloon|Flight")
	void SetTurnInput(float Input);

	UFUNCTION(BlueprintPure, Category = "Balloon|Flight")
	float GetCurrentSpeed() const { return CurrentSpeed; }

	UFUNCTION(BlueprintPure, Category = "Balloon|Flight")
	float GetTargetSpeed() const { return TargetSpeed; }

	UFUNCTION(BlueprintPure, Category = "Balloon|Flight")
	float GetCurrentTurnRate() const { return CurrentTurnRate; }

protected:
	/** Slowest forward speed; the balloon never stops. Needs tuning: the design value is TBD (balloon-controls.md). // TBD: prototype value */
	UPROPERTY(EditAnywhere, Category = "Balloon|Flight", meta = (ClampMin = "0"))
	float MinSpeed = 50.f;

	/** Fastest forward speed. Needs tuning: the design value is TBD (balloon-controls.md). // TBD: prototype value */
	UPROPERTY(EditAnywhere, Category = "Balloon|Flight", meta = (ClampMin = "0"))
	float MaxSpeed = 300.f;

	/** Forward speed at BeginPlay, clamped to [MinSpeed, MaxSpeed]. Needs tuning: the design value is TBD (balloon-controls.md). // TBD: prototype value */
	UPROPERTY(EditAnywhere, Category = "Balloon|Flight", meta = (ClampMin = "0"))
	float InitialSpeed = 100.f;

	/** cm/s gained per second while speeding up. Needs tuning: the design value is TBD (balloon-controls.md). // TBD: prototype value */
	UPROPERTY(EditAnywhere, Category = "Balloon|Flight", meta = (ClampMin = "0"))
	float Acceleration = 40.f;

	/** cm/s lost per second while slowing down. Needs tuning: the design value is TBD (balloon-controls.md). // TBD: prototype value */
	UPROPERTY(EditAnywhere, Category = "Balloon|Flight", meta = (ClampMin = "0"))
	float Deceleration = 40.f;

	/** Turn rate at full lever input, deg/s. Needs tuning: the design value is TBD (balloon-controls.md). // TBD: prototype value */
	UPROPERTY(EditAnywhere, Category = "Balloon|Flight", meta = (ClampMin = "0"))
	float MaxTurnRate = 30.f;

	/** deg/s^2 while the turn rate magnitude grows in the same direction. Needs tuning: the design value is TBD (balloon-controls.md). // TBD: prototype value */
	UPROPERTY(EditAnywhere, Category = "Balloon|Flight", meta = (ClampMin = "0"))
	float TurnAcceleration = 60.f;

	/** deg/s^2 while the turn rate magnitude shrinks or while reversing direction (until it crosses zero). Needs tuning: the design value is TBD (balloon-controls.md). // TBD: prototype value */
	UPROPERTY(EditAnywhere, Category = "Balloon|Flight", meta = (ClampMin = "0"))
	float TurnDeceleration = 60.f;

protected:
	/** Temporary debug logging (timing). Keep false in normal play. */
	UPROPERTY(EditAnywhere, Category = "Balloon|Flight|Debug")
	bool bDebugLog = false;

	/** Automated test sequence for PIE timing measurements. Keep false in normal play. */
	UPROPERTY(EditAnywhere, Category = "Balloon|Flight|Debug")
	bool bAutoTest = false;

private:
	/** Requested forward speed, always inside [MinSpeed, MaxSpeed]. */
	float TargetSpeed = 0.f;

	/** Current forward speed, eased toward TargetSpeed every tick. */
	float CurrentSpeed = 0.f;

	/** Lever input clamped to [-1, 1], kept until the next SetTurnInput call. */
	float TurnInput = 0.f;

	/** Current turn rate in deg/s, eased toward TurnInput * MaxTurnRate every tick. */
	float CurrentTurnRate = 0.f;

	// Debug timing (only used when bDebugLog is true).
	double DebugAccelStartTime = 0.0;
	bool bDebugAccelDone = false;
	double DebugTurnStartTime = 0.0;
	float DebugTurnStartYaw = 0.f;
	bool bDebugTurnActive = false;

	// Auto-test state (only used when bAutoTest is true).
	int32 AutoTestState = 0;
	bool bAutoTestTurning = false;
	float AutoTestTurnStartYaw = 0.f;
};
