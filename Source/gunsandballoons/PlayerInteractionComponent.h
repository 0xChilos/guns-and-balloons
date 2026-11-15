#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PlayerInteractionComponent.generated.h"

/** Camera traces and interface dispatch. Objects retain their own gameplay state and behavior. */
UCLASS(ClassGroup=(Player), meta=(BlueprintSpawnableComponent))
class GUNSANDBALLOONS_API UPlayerInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPlayerInteractionComponent();

	bool Trace(FHitResult& OutHit, const FVector& Start, const FRotator& View,
		float Range, ECollisionChannel Channel, bool bDebugDraw) const;
	AActor* FindAimedActor(const FHitResult& Hit) const;

	/**
	 * The ammo supply aimed at, for RMB: like Trace, but weapon racks' click boxes don't block it (a rack takes
	 * no RMB, and the guns standing by the balloon's ammo box would hide most of it). Null if none.
	 */
	class UAmmoSupplyComponent* TraceSupply(const FVector& Start, const FRotator& View,
		float Range, ECollisionChannel Channel, bool bDebugDraw) const;
	bool TryInteract(const FHitResult& Hit, APawn* InstigatorPawn) const;
};
