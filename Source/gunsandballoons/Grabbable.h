#pragma once

#include "CoreMinimal.h"
#include "Engine/HitResult.h"
#include "UObject/Interface.h"
#include "Grabbable.generated.h"

class APawn;

UINTERFACE(MinimalAPI, Blueprintable)
class UGrabbable : public UInterface
{
	GENERATED_BODY()
};

/** Implement in C++ through _Implementation overrides or in Blueprint. */
class GUNSANDBALLOONS_API IGrabbable
{
	GENERATED_BODY()

public:
	/** False when the target has ended the grab itself (for example, by slipping). */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction|Grab")
	bool IsGrabbed() const;
	virtual bool IsGrabbed_Implementation() const { return false; }

	/** Filter the traced hit and check availability for this pawn. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction|Grab")
	bool CanGrab(APawn* Pawn, const FHitResult& Hit) const;

	/** Begin holding the object at the traced hit. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction|Grab")
	void BeginGrab(APawn* Pawn, const FHitResult& Hit);

	/** Apply a world-space displacement in the camera plane, in centimeters. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction|Grab")
	void UpdateGrab(APawn* Pawn, FVector WorldDragDelta);

	/** Release the object, including when the grab is interrupted. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction|Grab")
	void EndGrab(APawn* Pawn);

	/** World-space grab location used to check the break distance. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction|Grab")
	FVector GetGrabLocation() const;
};
