#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NemesisTowerGuard.generated.h"

class ANemesisBullet;
class UBoxComponent;
class UPointLightComponent;
class UStaticMeshComponent;

UENUM(BlueprintType)
enum class ETowerGuardState : uint8
{
	Idle,
	Aiming,
	Reloading,
	Dead,
};

/**
 * Desert tower with a guard on top (AI ворогів, 7.1). Placeholder cubes. The tower is solid level
 * geometry; the guard is the target. With the basket inside Range and in line of sight (a sandstorm
 * blocks it) the guard aims for AimTime with a scope glint, then fires one bullet straight at the
 * basket (no drop), then reloads. One hit from any weapon kills the guard.
 */
UCLASS()
class GUNSANDBALLOONS_API ANemesisTowerGuard : public AActor
{
	GENERATED_BODY()

public:
	ANemesisTowerGuard();

	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintPure, Category = "Tower")
	ETowerGuardState GetState() const { return State; }

	/** Aiming started (glint on) / stopped. Hook for a sound. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Tower")
	void OnAimChanged(bool bAiming);

	UFUNCTION(BlueprintImplementableEvent, Category = "Tower")
	void OnShot();

	UFUNCTION(BlueprintImplementableEvent, Category = "Tower")
	void OnGuardKilled();

protected:
	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;

	UPROPERTY(VisibleAnywhere, Category = "Tower")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "Tower")
	TObjectPtr<UStaticMeshComponent> Tower;

	UPROPERTY(VisibleAnywhere, Category = "Tower")
	TObjectPtr<UStaticMeshComponent> Guard;

	UPROPERTY(VisibleAnywhere, Category = "Tower")
	TObjectPtr<UPointLightComponent> Glint;

	/** Invisible hit zone around the guard, bigger than the model so it can be hit at 50–180 m
	 *  with sway and bullet drop (a 0.8 m guard at 70 m is ~0.6°). Any hit in it kills the guard. */
	UPROPERTY(VisibleAnywhere, Category = "Tower")
	TObjectPtr<UBoxComponent> HitBox;

	/** HitBox half size, cm. TBD */
	UPROPERTY(EditAnywhere, Category = "Tower")
	FVector HitBoxExtent = FVector(130.f, 130.f, 160.f);

	/** Tower height, cm (the guard stands on top). */
	UPROPERTY(EditAnywhere, Category = "Tower", meta = (ClampMin = "100"))
	float TowerHeight = 2500.f;

	/** cm (180 m). */
	UPROPERTY(EditAnywhere, Category = "Tower", meta = (ClampMin = "0"))
	float Range = 18000.f;

	UPROPERTY(EditAnywhere, Category = "Tower", meta = (ClampMin = "0"))
	float AimTime = 2.f;

	UPROPERTY(EditAnywhere, Category = "Tower", meta = (ClampMin = "0"))
	float ReloadTime = 4.f;

	UPROPERTY(EditAnywhere, Category = "Tower", meta = (ClampMin = "0"))
	float Damage = 12.f;

	/** cm/s (250 m/s). */
	UPROPERTY(EditAnywhere, Category = "Tower", meta = (ClampMin = "1"))
	float BulletSpeed = 25000.f;

	UPROPERTY(EditAnywhere, Category = "Tower")
	TSubclassOf<ANemesisBullet> BulletClass;

private:
	UFUNCTION()
	void HandlePointDamage(AActor* DamagedActor, float InDamage, AController* InstigatedBy, FVector HitLocation,
		UPrimitiveComponent* HitComponent, FName BoneName, FVector ShotFromDirection, const UDamageType* DamageType, AActor* DamageCauser);

	bool CanSeeBasket(FVector& OutBasket) const;
	void SetState(ETowerGuardState NewState);
	void Fire(const FVector& Basket);

	ETowerGuardState State = ETowerGuardState::Idle;
	float StateTime = 0.f;
};
