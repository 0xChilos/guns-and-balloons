#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Curves/RichCurve.h"
#include "BalloonLevelTypes.generated.h"

UENUM(BlueprintType)
enum class EBalloonLevelGenerationStatus : uint8
{
	NotStarted, Planning, Validating, Ready, Realizing, Complete,
	InvalidConfig, InsufficientSpace, Unreachable, Failed
};

UENUM(BlueprintType)
enum class EBalloonLightingPreset : uint8 { Day, Night };

UENUM(BlueprintType)
enum class EBalloonBorderInfluencePolicy : uint8 { Blocked, Damaging, Pushback, VisualOnly };

/** Stationary sphere; projected onto the fixed flight envelope for XY planning. All distances cm. */
USTRUCT(BlueprintType)
struct GUNSANDBALLOONS_API FBalloonBorderInfluence
{
 GENERATED_BODY()
 UPROPERTY(BlueprintReadOnly) int32 InstanceId = 0;
 UPROPERTY(BlueprintReadOnly) int32 SegmentId = 0;
 UPROPERTY(BlueprintReadOnly) int32 Side = 1;
 UPROPERTY(BlueprintReadOnly) float DistanceCm = 0.f;
 UPROPERTY(BlueprintReadOnly) FVector Center = FVector::ZeroVector;
 UPROPERTY(BlueprintReadOnly) float RadiusCm = 0.f;
 UPROPERTY(BlueprintReadOnly) float IntrusionCm = 0.f;
 UPROPERTY(BlueprintReadOnly) EBalloonBorderInfluencePolicy Policy = EBalloonBorderInfluencePolicy::Damaging;
};

/** Exact CSV/JSON contract. Technical placement metadata belongs outside this row. */
USTRUCT(BlueprintType)
struct GUNSANDBALLOONS_API FBalloonLevelAssetRow : public FTableRowBase
{
	GENERATED_BODY()
	/** StaticMesh object path or Blueprint-generated Actor class path (..._C), never a Blueprint asset. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FSoftObjectPath Asset;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName Category;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) bool Collision = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float DensityPerKm2 = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float ScaleMin = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float ScaleMax = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float MinSpacingM = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) bool RandomYaw = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) bool AlignToSlope = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 ClusterSize = 1;
	/** scatter: density/clusters; ring: inward mountains; line: chained borders;
	 * street: 90-degree grid; wind_aligned: common yaw +/-10; flock: bird Actor groups. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName Rule;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FString Notes;
};

struct FBalloonCatalogEntry
{
	FName RowName;
	FString SourceTable;
	FBalloonLevelAssetRow Row;
};

struct FBalloonLevelSegment
{
	int32 SegmentId = 0;
	double StartS = 0.; // cm; [StartS, EndS), final segment includes EndS.
	double EndS = 0.;
	TArray<FVector> Samples;
	FBox BaseBounds = FBox(ForceInit);
	TArray<FString> Diagnostics;
};

/** Descriptors only; no UObject ownership, collision proof or realized content in M1. */
struct FBalloonLevelPlan
{
	int32 Seed = 0;
	FName BiomeId;
	FString CatalogVersion;
	EBalloonLightingPreset LightingPreset = EBalloonLightingPreset::Day;
	TArray<FVector> SplineSamples;
	TArray<double> SampleDistances;
	TArray<double> SampleWidths;
	FTransform Start;
	FTransform Finish;
	double LengthCm = 0.;
	FRichCurve WidthCurve;
	double BaseWidthCm = 0.;
	double MinWidthCm = 0.;
	TArray<FBalloonLevelSegment> Segments;
	TArray<FBalloonCatalogEntry> Catalog;
 double BalloonRadiusCm = 0.;
 double BalloonHalfHeightCm = 0.;
 TArray<FBalloonBorderInfluence> Borders;
 TArray<FVector> LeftBoundary;
 TArray<FVector> RightBoundary;
};
