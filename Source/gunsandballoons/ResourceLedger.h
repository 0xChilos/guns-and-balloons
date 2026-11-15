#pragma once

#include "CoreMinimal.h"
#include "ResourceType.h"
#include "Subsystems/WorldSubsystem.h"
#include "ResourceLedger.generated.h"

class AHookPickup;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnResourceCountChanged, EResourceType, Type, int32, NewCount, int32, Delta);

/**
 * How much of each resource the player has brought in this level (docs/features/grappling-hook.md): a count
 * per resource, raised when a hooked pickup arrives at the player. Displays (AResourceCounterBoard) listen to
 * OnCountChanged. Lives as long as the world: a new level starts from 0.
 */
UCLASS()
class GUNSANDBALLOONS_API UResourceLedger : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Adds Amount of Type. None or Amount <= 0 changes nothing. */
	UFUNCTION(BlueprintCallable, Category = "Resources")
	void Add(EResourceType Type, int32 Amount);

	UFUNCTION(BlueprintPure, Category = "Resources")
	int32 GetCount(EResourceType Type) const;

	/**
	 * Counts a pickup the hook has brought in: each of its Contents adds a random amount in its range, once per
	 * pickup (one that is dropped and hooked again is not counted twice). Returns whether it counted.
	 */
	bool CollectPickup(AHookPickup* Pickup);

	UPROPERTY(BlueprintAssignable, Category = "Resources")
	FOnResourceCountChanged OnCountChanged;

private:
	int32 Counts[ResourceTypes::Num] = {};
};
