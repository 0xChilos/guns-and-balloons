#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interactable.h"
#include "AmmoCrate.generated.h"

class UStaticMeshComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAmmoCrateOpened, AAmmoCrate*, Crate, APawn*, InstigatorPawn);

/**
 * An ammo crate the player opens with LMB.
 *
 * Holds two visual states at the same spot:
 *  - ClosedState (ClosedBody + ClosedLid) - shown at start.
 *  - OpenState   (OpenBody + OpenLid)     - swapped in instantly on interaction.
 *
 * Add, remove or re-mesh components under either state in a Blueprint child; everything
 * under a state root is shown/hidden and gets collision toggled together.
 * Tick "Start Open" on an instance to preview / edit the open layout in the editor.
 */
UCLASS(Blueprintable)
class GUNSANDBALLOONS_API AAmmoCrate : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	AAmmoCrate();

	virtual void Interact_Implementation(APawn* InstigatorPawn) override;
	virtual bool CanInteract_Implementation(APawn* InstigatorPawn) const override;

	/** Swap the closed meshes for the open ones. Does nothing if already open. */
	UFUNCTION(BlueprintCallable, Category = "Ammo Crate")
	void Open(APawn* InstigatorPawn = nullptr);

	UFUNCTION(BlueprintPure, Category = "Ammo Crate")
	bool IsOpen() const { return bIsOpen; }

	/** Fires right after the crate switches to the open state (hook ammo pickup, sound, VFX here). */
	UPROPERTY(BlueprintAssignable, Category = "Ammo Crate")
	FOnAmmoCrateOpened OnOpened;

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> ClosedState;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> ClosedBody;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> ClosedLid;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> OpenState;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> OpenBody;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> OpenLid;

	/** Begin play already open (also previews the open state in the editor). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ammo Crate")
	bool bStartOpen = false;

private:
	void ApplyState(bool bOpen);

	bool bIsOpen = false;
};
