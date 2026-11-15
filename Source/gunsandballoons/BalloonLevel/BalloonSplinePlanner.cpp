#include "BalloonSplinePlanner.h"

namespace
{
 int32 EdgeAt(const FBalloonLevelPlan& P, double S)
 {
  int32 L = 0, R = P.SampleDistances.Num() - 1;
  while (L + 1 < R) { const int32 M = (L + R) / 2; if (P.SampleDistances[M] <= S) L = M; else R = M; }
  return L;
 }
}

int32 BalloonSplinePlanning::SegmentAt(const FBalloonLevelPlan& P, double S)
{
 if (P.Segments.IsEmpty()) return INDEX_NONE;
 int32 L=0, R=P.Segments.Num();
 while (L+1<R) { const int32 M=(L+R)/2; if(P.Segments[M].StartS<=S) L=M; else R=M; }
 return L;
}

FVector BalloonSplinePlanning::PositionAt(const FBalloonLevelPlan& P, double S)
{
 if (P.SplineSamples.Num() < 2) return FVector::ZeroVector;
 S = FMath::Clamp(S, 0., P.LengthCm);
 const int32 I = EdgeAt(P, S);
 return FMath::Lerp(P.SplineSamples[I], P.SplineSamples[I+1], (S-P.SampleDistances[I])/(P.SampleDistances[I+1]-P.SampleDistances[I]));
}

double BalloonSplinePlanning::WidthAt(const FBalloonLevelPlan& P, double S)
{
 if (P.LengthCm <= 0.) return 0.;
 S = FMath::Clamp(S, 0., P.LengthCm);
 return FMath::Max(P.MinWidthCm, P.BaseWidthCm * (P.WidthCurve.GetNumKeys() ? P.WidthCurve.Eval(S/P.LengthCm) : 1.));
}

bool BalloonSplinePlanning::Build(const UBalloonLevelConfig& C, FBalloonLevelPlan& P, TArray<FString>& Errors, double PawnMinimumRadiusCm)
{
 P = FBalloonLevelPlan();
 if (!C.Validate(Errors)) return false;
 P.Seed=C.Seed; P.BiomeId=C.BiomeId; P.CatalogVersion=C.CatalogVersion; P.LightingPreset=C.Preset;
 P.LengthCm=double(C.LengthMeters)*100.;
 P.BaseWidthCm=double(C.BaseWidthMeters)*100.; P.MinWidthCm=double(C.MinWidthMeters)*100.;
 P.WidthCurve=*C.WidthProfile.GetRichCurveConst();
 // Stable Geometry/v1 stream, independent of catalogs, scenery, lighting and segment partition.
 FRandomStream Random(static_cast<int32>(static_cast<uint32>(C.Seed) ^ 0x47454F31u));
 const double Phase=Random.FRandRange(-PI, PI);
 const double Cycles=Random.FRandRange(0.75f, 1.5f);
 const double Omega=2.*PI*Cycles/P.LengthCm;
 const double RadiusCm = FMath::Max(double(C.MinimumTurningRadiusMeters)*100., PawnMinimumRadiusCm);
 if (!FMath::IsFinite(PawnMinimumRadiusCm) || PawnMinimumRadiusCm < 0.)
 { Errors.Add(TEXT("Invalid pawn minimum turning radius")); P={}; return false; }
 // Reserve curvature for the prover's quintic corner smoothing. The margin is
 // prototype tuning, not a substitute for the continuous movement/collision proof.
 // Heading derivative is bounded by A*Omega. Headings stay within +/-45 degrees,
 // so forward projection is strictly positive: crossings/loops are impossible by construction.
 const double A=FMath::Min(FMath::DegreesToRadians(double(C.MaxHeadingDegrees))/2., 1./(Omega*RadiusCm*C.RouteCurvatureMargin));
 const double Step=FMath::Min(double(C.GeometrySampleMeters)*100., double(C.MinimumTurningRadiusMeters)*5.);
 if (FMath::CeilToDouble(P.LengthCm/Step)>100000.) { Errors.Add(TEXT("Turn-radius sampling exceeds 100000 samples")); P={}; return false; }
 const int32 Count=FMath::Max(1, int32(FMath::CeilToDouble(P.LengthCm/Step)));
 const double DS=P.LengthCm/Count;
 const double Yaw=FMath::DegreesToRadians(C.StartTransform.Rotator().Yaw);
 FVector Point(C.StartTransform.GetLocation().X,C.StartTransform.GetLocation().Y,double(C.FlightHeightMeters)*100.);
 P.SplineSamples.Add(Point); P.SampleDistances.Add(0.);
 for (int32 I=1; I<=Count; ++I)
 {
  const double S=(I-1)*DS;
  // Smooth heading field, starting at the authored yaw. Polyline is the canonical metric spline.
  const double Heading=Yaw+A*(FMath::Sin(Omega*S+Phase)-FMath::Sin(Phase));
  Point+=FVector(FMath::Cos(Heading),FMath::Sin(Heading),0.)*DS;
  P.SplineSamples.Add(Point); P.SampleDistances.Add(I*DS);
 }
 const FRichCurve* Curve=&P.WidthCurve;
 // Global conservative Bezier control-hull bound, including weighted tangents and extrapolation.
 // Broader than a per-segment bound, but never misses a narrow authored width peak.
 double WidthUpper=1.;
 const auto& Keys=Curve->GetConstRefOfKeys();
 for (int32 K=0; K<Keys.Num(); ++K)
 {
  const auto& Key=Keys[K];
  const double Span=Keys.Num()>1 ? Keys.Last().Time-Keys[0].Time : 0.;
  WidthUpper=FMath::Max(WidthUpper,FMath::Abs(double(Key.Value))+
   (FMath::Abs(double(Key.ArriveTangent))+FMath::Abs(double(Key.LeaveTangent)))*(Span+1.)+
   FMath::Abs(double(Key.ArriveTangentWeight))+FMath::Abs(double(Key.LeaveTangentWeight)));
 }
 if(Keys.Num()>1 && Keys.Last().Time>Keys[0].Time)
 {
  const double Span=double(Keys.Last().Time)-Keys[0].Time;
  const double CyclesOutside=FMath::CeilToDouble((1.+FMath::Abs(double(Keys[0].Time))+FMath::Abs(double(Keys.Last().Time)))/Span);
  WidthUpper+=CyclesOutside*FMath::Abs(double(Keys.Last().Value)-Keys[0].Value);
 }
 const double BoundsWidth=FMath::Max(P.MinWidthCm,P.BaseWidthCm*WidthUpper);
 if (!FMath::IsFinite(BoundsWidth)) { Errors.Add(TEXT("WidthProfile bounds overflow")); P={}; return false; }
 for (double S : P.SampleDistances)
 {
  const double Multiplier=Curve->GetNumKeys() ? Curve->Eval(S/P.LengthCm) : 1.;
  if (!FMath::IsFinite(Multiplier)) { Errors.Add(TEXT("WidthProfile must evaluate to finite values")); P={}; return false; }
  P.SampleWidths.Add(FMath::Max(double(C.MinWidthMeters)*100.,double(C.BaseWidthMeters)*100.*Multiplier));
 }
 auto Frame=[&](int32 I) { return FTransform((P.SplineSamples[I+1]-P.SplineSamples[I]).Rotation(), P.SplineSamples[I]); };
 P.Start=Frame(0); P.Start.SetRotation(FRotator(0.,C.StartTransform.Rotator().Yaw,0.).Quaternion());
 P.Finish=Frame(Count-1); P.Finish.SetLocation(P.SplineSamples.Last());
 const double Target=double(C.SegmentTargetMeters)*100.;
 const int32 Segments=int32(FMath::CeilToDouble(P.LengthCm/Target));
 for (int32 I=0; I<Segments; ++I)
 {
  FBalloonLevelSegment Segment; Segment.SegmentId=I; Segment.StartS=I*Target; Segment.EndS=FMath::Min((I+1)*Target,P.LengthCm);
  Segment.Samples.Add(PositionAt(P,Segment.StartS));
  double MaxWidth=BoundsWidth;
  for (int32 J=EdgeAt(P,Segment.StartS)+1; J<P.SampleDistances.Num() && P.SampleDistances[J]<Segment.EndS; ++J)
  { Segment.Samples.Add(P.SplineSamples[J]); MaxWidth=FMath::Max(MaxWidth,P.SampleWidths[J]); }
  Segment.Samples.Add(PositionAt(P,Segment.EndS));
  for (const FVector& V : Segment.Samples) Segment.BaseBounds+=V;
  // Conservative XY bounds include the entire variable-width sampled ribbon, including bends.
  Segment.BaseBounds=Segment.BaseBounds.ExpandBy(FVector(MaxWidth/2.,MaxWidth/2.,0.));
  P.Segments.Add(MoveTemp(Segment));
 }
 return true;
}

