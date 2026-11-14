#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "GameFramework/Actor.h"
#include "GunSmoke.generated.h"

struct FCollisionQueryParams;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UTexture2D;

/**
 * One kind of smoke a gun gives off (muzzle blast, smoking barrel, open breech): a burst all at once
 * (Duration 0) or a stream that thins out to nothing over Duration. How each puff then moves is up to
 * the AGunSmoke that simulates it.
 */
USTRUCT(BlueprintType)
struct FGunSmokePuffs
{
	GENERATED_BODY()

	/** Burst: puffs at once. Stream: puffs per second at the start, falling to 0 over Duration. 0 = off. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Smoke", meta = (ClampMin = "0.0"))
	float Amount = 0.f;

	/** 0 = a single burst; otherwise how long (s) the stream lasts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Smoke", meta = (ClampMin = "0.0", Units = "Seconds"))
	float Duration = 0.f;

	/** Delay (s) before a stream starts, e.g. to let the muzzle blast clear first. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Smoke", meta = (ClampMin = "0.0", Units = "Seconds"))
	float Delay = 0.f;

	/** Speed range (cm/s) out of the source, along its direction. Air drag slows it down from there. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Smoke", meta = (ClampMin = "0.0"))
	float SpeedMin = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Smoke", meta = (ClampMin = "0.0"))
	float SpeedMax = 0.f;

	/** Largest angle (degrees) a puff leaves off the source's direction. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Smoke", meta = (ClampMin = "0.0", ClampMax = "90.0", Units = "Degrees"))
	float ConeAngle = 15.f;

	/** Puff radius (cm) as it leaves the source, and what it spreads out to as it mixes with the air. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Smoke", meta = (ClampMin = "0.1"))
	float StartRadius = 3.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Smoke", meta = (ClampMin = "0.1"))
	float EndRadius = 25.f;

	/** Roughly how long (s) the spreading takes (time constant). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Smoke", meta = (ClampMin = "0.01", Units = "Seconds"))
	float GrowTime = 0.6f;

	/**
	 * How thick the smoke is: optical depth through a fresh puff (at StartRadius). The same smoke spread
	 * over a bigger puff gets thinner with the square of the radius, so a puff fades as it grows
	 * (opacity = 1 - e^-depth).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Smoke", meta = (ClampMin = "0.0"))
	float Density = 3.f;

	/** Lifetime range (s) of a puff. It fades out over the last part of it (AGunSmoke::FadeOutShare). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Smoke", meta = (ClampMin = "0.05", Units = "Seconds"))
	float LifeMin = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Smoke", meta = (ClampMin = "0.05", Units = "Seconds"))
	float LifeMax = 3.f;

	/** Upward pull (cm/s²) of fresh, hot smoke. It fades as the puff cools (CoolTime). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Smoke")
	float Buoyancy = 50.f;

	/** Roughly how long (s) a puff stays hot enough to rise (time constant). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Smoke", meta = (ClampMin = "0.01", Units = "Seconds"))
	float CoolTime = 1.5f;

	/** Share of the source's own motion a puff carries along (the balloon moving, a barrel swinging). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Smoke", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float InheritVelocity = 0.5f;
};

/**
 * Simulates and draws the smoke of one gun in the world (not on the first-person mesh, so the player
 * moves through it and it stays behind). Every puff is a camera-facing sprite moved by physics:
 * quadratic + linear air drag relative to the Wind, buoyancy while hot, a shared turbulence field, and
 * collision with the world; it spreads out and thins (Beer-Lambert) as it mixes with the air.
 *
 * Drawn in the game's PS1 look (docs/art-style.md): a small blocky blob, flat two-tone (light core,
 * darker rim), no smooth alpha - a thinner puff is a 4x4 Bayer dither pattern cut out of it, so it
 * breaks up into pixels as it fades. Puffs stay upright (a random quarter turn) and move at a low
 * AnimationRate, like old sprite effects.
 *
 * Spawned by UGunComponent at BeginPlay; the gun decides where and when to emit (Emit). Needs no assets:
 * the dithered puff textures are generated at runtime, drawn on the engine plane with the engine's
 * masked pass-through material. Make a Blueprint child to tune it and set it as the gun's SmokeClass.
 */
UCLASS()
class GUNSANDBALLOONS_API AGunSmoke : public AActor
{
	GENERATED_BODY()

public:
	AGunSmoke();

	/**
	 * Start Count puffs of the given kind at Location, heading along Direction (world). SourceVelocity is
	 * the motion of what gave them off; Puffs.InheritVelocity of it is kept. Strength scales the density.
	 */
	void Emit(const FGunSmokePuffs& Puffs, int32 Count, const FVector& Location, const FVector& Direction, const FVector& SourceVelocity, float Strength = 1.f);

	UFUNCTION(BlueprintPure, Category = "Smoke")
	int32 GetNumPuffs() const { return Puffs.Num(); }

	virtual void Tick(float DeltaSeconds) override;

	// --- Air -------------------------------------------------------------------------------

