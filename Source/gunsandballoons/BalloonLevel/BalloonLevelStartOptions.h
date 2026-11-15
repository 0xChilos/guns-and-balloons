#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "BalloonLevelTypes.h"
#include "BalloonLevelStartOptions.generated.h"

/** User-selected level setup. Numeric defaults are prototype tuning, not final balance. */
USTRUCT(BlueprintType)
struct GUNSANDBALLOONS_API FBalloonLevelStartOptions
{
 GENERATED_BODY()
 UPROPERTY(EditAnywhere, BlueprintReadWrite) FName Biome = TEXT("Forest");
 /** Fixed Day or Night for this level, without time progression. */
 UPROPERTY(EditAnywhere, BlueprintReadWrite) EBalloonLightingPreset Cycle = EBalloonLightingPreset::Day;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(Units="m")) float FlightHeight = 100.f;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="1", Units="m")) float LevelWidth = 600.f;
 /** Multipliers: zero disables the category, one keeps the authored density. */
 UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", ClampMax="10")) float EnemyDensity = 1.f;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", ClampMax="10")) float ObstacleDensity = 1.f;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", ClampMax="10")) float FogDensity = .03f;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", Units="m")) float DistanceToNemesis = 250.f;
 bool Validate(TArray<FString>& Errors) const;
};

/** Carries one menu launch across map travel, isolated to this game instance. */
UCLASS()
class GUNSANDBALLOONS_API UBalloonLevelLaunchSubsystem : public UGameInstanceSubsystem
{
 GENERATED_BODY()
public:
 /** Reusable entry point for any menu: validate, carry options across travel, then open the map.
  * Leave Level empty to use MainLevel. True means travel requested, not generation completed. */
 UFUNCTION(BlueprintCallable, Category="Level Start", meta=(AdvancedDisplay="Level"))
 bool StartLevel(const FBalloonLevelStartOptions& InOptions, TSoftObjectPtr<UWorld> Level, FText& OutError);
 UPROPERTY(BlueprintReadOnly, Category="Level Start") FBalloonLevelStartOptions Options;
 UPROPERTY(BlueprintReadOnly, Category="Level Start") bool Pending = false;
 UFUNCTION(BlueprintCallable, Category="Level Start") bool QueueLevel(const FBalloonLevelStartOptions& InOptions);
 bool Consume(FBalloonLevelStartOptions& OutOptions);
};
