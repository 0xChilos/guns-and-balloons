#pragma once
#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Interactable.h"
#include "FuelSupplyComponent.generated.h"
class UStoveFuelType;
class UBoxComponent;
class UStaticMeshComponent;
class UFuelSupplyComponent;
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnFuelContentsChanged, UFuelSupplyComponent*, Supply, int32, NewCount);
/** One bucket holding one fuel type. */
UCLASS(ClassGroup=(Stove), meta=(BlueprintSpawnableComponent))
class GUNSANDBALLOONS_API UFuelSupplyComponent : public USceneComponent, public IInteractable
{
	GENERATED_BODY()
public:
	UFuelSupplyComponent();
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Fuel") TEnumAsByte<ECollisionChannel> InteractTraceChannel = ECC_Visibility;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Fuel") TObjectPtr<UStoveFuelType> Fuel;
	// TBD: prototype value
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Fuel", meta=(ClampMin="0")) int32 Capacity = 10;
	// TBD: prototype value; configure 4 wood / 4 coal / 2 crystal per bucket.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Fuel", meta=(ClampMin="0")) int32 StartCount = 4;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Fuel") TObjectPtr<UBoxComponent> InteractShape;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Fuel") TObjectPtr<UStaticMeshComponent> BucketMesh;
	UPROPERTY(BlueprintAssignable, Category="Fuel") FOnFuelContentsChanged OnContentsChanged;
	UFUNCTION(BlueprintCallable, Category="Fuel") int32 AddFuel(int32 Amount);
	UFUNCTION(BlueprintPure, Category="Fuel") int32 GetCount() const { return Count; }
	UFUNCTION(BlueprintPure, Category="Fuel") int32 GetCapacity() const { return FMath::Max(0, Capacity); }
	static UFuelSupplyComponent* FindSupplyFor(AActor* Owner, const UStoveFuelType* Type);
	virtual bool AcceptsHit_Implementation(const FHitResult& Hit) const override;
	virtual bool CanInteract_Implementation(APawn* Pawn) const override;
	virtual void Interact_Implementation(APawn* Pawn) override;
protected:
	virtual void OnRegister() override;
	virtual void BeginPlay() override;
public:
	/** Sets the pieces in the bucket (clamped to Capacity); the run's fuel, UBalloonRunSubsystem. */
	void SetCount(int32 Value);
private:
	UPROPERTY(Transient) int32 Count = 0;
};
