#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NemesisFx.h"
#include "NemesisHomingMissile.generated.h"

class ANemesisFlak;
class UPointLightComponent;
class USphereComponent;
class UStaticMeshComponent;

/**
 * The AA gun's homing missile (team decision 2026-11-15). Steers at the basket with a limited turn
 * rate for as long as its AA gun is engaged (the basket inside its radius). The moment the basket
 * leaves the radius, the missile blows up harmlessly where it is. A direct hit on the basket deals
 * Damage. One shot from any weapon blows it up. Uses the airship rocket's model, smaller.
 */
UCLASS()
class GUNSANDBALLOONS_API ANemesisHomingMissile : public AActor
{
	GENERATED_BODY()

public:
	ANemesisHomingMissile();

	void Launch(ANemesisFlak* InFlak);

	virtual void Tick(float DeltaSeconds) override;

	/** Hook for VFX/SFX. bHitBalloon: direct hit on the basket. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Missile")
	void OnExploded(bool bHitBalloon);

protected:
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

	UPROPERTY(VisibleAnywhere, Category = "Missile")
	TObjectPtr<USphereComponent> HitSphere;

	UPROPERTY(VisibleAnywhere, Category = "Missile")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, Category = "Missile")
	TObjectPtr<UStaticMeshComponent> Exhaust;

	UPROPERTY(VisibleAnywhere, Category = "Missile")
	TObjectPtr<UPointLightComponent> Light;

	/** Balloon damage on a direct hit. TBD */
	UPROPERTY(EditAnywhere, Category = "Missile", meta = (ClampMin = "0"))
	float Damage = 50.f;

	/**
	 * Cruise speed, cm/s (9 m/s). TBD. Slow enough that a player who reacts can get out of the AA
	 * gun's radius before it arrives (~5 s from the radius edge), still 3× the balloon's top speed
	 * (3 m/s), so it can't simply be outrun inside the radius. 18 m/s left no time to react.
	 */
	UPROPERTY(EditAnywhere, Category = "Missile", meta = (ClampMin = "1"))
	float Speed = 900.f;

	/** Speed off the launcher, ramping to Speed over BoostTime, cm/s / s. */
	UPROPERTY(EditAnywhere, Category = "Missile", meta = (ClampMin = "0"))
	float LaunchSpeed = 300.f;

	UPROPERTY(EditAnywhere, Category = "Missile", meta = (ClampMin = "0"))
	float BoostTime = 1.5f;

	/** How fast it can turn towards the basket, degrees/s: lower = easier to dodge. TBD */
	UPROPERTY(EditAnywhere, Category = "Missile", meta = (ClampMin = "0"))
	float TurnRate = 90.f;

	/** Its path passing this close to the basket centre = hit, cm. */
	UPROPERTY(EditAnywhere, Category = "Missile", meta = (ClampMin = "0"))
	float BasketHitRadius = 150.f;

	/** Blows up by itself after this long, s. */
	UPROPERTY(EditAnywhere, Category = "Missile", meta = (ClampMin = "0"))
	float MaxLifeTime = 15.f;

	UPROPERTY(EditAnywhere, Category = "Missile", meta = (ClampMin = "0"))
	float SpinSpeed = 540.f;

	UPROPERTY(EditAnywhere, Category = "Missile", meta = (ClampMin = "0"))
	float TrailRate = 90.f;

private:
	UFUNCTION()
	void HandleTakeAnyDamage(AActor* DamagedActor, float InDamage, const UDamageType* DamageType, AController* InstigatedBy, AActor* DamageCauser);

	void Explode(bool bHitBalloon);

	TWeakObjectPtr<ANemesisFlak> Flak;
	NemesisFx::FStream Trail;
	FVector Direction = FVector::ForwardVector;
	FVector ExhaustBase = FVector::OneVector;
	float Age = 0.f;
	float Roll = 0.f;
	bool bLaunched = false;
	bool bExploded = false;
};
