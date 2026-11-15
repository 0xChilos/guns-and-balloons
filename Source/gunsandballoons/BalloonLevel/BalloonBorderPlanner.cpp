#include "BalloonBorderPlanner.h"
#include "BalloonSplinePlanner.h"

FVector BalloonBorderPlanning::RightAt(const FBalloonLevelPlan& P, double S)
{
 int32 I = 0, End = P.SampleDistances.Num()-1;
 while(I+1<End) {const int32 Mid=(I+End)/2; if(P.SampleDistances[Mid]<=S) I=Mid; else End=Mid;}
 const FVector T = (P.SplineSamples[I+1]-P.SplineSamples[I]).GetSafeNormal2D();
 return FVector(-T.Y,T.X,0.);
}

double BalloonBorderPlanning::ProjectedRadius(const FBalloonBorderInfluence& T, double Z, double H)
{
 const double Gap = FMath::Max(0., FMath::Abs(T.Center.Z-Z)-H);
 if (Gap > T.RadiusCm) return -1.;
 return FMath::Sqrt(FMath::Max(0., double(T.RadiusCm)*T.RadiusCm-Gap*Gap));
}

bool BalloonBorderPlanning::CrossSection(const FBalloonBorderInfluence& T, const FVector& O, const FVector& R, double H, double BalloonR, double& A, double& B)
{
 if (T.Policy == EBalloonBorderInfluencePolicy::VisualOnly) return false;
 const double Slice = ProjectedRadius(T,O.Z,H);
 if (Slice < 0.) return false;
 const double Radius = Slice+BalloonR;
 const FVector D(T.Center.X-O.X,T.Center.Y-O.Y,0.);
 const double Along = FVector::DotProduct(D,R);
 const double PerpSq = FMath::Max(0.,D.SizeSquared()-Along*Along);
 if (PerpSq > Radius*Radius) return false;
 const double Extent = FMath::Sqrt(FMath::Max(0.,Radius*Radius-PerpSq));
 A=Along-Extent; B=Along+Extent; return true;
}

double BalloonBorderPlanning::UnsafeWidth(const FBalloonLevelPlan& P, double S, double H, double BalloonR)
{
 const FVector O=BalloonSplinePlanning::PositionAt(P,S), R=RightAt(P,S);
 const double Half=BalloonSplinePlanning::WidthAt(P,S)*0.5;
 TArray<FVector2D> Intervals;
 for(const auto& T:P.Borders)
 {
  double A,B;
  if(CrossSection(T,O,R,H,BalloonR,A,B) && B>=-Half && A<=Half)
   Intervals.Emplace(FMath::Max(A,-Half),FMath::Min(B,Half));
 }
 Intervals.Sort([](const FVector2D& A,const FVector2D& B){return A.X<B.X;});
 double Total=0., End=-Half;
 for(const auto& I:Intervals) {Total+=FMath::Max(0.,I.Y-FMath::Max(End,I.X)); End=FMath::Max(End,I.Y);}
 return Total;
}

