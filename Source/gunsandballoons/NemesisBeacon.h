#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NemesisFx.h"
#include "NemesisBeacon.generated.h"

class UBoxComponent;
class UPoseableMeshComponent;
class USpotLightComponent;
class UStaticMeshComponent;

/**
 * Desert beacon (AI ворогів, 7.2). Placeholder cubes. A lamp on a tower sweeps a visible ORANGE
 * cone of light left and right. Detection treats the beam as a vertical fan: only the horizontal
 * angle to the basket matters (plus Range and line of sight).
 * Once the beam touches the basket it locks on: the light turns RED and follows the basket in yaw
 * and pitch, until the lamp is shot or the basket gets farther than TrackRange from the beacon (then
 * it goes back to searching). SpotTime after locking on it calls ONE crop duster from the nemesis
 * airship's hangar (team decision 2026-11-15). While it stays locked it calls one more every Cooldown,
 * but only while fewer than the map-wide limit of crop dusters are attacking (3): when the map is
 * full it waits, and Cooldown after a place frees up it calls the next one. The beacon's planes don't
 * count toward the airship's own limit. The beacon does no damage itself.
 * One hit on the lamp breaks it for good; shooting it before it spots you prevents the call. The
 * lamp shatters (flash, sparks, smoke), goes dark and hangs askew; the tower stays standing and smokes
 * for a few seconds.
 * Model (2026-11-15): the team's SK_Lighthouse (Content/Assets/Locations/Desert, 38 m, lamp bone at
 * 32.6 m), scaled uniformly so its lamp is where the cube lamp would be; its head bone turns with the
 * beam. An invisible Tower cube inside keeps the collision (the model has none). Cubes if it's missing.
 */
