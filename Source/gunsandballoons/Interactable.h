#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Interactable.generated.h"

UINTERFACE(MinimalAPI, Blueprintable)
class UInteractable : public UInterface
{
	GENERATED_BODY()
};

/**
 * Anything the player can aim at and press LMB on (see AFirstPersonCameraPawn::TryInteract).
 * Implement in C++ (override the _Implementation functions) or in a Blueprint (Class Settings > Interfaces).
 */
class GUNSANDBALLOONS_API IInteractable
{
	GENERATED_BODY()

public:
	/** Called when the player presses LMB while aiming at this actor and CanInteract returned true. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	void Interact(APawn* InstigatorPawn);

	/** Return false to ignore clicks (e.g. a crate that is already open). */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	bool CanInteract(APawn* InstigatorPawn) const;

	virtual void Interact_Implementation(APawn* InstigatorPawn) {}
	virtual bool CanInteract_Implementation(APawn* InstigatorPawn) const { return true; }
};
