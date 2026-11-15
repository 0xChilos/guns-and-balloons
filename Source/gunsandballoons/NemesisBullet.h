#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NemesisBullet.generated.h"

class UStaticMeshComponent;

/**
 * Enemy bullet. Flies straight (no gravity, team decision). Damages the balloon only if its path
 * crosses the basket. Placeholder glowing cube.
 */
UCLASS()
class GUNSANDBALLOONS_API ANemesisBullet : public AActor
{
	GENERATED_BODY()

public:
	ANemesisBullet();

	/** Flies straight along Direction. Optional overrides (<= 0 keeps the default): speed cm/s, damage. */
	void Fire(const FVector& Direction, float InSpeed = 0.f, float InDamage = 0.f);

	virtual void Tick(float DeltaSeconds) override;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Bullet")
	TObjectPtr<UStaticMeshComponent> Mesh;

	/** cm/s (150 m/s). */
	UPROPERTY(EditAnywhere, Category = "Bullet", meta = (ClampMin = "1"))
	float Speed = 15000.f;

	UPROPERTY(EditAnywhere, Category = "Bullet", meta = (ClampMin = "0"))
	float Damage = 3.f;

	/** Path within this distance of the basket centre = hit, cm. TBD */
	UPROPERTY(EditAnywhere, Category = "Bullet", meta = (ClampMin = "0"))
	float BasketHitRadius = 130.f;

	UPROPERTY(EditAnywhere, Category = "Bullet", meta = (ClampMin = "0"))
	float LifeTime = 2.f;

private:
	FVector Velocity = FVector::ZeroVector;
	float Age = 0.f;
};
