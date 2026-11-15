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
	bool TryInteract(const FHitResult& Hit, APawn* InstigatorPawn) const;
};
