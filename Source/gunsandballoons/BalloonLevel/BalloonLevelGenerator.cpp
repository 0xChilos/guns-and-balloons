#include "BalloonLevelGenerator.h"
#include "BalloonAssetCatalog.h"
#include "BalloonBorderPlanner.h"
#include "DrawDebugHelpers.h"
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
	Progress = FBalloonLevelProgress();
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
	if (!BalloonSplinePlanning::Build(*LevelConfig, Candidate, Diagnostics))
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
	if (!BalloonBorderPlanning::Build(*LevelConfig, Candidate, Diagnostics))
 {
  Status = EBalloonLevelGenerationStatus::InsufficientSpace;
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
	Diagnostics.Add(TEXT("M3 Ready: XY geometry and border descriptors only; horizontal reachability is NOT validated and no gameplay content is realized."));
	Status = EBalloonLevelGenerationStatus::Ready;
	return true;
}

float ABalloonLevelGenerator::GetNormalizedProgress(const FVector& WorldPosition) const
{
 int32 Edge=INDEX_NONE;
 return Plan.LengthCm>0. ? BalloonSplinePlanning::NearestDistance(Plan,WorldPosition,Edge,true)/Plan.LengthCm : 0.f;
}
void ABalloonLevelGenerator::UpdateProgress(const FVector& WorldPosition, bool Teleported)
{
 Progress.Update(Plan,WorldPosition,Teleported);
}

TArray<FBalloonBorderInfluence> ABalloonLevelGenerator::GetBorderInfluences() const { return Plan.Borders; }

void ABalloonLevelGenerator::DrawBorderDebug(float Duration) const
{
 if(!GetWorld() || Plan.SplineSamples.Num()<2 || !FMath::IsFinite(Duration) || Duration<0.f) return;
 for(int32 I=1;I<Plan.LeftBoundary.Num();++I)
 {
  DrawDebugLine(GetWorld(),Plan.LeftBoundary[I-1],Plan.LeftBoundary[I],FColor::Cyan,false,Duration);
  DrawDebugLine(GetWorld(),Plan.RightBoundary[I-1],Plan.RightBoundary[I],FColor::Cyan,false,Duration);
 }
 for(const auto& T:Plan.Borders)
 {
  const double Radius=BalloonBorderPlanning::ProjectedRadius(T,Plan.Start.GetLocation().Z,Plan.BalloonHalfHeightCm);
  const FColor Color=T.Policy==EBalloonBorderInfluencePolicy::VisualOnly?FColor::Silver:FColor::Red;
  DrawDebugSphere(GetWorld(),T.Center,T.RadiusCm,24,Color,false,Duration);
  if(Radius>=0.)
  {
   const FVector Center(T.Center.X,T.Center.Y,Plan.Start.GetLocation().Z);
   DrawDebugCircle(GetWorld(),Center,Radius,48,Color,false,Duration,0,2.f,FVector::ForwardVector,FVector::RightVector,false);
   DrawDebugString(GetWorld(),Center,FString::Printf(TEXT("Border %d side %d R=%.1fm intrusion=%.1fm"),T.InstanceId,T.Side,T.RadiusCm/100.,T.IntrusionCm/100.),nullptr,Color,Duration);
  }
 }
}
