#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NemesisMine.generated.h"

class UPointLightComponent;
class UStaticMeshComponent;
class UMaterialInterface;

/**
 * Parachute mine (AI ворогів, section 3). Placeholder cubes. Two targets, one shot each:
 * - shoot the mine → it explodes in place; the basket inside Radius takes flat Damage;
 * - shoot the parachute → the mine drops and explodes harmlessly below.
 * The balloon flying within TriggerDistance sets it off. Glows red while the basket is inside Radius.
 */
UCLASS()
class GUNSANDBALLOONS_API ANemesisMine : public AActor
{
	GENERATED_BODY()

public:
	ANemesisMine();

	/** DesiredFallSpeed (cm/s) is clamped to [MinFallSpeed, MaxFallSpeed]; 0 = MaxFallSpeed. */
	void Arm(float DesiredFallSpeed = 0.f);

	virtual void Tick(float DeltaSeconds) override;

	/** Hook for VFX/SFX. bDamagedBalloon: the basket was inside the radius. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Mine")
	void OnExploded(bool bDamagedBalloon);

	UFUNCTION(BlueprintImplementableEvent, Category = "Mine")
	void OnParachuteCut();

protected:
	UPROPERTY(VisibleAnywhere, Category = "Mine")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "Mine")
	TObjectPtr<UStaticMeshComponent> MineMesh;

	UPROPERTY(VisibleAnywhere, Category = "Mine")
	TObjectPtr<UStaticMeshComponent> Parachute;

	UPROPERTY(VisibleAnywhere, Category = "Mine")
	TObjectPtr<UPointLightComponent> WarningLight;

	UPROPERTY(EditAnywhere, Category = "Mine", meta = (ClampMin = "0"))
	float Damage = 30.f;

	/** Flat damage inside, nothing outside; also the red glow radius. cm */
	UPROPERTY(EditAnywhere, Category = "Mine", meta = (ClampMin = "0"))
	float Radius = 500.f; // 8 m was too big (playtest 2026-11-15). TBD

	/** The balloon this close sets it off. cm */
	UPROPERTY(EditAnywhere, Category = "Mine", meta = (ClampMin = "0"))
	float TriggerDistance = 400.f;

	/** Descent on the parachute, cm/s. The bomber picks a speed in this range so the mine
	 *  reaches basket height when the balloon arrives (design value was 1.5 m/s). */
	UPROPERTY(EditAnywhere, Category = "Mine", meta = (ClampMin = "0"))
	float MinFallSpeed = 20.f;

	UPROPERTY(EditAnywhere, Category = "Mine", meta = (ClampMin = "0"))
	float MaxFallSpeed = 150.f;

	/** Removed once the balloon is this far past it, cm. */
	UPROPERTY(EditAnywhere, Category = "Mine", meta = (ClampMin = "0"))
	float PassedDistance = 2000.f;

	/** Free fall after the parachute is shot, cm/s, and the delay before the harmless blast. */
	UPROPERTY(EditAnywhere, Category = "Mine", meta = (ClampMin = "0"))
	float CutFallSpeed = 500.f;

	UPROPERTY(EditAnywhere, Category = "Mine", meta = (ClampMin = "0"))
	float CutExplodeDelay = 2.f;

	/** Safety limit, s. */
	UPROPERTY(EditAnywhere, Category = "Mine", meta = (ClampMin = "0"))
	float LifeTime = 120.f;

private:
	UFUNCTION()
	void HandlePointDamage(AActor* DamagedActor, float InDamage, AController* InstigatedBy, FVector HitLocation,
		UPrimitiveComponent* HitComponent, FName BoneName, FVector ShotFromDirection, const UDamageType* DamageType, AActor* DamageCauser);

	void Explode(bool bCanDamage);
	bool IsPassed() const;

	UPROPERTY() TObjectPtr<UMaterialInterface> IdleMaterial;
	UPROPERTY() TObjectPtr<UMaterialInterface> WarnMaterial;
	float Age = 0.f;
	float FallSpeed = 150.f;
	float CutTime = -1.f;
	bool bWarning = false;
	bool bExploded = false;
};
