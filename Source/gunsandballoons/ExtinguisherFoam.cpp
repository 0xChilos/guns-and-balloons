#include "ExtinguisherFoam.h"

#include "Math/RandomStream.h"

AExtinguisherFoam::AExtinguisherFoam()
{
	// First-pass values; tune in a Blueprint child.

	// Foam is a liquid jet, not a cloud: it keeps its speed through the air and barely swirls.
	QuadraticDrag = 0.0008f;
	LinearDrag = 0.25f;
	Turbulence = 20.f;
	TurbulenceScale = 40.f;

	// Cool white: the bubble tones are drawn into the textures, this tints them all.
	Color = FLinearColor(0.9f, 0.94f, 0.97f);
	MaxOpacity = 1.f;
	FadeOutShare = 0.3f;
	NearFadeDistance = 15.f;

	// A thick stream, a cloud around it and the piles it leaves: many sprites alive at once.
	MaxPuffs = 450;

	// Big enough for bubbles of a few texels each, still blocky up close (nearest filtering).
	TextureSize = 32;
	ShapeVariants = 8;

	// Upright, so every bubble's highlight sits on the same side (one light direction).
	bTurnPuffs = false;

	// Choppy like an old sprite effect, but fast enough that the jet still reads as a stream.
	AnimationRate = 20.f;
}

void AExtinguisherFoam::DrawPuffShape(int32 ShapeIndex, int32 Size, TArrayView<float> Density, TArrayView<uint8> Shade) const
{
	struct FBubble
	{
		FVector2D Centre;
		float Radius;
	};

	// The same bubbles every run for a shape (no flicker between plays), different for each shape.
	FRandomStream Random(7919 * (ShapeIndex + 1));

	// One big bubble near the middle, then smaller ones around it, drawn over it (nearer the eye).
	TArray<FBubble, TInlineAllocator<32>> Bubbles;
	const float MinRadius = FMath::Min(BubbleRadiusMin, BubbleRadiusMax);
	const float MaxRadius = FMath::Max(BubbleRadiusMin, BubbleRadiusMax);
	Bubbles.Add({ FVector2D(Random.FRandRange(-0.12f, 0.12f), Random.FRandRange(-0.12f, 0.12f)), float(Random.FRandRange(0.28f, 0.36f)) });
	for (int32 i = 0; i < BubblesPerPuff; ++i)
	{
		const float Angle = float(Random.FRandRange(0.f, UE_TWO_PI));
		const float Distance = float(Random.FRandRange(0.15f, 0.65f));
		// Inside the sprite: bubbles further out are smaller.
		const float Radius = FMath::Min(float(Random.FRandRange(MinRadius, MaxRadius)), 0.95f - Distance);
		if (Radius > 0.04f)
		{
			Bubbles.Add({ FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Distance, Radius });
		}
	}

	auto ToByte = [](float Value) { return static_cast<uint8>(FMath::RoundToInt(FMath::Clamp(Value, 0.f, 1.f) * 255.f)); };
	const uint8 Gap = ToByte(GapShade);
	const uint8 Rim = ToByte(BubbleRimShade);
	const uint8 Body = ToByte(BubbleShade);
	const uint8 Highlight = ToByte(HighlightShade);
	const float Slice = 0.53f + 2.17f * ShapeIndex;

	for (int32 Y = 0; Y < Size; ++Y)
	{
		for (int32 X = 0; X < Size; ++X)
		{
			const int32 i = Y * Size + X;
			const FVector2D P((X + 0.5f) / Size * 2.f - 1.f, (Y + 0.5f) / Size * 2.f - 1.f);

			// The foam the bubbles sit in: a lumpy mass in the middle, in shadow. Its outline dithers away first.
			const float Lumps = FMath::PerlinNoise3D(FVector(P.X * 2.6f, P.Y * 2.6f, Slice));
			const float MassEdge = P.Size() * 1.45f + 0.25f * Lumps;
			float Solid = FMath::Clamp((1.f - MassEdge) / 0.35f, 0.f, 1.f);
			uint8 Tone = Gap;

			for (const FBubble& Bubble : Bubbles)
			{
				const float D = FVector2D::Distance(P, Bubble.Centre) / Bubble.Radius;
				if (D >= 1.f)
				{
					continue;
				}
				// A bubble is solid foam; only its outermost ring thins out (so a fading puff breaks into bubbles).
				Solid = FMath::Max(Solid, D < 0.8f ? 1.f : 0.75f);
				// Flat shading lit from one side: a highlight texel up-left, the body, a darker rim.
				const FVector2D Shine = Bubble.Centre - FVector2D(0.4f, 0.4f) * Bubble.Radius;
				if (FVector2D::Distance(P, Shine) < FMath::Max(0.22f * Bubble.Radius, 1.45f / Size))
				{
					Tone = Highlight;
				}
				else
				{
					Tone = D > 0.7f ? Rim : Body;
				}
			}

			Density[i] = Solid;
			Shade[i] = Tone;
		}
	}
}
