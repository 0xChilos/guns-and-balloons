#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BalloonLevelTypes.h"
#include "BalloonLevelRealizer.generated.h"
class UHierarchicalInstancedStaticMeshComponent;
/** Game-thread transactional realization. Only explicitly tracked objects are removed. */
UCLASS()
class GUNSANDBALLOONS_API UBalloonLevelRealizer : public UActorComponent
{
 GENERATED_BODY()
public:
 UBalloonLevelRealizer();
 bool Realize(const FBalloonLevelPlan& Plan, TArray<FString>& Diagnostics, UClass* BorderActorClass = nullptr, float ItemDensity = 1.f);
 void Clear();
 virtual void EndPlay(const EEndPlayReason::Type Reason) override;
 virtual void OnComponentDestroyed(bool DestroyingHierarchy) override;
 UPROPERTY(Transient) TArray<TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> Groups;
 TArray<TWeakObjectPtr<AActor>> Actors;
 double LastMilliseconds = 0.;
private:
 // Hard references keep runtime-spawned loot in cooked builds.
 UPROPERTY() TArray<TObjectPtr<UStaticMesh>> LootMeshes;
 UPROPERTY() TArray<TObjectPtr<class USoundBase>> LootSounds;
 UPROPERTY() TObjectPtr<class UNiagaraSystem> LootSparkle;
};
