#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "HealthTestObserver.generated.h"

class UHealthComponent;

/** Reflected receiver needed to exercise the Blueprint multicast delegates. */
UCLASS(Transient, NotBlueprintable)
class UHealthTestObserver : public UObject
{
	GENERATED_BODY()
public:
	int32 Changes = 0;
	int32 Depletions = 0;
	int32 Restorations = 0;
	float LastDelta = 0.f;
	float LastHealth = 0.f;
	UHealthComponent* LastSource = nullptr;
	UFUNCTION()
	void Changed(UHealthComponent* Source, float Value, float Delta)
	{ ++Changes; LastSource = Source; LastHealth = Value; LastDelta = Delta; }
	UFUNCTION()
	void Depleted(UHealthComponent* Source) { ++Depletions; }
	UFUNCTION()
	void Restored(UHealthComponent* Source) { ++Restorations; }
};
