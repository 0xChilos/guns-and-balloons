#pragma once

#include "CoreMinimal.h"
#include "GunSmoke.h"

class UWorld;

/**
 * Enemy effects drawn with the guns' PS1 smoke (AGunSmoke: blocky dithered puffs, world physics), so
 * everything shares one look: black engine smoke, fire, the rocket's smoke trail, explosions.
 * Each effect is its own AGunSmoke actor in the world; it outlives its source by its puffs' life.
 */
namespace NemesisFx
{
	enum class EKind : uint8 { EngineSmoke, Fire, RocketTrail, BlastFire, BlastSmoke };

	/** Smoke actor with the colour and air settings for that kind. */
	AGunSmoke* Spawn(UWorld* World, EKind Kind);

	/** Puff settings for that kind. */
	FGunSmokePuffs Puffs(EKind Kind);

	/** One-shot explosion (fire + smoke) at Location; Scale 1 = rocket / mine size. */
	void Explosion(UWorld* World, const FVector& Location, float Scale = 1.f);

	/** A continuous stream into one smoke actor (call Tick every frame while it should emit). */
	struct FStream
	{
		TWeakObjectPtr<AGunSmoke> Smoke;
		FGunSmokePuffs Settings;
		float Rate = 0.f; // puffs per second
		float Carry = 0.f;
		FVector LastLocation = FVector::ZeroVector;
		bool bHasLast = false;

		void Start(UWorld* World, EKind Kind, float InRate);
		bool IsActive() const { return Smoke.IsValid(); }
		/** Puffs are spread along the path since the last Tick, so the trail stays unbroken at any frame rate. */
		void Tick(float DeltaSeconds, const FVector& Location, const FVector& Direction, const FVector& SourceVelocity);
		/** Stop emitting; the actor stays until its last puffs fade, then removes itself. */
		void Stop();
	};
}
