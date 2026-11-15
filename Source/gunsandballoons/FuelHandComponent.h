#pragma once
#include "CoreMinimal.h"
#include "GunComponent.h"
#include "FuelHandComponent.generated.h"
class UStoveFuelType;
class UStaticMeshComponent;
/** A single non-firing fuel prop carried from a bucket to the stove. */
UCLASS(ClassGroup=(Weapon), meta=(BlueprintSpawnableComponent))
class GUNSANDBALLOONS_API UFuelHandComponent : public UGunComponent
{
	GENERATED_BODY()
public:
	UFuelHandComponent();
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Fuel") TObjectPtr<const UStoveFuelType> Fuel;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Fuel") TObjectPtr<UStaticMeshComponent> FuelProp;
	UFUNCTION(BlueprintCallable, Category="Fuel") void SetFuel(const UStoveFuelType* Type);
	virtual void PullTrigger() override {}
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction) override {}
};
