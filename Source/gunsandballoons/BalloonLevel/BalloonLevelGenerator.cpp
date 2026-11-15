#include "BalloonLevelGenerator.h"
#include "BalloonAssetCatalog.h"
#include "BalloonBorderPlanner.h"
#include "BalloonPlacementPlanner.h"
#include "BalloonHorizontalRouteValidator.h"
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
 BalloonHorizontalRoute::FEnvelope Envelope;
 FString EnvelopeError;
 if(!RoutePawn || !GetWorld() || !BalloonHorizontalRoute::InspectPawn(*RoutePawn,Envelope,EnvelopeError))
 { Diagnostics.Add(RoutePawn?EnvelopeError:TEXT("M5: RoutePawn instance is required for collision/movement inspection")); Status=EBalloonLevelGenerationStatus::Unreachable;return false; }
 Envelope.Safety=LevelConfig->RouteSafetyMeters*100.;Envelope.Wind=LevelConfig->RouteWindMeters*100.;
 Envelope.MinimumRadius=FMath::Max(Envelope.MinimumRadius,double(LevelConfig->MinimumTurningRadiusMeters)*100.);
 Candidate.BalloonRadiusCm=(Envelope.Radius+Envelope.Safety+Envelope.Wind)*UE_DOUBLE_SQRT_2;
 Candidate.BalloonHalfHeightCm=Envelope.HalfHeight+Envelope.Safety;
 BalloonHorizontalRoute::FSettings Settings;Settings.CellCm=LevelConfig->RouteCellMeters*100.;Settings.MaxAttempts=1;
 if(LevelConfig->RouteAttempts<1 || LevelConfig->RouteAttempts>12)
 {Diagnostics.Add(TEXT("M5: RouteAttempts must be [1,12]"));Status=EBalloonLevelGenerationStatus::InvalidConfig;return false;}
 TArray<BalloonPlacementPlanning::FResolvedRow> Rows;
 if(!BalloonPlacementPlanning::Resolve(*LevelConfig,Candidate.Catalog,Rows,Diagnostics))
 {Status=EBalloonLevelGenerationStatus::InvalidConfig;return false;}
 const auto Sweep=BalloonHorizontalRoute::WorldSweep(*GetWorld(),RoutePawn);
 bool Reachable=false;
 for(int32 Attempt=0;Attempt<LevelConfig->RouteAttempts;++Attempt)
 {
  Candidate.PlacementAttempt=Attempt;
  if(!BalloonPlacementPlanning::Build(*LevelConfig,Candidate,Rows,Diagnostics))continue;
  const auto Result=BalloonHorizontalRoute::Validate(Candidate,Envelope,Settings,Sweep);
  Diagnostics.Add(FString::Printf(TEXT("M5 seed=%d placement attempt=%d visited=%d: %s"),Candidate.Seed,Attempt+1,Result.CellsVisited,Result.Reachable?TEXT("fixed-Z route proved"):*Result.FailureReason));
  if(Result.Reachable) {Candidate.RoutePoints=Result.Points;Reachable=true;break;}
 }
 if(!Reachable) {Status=EBalloonLevelGenerationStatus::Unreachable;return false;}
 Plan = MoveTemp(Candidate);
	for (const FVector& Point : Plan.SplineSamples)
	{
		const int32 Index = Spline->GetNumberOfSplinePoints();
		Spline->AddSplinePoint(Point, ESplineCoordinateSpace::World, false);
		Spline->SetSplinePointType(Index, ESplinePointType::Linear, false);
	}
	Spline->SetClosedLoop(false, false);
	Spline->UpdateSpline();
	Diagnostics.Add(TEXT("M5 Ready: fixed-Z horizontal route certified against conservative Pawn/placement bounds and current world; descriptors only, realization remains M8."));
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
 for(int32 I=1;I<Plan.RoutePoints.Num();++I)
  DrawDebugLine(GetWorld(),Plan.RoutePoints[I-1],Plan.RoutePoints[I],FColor::Green,false,Duration,0,3.f);
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
