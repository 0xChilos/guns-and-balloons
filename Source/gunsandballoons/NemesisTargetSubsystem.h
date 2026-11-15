#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "NemesisTargetSubsystem.generated.h"

class ANemesisAirship;
class UBalloonFlightComponent;
class UEngineHealthComponent;

/**
 * What every enemy needs to know about the player's balloon, in one place: where it and its
 * basket are, how it moves, and how to damage it. Also shared limits across enemy actors
 * (mines in the air, active bird flocks, engaged patrol groups). Enemies that don't come from the
 * nemesis airship (towers, beacons, birds, patrols) only depend on this.
 */
UCLASS()
class GUNSANDBALLOONS_API UNemesisTargetSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UNemesisTargetSubsystem* Get(const UObject* WorldContext);

	/** The actor with UBalloonFlightComponent (cached). Adds UEngineHealthComponent if missing. */
	AActor* GetBalloon();
	UBalloonFlightComponent* GetFlight();
	UEngineHealthComponent* GetHealth();

	/** Basket centre: hit point and aim point for every enemy. */
	FVector GetBasketLocation();
	FVector GetBalloonForward();
	FVector GetBalloonVelocity();

	/** Damage to the balloon: hits the basket, taken by the engine. */
	void DamageBalloon(float Amount, AActor* Causer);

	/** Basket centre relative to the balloon actor. */
	FVector BasketOffset = FVector(0.f, 0.f, 60.f);

	// --- Shared limits (prototype values, TBD) ---
	int32 MaxMines = 3;
	int32 MaxActiveFlocks = 1;
	int32 MaxEngagedPatrols = 2;

	bool CanSpawnMine();
	void RegisterMine(AActor* Mine);

	/** A flock asks before it starts crossing; it must ReleaseFlock when done. */
	bool TryClaimFlock(AActor* Flock);
	void ReleaseFlock(AActor* Flock);

	/** A patrol group asks before it attacks; it must ReleasePatrol when it returns to its route. */
	bool TryEngagePatrol(AActor* Patrol);
	void ReleasePatrol(AActor* Patrol);

	ANemesisAirship* GetAirship();

	/** A sandstorm blocks the tower guards' sight. Set by the (future) desert weather system. */
	bool IsSandstormActive() const { return bSandstormActive; }
	void SetSandstormActive(bool bActive) { bSandstormActive = bActive; }

private:
	void FindBalloon();

	TWeakObjectPtr<AActor> Balloon;
	TWeakObjectPtr<UBalloonFlightComponent> Flight;
	TWeakObjectPtr<UEngineHealthComponent> Health;
	TWeakObjectPtr<ANemesisAirship> Airship;
	TArray<TWeakObjectPtr<AActor>> Mines;
	TArray<TWeakObjectPtr<AActor>> Flocks;
	TArray<TWeakObjectPtr<AActor>> Patrols;
	bool bSandstormActive = false;
};
