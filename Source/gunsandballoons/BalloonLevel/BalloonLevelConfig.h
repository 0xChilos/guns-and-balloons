#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Curves/CurveFloat.h"
#include "BalloonLevelTypes.h"
#include "BalloonLevelConfig.generated.h"

UCLASS(BlueprintType)
class GUNSANDBALLOONS_API UBalloonLevelConfig : public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Geometry") int32 Seed = 12345;
	/** Prototype authoring values, require tuning; not approved gameplay defaults. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Geometry", meta=(Units="m")) float LengthMeters = 1500.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Geometry", meta=(Units="m")) float BaseWidthMeters = 600.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Geometry", meta=(Units="m")) float SegmentTargetMeters = 250.f;
	/** Absolute world Z datum; StartTransform Z does not add altitude. TBD: tune per level. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Geometry", meta=(Units="m")) float FlightHeightMeters = 100.f;
	/** Translation XY and yaw only; scale, pitch and roll do not alter metric geometry. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Geometry") FTransform StartTransform;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Biome") FName BiomeId = TEXT("Forest");
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Biome") TObjectPtr<UDataTable> SharedAssetTable;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Biome") TObjectPtr<UDataTable> BiomeAssetTable;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Biome") FString CatalogVersion = TEXT("1");
	/** Captured in plan; M1 does not apply environment presets. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Lighting") EBalloonLightingPreset Preset = EBalloonLightingPreset::Day;
	/** M2 prototype values: tune with the actual balloon envelope in M5. Zero heading produces straight geometry. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Geometry") float MaxHeadingDegrees = 25.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Geometry", meta=(Units="m")) float MinimumTurningRadiusMeters = 100.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Geometry", meta=(Units="m")) float GeometrySampleMeters = 10.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Geometry", meta=(Units="m")) float MinWidthMeters = 40.f;
	/** Normalized distance -> width multiplier; empty curve means 1. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Geometry") FRuntimeFloatCurve WidthProfile;
	bool Validate(TArray<FString>& Errors) const;
};

namespace BalloonLevelPlanning
{
	GUNSANDBALLOONS_API bool BuildStraightPlan(const UBalloonLevelConfig& Config, FBalloonLevelPlan& Plan, TArray<FString>& Errors);
}
