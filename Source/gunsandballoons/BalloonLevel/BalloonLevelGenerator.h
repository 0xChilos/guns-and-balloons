#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BalloonLevelConfig.h"
#include "BalloonSplinePlanner.h"
#include "BalloonLevelGenerator.generated.h"

class USplineComponent;

UCLASS(Blueprintable)
class GUNSANDBALLOONS_API ABalloonLevelGenerator : public AActor
{
	GENERATED_BODY()
public:
	ABalloonLevelGenerator();
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Balloon Level") TObjectPtr<UBalloonLevelConfig> LevelConfig;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Balloon Level") TObjectPtr<USplineComponent> Spline;
	/** M2 succeeds at Ready (abstract geometry/catalog only), never Complete/playable. */
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
	const FBalloonLevelPlan& GetPlan() const { return Plan; }
private:
	UPROPERTY(VisibleInstanceOnly, Category="Balloon Level") EBalloonLevelGenerationStatus Status = EBalloonLevelGenerationStatus::NotStarted;
	UPROPERTY(VisibleInstanceOnly, Category="Balloon Level") TArray<FString> Diagnostics;
	FBalloonLevelPlan Plan;
	FBalloonLevelProgress Progress;
};
