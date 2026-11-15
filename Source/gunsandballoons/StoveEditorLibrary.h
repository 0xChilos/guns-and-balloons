#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "StoveEditorLibrary.generated.h"

UCLASS()
class GUNSANDBALLOONS_API UStoveEditorLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	/** Editor-only: append stove controls to the existing component-space pose and add door query body. */
	UFUNCTION(BlueprintCallable, Category="Stove|Editor")
	static bool ConfigureAnimationAndDoorBody();
};
