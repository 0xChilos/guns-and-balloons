#pragma once

#include "CoreMinimal.h"
#include "GunSmoke.h"
#include "ExtinguisherFoam.generated.h"

/**
 * The fire extinguisher's foam: the same PS1-style sprite simulation as the gun smoke (choppy motion,
 * Bayer-dithered instead of alpha, nearest-filtered low-res textures), but drawn as foam: every sprite is a
 * cluster of round bubbles on a foam base, in four flat tones (shadowed gaps, bubble rim, bubble body, one
 * highlight texel). Tuned for a heavy liquid spray: white, opaque, little air drag, hardly any swirl. How a
 * puff flies, falls and sticks where it lands is set per emission (UExtinguisherComponent::FoamSpray and
 * FoamMist; FGunSmokePuffs::Gravity and StickTime).
 *
 * Spawned by UExtinguisherComponent as its SmokeClass. Make a Blueprint child to tune it.
 */
UCLASS()
class GUNSANDBALLOONS_API AExtinguisherFoam : public AGunSmoke
{
	GENERATED_BODY()

public:
	AExtinguisherFoam();

	// --- Bubbles (the generated textures; read at BeginPlay) --------------------------------

	/** Bubbles per sprite besides the big one in the middle. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Smoke|Foam", meta = (ClampMin = "0", ClampMax = "24"))
	int32 BubblesPerPuff = 10;

	/** Smallest and largest bubble radius, as a share of the sprite's radius. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Smoke|Foam", meta = (ClampMin = "0.03", ClampMax = "0.5"))
	float BubbleRadiusMin = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Smoke|Foam", meta = (ClampMin = "0.03", ClampMax = "0.5"))
	float BubbleRadiusMax = 0.24f;

	/** Flat tones (0..1, times Color): the foam between bubbles, a bubble's rim, its body, its highlight. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Smoke|Foam", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float GapShade = 0.55f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Smoke|Foam", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float BubbleRimShade = 0.72f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Smoke|Foam", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float BubbleShade = 0.88f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Smoke|Foam", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float HighlightShade = 1.f;

protected:
	virtual void DrawPuffShape(int32 ShapeIndex, int32 Size, TArrayView<float> Density, TArrayView<uint8> Shade) const override;
};