	/** Velocity (cm/s) of the air. Smoke drifts with it. Set it from a wind system once there is one. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Smoke|Air")
	FVector Wind = FVector::ZeroVector;

	/** Quadratic drag (1/cm): a fast puff loses half its speed over ln2 / this (0.012 -> ~58cm). Stops the muzzle blast. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Smoke|Air", meta = (ClampMin = "0.0"))
	float QuadraticDrag = 0.012f;

	/** Linear drag (1/s): how quickly a slow puff settles into the air's motion. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Smoke|Air", meta = (ClampMin = "0.0"))
	float LinearDrag = 1.5f;

	/** Strength (cm/s²) of the swirling turbulence field the puffs drift through. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Smoke|Air", meta = (ClampMin = "0.0"))
	float Turbulence = 35.f;

	/** Size (cm) of the turbulence eddies. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Smoke|Air", meta = (ClampMin = "1.0"))
	float TurbulenceScale = 40.f;

	/** How fast the turbulence field changes (per second). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Smoke|Air", meta = (ClampMin = "0.0"))
	float TurbulenceSpeed = 0.4f;

	/** Puffs slide along walls and the basket instead of passing through (one line trace per puff per tick). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Smoke|Air")
	bool bCollide = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Smoke|Air", meta = (EditCondition = "bCollide"))
	TEnumAsByte<ECollisionChannel> CollisionChannel = ECC_Visibility;

	// --- Look ------------------------------------------------------------------------------

	/** Black-powder smoke: thick and pale grey. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Smoke|Look")
	FLinearColor Color = FLinearColor(0.78f, 0.76f, 0.72f);

	/** Brightness of a puff's rim relative to its core (flat two-tone shading). 1 = one flat colour. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Smoke|Look", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float RimShade = 0.78f;

	/** Opacity cap of a single puff, however thick: the share of its pixels drawn (dither). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Smoke|Look", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MaxOpacity = 0.85f;

	/** Last share of its life over which a puff fades out. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Smoke|Look", meta = (ClampMin = "0.01", ClampMax = "1.0"))
	float FadeOutShare = 0.4f;

	/** A puff fades out as the camera comes within this distance (cm) plus its radius, so it never cuts at the near plane. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Smoke|Look", meta = (ClampMin = "0.0"))
	float NearFadeDistance = 10.f;

	/** Puffs alive at once; the oldest gives way to a new one. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Smoke|Look", meta = (ClampMin = "1", ClampMax = "256"))
	int32 MaxPuffs = 64;

	/** Size (px) of the generated puff textures. Small and Nearest-filtered: blocky, PS1-like. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Smoke|Look", meta = (ClampMin = "8", ClampMax = "64"))
	int32 TextureSize = 16;

	/** How many different blob shapes the puffs pick from at random. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Smoke|Look", meta = (ClampMin = "1", ClampMax = "8"))
	int32 ShapeVariants = 4;

	/**
	 * Updates per second of the puffs' motion, size and fade (they still face the camera every frame).
	 * Low = choppy, old-sprite motion. 0 = every frame.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Smoke|Look", meta = (ClampMin = "0.0", Units = "Hertz"))
	float AnimationRate = 15.f;

	/** Quad each puff is drawn on (faces the camera). 100x100cm, facing +Z, like /Engine/BasicShapes/Plane. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Smoke|Look")
	TObjectPtr<UStaticMesh> PuffMesh;

	/**
	 * Unlit masked material of the puffs. Takes the generated texture as "SlateUI" (shade in RGB,
	 * dithered cut-out in alpha) and the colour as "TintColorAndOpacity" (the engine's
	 * Widget3DPassThrough_Masked). A custom one must use the same parameter names.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Smoke|Look")
	TObjectPtr<UMaterialInterface> PuffMaterial;

protected:
	virtual void BeginPlay() override;

private:
	struct FPuff
	{
		FVector Location;
		FVector Velocity;
		float Age;
		float Life;
		float StartRadius;
		float EndRadius;
		float GrowTime;
		float Density;
		float Buoyancy;
		float CoolTime;
		/** Which generated blob shape it shows. */
		int32 Shape;
		/** Turn on screen, in quarter turns (0-3). */
		int32 Quarter;
		/** Look as of the last animation step: radius (cm) and opacity before the near-camera fade. */
		float Radius;
		float Opacity;
	};

	/** Radius and opacity of a puff for its current age (stored on it). */
	void UpdateLook(FPuff& Puff) const;

	/** The dithered texture for a shape at an opacity, or null when nothing of it would show. */
	UTexture2D* GetPuffTexture(int32 Shape, float Opacity) const;

	/** Moves one puff on by DeltaSeconds; Query (null = no collision) is what it may bump into. */
	void Simulate(FPuff& Puff, float DeltaSeconds, float Time, const FCollisionQueryParams* Query) const;

	/** A puff sprite: the pooled component for slot Index, created on first use. */
	UStaticMeshComponent* GetPuffComponent(int32 Index);

	/** The blocky puff textures: per shape, one per dither level (two-tone grey, cut-out in alpha). */
	void CreatePuffTextures();

	TArray<FPuff> Puffs;

	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> Root;

	/** Sprite components, slot i drawing Puffs[i]; slots past the live puffs are hidden. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> PuffComponents;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> PuffMaterials;

	/** Shape-major: [Shape * DitherLevels + Level - 1] draws Level / DitherLevels of the puff's pixels. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTexture2D>> PuffTextures;

	/** Texture each slot's material shows now, so it is only set when it changes. */
	TArray<UTexture2D*> SlotTextures;

	/** Shapes PuffTextures was made with. */
	int32 NumShapes = 0;

	/** Time (s) not yet stepped (AnimationRate). */
	float PendingTime = 0.f;

	/** Slots shown last tick (the rest are already hidden). */
	int32 NumVisible = 0;
};
