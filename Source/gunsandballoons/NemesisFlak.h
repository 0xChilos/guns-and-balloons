#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NemesisFlak.generated.h"

class ANemesisHomingMissile;
class UBoxComponent;
class UCapsuleComponent;
class UPoseableMeshComponent;
class USphereComponent;
class USpotLightComponent;
class UStaticMeshComponent;

/**
 * AA gun (ППО): a small flying balloon with a missile launcher and a searchlight. Uses the team's
 * SK_RocketBalloon model (Content/Assets/Map_edge: a 6-tube rocket pod on bones launcher /
 * launcher_pitch, a red lamp) when present, placeholder sphere + cubes otherwise. Placed near the map edges; it keeps its X/Y and hovers at the player's
 * altitude (basket height + AltitudeOffset, ClimbSpeed, a slight bob). Guards a radius around itself.
 * Outside EngageRadius its searchlight shines YELLOW and slowly circles the sky.
 * When the basket comes within EngageRadius (and in line of sight) the light turns RED, the turret
 * and the light track the basket and, after WarmUp, it launches a homing missile (one in the air at
 * a time; the next one MissileCooldown after the last is gone). The missile chases the basket while
 * it stays in the radius and blows up harmlessly the moment it leaves (team decision 2026-11-15).
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

	/** The team's model; its launcher bones are turned from code. */
	UPROPERTY(VisibleAnywhere, Category = "Flak")
	TObjectPtr<UPoseableMeshComponent> Body;

	/** Collision for the model (the poseable mesh has none): envelope, skirt, deck and pod. */
	UPROPERTY(VisibleAnywhere, Category = "Flak")
	TObjectPtr<USphereComponent> EnvelopeCollision;

	UPROPERTY(VisibleAnywhere, Category = "Flak")
	TObjectPtr<UCapsuleComponent> SkirtCollision;

	UPROPERTY(VisibleAnywhere, Category = "Flak")
	TObjectPtr<UBoxComponent> DeckCollision;

	/** The balloon's envelope (placeholder). */
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

	/** Seconds from engaging to the first missile. TBD */
	UPROPERTY(EditAnywhere, Category = "Flak", meta = (ClampMin = "0"))
	float WarmUp = 1.f;

	/** Next missile this long after the previous one is gone, s. TBD */
	UPROPERTY(EditAnywhere, Category = "Flak", meta = (ClampMin = "0"))
	float MissileCooldown = 4.f;

	/** Launches only when the launcher is within this many degrees of the basket. */
	UPROPERTY(EditAnywhere, Category = "Flak", meta = (ClampMin = "0"))
	float FireTolerance = 10.f;

	/** Damage, speed and turn rate are on the missile class. */
	UPROPERTY(EditAnywhere, Category = "Flak")
	TSubclassOf<ANemesisHomingMissile> MissileClass;

private:
	bool CanSeeBasket() const;
	void SetEngaged(bool bNow);
	void LaunchMissile();
	void UpdateAltitude(float DeltaSeconds);
	/** Model: turn the launcher bones and point the searchlight at Yaw / Pitch. */
	void UpdateModelPose();

	bool bModel = false;
	FTransform RefLauncher;
	FTransform RefLauncherPitch;
	int32 NextMuzzle = 0;
	float HoverZ = 0.f;
	float BobTime = 0.f;
	bool bAltitudeSnapped = false;
	float Yaw = 0.f;
	float Pitch = 0.f;
	float NextLaunchIn = 0.f;
	TWeakObjectPtr<ANemesisHomingMissile> ActiveMissile;
	bool bEngaged = false;
};
