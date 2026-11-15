#include "BalloonLevelGenerator.h"
#include "BalloonAssetCatalog.h"
#include "BalloonBorderPlanner.h"
#include "BalloonPlacementPlanner.h"
#include "BalloonHorizontalRouteValidator.h"
#include "BalloonBorderThreat.h"
#include "DrawDebugHelpers.h"
#include "Components/SplineComponent.h"
#include "Components/SphereComponent.h"

ABalloonLevelGenerator::ABalloonLevelGenerator()
{
	PrimaryActorTick.bCanEverTick = false;
 EventManager = CreateDefaultSubobject<UBalloonLevelEventManager>(TEXT("EventManager"));
	Spline = CreateDefaultSubobject<USplineComponent>(TEXT("ProgressionSpline"));
	SetRootComponent(Spline);
	Spline->ClearSplinePoints();
}

void ABalloonLevelGenerator::ClearGeneratedLevel()
{
 EventManager->ResetEventTimeline();
 BorderProxies.Reset();
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
 if(Plan.LengthCm>0.) EventManager->UpdatePlayerProgress(Progress.CurrentS/Plan.LengthCm,Progress.CurrentS/100.);
}

bool ABalloonLevelGenerator::RegisterBorderProxy(ABalloonBorderThreat* Proxy)
{
 if(!IsValid(Proxy) || !IsValid(Proxy->InfluenceCollision) || Proxy->GetWorld()!=GetWorld()) return false;
 const auto T=Proxy->GetInfluence();
 const auto* Descriptor=Plan.Borders.FindByPredicate([&](const auto& B){return B.InstanceId==T.InstanceId;});
 if(!Descriptor || !Descriptor->Center.Equals(T.Center) || Descriptor->RadiusCm!=T.RadiusCm || Descriptor->Policy!=T.Policy ||
    !Proxy->GetActorLocation().Equals(T.Center) || !Proxy->GetActorScale3D().Equals(FVector::OneVector) ||
    !Proxy->InfluenceCollision->GetComponentLocation().Equals(T.Center) || !Proxy->InfluenceCollision->GetComponentScale().Equals(FVector::OneVector) ||
    Proxy->InfluenceCollision->GetUnscaledSphereRadius()!=T.RadiusCm || BorderProxies.Contains(T.InstanceId)) return false;
 BorderProxies.Add(T.InstanceId,Proxy);return true;
}

bool ABalloonLevelGenerator::TryEventBorderChange(const FBalloonLevelEventDefinition& Event,FString& Reason)
{
 if(Status!=EBalloonLevelGenerationStatus::Ready || !IsValid(RoutePawn) || !LevelConfig || !GetWorld() || Event.RequestsLightingSwap ||
    !FMath::IsFinite(Event.Value) || Event.Action==EBalloonEventAction::Blueprint || uint8(Event.Action)>uint8(EBalloonEventAction::ScaleBorderRadius))
 {Reason=TEXT("Border transaction requires Ready, a live Pawn, and a valid radius action; lighting swaps forbidden");return false;}
 auto Candidate=Plan.Borders;
 bool Matched=false;
 for(auto& B:Candidate)
 {
  if(Event.BorderId>=0 && B.InstanceId!=Event.BorderId)continue;
  if(Event.SegmentId>=0 && B.SegmentId!=Event.SegmentId)continue;
  auto* Entry=BorderProxies.Find(B.InstanceId);
  ABalloonBorderThreat* Proxy=Entry?Entry->Get():nullptr;
  if(!Event.TargetTag.IsNone() && (!IsValid(Proxy) || !Proxy->ActorHasTag(Event.TargetTag)))continue;
  // A registered but destroyed/moved/rescaled proxy invalidates the transaction before any commit.
  if(Entry && (!IsValid(Proxy) || !IsValid(Proxy->InfluenceCollision) || !Proxy->GetActorLocation().Equals(B.Center) || !Proxy->GetActorScale3D().Equals(FVector::OneVector) ||
     Proxy->GetInfluence().RadiusCm!=B.RadiusCm || Proxy->GetInfluence().Policy!=B.Policy ||
     !Proxy->InfluenceCollision->GetComponentLocation().Equals(B.Center) || !Proxy->InfluenceCollision->GetComponentScale().Equals(FVector::OneVector) ||
     Proxy->InfluenceCollision->GetUnscaledSphereRadius()!=B.RadiusCm))
  {Reason=TEXT("Registered border proxy is stale or transformed");return false;}
  const double Radius=Event.Action==EBalloonEventAction::ScaleBorderRadius ? B.RadiusCm*Event.Value :
   Event.Action==EBalloonEventAction::AddBorderRadius ? B.RadiusCm+Event.Value*100. : Event.Value*100.;
  if(!FMath::IsFinite(Radius) || Radius<=0. || Radius>MAX_flt) {Reason=TEXT("Radius must be finite and positive");return false;}
  B.RadiusCm=Radius;
  const FVector Center=BalloonSplinePlanning::PositionAt(Plan,B.DistanceCm);
  const FVector Right=BalloonBorderPlanning::RightAt(Plan,B.DistanceCm);
  double A=0.,Z=0.;B.IntrusionCm=0.f;
  if(BalloonBorderPlanning::CrossSection(B,Center,Right,Plan.BalloonHalfHeightCm,0.,A,Z))
  {
   const double Half=BalloonSplinePlanning::WidthAt(Plan,B.DistanceCm)*.5;
   B.IntrusionCm=FMath::Max(0.,FMath::Min(Z,Half)-FMath::Max(A,-Half));
  }
  Matched=true;
 }
 if(!Matched) {Reason=TEXT("Border selector matched no threats");return false;}
 BalloonHorizontalRoute::FEnvelope Envelope;
 if(!BalloonHorizontalRoute::InspectPawn(*RoutePawn,Envelope,Reason))return false;
 Envelope.Safety=LevelConfig->RouteSafetyMeters*100.;Envelope.Wind=LevelConfig->RouteWindMeters*100.;
 Envelope.MinimumRadius=FMath::Max(Envelope.MinimumRadius,double(LevelConfig->MinimumTurningRadiusMeters)*100.);
 BalloonHorizontalRoute::FSettings Settings;Settings.CellCm=LevelConfig->RouteCellMeters*100.;Settings.MaxAttempts=1;
 const auto Result=BalloonHorizontalRoute::TryBorders(Plan,Candidate,RoutePawn->GetActorLocation(),Envelope,Settings,BalloonHorizontalRoute::WorldSweep(*GetWorld(),RoutePawn));
 if(!Result.Reachable) {Reason=Result.FailureReason;return false;}
 // No BP callbacks until every native representation has committed.
 for(const auto& B:Plan.Borders) if(auto* Entry=BorderProxies.Find(B.InstanceId); Entry && IsValid(Entry->Get())) Entry->Get()->CommitValidatedInfluence(B);
 OnBorderChanged.Broadcast();
 return true;
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
