#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "GameFramework/Actor.h"
#include "GunProjectile.generated.h"

class AController;
class UGunComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UStaticMeshComponent;
class UTexture;

/** Everything a bullet needs to know when it leaves the gun (filled by UGunComponent::FireShot). */
struct FGunShot
{
	/** Where the bullet really flies from: the eye, so it lands on the crosshair. */
	FVector Start = FVector::ZeroVector;
	FVector Velocity = FVector::ZeroVector;
	/** Where the tracer is drawn from: the muzzle of the viewmodel, as seen in the world. */
	FVector Muzzle = FVector::ZeroVector;

	/** cm/s^2 along Z (negative = down). */
	float GravityZ = 0.f;
	/** Quadratic air drag (1/cm): deceleration = Drag * speed^2. 0 = none. */
	float Drag = 0.f;
	/** Size of the drawn bullet and streak (1 = a full bullet; pellets are smaller). */
	float VisualScale = 1.f;
	/** Flight distance after which the bullet is gone. */
	float MaxDistance = 0.f;
	float Damage = 0.f;
	float HitImpulse = 0.f;
	ECollisionChannel TraceChannel = ECC_Visibility;
	bool bDebug = false;

	TWeakObjectPtr<AActor> Shooter;
	TWeakObjectPtr<AController> InstigatorController;
	/** Gets OnShotHit when the bullet lands. */
	TWeakObjectPtr<UGunComponent> Gun;
};

/**
 * A ballistic bullet with a tracer. Not hitscan: it flies under gravity from where it was fired, and
 * every tick traces the segment it covered (complex collision, on the gun's channel), so a fast bullet
 * never skips through thin geometry. On a hit it applies point damage and an impulse on physics
 * objects, tells the gun (OnShotHit) and is gone; after MaxDistance it is gone without a hit.
 *
 * The physics path starts at the eye so shots land on the crosshair. What you see - a glowing bullet
 * with a tracer streak behind it - is drawn from the gun's muzzle and merges into the real path over
 * VisualConvergeDistance, so it leaves the barrel. Both are engine shapes (sphere, and a cylinder
 * stretched along the velocity) with an unlit, additive material; they never collide.
 */
UCLASS(Blueprintable)
class GUNSANDBALLOONS_API AGunProjectile : public AActor
{
	GENERATED_BODY()

public:
	AGunProjectile();

	/** Start flying. Call right after spawning. */
	void Launch(const FGunShot& InShot);

	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintPure, Category = "Projectile")
	FVector GetBulletVelocity() const { return Velocity; }

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	TObjectPtr<USceneComponent> Root;

	/** The tracer streak behind the bullet. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	TObjectPtr<UStaticMeshComponent> Tracer;

	/** The glowing bullet itself, at the front of the streak. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	TObjectPtr<UStaticMeshComponent> Bullet;

	/** Unlit material for the streak, with a "Color" vector and a "Texture" texture parameter. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Tracer")
	TObjectPtr<UMaterialInterface> TracerMaterial;

	/** Plain texture for the material's "Texture" parameter (its default is a grid). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Tracer")
	TObjectPtr<UTexture> TracerTexture;

	/** Emissive colour (HDR: values above 1 glow). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Tracer")
	FLinearColor TracerColor = FLinearColor(12.f, 5.f, 1.2f);

	/** Streak length as the distance the bullet covers in this many seconds. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Tracer", meta = (ClampMin = "0.0"))
	float TracerSeconds = 0.012f;

	/** Streak thickness (cm) close up. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Tracer", meta = (ClampMin = "0.0"))
	float TracerWidth = 0.8f;

	/** Thickness never drops below this fraction of the distance to the camera, so far tracers stay visible. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Tracer", meta = (ClampMin = "0.0"))
	float TracerMinWidthPerDistance = 0.0012f;

	/** Bullet glow (HDR), brighter than the streak so the round reads as a point of light. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Bullet")
	FLinearColor BulletColor = FLinearColor(25.f, 14.f, 5.f);

	/** Bullet diameter (cm) close up. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Bullet", meta = (ClampMin = "0.0"))
	float BulletSize = 1.6f;

	/** Diameter never drops below this fraction of the distance to the camera, so a far bullet stays visible. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Bullet", meta = (ClampMin = "0.0"))
	float BulletMinSizePerDistance = 0.0022f;

	/** Flight distance (cm) over which the streak moves from the muzzle onto the real path from the eye. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Tracer", meta = (ClampMin = "0.0"))
	float VisualConvergeDistance = 1500.f;

	/** Safety net: the bullet is removed after this many seconds even if it never got MaxDistance away. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile", meta = (ClampMin = "0.1"))
	float MaxLifeSeconds = 10.f;

private:
	void HandleHit(const FHitResult& Hit);
	void UpdateTracer();

	FGunShot Shot;
	FVector Position = FVector::ZeroVector;
	FVector Velocity = FVector::ZeroVector;
	/** Path length flown so far. */
	float Traveled = 0.f;
	bool bLaunched = false;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> TracerMID;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> BulletMID;
};
