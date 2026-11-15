#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BalloonLevelConfig.h"
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
	/** M1 succeeds at Ready (abstract geometry/catalog only), never Complete/playable. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category="Balloon Level") bool GenerateLevel();
	UFUNCTION(BlueprintCallable, CallInEditor, Category="Balloon Level") void ClearGeneratedLevel();
	UFUNCTION(BlueprintCallable, CallInEditor, Category="Balloon Level") bool RegenerateLevel();
	UFUNCTION(BlueprintPure, Category="Balloon Level") EBalloonLevelGenerationStatus GetGenerationStatus() const { return Status; }
	UFUNCTION(BlueprintPure, Category="Balloon Level") TArray<FString> GetDiagnostics() const { return Diagnostics; }
	const FBalloonLevelPlan& GetPlan() const { return Plan; }
private:
	UPROPERTY(VisibleInstanceOnly, Category="Balloon Level") EBalloonLevelGenerationStatus Status = EBalloonLevelGenerationStatus::NotStarted;
	UPROPERTY(VisibleInstanceOnly, Category="Balloon Level") TArray<FString> Diagnostics;
	FBalloonLevelPlan Plan;
};