bool BalloonBorderPlanning::Build(const UBalloonLevelConfig& C, FBalloonLevelPlan& P, TArray<FString>& D)
{
 P.Borders.Reset(); P.LeftBoundary.Reset(); P.RightBoundary.Reset();
 P.BalloonRadiusCm=double(C.BalloonRadiusMeters)*100.; P.BalloonHalfHeightCm=double(C.BalloonHalfHeightMeters)*100.;
 if(!C.Validate(D)) return false;
 if(P.SplineSamples.Num()<2 || P.SampleDistances.Num()!=P.SplineSamples.Num()) {D.Add(TEXT("Borders require M2 geometry")); return false;}
 for(double S:P.SampleDistances)
 {
  const FVector O=BalloonSplinePlanning::PositionAt(P,S), R=RightAt(P,S)*BalloonSplinePlanning::WidthAt(P,S)*0.5;
  P.LeftBoundary.Add(O-R); P.RightBoundary.Add(O+R);
 }
 if(!C.BordersEnabled) return true;
 const double Target=C.BorderExactCount>=0 ? C.BorderExactCount : FMath::RoundToDouble(2.*P.LengthCm/100000.*C.BorderDensityPerKm);
 if(!FMath::IsFinite(Target) || Target>10000.) {D.Add(TEXT("Border count exceeds 10000; reduce density")); return false;}
 const int32 Count=int32(Target);
 if(double(Count)*(P.SampleDistances.Num()+Count)>5000000.)
 {D.Add(TEXT("Border cross-section diagnostic budget exceeds 5000000; reduce count or geometry resolution")); return false;}
 for(int32 I=0;I<Count;++I)
 {
  // Border/v1, stable item stream; geometry/catalog streams are never consumed.
  FRandomStream Random(static_cast<int32>(uint32(C.Seed)^0x424F5231u^(uint32(I)*0x9E3779B9u)));
  FBalloonBorderInfluence T; T.InstanceId=I; T.Side=(I%2==0)?-1:1; T.Policy=C.BorderPolicy;
  T.RadiusCm=Random.FRandRange(C.BorderRadiusMinMeters,C.BorderRadiusMaxMeters)*100.f;
  bool Accepted=false;
  for(int32 Attempt=0;Attempt<128;++Attempt)
  {
   T.DistanceCm=Random.FRand()*P.LengthCm;
   const FVector O=BalloonSplinePlanning::PositionAt(P,T.DistanceCm);
   T.Center=O+RightAt(P,T.DistanceCm)*T.Side*(BalloonSplinePlanning::WidthAt(P,T.DistanceCm)*0.5+T.RadiusCm*(1.-C.BorderIntrusionFraction));
   T.Center.Z+=C.BorderHeightOffsetMeters*100.;
   Accepted=true;
   for(const auto& Other:P.Borders)
   {
    const double Spacing=FMath::Max(double(C.BorderMinSpacingMeters)*100.,C.BorderAllowOverlap?0.:double(T.RadiusCm+Other.RadiusCm));
    if(FVector::DistSquaredXY(T.Center,Other.Center)<Spacing*Spacing) {Accepted=false;break;}
   }
   if(Accepted) break;
  }
  if(!Accepted) {D.Add(FString::Printf(TEXT("Border %d/%d: spacing/overlap impossible after 128 attempts; no partial plan"),I,Count)); P.Borders.Reset(); return false;}
  T.SegmentId=BalloonSplinePlanning::SegmentAt(P,T.DistanceCm);
  double A,B;
  if(CrossSection(T,BalloonSplinePlanning::PositionAt(P,T.DistanceCm),RightAt(P,T.DistanceCm),C.BalloonHalfHeightMeters*100.,0.,A,B))
  {
   const double Half=BalloonSplinePlanning::WidthAt(P,T.DistanceCm)*0.5;
   T.IntrusionCm=FMath::Max(0.,FMath::Min(B,Half)-FMath::Max(A,-Half));
  }
  int32 Overlaps=0;
  for(const auto& Other:P.Borders)
   if(FVector::DistSquaredXY(T.Center,Other.Center)<FMath::Square(double(T.RadiusCm+Other.RadiusCm))) ++Overlaps;
  D.Add(FString::Printf(TEXT("Border %d segment %d side %d radius %.1fcm intrusion %.1fcm overlaps %d"),I,T.SegmentId,T.Side,T.RadiusCm,T.IntrusionCm,Overlaps));
  P.Borders.Add(T);
 }
 TArray<double> CrossSections=P.SampleDistances;
 for(const auto& T:P.Borders) CrossSections.Add(T.DistanceCm);
 for(double S:CrossSections)
  if(UnsafeWidth(P,S,C.BalloonHalfHeightMeters*100.,C.BalloonRadiusMeters*100.) >= BalloonSplinePlanning::WidthAt(P,S)-0.01)
   D.Add(FString::Printf(TEXT("Border sampled horizontal closure at %.1fcm; M5 route proof required"),S));
 return true;
}
