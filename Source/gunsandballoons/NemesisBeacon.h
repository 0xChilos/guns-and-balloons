#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NemesisBeacon.generated.h"

class USpotLightComponent;
class UStaticMeshComponent;

/**
 * Desert beacon (AI ворогів, 7.2). Placeholder cubes. A lamp on a tower sweeps a visible YELLOW
 * cone of light left and right. Detection treats the beam as a vertical fan: only the horizontal
 * angle to the basket matters (plus range and line of sight).
 * Once the beam touches the basket it locks for good: the light turns RED and follows the basket in
 * yaw and pitch until the lamp is shot. While locked and the basket is within Range, after SpotTime
 * and then every Cooldown it calls an extra crop duster from the nemesis airship (outside the
 * airship's cooldown, still within its limit). The beacon does no damage itself.
 * One hit on the lamp breaks it for good.
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

	/** Locked on the balloon (red light). Stays true until the lamp is broken. */
	UFUNCTION(BlueprintPure, Category = "Beacon")
	bool IsLocked() const { return bLit; }

	/** Locked on the balloon (true), or the lamp broke while locked (false). Hook for an alarm sound. */
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

	UPROPERTY(VisibleAnywhere, Category = "Beacon")
	TObjectPtr<UStaticMeshComponent> Tower;

	/** Turns to sweep the beam. */
	UPROPERTY(VisibleAnywhere, Category = "Beacon")
	TObjectPtr<USceneComponent> LampPivot;

	UPROPERTY(VisibleAnywhere, Category = "Beacon")
	TObjectPtr<UStaticMeshComponent> Lamp;

	UPROPERTY(VisibleAnywhere, Category = "Beacon")
	TObjectPtr<USpotLightComponent> Beam;

	/** Visible glowing cone of the beam. */
	UPROPERTY(VisibleAnywhere, Category = "Beacon")
	TObjectPtr<UStaticMeshComponent> BeamCone;

	UPROPERTY(EditAnywhere, Category = "Beacon", meta = (ClampMin = "100"))
	float TowerHeight = 1500.f;

	/** Full cone angle, degrees. */
	UPROPERTY(EditAnywhere, Category = "Beacon", meta = (ClampMin = "1", ClampMax = "90"))
	float BeamAngle = 15.f;

	/** cm (200 m). */
	UPROPERTY(EditAnywhere, Category = "Beacon", meta = (ClampMin = "0"))
	float Range = 20000.f;

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

	/** Seconds locked before the first plane is called. */
	UPROPERTY(EditAnywhere, Category = "Beacon", meta = (ClampMin = "0"))
	float SpotTime = 1.f;

	UPROPERTY(EditAnywhere, Category = "Beacon", meta = (ClampMin = "0"))
	float Cooldown = 15.f;

private:
	UFUNCTION()
	void HandlePointDamage(AActor* DamagedActor, float InDamage, AController* InstigatedBy, FVector HitLocation,
		UPrimitiveComponent* HitComponent, FName BoneName, FVector ShotFromDirection, const UDamageType* DamageType, AActor* DamageCauser);

	bool IsBasketInBeam() const;

	void SetLocked(bool bLocked);

	float SweepYaw = 0.f;
	float BeamPitch = 0.f;
	float SweepDir = 1.f;
	float LitTime = 0.f;
	float CooldownLeft = 0.f;
	bool bLit = false;
	bool bBroken = false;
};
