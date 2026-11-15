#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BalloonLevelConfig.h"
#include "BalloonSplinePlanner.h"
#include "BalloonLevelEventManager.h"
#include "BalloonLevelGenerator.generated.h"

class USplineComponent;
class ABalloonBorderThreat;
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FBalloonBorderChanged);

UCLASS(Blueprintable)
class GUNSANDBALLOONS_API ABalloonLevelGenerator : public AActor
{
	GENERATED_BODY()
public:
	ABalloonLevelGenerator();
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Balloon Level|Events") TObjectPtr<UBalloonLevelEventManager> EventManager;
 UPROPERTY(BlueprintAssignable, Category="Balloon Level|Events") FBalloonBorderChanged OnBorderChanged;
 /** Explicit registration for existing proxies. Automatic realization belongs to M8. */
 UFUNCTION(BlueprintCallable, Category="Balloon Level|Events") bool RegisterBorderProxy(ABalloonBorderThreat* Proxy);
 bool TryEventBorderChange(const FBalloonLevelEventDefinition& Event, FString& Reason);
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Balloon Level") TObjectPtr<UBalloonLevelConfig> LevelConfig;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Balloon Level") TObjectPtr<USplineComponent> Spline;
 /** Selected balloon instance: active shape bounds and authored flight limits are inspected at generation. */
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Balloon Level") TObjectPtr<AActor> RoutePawn;
	/** Ready requires M5 fixed-Z route proof; world realization remains M8. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category="Balloon Level") bool GenerateLevel();
	UFUNCTION(BlueprintCallable, CallInEditor, Category="Balloon Level") void ClearGeneratedLevel();
	UFUNCTION(BlueprintCallable, CallInEditor, Category="Balloon Level") bool RegenerateLevel();
	UFUNCTION(BlueprintPure, Category="Balloon Level") EBalloonLevelGenerationStatus GetGenerationStatus() const { return Status; }
	UFUNCTION(BlueprintPure, Category="Balloon Level") TArray<FString> GetDiagnostics() const { return Diagnostics; }
	/** Stateless full XY projection; for tracked event progression call UpdateProgress instead. */
	UFUNCTION(BlueprintCallable, Category="Balloon Level") float GetNormalizedProgress(const FVector& WorldPosition) const;
	UFUNCTION(BlueprintCallable, Category="Balloon Level") void UpdateProgress(const FVector& WorldPosition, bool Teleported = false);
	UFUNCTION(BlueprintPure, Category="Balloon Level") float GetCurrentProgressCm() const { return Progress.CurrentS; }
	UFUNCTION(BlueprintPure, Category="Balloon Level") float GetMaxReachedProgressCm() const { return Progress.MaxReachedS; }
	UFUNCTION(BlueprintPure, Category="Balloon Level") FTransform GetStartTransform() const { return Plan.Start; }
	UFUNCTION(BlueprintPure, Category="Balloon Level") FTransform GetFinishTransform() const { return Plan.Finish; }
	UFUNCTION(BlueprintPure, Category="Balloon Level") float GetWidthAtDistanceCm(float DistanceCm) const { return BalloonSplinePlanning::WidthAt(Plan, DistanceCm); }
	UFUNCTION(BlueprintPure, Category="Balloon Level") TArray<FBalloonBorderInfluence> GetBorderInfluences() const;
 UFUNCTION(BlueprintCallable, Category="Balloon Level") void DrawBorderDebug(float Duration = 10.f) const;
 const FBalloonLevelPlan& GetPlan() const { return Plan; }
private:
 UPROPERTY(Transient) TMap<int32, TObjectPtr<ABalloonBorderThreat>> BorderProxies;
	UPROPERTY(VisibleInstanceOnly, Category="Balloon Level") EBalloonLevelGenerationStatus Status = EBalloonLevelGenerationStatus::NotStarted;
	UPROPERTY(VisibleInstanceOnly, Category="Balloon Level") TArray<FString> Diagnostics;
	FBalloonLevelPlan Plan;
	FBalloonLevelProgress Progress;
};
