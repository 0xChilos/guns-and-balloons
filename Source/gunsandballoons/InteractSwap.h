#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interactable.h"
#include "InteractSwap.generated.h"

class AInteractSwap;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnInteractSwapped, AInteractSwap*, Swap, APawn*, InstigatorPawn);

/**
 * Swaps one set of placed level actors for another when the player clicks on them.
 *
 * Setup: place this actor anywhere, fill BeforeActors (e.g. the closed crate body + lid) and
 * AfterActors (e.g. the open crate body + lid, placed anywhere in the level as a reference).
 *
 * At BeginPlay the AfterActors are hidden. Aiming at any BeforeActor and pressing LMB hides the
 * BeforeActors and shows the AfterActors - moved so AfterActors[0] lands exactly on BeforeActors[0],
 * keeping the rest of the AfterActors' layout relative to it.
 */
UCLASS()
class GUNSANDBALLOONS_API AInteractSwap : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	AInteractSwap();

	virtual void Interact_Implementation(APawn* InstigatorPawn) override;
	virtual bool CanInteract_Implementation(APawn* InstigatorPawn) const override;

	UFUNCTION(BlueprintCallable, Category = "Swap")
	void Swap(APawn* InstigatorPawn = nullptr);

	UFUNCTION(BlueprintPure, Category = "Swap")
	bool HasSwapped() const { return bSwapped; }

	UPROPERTY(BlueprintAssignable, Category = "Swap")
	FOnInteractSwapped OnSwapped;

protected:
	virtual void BeginPlay() override;

	/** Shown at start; clicking any of these triggers the swap. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Swap")
	TArray<TObjectPtr<AActor>> BeforeActors;

	/** Hidden at start; shown in place of BeforeActors after the swap. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Swap")
	TArray<TObjectPtr<AActor>> AfterActors;

	/** Move AfterActors so AfterActors[0] takes BeforeActors[0]'s transform. Off = show them where they were placed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Swap")
	bool bMoveAfterActorsIntoPlace = true;

private:
	static void SetActorActive(AActor* Actor, bool bActive);

	bool bSwapped = false;
};
