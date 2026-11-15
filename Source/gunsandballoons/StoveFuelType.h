#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "StoveFuelType.generated.h"

class UStaticMesh;

/** Fuel tuning and the prop displayed when a piece is held in the player's hand. */
UCLASS(BlueprintType)
class GUNSANDBALLOONS_API UStoveFuelType : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stove Fuel")
	FText DisplayName;

	/** Burn time added by one piece, in seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stove Fuel", meta = (ClampMin = "0", Units = "s"))
	float BurnTime = 0.f;

	/** Cooling pause from insertion, including ignition; negative uses the stove default. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stove Fuel", meta = (ClampMin = "-1", Units = "s"))
	float DecayPauseOverride = -1.f;

	/** Temperature added by one piece, in degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stove Fuel", meta = (ClampMin = "0"))
	float HeatAdd = 0.f;

	/** Instantly reach the target temperature and skip ignition delay (crystal). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stove Fuel")
	bool bInstantHeat = false;

	/** Degrees per second; zero uses the stove's HeatRate. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stove Fuel", meta = (ClampMin = "0"))
	float HeatRateOverride = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stove Fuel|Hand")
	TSoftObjectPtr<UStaticMesh> HandMesh;

	// TBD: prototype value; tune the held prop's offset, rotation and scale with the hand system.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stove Fuel|Hand")
	FTransform HandTransform = FTransform::Identity;
};