double BalloonSplinePlanning::NearestDistance(const FBalloonLevelPlan& P, const FVector& Position, int32& Edge, bool FullSearch)
{
 if (P.SplineSamples.Num()<2 || !FMath::IsFinite(Position.X) || !FMath::IsFinite(Position.Y)) return 0.;
 int32 First=0, Last=P.SplineSamples.Num()-2;
 if (!FullSearch && Edge!=INDEX_NONE)
 {
  // Search previous logical segment and neighbors, rather than jumping across nearby bends.
  const double Previous=P.SampleDistances[FMath::Clamp(Edge,0,Last)];
  const int32 Segment=SegmentAt(P,Previous);
  First=EdgeAt(P,P.Segments[FMath::Max(0,Segment-1)].StartS);
  Last=EdgeAt(P,P.Segments[FMath::Min(P.Segments.Num()-1,Segment+1)].EndS);
 }
 double Best=TNumericLimits<double>::Max(), Result=0.;
 const FVector2D Q(Position.X,Position.Y);
 for (int32 I=First; I<=Last; ++I)
 {
  const FVector2D A(P.SplineSamples[I]), D=FVector2D(P.SplineSamples[I+1])-A;
  const double T=FMath::Clamp(FVector2D::DotProduct(Q-A,D)/D.SizeSquared(),0.,1.);
  const double Distance=(Q-A-D*T).SizeSquared();
  if (Distance<Best) { Best=Distance; Result=FMath::Lerp(P.SampleDistances[I],P.SampleDistances[I+1],T); Edge=I; }
 }
 return Result;
}

void FBalloonLevelProgress::Update(const FBalloonLevelPlan& P, const FVector& Position, bool Teleported)
{
 CurrentS=BalloonSplinePlanning::NearestDistance(P,Position,Edge,Teleported || Edge==INDEX_NONE);
 MaxReachedS=FMath::Max(MaxReachedS,CurrentS);
}
