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
	 * Adds a set of collision primitives (sphere + boxes) to the BP_Balloon Blueprint's
	 * SimpleConstructionScript, parented to the existing BalloonCollision root node.
	 * Returns true on success. Run once; repeated calls add duplicates.
	 */
	UFUNCTION(BlueprintCallable, Category = "Balloon|Collision")
	static bool AddBalloonCollisionPrimitives(const FString& BlueprintPath);
};
