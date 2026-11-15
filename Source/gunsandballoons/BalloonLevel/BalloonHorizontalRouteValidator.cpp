#include "BalloonHorizontalRouteValidator.h"
#include "BalloonSplinePlanner.h"
#include "BalloonBorderPlanner.h"
#include "Components/ShapeComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "../BalloonFlightComponent.h"
#include "Algo/Reverse.h"
#include "UObject/UnrealType.h"

namespace BalloonHorizontalRoute
{
bool InspectPawn(const AActor& Pawn, FEnvelope& E, FString& Error)
{
 E=FEnvelope();
 TInlineComponentArray<UShapeComponent*> Shapes(&Pawn);
 int32 Count=0;
 for(const UShapeComponent* Shape:Shapes)
 {
  if(!Shape->IsQueryCollisionEnabled()) continue;
  const FBox B=Shape->CalcBounds(Shape->GetComponentTransform()).GetBox();
  for(int32 I=0;I<8;++I)
  {
   const FVector V=FVector(I&1?B.Max.X:B.Min.X,I&2?B.Max.Y:B.Min.Y,I&4?B.Max.Z:B.Min.Z)-Pawn.GetActorLocation();
   E.Radius=FMath::Max(E.Radius,V.Size2D()); E.HalfHeight=FMath::Max(E.HalfHeight,FMath::Abs(V.Z));
  }
  ++Count;
 }
 const UBalloonFlightComponent* Flight=Pawn.FindComponentByClass<UBalloonFlightComponent>();
 if(!Flight) {Error=TEXT("Route: selected Pawn has no BalloonFlightComponent");return false;}
 // Authored limits are protected reflected properties; inspect without modifying movement.
 auto Read=[&](const TCHAR* Name)->double
 {
  const FFloatProperty* Property=FindFProperty<FFloatProperty>(Flight->GetClass(),Name);
  return Property?Property->GetPropertyValue_InContainer(Flight):0.;
 };
 E.Speed=Read(TEXT("MaxSpeed"));E.MaxYawRate=FMath::DegreesToRadians(Read(TEXT("MaxTurnRate")));
 E.YawAcceleration=FMath::DegreesToRadians(FMath::Min(Read(TEXT("TurnAcceleration")),Read(TEXT("TurnDeceleration"))));
 E.SpeedAcceleration=FMath::Max(Read(TEXT("Acceleration")),Read(TEXT("Deceleration")));
 if(!Count || E.Radius<=0. || E.HalfHeight<=0. || E.Speed<=0. || E.MaxYawRate<=0. || E.YawAcceleration<=0. ||
  !FMath::IsFinite(E.Radius+E.HalfHeight+E.Speed+E.MaxYawRate+E.YawAcceleration))
 { Error=TEXT("Route: selected Pawn needs active query shapes and valid BalloonFlightComponent movement limits");return false; }
 if(!FMath::IsNearlyZero(Pawn.GetActorRotation().Pitch) || !FMath::IsNearlyZero(Pawn.GetActorRotation().Roll))
 { Error=TEXT("Route: tilted Pawn envelope requires an explicit passive-motion contract");return false; }
 E.MinimumRadius=E.Speed/E.MaxYawRate;
 E.InitialDirection=Pawn.GetActorForwardVector();
 E.InitialYawRate=FMath::DegreesToRadians(double(Flight->GetCurrentTurnRate()));
 return true;
}
FSweep WorldSweep(UWorld& World,const AActor* Ignore)
{
 return [&World,Ignore](const FVector& A,const FVector& B,const FVector& Extent)
 {
  FCollisionQueryParams Q(SCENE_QUERY_STAT(BalloonRoute),false,Ignore);
  if(Ignore) { TArray<AActor*> Riders; Ignore->GetAttachedActors(Riders,true,true); Q.AddIgnoredActors(Riders); }
  const FCollisionShape Shape=FCollisionShape::MakeBox(Extent);
  const FCollisionObjectQueryParams Objects(FCollisionObjectQueryParams::AllObjects);
  if(World.OverlapAnyTestByObjectType(A,FQuat::Identity,Objects,Shape,Q)) return true;
  FHitResult Hit;
  return World.SweepSingleByObjectType(Hit,A,B,FQuat::Identity,Objects,Shape,Q);
 };
}
namespace
{
 bool Intersects(const FBox& Box,const FVector& A,const FVector& B)
 {
  double Lo=0.,Hi=1.;const FVector D=B-A;
  for(int32 I=0;I<3;++I)
  {
   if(FMath::Abs(D[I])<1.e-9) { if(A[I]<Box.Min[I] || A[I]>Box.Max[I]) return false; }
   else { double U=(Box.Min[I]-A[I])/D[I],V=(Box.Max[I]-A[I])/D[I]; if(U>V) Swap(U,V);Lo=FMath::Max(Lo,U);Hi=FMath::Min(Hi,V);if(Lo>Hi)return false; }
  }
  return true;
 }
 struct FNode { int32 Cell=0,Parent=INDEX_NONE; FVector Direction=FVector::ZeroVector; double G=0.,F=0.; };
}
FResult Validate(const FBalloonLevelPlan& P,const FEnvelope& E,const FSettings& S,const FSweep& Sweep,const FVector* Current)
{
 FResult R;R.FixedZ=P.Start.GetLocation().Z;
 auto Fail=[&](const TCHAR* Reason) { R.Reachable=false;R.Points.Reset();R.FailureReason=Reason;return R; };
 const double Values[]={E.Radius,E.HalfHeight,E.Safety,E.Wind,E.Speed,E.MaxYawRate,E.YawAcceleration,E.MinimumRadius,E.SpeedAcceleration,S.CellCm};
 for(double V:Values) if(!FMath::IsFinite(V) || V<0.) return Fail(TEXT("Route: invalid finite nonnegative envelope/settings"));
 if(E.Radius<=0. || E.HalfHeight<=0. || E.Speed<=0. || E.MaxYawRate<=0. || E.YawAcceleration<=0. || S.CellCm<=0. || S.MaxStates<1 || S.MaxStates>100000 || S.MaxAttempts<1 || S.MaxAttempts>12 || !Sweep)
  return Fail(TEXT("Route: missing envelope, movement limits, sweep or bounded settings"));
 if(E.InitialDirection.ContainsNaN() || FMath::Abs(E.InitialDirection.Z)>.001 || !FMath::IsNearlyEqual(E.InitialDirection.Size(),1.,.001) || !FMath::IsFinite(E.InitialYawRate))
  return Fail(TEXT("Route: invalid initial heading/yaw rate"));
 if(P.SplineSamples.Num()<2 || P.SampleDistances.Num()!=P.SplineSamples.Num() || P.LengthCm<=0.) return Fail(TEXT("Route: missing canonical geometry"));
 for(int32 I=0;I<P.SplineSamples.Num();++I)
  if(P.SplineSamples[I].ContainsNaN() || FMath::Abs(P.SplineSamples[I].Z-R.FixedZ)>.01 || !FMath::IsFinite(P.SampleDistances[I]) || (I>0 && P.SampleDistances[I]<=P.SampleDistances[I-1]))
   return Fail(TEXT("Route: canonical lattice must be strictly ordered and fixed Z; ascent/descent rejected"));
 const FVector Start=Current?*Current:P.Start.GetLocation(),Finish=P.Finish.GetLocation();
 if(Start.ContainsNaN() || FMath::Abs(Start.Z-R.FixedZ)>.01 || FMath::Abs(Finish.Z-R.FixedZ)>.01)
  return Fail(TEXT("Route: ascent/descent is forbidden; current and finish must share fixed flight Z"));
 for(const auto& O:P.Placements) if(O.Collision && O.Representation!=EBalloonPlacementRepresentation::HISM &&
  (!O.CollisionBounds.IsValid || O.CollisionBounds.Min.ContainsNaN() || O.CollisionBounds.Max.ContainsNaN()))
  return Fail(TEXT("Route: colliding placement lacks certified 3D bounds (Blueprint actors require collision resolver)"));
 // The box circumscribes the yaw-invariant cylindrical envelope. Use its diagonal at corridor/border checks.
 const double H=E.Radius+E.Safety+E.Wind,BoundaryRadius=H*UE_DOUBLE_SQRT_2;
 const FVector Extent(H,H,E.HalfHeight+E.Safety);
 const double Radius=FMath::Max(E.MinimumRadius,E.Speed/E.MaxYawRate);
 float CurveMin=1.f,CurveMax=1.f;
 if(P.WidthCurve.GetNumKeys()) P.WidthCurve.GetValueRange(CurveMin,CurveMax);
 // Global analytic curve minimum prevents missing a narrow cubic-width dip between samples.
 const double SafeWidth=FMath::Max(P.MinWidthCm,P.BaseWidthCm*CurveMin);
 if(!FMath::IsFinite(SafeWidth) || SafeWidth<=0.)return Fail(TEXT("Route: invalid corridor width"));
 int32 Operations=0;
 bool Budget=false;
 auto Clear=[&](const FVector& A,const FVector& B,double Extra=0.)->bool
 {
  if(++Operations>500000) {Budget=true;return false;}
  const FVector Inflated=Extent+FVector(Extra,Extra,0.);
  if(Sweep(A,B,Inflated)) return false;
  for(const auto& O:P.Placements)
   if(O.Collision && O.Representation!=EBalloonPlacementRepresentation::HISM && Intersects(O.CollisionBounds.ExpandBy(Inflated),A,B)) return false;
  for(const auto& T:P.Borders)
  {
   if(T.Policy==EBalloonBorderInfluencePolicy::VisualOnly) continue;
   if(!FMath::IsFinite(T.RadiusCm) || T.RadiusCm<0. || T.Center.ContainsNaN()) return false;
   const double Projected=BalloonBorderPlanning::ProjectedRadius(T,R.FixedZ,Extent.Z);
   if(Projected>=0. && FMath::PointDistToSegment(FVector(T.Center.X,T.Center.Y,R.FixedZ),A,B)<=Projected+BoundaryRadius+Extra*UE_DOUBLE_SQRT_2) return false;
  }
  const double Length=FVector::Dist(A,B);
  if(Length/FMath::Min(100.,S.CellCm*.1)>100000.) {Budget=true;return false;}
  const int32 Steps=FMath::Max(1,FMath::CeilToInt(Length/FMath::Min(100.,S.CellCm*.1)));
  if(Steps>100000) {Budget=true;return false;}
  // Lipschitz guard between corridor samples; widths are checked at canonical breakpoints too below.
  for(int32 I=0;I<=Steps;++I)
  {
   const FVector V=FMath::Lerp(A,B,double(I)/Steps);int32 Edge=INDEX_NONE;
   const double Progress=BalloonSplinePlanning::NearestDistance(P,V,Edge,true);
   if(FVector::Dist2D(V,BalloonSplinePlanning::PositionAt(P,Progress))+BoundaryRadius+Extra*UE_DOUBLE_SQRT_2+Length/Steps > SafeWidth*.5) return false;
  }
  return true;
 };
 if(!Clear(Start,Start) || !Clear(Finish,Finish)) return Fail(TEXT("Route: start/current or finish reservation blocked"));
 if(FVector::DistSquared(Start,Finish)<.0001) {R.Reachable=true;R.Points={Finish};R.AttemptCount=1;return R;}
 auto Prove=[&](const TArray<FVector>& Input)->bool
 {
  R.MaximumCurvature=0.; TArray<FVector> Q; Q.Add(Input[0]);
  // Quintic Bezier corners have collinear, equally spaced end controls: C2 joins
  // to the straight edges (zero endpoint curvature). Bernstein derivative hulls
  // bound curvature and yaw acceleration over the ENTIRE curve, not just samples.
  TFunction<bool(const TArray<FVector>&,int32)> Curve;
  Curve=[&](const TArray<FVector>& B,int32 Depth)->bool
  {
   if(++Operations>500000) {Budget=true;return false;}
   TArray<FVector> V,A,J;
   for(int32 I=0;I<5;++I)V.Add(5.*(B[I+1]-B[I]));
   for(int32 I=0;I<4;++I)A.Add(4.*(V[I+1]-V[I]));
   for(int32 I=0;I<3;++I)J.Add(3.*(A[I+1]-A[I]));
   const FVector Axis=(B.Last()-B[0]).GetSafeNormal();
   double MinV=DBL_MAX,MaxA=0.,Cross=0.,CrossJ=0.;
   for(const FVector& X:V)MinV=FMath::Min(MinV,FVector::DotProduct(X,Axis));
   for(const FVector& X:A)MaxA=FMath::Max(MaxA,X.Size());
   for(const FVector& X:V)for(const FVector& Y:A)Cross=FMath::Max(Cross,FVector::CrossProduct(X,Y).Size());
   for(const FVector& X:V)for(const FVector& Y:J)CrossJ=FMath::Max(CrossJ,FVector::CrossProduct(X,Y).Size());
   const double K=MinV>1.e-9?Cross/FMath::Pow(MinV,3.):DBL_MAX;
   const double DK=MinV>1.e-9?CrossJ/FMath::Pow(MinV,4.)+3.*Cross*MaxA/FMath::Pow(MinV,5.):DBL_MAX;
   double Deviation=0.;for(const FVector& X:B)Deviation=FMath::Max(Deviation,double(FMath::PointDistToSegment(X,B[0],B.Last())));
   if(K<=1./Radius && DK*E.Speed*E.Speed+K*E.SpeedAcceleration<=E.YawAcceleration && Deviation<=5.)
   {
    if(!Clear(B[0],B.Last(),Deviation))return false;
    R.MaximumCurvature=FMath::Max(R.MaximumCurvature,K);Q.Add(B.Last());return true;
   }
   if(Depth>=10 || Budget)return false;
   TArray<FVector> Left,Right,Work=B;Left.Add(Work[0]);Right.Add(Work.Last());
   for(int32 N=5;N>0;--N) {for(int32 I=0;I<N;++I)Work[I]=.5*(Work[I]+Work[I+1]);Left.Add(Work[0]);Right.Add(Work[N-1]);}
   Algo::Reverse(Right);return Curve(Left,Depth+1)&&Curve(Right,Depth+1);
  };
  auto Line=[&](const FVector& B) {if(!Clear(Q.Last(),B))return false;Q.Add(B);return true;};
  // Initial heading and yaw rate are part of the proof, rather than an instantaneous turn.
  // Runtime callers supply their current heading/rate through the envelope below.
  const FVector Initial=(Input[1]-Input[0]).GetSafeNormal();
  const FVector Heading=Current?E.InitialDirection:P.Start.GetRotation().GetForwardVector();
  if(FVector::DotProduct(Initial,Heading)<1.-1.e-6 || FMath::Abs(E.InitialYawRate)>1.e-6)return false;
  for(int32 I=1;I<Input.Num()-1;++I)
  {
   const FVector B=Input[I],A=FMath::Lerp(B,Input[I-1],.45),C=FMath::Lerp(B,Input[I+1],.45);
   const FVector U=(B-A)/3.,V=(C-B)/3.;
   if(!Line(A) || !Curve({A,A+U,A+2.*U,C-2.*V,C-V,C},0))return false;
  }
  if(!Line(Input.Last()))return false;
  R.Points=MoveTemp(Q);R.MinimumClearanceCm=E.Safety+E.Wind;return true;
 };
 // Direct route is a useful independent proof and avoids discretization failure on straight levels.
 if(Prove({Start,Finish})) {R.Reachable=true;R.AttemptCount=1;return R;}
 int32 StartEdge=INDEX_NONE;const double BeginS=BalloonSplinePlanning::NearestDistance(P,Start,StartEdge,true);
 double MaxWidth=0.;for(double W:P.SampleWidths)MaxWidth=FMath::Max(MaxWidth,W);
 if(MaxWidth<=0.) return Fail(TEXT("Route: missing sampled widths"));
 for(int32 Attempt=0;Attempt<S.MaxAttempts && !Budget;++Attempt)
 {
  R.AttemptCount=Attempt+1;
  // Stable resolution retries, no density relaxation and no global RNG.
  const double Cell=S.CellCm/(1.+.25*Attempt);
  const double RowCount=FMath::CeilToDouble((P.LengthCm-BeginS)/Cell)+1.;
  const double ColCount=2.*FMath::CeilToDouble(MaxWidth*.5/Cell)+1.;
  if(RowCount*ColCount*3.>S.MaxStates || RowCount<2.) return Fail(TEXT("Route: lattice allocation limit; increase cell size or state budget"));
  const int32 Rows=int32(RowCount),Cols=int32(ColCount),Mid=Cols/2;
  auto Point=[&](int32 CellId)
  {
   const int32 Row=CellId/Cols,Col=CellId%Cols;
   if(Row==0)return Start;
   if(Row==Rows-1)return Finish;
   if(Row==1 && Col==Mid)
    return Start+(Current?E.InitialDirection:P.Start.GetRotation().GetForwardVector())*((P.LengthCm-BeginS)/(Rows-1));
   const double Progress=FMath::Lerp(BeginS,P.LengthCm,double(Row)/(Rows-1));
   return BalloonSplinePlanning::PositionAt(P,Progress)+BalloonBorderPlanning::RightAt(P,Progress)*((Col-Mid)*Cell);
  };
  TArray<FNode> Nodes;TArray<int32> Open;TMap<int64,double> Best;
  FNode Root;Root.Cell=Mid;Root.F=FVector::Dist(Start,Finish);Nodes.Add(Root);Open.Add(0);
  while(Open.Num() && Nodes.Num()<S.MaxStates && !Budget)
  {
   int32 Pick=0;for(int32 I=1;I<Open.Num();++I)if(Nodes[Open[I]].F<Nodes[Open[Pick]].F)Pick=I;
   const int32 Id=Open[Pick];Open.RemoveAtSwap(Pick,EAllowShrinking::No);const FNode N=Nodes[Id];++R.CellsVisited;
   const int32 Row=N.Cell/Cols,Col=N.Cell%Cols;
   if(Row==Rows-1)
   {
    TArray<FVector> Path;for(int32 I=Id;I!=INDEX_NONE;I=Nodes[I].Parent)Path.Add(Point(Nodes[I].Cell));
    Algo::Reverse(Path);
    if(Prove(Path)) {R.Reachable=true;return R;}
    continue;
   }
   for(int32 Delta=-1;Delta<=1;++Delta)
   {
    // Reserve the first edge for the known heading; no instantaneous initial yaw.
    if(Row==0 && Delta!=0)continue;
    const int32 NextCol=Col+Delta;if(NextCol<0 || NextCol>=Cols)continue;
    if(Row+1==Rows-1 && NextCol!=Mid)continue;
    const int32 Next=(Row+1)*Cols+NextCol;const FVector A=Point(N.Cell),B=Point(Next),D=(B-A).GetSafeNormal();
    const double Angle=N.Parent==INDEX_NONE?0.:FMath::Acos(FMath::Clamp(FVector::DotProduct(D,N.Direction),-1.,1.));
    if(!Clear(A,B,Cell*.25))continue;
    const double G=N.G+FVector::Dist(A,B)+Angle*Radius+FMath::Abs(NextCol-Mid)*.01;
    const int64 Key=int64(Next)*3+Delta+1;
    if(const double* Previous=Best.Find(Key))if(*Previous<=G)continue;
    Best.Add(Key,G);FNode Child;Child.Cell=Next;Child.Parent=Id;Child.Direction=D;Child.G=G;Child.F=G+FVector::Dist(B,Finish);
    Open.Add(Nodes.Add(Child));
   }
  }
 }
 return Fail(Budget?TEXT("Route: sweep/occupancy operation budget exhausted"):TEXT("Route: no fixed-Z route satisfies collision, wind, turning and inertia within bounded retries"));
}
FResult TryBorders(FBalloonLevelPlan& P,const TArray<FBalloonBorderInfluence>& Proposed,const FVector& Current,const FEnvelope& E,const FSettings& S,const FSweep& Sweep)
{
 FBalloonLevelPlan Candidate=P;Candidate.Borders=Proposed;
 FResult R=Validate(Candidate,E,S,Sweep,&Current);
 if(R.Reachable) {P.Borders=Proposed;P.RoutePoints=R.Points;}
 return R;
}
}
