#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HandItemPickup.generated.h"

class UWeaponRackComponent;

/**
 * A spot in the world where one of the player's hand items lies (revolver, rifle, shotgun, grappling
 * hook, fire extinguisher): a UWeaponRackComponent on its own actor, with the click box fitted to the item.
 * Place it within the player's InteractRange and pick the item with Rack > Item Class. LMB takes the item,
 * puts it back, or swaps it for the one in hand; all of that is the rack's and the pawn's logic.
 */
UCLASS(Blueprintable)
class GUNSANDBALLOONS_API AHandItemPickup : public AActor
{
	GENERATED_BODY()

public:
	AHandItemPickup();

	UWeaponRackComponent* GetRack() const { return Rack; }

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hand Item")
	TObjectPtr<UWeaponRackComponent> Rack;
};
