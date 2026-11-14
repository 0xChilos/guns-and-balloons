#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AmmoSupplyComponent.generated.h"

class UAmmoSupplyComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAmmoRoundTaken, UAmmoSupplyComponent*, Supply, APawn*, InstigatorPawn);

/**
 * Hands out ammo one round per click. Add it to any actor the player can aim at (an ammo crate,
 * the ammo pouch on the basket wall, ...): clicking that actor, or any actor it owns, gives the
 * player's revolver a single round (URevolverComponent::LoadRound). There is no reload key.
 *
 * Works on its own or alongside IInteractable on the same actor (e.g. AInteractSwap opens the
 * crate on the first click and the same click already takes a round).
 */
UCLASS(ClassGroup = (Weapon), meta = (BlueprintSpawnableComponent))
class GUNSANDBALLOONS_API UAmmoSupplyComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	/** True while there is ammo left to take. */
	UFUNCTION(BlueprintPure, Category = "Ammo")
	bool HasRounds() const { return Rounds != 0; }

	/** Rounds left; -1 means unlimited. */
	UFUNCTION(BlueprintPure, Category = "Ammo")
	int32 GetRounds() const { return Rounds; }

	/**
	 * Give one round to the pawn's revolver. Returns false, taking nothing, if the supply is empty
	 * or the revolver cannot take another round (every chamber live or already queued).
	 */
	UFUNCTION(BlueprintCallable, Category = "Ammo")
	bool GiveRound(APawn* InstigatorPawn);

	/** A round left the supply and went into the revolver's loading queue. */
	UPROPERTY(BlueprintAssignable, Category = "Ammo")
	FOnAmmoRoundTaken OnRoundTaken;

	/** Click was ignored because the revolver is full (hook a "no" sound here). */
	UPROPERTY(BlueprintAssignable, Category = "Ammo")
	FOnAmmoRoundTaken OnRoundRefused;

protected:
	/** Rounds this supply holds; -1 = unlimited. Needs tuning: the design value is TBD (weapons.md). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ammo", meta = (ClampMin = "-1"))
	int32 Rounds = -1;
};