UCLASS()
class GUNSANDBALLOONS_API ANemesisBeacon : public AActor
{
	GENERATED_BODY()

public:
	ANemesisBeacon();

	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintPure, Category = "Beacon")
	bool IsBroken() const { return bBroken; }

	/** Locked on the balloon (red light): until the lamp is broken or the basket is beyond TrackRange. */
	UFUNCTION(BlueprintPure, Category = "Beacon")
	bool IsLocked() const { return bLit; }

	/** Locked on the balloon (true) / lost it or the lamp broke while locked (false). Hook for an alarm sound. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Beacon")
	void OnLitChanged(bool bNowLit);

	/** The beacon called a crop duster. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Beacon")
	void OnBalloonSpotted(bool bPlaneLaunched);

	UFUNCTION(BlueprintImplementableEvent, Category = "Beacon")
	void OnLampBroken();

protected:
	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;

	UPROPERTY(VisibleAnywhere, Category = "Beacon")
	TObjectPtr<USceneComponent> Root;

	/** Collision (and the placeholder look without the model). */
	UPROPERTY(VisibleAnywhere, Category = "Beacon")
	TObjectPtr<UStaticMeshComponent> Tower;

	/** The team's lighthouse model; head and lamp bones posed from code. */
	UPROPERTY(VisibleAnywhere, Category = "Beacon")
	TObjectPtr<UPoseableMeshComponent> Body;

	/** Turns to sweep the beam. */
	UPROPERTY(VisibleAnywhere, Category = "Beacon")
	TObjectPtr<USceneComponent> LampPivot;

	UPROPERTY(VisibleAnywhere, Category = "Beacon")
	TObjectPtr<UStaticMeshComponent> Lamp;

	/** Invisible hit zone around the lamp, bigger than it, so it can be hit at range (any hit in it
	 *  breaks the lamp). 2026-11-15. */
	UPROPERTY(VisibleAnywhere, Category = "Beacon")
	TObjectPtr<UBoxComponent> LampHitBox;

	/** LampHitBox half size, cm. TBD */
	UPROPERTY(EditAnywhere, Category = "Beacon")
	FVector LampHitBoxExtent = FVector(130.f, 130.f, 130.f);

	UPROPERTY(VisibleAnywhere, Category = "Beacon")
	TObjectPtr<USpotLightComponent> Beam;

	/** What the broken lamp looks like (dark, burnt). */
	UPROPERTY(EditAnywhere, Category = "Beacon")
	TObjectPtr<UMaterialInterface> BrokenLampMaterial;

	/** The broken lamp smokes this long, s. */
	UPROPERTY(EditAnywhere, Category = "Beacon", meta = (ClampMin = "0"))
	float BrokenSmokeTime = 6.f;

	/** Visible glowing cone of the beam. */
	UPROPERTY(VisibleAnywhere, Category = "Beacon")
	TObjectPtr<UStaticMeshComponent> BeamCone;

	UPROPERTY(EditAnywhere, Category = "Beacon", meta = (ClampMin = "100"))
	float TowerHeight = 1500.f;

	/** At BeginPlay, grow the tower so the lamp is at the player's height (basket + LampHeightOffset),
	 *  so the player can shoot it. Off: TowerHeight as set. Team decision 2026-11-15. */
	UPROPERTY(EditAnywhere, Category = "Beacon")
	bool bLampAtPlayerHeight = true;

	/** Lamp height relative to the basket centre, cm (with bLampAtPlayerHeight). TBD */
	UPROPERTY(EditAnywhere, Category = "Beacon")
	float LampHeightOffset = 0.f;

	/** Full cone angle, degrees. */
	UPROPERTY(EditAnywhere, Category = "Beacon", meta = (ClampMin = "1", ClampMax = "90"))
	float BeamAngle = 15.f;

	/** Spotting range and beam length, cm (60 m; was 200 m). TBD */
	UPROPERTY(EditAnywhere, Category = "Beacon", meta = (ClampMin = "0"))
	float Range = 6000.f;

	/** Sweep ±this many degrees around the actor's forward, at SweepSpeed. */
	UPROPERTY(EditAnywhere, Category = "Beacon", meta = (ClampMin = "0", ClampMax = "180"))
	float SweepAngle = 60.f;

	UPROPERTY(EditAnywhere, Category = "Beacon", meta = (ClampMin = "0"))
	float SweepSpeed = 20.f;

	/** The searching beam is tilted up this much, degrees. TBD */
	UPROPERTY(EditAnywhere, Category = "Beacon", meta = (ClampMin = "-30", ClampMax = "80"))
	float SearchPitch = 8.f;

	/** How fast the locked beam follows the basket, degrees/s. */
	UPROPERTY(EditAnywhere, Category = "Beacon", meta = (ClampMin = "0"))
	float TrackSpeed = 60.f;

	/** Locked on, it stops following the basket beyond this distance, cm (80 m; was 250 m). TBD */
	UPROPERTY(EditAnywhere, Category = "Beacon", meta = (ClampMin = "0"))
	float TrackRange = 8000.f;


	/** Seconds locked before the planes are called. */
	UPROPERTY(EditAnywhere, Category = "Beacon", meta = (ClampMin = "0"))
	float SpotTime = 1.f;

	/** Between two crop dusters called by this beacon, and after the map stops being full, s. TBD */
	UPROPERTY(EditAnywhere, Category = "Beacon", meta = (ClampMin = "0"))
	float Cooldown = 7.f;

private:
	UFUNCTION()
	void HandlePointDamage(AActor* DamagedActor, float InDamage, AController* InstigatedBy, FVector HitLocation,
		UPrimitiveComponent* HitComponent, FName BoneName, FVector ShotFromDirection, const UDamageType* DamageType, AActor* DamageCauser);

	bool IsBasketInBeam() const;

	void SetLocked(bool bLocked);
	/** Tower size and lamp position from TowerHeight. */
	void ApplyLayout();

	/** Pose the model: the head turns to SweepYaw; a broken lamp hangs askew. */
	void UpdateModelPose();
	bool bModel = false;
	FTransform RefHead;
	FTransform RefLamp;

	float SweepYaw = 0.f;
	float BeamPitch = 0.f;
	float SweepDir = 1.f;
	float LitTime = 0.f;
	bool bCalledThisLock = false;
	NemesisFx::FStream BrokenSmoke;
	float BrokenTime = 0.f;
	float CooldownLeft = 0.f;
	bool bLit = false;
	bool bBroken = false;
};
