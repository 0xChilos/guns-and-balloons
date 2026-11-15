#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NemesisFlak.generated.h"

class ANemesisBullet;
class USpotLightComponent;
class UStaticMeshComponent;

/**
 * AA gun (ППО): a small flying balloon (placeholder sphere + cubes) with a turret and a searchlight
 * hanging under its gondola. Placed near the map edges; it keeps its X/Y and hovers at the player's
 * altitude (basket height + AltitudeOffset, ClimbSpeed, a slight bob). Guards a radius around itself.
 * Outside EngageRadius its searchlight shines YELLOW and slowly circles the sky.
 * When the basket comes within EngageRadius (and in line of sight) the light turns RED, the turret
 * and the light track the basket and, after WarmUp, fire bursts of bullets (aimed with lead).
 * Leaving the radius (+ ReleaseMargin) or breaking line of sight returns it to searching.
 * Indestructible (decided 2026-11-15).
 */
UCLASS()
class GUNSANDBALLOONS_API ANemesisFlak : public AActor
{
	GENERATED_BODY()

public:
	ANemesisFlak();

	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintPure, Category = "Flak")
	bool IsEngaged() const { return bEngaged; }

	/** Started (true) or stopped (false) engaging the balloon. Hook for an alarm sound. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Flak")
	void OnEngagedChanged(bool bNowEngaged);

	UFUNCTION(BlueprintImplementableEvent, Category = "Flak")
	void OnShot();

protected:
	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;

	UPROPERTY(VisibleAnywhere, Category = "Flak")
	TObjectPtr<USceneComponent> Root;

	/** The balloon's envelope. */
	UPROPERTY(VisibleAnywhere, Category = "Flak")
	TObjectPtr<UStaticMeshComponent> Envelope;

	/** Gondola (the actor's origin); the turret hangs under it. */
	UPROPERTY(VisibleAnywhere, Category = "Flak")
	TObjectPtr<UStaticMeshComponent> Gondola;

	UPROPERTY(VisibleAnywhere, Category = "Flak")
	TArray<TObjectPtr<UStaticMeshComponent>> Ropes;

	/** Turns the turret (yaw). */
	UPROPERTY(VisibleAnywhere, Category = "Flak")
	TObjectPtr<USceneComponent> TurretYaw;

	UPROPERTY(VisibleAnywhere, Category = "Flak")
	TObjectPtr<UStaticMeshComponent> TurretBody;

	/** Raises the barrel and the searchlight (pitch). */
	UPROPERTY(VisibleAnywhere, Category = "Flak")
	TObjectPtr<USceneComponent> TurretPitch;

	UPROPERTY(VisibleAnywhere, Category = "Flak")
	TObjectPtr<UStaticMeshComponent> Barrel;

	UPROPERTY(VisibleAnywhere, Category = "Flak")
	TObjectPtr<UStaticMeshComponent> Lamp;

	UPROPERTY(VisibleAnywhere, Category = "Flak")
	TObjectPtr<USpotLightComponent> Light;

	UPROPERTY(VisibleAnywhere, Category = "Flak")
	TObjectPtr<UStaticMeshComponent> BeamCone;

	/** Hover at the player's altitude (basket height + AltitudeOffset). Off = stays where placed. */
	UPROPERTY(EditAnywhere, Category = "Flak")
	bool bMatchPlayerAltitude = true;

	/** Gondola height relative to the basket, cm. TBD */
	UPROPERTY(EditAnywhere, Category = "Flak")
	float AltitudeOffset = 0.f;

	/** Vertical speed when following the player's altitude, cm/s. TBD */
	UPROPERTY(EditAnywhere, Category = "Flak", meta = (ClampMin = "0"))
	float ClimbSpeed = 300.f;

	/** Gentle up/down bob: amplitude cm, period s. */
	UPROPERTY(EditAnywhere, Category = "Flak", meta = (ClampMin = "0"))
	float BobAmplitude = 40.f;

	UPROPERTY(EditAnywhere, Category = "Flak", meta = (ClampMin = "0.1"))
	float BobPeriod = 4.f;

	/**
	 * The gun opens fire when the basket is this close, cm (50 m). Small on purpose: AA guns along the
	 * map edges mark the line the player must not cross, they are not a constant threat. TBD
	 */
	UPROPERTY(EditAnywhere, Category = "Flak", meta = (ClampMin = "0"))
	float EngageRadius = 5000.f;

	/** Stops engaging beyond EngageRadius + this, cm (no flicker at the edge). */
	UPROPERTY(EditAnywhere, Category = "Flak", meta = (ClampMin = "0"))
	float ReleaseMargin = 500.f;

	/** Visible beam: full cone angle, degrees, and length, cm. */
	UPROPERTY(EditAnywhere, Category = "Flak", meta = (ClampMin = "1", ClampMax = "90"))
	float BeamAngle = 10.f;

	/** 0 = EngageRadius: the circling beam shows how far the gun reaches. */
	UPROPERTY(EditAnywhere, Category = "Flak", meta = (ClampMin = "0"))
	float BeamLength = 0.f;

	/** Searching: the light circles the sky at this speed (deg/s) and pitch (deg). TBD */
	UPROPERTY(EditAnywhere, Category = "Flak", meta = (ClampMin = "0"))
	float SearchSpeed = 25.f;

	UPROPERTY(EditAnywhere, Category = "Flak", meta = (ClampMin = "-30", ClampMax = "60"))
	float SearchPitch = 10.f;

	/** Turret turn speed while tracking, degrees/s. */
	UPROPERTY(EditAnywhere, Category = "Flak", meta = (ClampMin = "0"))
	float TrackSpeed = 90.f;

	/** Seconds from engaging to the first burst. TBD */
	UPROPERTY(EditAnywhere, Category = "Flak", meta = (ClampMin = "0"))
	float WarmUp = 1.f;

	UPROPERTY(EditAnywhere, Category = "Flak", meta = (ClampMin = "1"))
	int32 BurstShots = 5;

	UPROPERTY(EditAnywhere, Category = "Flak", meta = (ClampMin = "0.01"))
	float ShotInterval = 0.12f;

	/** Pause between bursts, s. TBD */
	UPROPERTY(EditAnywhere, Category = "Flak", meta = (ClampMin = "0"))
	float BurstCooldown = 2.5f;

	/** cm/s (300 m/s). */
	UPROPERTY(EditAnywhere, Category = "Flak", meta = (ClampMin = "1"))
	float BulletSpeed = 30000.f;

	/** Balloon damage per bullet that crosses the basket. TBD */
	UPROPERTY(EditAnywhere, Category = "Flak", meta = (ClampMin = "0"))
	float Damage = 5.f; // 4 full bursts (20 bullets) destroy a 100 HP engine. TBD

	/** Random cone half-angle per shot, degrees. TBD */
	UPROPERTY(EditAnywhere, Category = "Flak", meta = (ClampMin = "0"))
	float Spread = 2.5f;

	/** Fires only when the barrel is within this many degrees of the aim point. */
	UPROPERTY(EditAnywhere, Category = "Flak", meta = (ClampMin = "0"))
	float FireTolerance = 6.f;

	UPROPERTY(EditAnywhere, Category = "Flak")
	TSubclassOf<ANemesisBullet> BulletClass;

private:
	bool CanSeeBasket() const;
	void SetEngaged(bool bNow);
	void FireShot(const FVector& AimPoint);
	void UpdateAltitude(float DeltaSeconds);

	float HoverZ = 0.f;
	float BobTime = 0.f;
	bool bAltitudeSnapped = false;
	float Yaw = 0.f;
	float Pitch = 0.f;
	float EngagedTime = 0.f;
	float NextShotIn = 0.f;
	int32 ShotsLeftInBurst = 0;
	bool bEngaged = false;
};
