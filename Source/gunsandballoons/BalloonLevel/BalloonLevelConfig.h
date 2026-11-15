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
	/** Border settings are prototypes requiring gameplay tuning. Independent from interior enemies (M4). */
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Borders") bool BordersEnabled = false;
 /** -1 uses density per combined border kilometer; 0 explicitly requests none. */
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Borders", meta=(ClampMin="-1")) int32 BorderExactCount = -1;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Borders") float BorderDensityPerKm = 8.f;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Borders", meta=(Units="m")) float BorderRadiusMinMeters = 12.f;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Borders", meta=(Units="m")) float BorderRadiusMaxMeters = 35.f;
 /** Center is placed radius*(1-fraction) outside its local boundary. */
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Borders") float BorderIntrusionFraction = 0.5f;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Borders", meta=(Units="m")) float BorderMinSpacingMeters = 0.f;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Borders") bool BorderAllowOverlap = true;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Borders") EBalloonBorderInfluencePolicy BorderPolicy = EBalloonBorderInfluencePolicy::Damaging;
 /** Threat center offset from fixed flight Z; never introduces a maneuver axis. */
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Borders", meta=(Units="m")) float BorderHeightOffsetMeters = 0.f;
 /** Conservative envelope authoring inputs; TBD tune to actual Pawn collider in M5. */
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Borders", meta=(Units="m")) float BalloonRadiusMeters = 0.f;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Borders", meta=(Units="m")) float BalloonHalfHeightMeters = 0.f;
 /** M4 technical annotations by stable row name. Defaults are preview values requiring tuning. */
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Placement") TMap<FName, FBalloonPlacementMetadata> PlacementMetadata;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Placement") bool SceneryEnabled = true;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Placement") bool ObstaclesEnabled = false;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Placement") bool EnemiesEnabled = false;
 /** -1 retains row density; override replaces total budget, allocated by relative row density. */
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Placement") int32 ObstacleExactCount = -1;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Placement") int32 EnemyExactCount = -1;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Placement", meta=(Units="m")) float ClusterRadiusMeters = 30.f;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Placement", meta=(Units="m")) float StreetCellMeters = 100.f;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Placement", meta=(Units="m")) float StreetRoadMeters = 20.f;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Placement", meta=(Units="m")) float StreetLotDepthMeters = 30.f;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Placement", meta=(Units="m")) float StreetLampIntervalMeters = 20.f;
 /** Ground sampling fallback is a flat plane; unrelated to fixed flight height. */
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Placement", meta=(Units="m")) float PlacementGroundZMeters = 0.f;
 bool Validate(TArray<FString>& Errors) const;
};

namespace BalloonLevelPlanning
{
	GUNSANDBALLOONS_API bool BuildStraightPlan(const UBalloonLevelConfig& Config, FBalloonLevelPlan& Plan, TArray<FString>& Errors);
}
