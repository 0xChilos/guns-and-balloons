#include "BalloonLevelGenerator.h"
#include "BalloonAssetCatalog.h"
#include "Components/SplineComponent.h"

ABalloonLevelGenerator::ABalloonLevelGenerator()
{
	PrimaryActorTick.bCanEverTick = false;
	Spline = CreateDefaultSubobject<USplineComponent>(TEXT("ProgressionSpline"));
	SetRootComponent(Spline);
	Spline->ClearSplinePoints();
}

void ABalloonLevelGenerator::ClearGeneratedLevel()
{
	Spline->ClearSplinePoints();
	Plan = FBalloonLevelPlan();
	Diagnostics.Reset();
	Status = EBalloonLevelGenerationStatus::NotStarted;
}

bool ABalloonLevelGenerator::RegenerateLevel() { return GenerateLevel(); }

bool ABalloonLevelGenerator::GenerateLevel()
{
	ClearGeneratedLevel();
	Status = EBalloonLevelGenerationStatus::Planning;
	if (!LevelConfig)
	{
		Diagnostics.Add(TEXT("LevelConfig is required"));
		Status = EBalloonLevelGenerationStatus::InvalidConfig;
		return false;
	}
	FBalloonLevelPlan Candidate;
	if (!BalloonLevelPlanning::BuildStraightPlan(*LevelConfig, Candidate, Diagnostics))
	{
		Status = EBalloonLevelGenerationStatus::InvalidConfig;
		return false;
	}
	Status = EBalloonLevelGenerationStatus::Validating;
	if (!BalloonAssetCatalog::Merge(LevelConfig->SharedAssetTable, LevelConfig->BiomeAssetTable, Candidate.Catalog, Diagnostics))
	{
		Status = EBalloonLevelGenerationStatus::InvalidConfig;
		return false;
	}
	Plan = MoveTemp(Candidate);
	for (const FVector& Point : Plan.SplineSamples)
	{
		const int32 Index = Spline->GetNumberOfSplinePoints();
		Spline->AddSplinePoint(Point, ESplineCoordinateSpace::World, false);
		Spline->SetSplinePointType(Index, ESplinePointType::Linear, false);
	}
	Spline->SetClosedLoop(false, false);
	Spline->UpdateSpline();
	Diagnostics.Add(TEXT("M1 Ready: straight geometry/catalog only; horizontal reachability is NOT validated and no gameplay content is realized."));
	Status = EBalloonLevelGenerationStatus::Ready;
	return true;
}
