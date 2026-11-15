#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NemesisFx.h"
#include "NemesisRocket.generated.h"

class ANemesisAirship;
class UPointLightComponent;
class USphereComponent;
class UStaticMeshComponent;

/**
 * Nemesis rocket (AI ворогів, section 2). At launch it computes an intercept point from the
 * balloon's velocity and flies a fixed Bezier curve (up, then down onto the point); it never
 * homes and never self-destructs. Damage only on a direct hit: its path crosses the basket.
 * One shot from any weapon destroys it.
 */
UCLASS()
class GUNSANDBALLOONS_API ANemesisRocket : public AActor
{
	GENERATED_BODY()

public:
	ANemesisRocket();

	void Launch(ANemesisAirship* InAirship);

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

	UPROPERTY(VisibleAnywhere, Category = "Rocket")
	TObjectPtr<USphereComponent> HitSphere;

	UPROPERTY(VisibleAnywhere, Category = "Rocket")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, Category = "Rocket")
	TObjectPtr<UStaticMeshComponent> Exhaust;

	UPROPERTY(VisibleAnywhere, Category = "Rocket")
	TObjectPtr<UPointLightComponent> Light;

	/** cm/s (25 m/s). */
	UPROPERTY(EditAnywhere, Category = "Rocket", meta = (ClampMin = "1"))
	float Speed = 2500.f;

	/** Speed right after ignition, cm/s; the motor brings it up to Speed over BoostTime. */
	UPROPERTY(EditAnywhere, Category = "Rocket", meta = (ClampMin = "1"))
	float LaunchSpeed = 900.f;

	UPROPERTY(EditAnywhere, Category = "Rocket", meta = (ClampMin = "0"))
	float BoostTime = 0.7f;

	/** Roll around its axis, degrees/s (the canted fins spin it). */
	UPROPERTY(EditAnywhere, Category = "Rocket")
	float SpinSpeed = 540.f;

	/** Smoke trail puffs per second. */
	UPROPERTY(EditAnywhere, Category = "Rocket", meta = (ClampMin = "0"))
	float TrailRate = 35.f;

	/** Rise of the Bezier control point above the straight line, cm (10 m). */
	UPROPERTY(EditAnywhere, Category = "Rocket")
	float ArcHeight = 1000.f;

	UPROPERTY(EditAnywhere, Category = "Rocket", meta = (ClampMin = "0"))
	float Damage = 25.f;

	/** The path must pass this close to the basket centre to count as a hit, cm. TBD */
	UPROPERTY(EditAnywhere, Category = "Rocket", meta = (ClampMin = "0"))
	float BasketHitRadius = 150.f;

	/** A missed rocket is removed once it is this far from the balloon, cm. */
	UPROPERTY(EditAnywhere, Category = "Rocket", meta = (ClampMin = "0"))
	float DespawnDistance = 40000.f;

	/** Rocket explodes harmlessly when it flies into level geometry. */
	UPROPERTY(EditAnywhere, Category = "Rocket")
	bool bStopOnWorldGeometry = true;

	/** Hook for VFX/SFX. bHitBalloon = direct hit on the basket; false = shot down or crashed. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Rocket")
	void OnExploded(bool bHitBalloon);

private:
	UFUNCTION()
	void HandleTakeAnyDamage(AActor* DamagedActor, float InDamage, const UDamageType* DamageType, AController* InstigatedBy, AActor* DamageCauser);

	FVector EvalCurve(float T) const;
	FVector PositionAtDistance(float S, FVector& OutDir) const;
	void Explode(bool bHitBalloon);

	TWeakObjectPtr<ANemesisAirship> Airship;
	NemesisFx::FStream Trail;
	float Age = 0.f;
	float Roll = 0.f;
	FVector P0, P1, P2;
	TArray<float> ArcLengths; // cumulative length at evenly spaced T samples
	float Travelled = 0.f;
	bool bLaunched = false;
	bool bExploded = false;
};
