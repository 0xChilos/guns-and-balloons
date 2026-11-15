#pragma once
#include "GameFramework/Actor.h"
#include "BalloonLevelTypes.h"
#include "BalloonBorderThreat.generated.h"
class USphereComponent;
class UPrimitiveComponent;

/** Stationary generated influence proxy, optionally paired with a rocket-balloon actor. */
UCLASS(Blueprintable)
class GUNSANDBALLOONS_API ABalloonBorderThreat : public AActor
{
 GENERATED_BODY()
public:
 ABalloonBorderThreat();
 /** Sets the rocket's engagement radius and disables drift to match the certified influence. */
 bool BindThreatActor(AActor* Actor);
 /** Initialization only, rejected after BeginPlay. Runtime edits require M5/M6 safety transaction. */
 UFUNCTION(BlueprintCallable, Category="Border") bool InitializeInfluence(const FBalloonBorderInfluence& Descriptor);
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Border") TObjectPtr<USphereComponent> InfluenceCollision;
 UFUNCTION(BlueprintPure, Category="Border") FBalloonBorderInfluence GetInfluence() const { return Influence; }
 UFUNCTION(BlueprintImplementableEvent, Category="Border") void OnInfluenceInitialized(const FBalloonBorderInfluence& Descriptor);
 /** Blueprint owns optional damage/pushback/warnings; never implemented as pursuing AI here. */
 UFUNCTION(BlueprintImplementableEvent, Category="Border") void OnBalloonInfluenceEntered(AActor* Balloon, EBalloonBorderInfluencePolicy Policy);
private:
 friend class ABalloonLevelGenerator;
 void CommitValidatedInfluence(const FBalloonBorderInfluence& Descriptor);
 UPROPERTY(VisibleInstanceOnly, Category="Border") FBalloonBorderInfluence Influence;
 TWeakObjectPtr<AActor> ThreatActor;
 UFUNCTION() void HandleOverlap(UPrimitiveComponent* Component, AActor* Other, UPrimitiveComponent* OtherComponent, int32 BodyIndex, bool FromSweep, const FHitResult& Hit);
};
