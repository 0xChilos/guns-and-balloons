#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "BalloonCollisionSetupLibrary.generated.h"

UCLASS()
class GUNSANDBALLOONS_API UBalloonCollisionSetupLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Fits envelope spheres and hollow basket boxes to BP_Balloon in Mesh space,
	 * disabling the solid root box. Compile and save the Blueprint after calling.
	 * Returns true on success. Updates existing primitives; safe to run repeatedly.
	 */
	UFUNCTION(BlueprintCallable, Category = "Balloon|Collision")
	static bool AddBalloonCollisionPrimitives(const FString& BlueprintPath);
};
