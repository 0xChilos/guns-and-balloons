#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HookPickup.generated.h"

class UStaticMeshComponent;

/**
 * A resource the grappling hook (UHookComponent) can grab and pull back to the player.
 *
 * Placeholder look: an engine basic shape tinted with Color. Floating pickups hang still in the air
 * (like resources tied to balloons) until hooked; the others rest on the ground with physics.
 * While carried the pickup has no collision and no physics; when the hook brings it in it is
 * dropped in front of the player with physics on, so it can be hooked again for testing.
 */
UCLASS(Blueprintable)
class GUNSANDBALLOONS_API AHookPickup : public AActor
{
	GENERATED_BODY()

public:
	AHookPickup();

	virtual void OnConstruction(const FTransform& Transform) override;

	UStaticMeshComponent* GetMesh() const { return Mesh; }

	/** False while the hook is carrying it. */
	UFUNCTION(BlueprintPure, Category = "Hook Pickup")
	bool CanBeHooked() const { return !bCarried; }

	/** The hook grabbed it: no physics, no collision, so it rides on the hook head. */
	void OnHooked();

	/** The hook brought it in: falls (physics) from where it is, carrying Velocity. */
	void OnDelivered(const FVector& Velocity);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hook Pickup")
	TObjectPtr<UStaticMeshComponent> Mesh;

	/** Hangs in the air until hooked (no physics). Off: rests on the ground with physics. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hook Pickup")
	bool bFloating = true;

	/** Tint of the placeholder shape. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hook Pickup")
	FLinearColor Color = FLinearColor(0.9f, 0.55f, 0.1f);

protected:
	virtual void BeginPlay() override;

private:
	void ApplyColor();
	void ApplyPhysics();

	bool bCarried = false;
};
