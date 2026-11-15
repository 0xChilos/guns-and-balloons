#pragma once
#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "StoveTestObserver.generated.h"
UCLASS(Transient, NotBlueprintable)
class UStoveTestObserver : public UObject
{
	GENERATED_BODY()
public:
	int32 OverheatChanges = 0;
	int32 Ignitions = 0;
	int32 Extinctions = 0;
	UFUNCTION() void Overheat(bool bValue) { ++OverheatChanges; }
	UFUNCTION() void Ignited() { ++Ignitions; }
	UFUNCTION() void Extinguished() { ++Extinctions; }
};
