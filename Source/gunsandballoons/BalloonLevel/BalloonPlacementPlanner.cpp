#include "BalloonPlacementPlanner.h"
#include "BalloonSplinePlanner.h"
#include "BalloonBorderPlanner.h"
#include "BalloonAssetCatalog.h"
#include "Engine/StaticMesh.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/CollisionProfile.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "GameFramework/Actor.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "PhysicsEngine/BodySetup.h"
#include "Math/RotationMatrix.h"

namespace
{
 // FNV-1a over lowercase UTF-8, explicitly independent of FName pool and UObject addresses.
 uint32 Seed(const FBalloonLevelPlan& P, const FString& Tag)
 {
  const FTCHARToUTF8 Bytes(*(P.BiomeId.ToString()+TEXT("|")+P.CatalogVersion+TEXT("|Placement/v1|")+Tag).ToLower());
  uint32 H = 2166136261u ^ uint32(P.Seed) ^ (uint32(P.PlacementAttempt)*0x9E3779B9u);
  for (int32 I=0; I<Bytes.Length(); ++I) { H ^= uint8(Bytes.Get()[I]); H *= 16777619u; }
  return H;
 }
 double Area(const FBalloonLevelPlan& P)
 {
  double A=0.;
  for(int32 I=1; I<P.SampleDistances.Num(); ++I)
   A += (P.SampleDistances[I]-P.SampleDistances[I-1])*(BalloonSplinePlanning::WidthAt(P,P.SampleDistances[I])+BalloonSplinePlanning::WidthAt(P,P.SampleDistances[I-1]))*0.5;
  return A/1.e10; // cm² -> km²
 }
 bool Inside(const FBalloonLevelPlan& P, const FVector& V, double Margin=0.)
 {
  int32 Edge=INDEX_NONE;
  const double S=BalloonSplinePlanning::NearestDistance(P,V,Edge,true);
  if(S<=0. && FVector::DotProduct(V-P.SplineSamples[0],P.SplineSamples[1]-P.SplineSamples[0])<0.) return false;
  if(S>=P.LengthCm && FVector::DotProduct(V-P.SplineSamples.Last(),P.SplineSamples.Last()-P.SplineSamples[P.SplineSamples.Num()-2])>0.) return false;
  return FVector::Dist2D(V,BalloonSplinePlanning::PositionAt(P,S))+Margin <= BalloonSplinePlanning::WidthAt(P,S)*0.5;
 }
 // Component templates of a Blueprint live in its SimpleConstructionScript, not on the class CDO
 // (UE does not create SCS components for templates). Walk the SCS tree plus the CDO's native
 // components and return the actor-local AABB of everything that will collide at runtime.
 // Templates are unregistered, so transforms come from the SCS parent links and CalcBounds(Identity).
 struct FBlueprintBounds
 {
  const UBlueprintGeneratedClass* Class=nullptr;
  const AActor* CDO=nullptr;
  // Templates skip FBodyInstance::FixupData, so a serialized collision profile decides first.
  // Anything not explicitly NoCollision counts as colliding: an oversized box rejects routes,
  // a missing one hides them.
  static bool HasQueryCollision(const UPrimitiveComponent& P)
  {
   const FName Profile=P.GetCollisionProfileName();
   if(!Profile.IsNone()) return Profile!=UCollisionProfile::NoCollision_ProfileName;
   return P.IsQueryCollisionEnabled();
  }
  USCS_Node* FindNode(FName Name) const
  {
   for(const UClass* C=Class; C; C=C->GetSuperClass())
   {
    const UBlueprintGeneratedClass* BP=Cast<UBlueprintGeneratedClass>(C);
    if(!BP || !BP->SimpleConstructionScript) continue;
    for(USCS_Node* Node: BP->SimpleConstructionScript->GetAllNodes())
     if(Node && Node->GetVariableName()==Name) return Node;
   }
   return nullptr;
  }
  FTransform NativeToActor(const USceneComponent* Component) const
  {
   FTransform Result=Component->GetRelativeTransform();
   for(const USceneComponent* Parent=Component->GetAttachParent(); Parent; Parent=Parent->GetAttachParent())
    Result=Result*Parent->GetRelativeTransform();
   return Result;
  }
  FTransform NodeToActor(const USCS_Node* Node) const
  {
   const USceneComponent* Template=Cast<USceneComponent>(Node->GetActualComponentTemplate(const_cast<UBlueprintGeneratedClass*>(Class)));
   FTransform Result=Template ? Template->GetRelativeTransform() : FTransform::Identity;
   if(Node->ParentComponentOrVariableName!=NAME_None)
   {
    if(Node->bIsParentComponentNative)
    {
     for(USceneComponent* Native: TInlineComponentArray<USceneComponent*>(CDO))
      if(Native->GetFName()==Node->ParentComponentOrVariableName) { Result=Result*NativeToActor(Native); break; }
    }
    else if(const USCS_Node* Parent=FindNode(Node->ParentComponentOrVariableName)) Result=Result*NodeToActor(Parent);
   }
   return Result;
  }
  void Measure(FBox& Out) const
  {
   auto Add=[&Out](const UPrimitiveComponent& P, const FTransform& ToActor)
   {
    if(!HasQueryCollision(P)) return;
    const FBoxSphereBounds Local=P.CalcBounds(FTransform::Identity);
    if(Local.SphereRadius<=KINDA_SMALL_NUMBER || Local.BoxExtent.ContainsNaN()) return;
    Out+=Local.GetBox().TransformBy(ToActor);
   };
   for(UPrimitiveComponent* Native: TInlineComponentArray<UPrimitiveComponent*>(CDO))
    if(Native) Add(*Native,NativeToActor(Native));
   for(const UClass* C=Class; C; C=C->GetSuperClass())
   {
    const UBlueprintGeneratedClass* BP=Cast<UBlueprintGeneratedClass>(C);
    if(!BP || !BP->SimpleConstructionScript) continue;
    for(USCS_Node* Node: BP->SimpleConstructionScript->GetAllNodes())
     if(const auto* Primitive=Node ? Cast<UPrimitiveComponent>(Node->GetActualComponentTemplate(const_cast<UBlueprintGeneratedClass*>(Class))) : nullptr)
      Add(*Primitive,NodeToActor(Node));
   }
  }
 };
}

bool BalloonPlacementPlanning::Resolve(const UBalloonLevelConfig& C, const TArray<FBalloonCatalogEntry>& Catalog,
 TArray<FResolvedRow>& Rows, TArray<FString>& D)
{
 check(IsInGameThread());
 Rows.Reset();
 for(const auto& E:Catalog)
 {
  if(!BalloonAssetCatalog::ValidateRow(E,D)) {Rows.Reset();return false;}
  FResolvedRow R; R.Entry=E;
  if(const auto* M=C.PlacementMetadata.Find(E.RowName)) R.Metadata=*M;
  UObject* Asset=E.Row.Asset.TryLoad();
  R.BlueprintActor=Cast<UClass>(Asset)!=nullptr;
  if(auto* Mesh=Cast<UStaticMesh>(Asset))
  {
   R.LocalCollisionBounds=Mesh->GetBoundingBox();
   if(const UBodySetup* Body=Mesh->GetBodySetup())
   {
    const FBox Simple=Body->AggGeom.CalcAABB(FTransform::Identity);
    if(Simple.IsValid) R.LocalCollisionBounds+=Simple;
   }
   if(R.Metadata.SizeMeters.IsNearlyZero()) R.Metadata.SizeMeters=Mesh->GetBoundingBox().GetSize()/100.;
  }
  else if(UClass* Class=Cast<UClass>(Asset))
  {
   // Blueprint Actor rows have no mesh to measure. Certify collision from the components the
   // class will actually construct instead of trusting the authored SizeMeters.
   if(const auto* BP=Cast<UBlueprintGeneratedClass>(Class))
   {
    FBlueprintBounds Bounds; Bounds.Class=BP; Bounds.CDO=Cast<AActor>(Class->GetDefaultObject());
    FBox Box(ForceInit); Bounds.Measure(Box);
    if(Box.IsValid && !Box.GetExtent().ContainsNaN()) R.LocalCollisionBounds=Box;
   }
  }
  if(!Asset) { D.Add(FString::Printf(TEXT("Table %s row %s rule %s: asset unresolved"),*E.SourceTable,*E.RowName.ToString(),*E.Row.Rule.ToString())); Rows.Reset(); return false; }
  Rows.Add(R);
 }
 return true;
}

bool BalloonPlacementPlanning::Build(const UBalloonLevelConfig& C, FBalloonLevelPlan& P,
 const TArray<FResolvedRow>& Input, TArray<FString>& D, const FGroundSampler& Ground,
 const TArray<TArray<FVector>>& FieldBorders, const TArray<FVector>& ReservedRoute)
{
 P.Placements.Reset();
 if(P.SampleDistances.Num()<2 || P.SplineSamples.Num()!=P.SampleDistances.Num()) { D.Add(TEXT("Placement: missing M2 geometry")); return false; }
 if(!FMath::IsFinite(C.ClusterRadiusMeters) || C.ClusterRadiusMeters<0 ||
  !FMath::IsFinite(C.PlacementGroundZMeters) || !FMath::IsFinite(C.StreetCellMeters) ||
  !FMath::IsFinite(C.StreetRoadMeters) || !FMath::IsFinite(C.StreetLotDepthMeters) ||
  !FMath::IsFinite(C.StreetLampIntervalMeters) || C.StreetCellMeters<=0 || C.StreetRoadMeters<=0 ||
  C.StreetRoadMeters>=C.StreetCellMeters || C.StreetLotDepthMeters<=0 ||
  2*C.StreetLotDepthMeters>C.StreetCellMeters-C.StreetRoadMeters || C.StreetLampIntervalMeters<=0 ||
  C.ObstacleExactCount < -1 || C.EnemyExactCount < -1 || C.ObstacleExactCount>10000 || C.EnemyExactCount>10000)
 { D.Add(TEXT("Placement: invalid counts, cluster radius, ground or street dimensions")); return false; }
 TArray<FResolvedRow> Rows=Input;
 Rows.Sort([](const auto& A,const auto& B){return A.Entry.RowName.LexicalLess(B.Entry.RowName);});
 TArray<int32> Targets; Targets.Init(0,Rows.Num());
 const double EligibleArea=Area(P);
 FRandomStream Wind(Seed(P,TEXT("Wind"))); P.PrevailingWindYaw=Wind.FRandRange(0.f,360.f);
 FRandomStream Flocks(Seed(P,TEXT("FlockBudget"))); const int32 FlockTotal=Flocks.RandRange(3,6);
 int32 FlockRows=0;
 auto Group=[&](const FResolvedRow& R) { return R.Metadata.InteriorEnemy?2: (R.Entry.Row.Category==TEXT("Obstacle") || R.Entry.Row.Category==TEXT("Hazard")?1:0); };
 auto Enabled=[&](int32 G){return G==2?(C.EnemiesEnabled && C.EnemyDensity>0.f):G==1?(C.ObstaclesEnabled && C.ObstacleDensity>0.f):C.SceneryEnabled;};
 auto Active=[&](const FResolvedRow& R)
 {
  const FName Rule=R.Entry.Row.Rule;
  const int32 G=Group(R);
  return Enabled(G) && !(G==1 && C.ObstacleExactCount==0) && !(G==2 && C.EnemyExactCount==0) &&
   (Rule!=TEXT("ring") || P.BiomeId==TEXT("Forest")) &&
   (Rule!=TEXT("flock") || P.BiomeId==TEXT("Field")) && (Rule!=TEXT("street") || P.BiomeId==TEXT("City")) &&
   (Rule!=TEXT("line") || P.BiomeId==TEXT("Field")) && (Rule!=TEXT("wind_aligned") || P.BiomeId==TEXT("Desert"));
 };
 auto Fail=[&](int32 I,int32 Planned,const FString& Why)
 {
  const auto& E=Rows[I].Entry;
  D.Add(FString::Printf(TEXT("Table %s row %s rule %s: target=%d planned=%d region=%s corridor/annulus; %s"),
   *E.SourceTable,*E.RowName.ToString(),*E.Row.Rule.ToString(),Targets[I],Planned,*P.BiomeId.ToString(),*Why));
  P.Placements.Reset(); return false;
 };
 for(int32 I=0;I<Rows.Num();++I)
 {
  const auto& R=Rows[I].Entry.Row;
  if(!Active(Rows[I])) continue;
  if(R.Rule==TEXT("flock")) { ++FlockRows; continue; }
  const int32 G=Group(Rows[I]);
  if((G==1 && C.ObstacleExactCount>=0) || (G==2 && C.EnemyExactCount>=0)) continue;
  const double A=R.Rule==TEXT("ring")?PI*(9.-2.25):EligibleArea;
  const double N=FMath::RoundToDouble(A*R.DensityPerKm2);
  if(!FMath::IsFinite(N) || N>10000 || N<0) return Fail(I,0,TEXT("density target outside [0,10000]"));
  Targets[I]=int32(N);
 }
 // Exactly 3-6 per Field level, not per table row. Stable round-robin among eligible flock rows.
 for(int32 I=0,Rank=0;I<Rows.Num();++I) if(Active(Rows[I]) && Rows[I].Entry.Row.Rule==TEXT("flock"))
 { Targets[I]=FlockTotal/FlockRows+(Rank++<FlockTotal%FlockRows?1:0); }
 for(int32 G=1;G<=2;++G)
 {
  const int32 Override=G==1?C.ObstacleExactCount:C.EnemyExactCount;
  if(!Enabled(G) || Override<0) continue;
  int32 Fixed=0;
  for(int32 I=0;I<Rows.Num();++I) if(Active(Rows[I]) && Group(Rows[I])==G && Rows[I].Entry.Row.Rule==TEXT("flock")) Fixed+=Targets[I];
  if(Fixed>Override) {D.Add(TEXT("Placement: exact gameplay budget conflicts with mandatory Field flock budget"));return false;}
  const int32 Remaining=Override-Fixed;
  double Sum=0.; TArray<int32> Choices;
  for(int32 I=0;I<Rows.Num();++I) if(Active(Rows[I]) && Group(Rows[I])==G && Rows[I].Entry.Row.Rule!=TEXT("flock"))
  {Choices.Add(I); Sum+=Rows[I].Entry.Row.DensityPerKm2; Targets[I]=0;}
  if(Remaining && Sum<=0.) {D.Add(FString::Printf(TEXT("Placement %s: positive exact count requires eligible positive-density rows after reserved flock budget"),G==1?TEXT("obstacles"):TEXT("enemies"))); return false;}
  // Largest remainder with lexical ties, replacing total category density count.
  TArray<TPair<int32,double>> Remainders; int32 Assigned=0;
  for(int32 I:Choices) {const double N=Sum>0?Remaining*Rows[I].Entry.Row.DensityPerKm2/Sum:0.; Targets[I]=FMath::FloorToInt(N); Assigned+=Targets[I]; Remainders.Emplace(I,N-Targets[I]);}
  Remainders.StableSort([](const auto& A,const auto& B){return A.Value>B.Value;});
  for(int32 I=0;I<Remaining-Assigned;++I) ++Targets[Remainders[I].Key];
 }
 // Scale final category budgets so explicit counts and mandatory flock targets honour the same controls.
 for (int32 I=0; I<Rows.Num(); ++I)
 {
  const int32 G=Group(Rows[I]);
  const double Target=FMath::RoundToDouble(Targets[I] * double(G==2?C.EnemyDensity:G==1?C.ObstacleDensity:1.f));
  if (!FMath::IsFinite(Target) || Target<0. || Target>10000.) return Fail(I,0,TEXT("scaled density target outside [0,10000]"));
  Targets[I]=int32(Target);
 }
 TArray<FBalloonPlacementDescriptor> Output;
 TMap<FSoftObjectPath,int32> CullByMesh;
 TSet<FIntVector> OccupiedStreetLots;
 double HashCell=1.;
 for(const auto& R:Rows) if(Active(R) && R.Entry.Row.Rule!=TEXT("ring")) HashCell=FMath::Max(HashCell,double(R.Entry.Row.MinSpacingM)*100.);
 TMap<FIntPoint,TArray<int32>> Reservations;
 TArray<double> ReservedSpacing;
 auto CellAt=[&](const FVector& V){return FIntPoint(FMath::FloorToInt(V.X/HashCell),FMath::FloorToInt(V.Y/HashCell));};
 int32 Work=0;
 for(int32 RI=0;RI<Rows.Num();++RI)
 {
  const auto& Res=Rows[RI]; const auto& E=Res.Entry; const auto& R=E.Row; const auto& M=Res.Metadata;
  const int32 Target=Targets[RI]; if(!Target) continue;
  if(Output.Num()+Target>10000) return Fail(RI,0,TEXT("total placement limit 10000"));
  if(M.SizeMeters.ContainsNaN() || M.SizeMeters.GetMin()<=0 ||
   (M.CullEndCm!=0 && M.CullEndCm!=15000 && M.CullEndCm!=30000 && M.CullEndCm!=80000) ||
   (M.InteriorEnemy && (R.Category!=TEXT("Hazard") || !Res.BlueprintActor)))
   return Fail(RI,0,TEXT("positive resolved bounds required; interior enemy requires Hazard Blueprint Actor"));
  const bool Ring=R.Rule==TEXT("ring"), Line=R.Rule==TEXT("line"), Street=R.Rule==TEXT("street"), Flock=R.Rule==TEXT("flock");
  if(Flock && !Res.BlueprintActor) return Fail(RI,0,TEXT("flock requires resolved BP_BirdFlock Actor"));
  if(Ring && (Target<3 || R.Category!=TEXT("Background") || Res.BlueprintActor || R.Collision)) return Fail(RI,0,TEXT("ring requires >=3 noncolliding Background meshes"));
  FRandomStream Layout(Seed(P,E.RowName.ToString()+TEXT("|")+R.Rule.ToString()));
  const double Radius=Layout.FRandRange(150000.f,300000.f);
  const double Phase=Layout.FRandRange(0.f,2.f*PI);
  const FVector Center=BalloonSplinePlanning::PositionAt(P,P.LengthCm*0.5);


  const double Spacing=R.MinSpacingM*100.;
  double MaxWidth=0.; for(double W:P.SampleWidths) MaxWidth=FMath::Max(MaxWidth,W);
  const double MaxEdgeWidth=FMath::Max(MaxWidth,P.BaseWidthCm); // rejection-sampled width-weighted corridor
  FVector ClusterAnchor;
  TArray<TArray<FVector>> Chains=FieldBorders;
  if(Line && Chains.IsEmpty()) for(int32 Side:{-1,1})
  {
   TArray<FVector> Chain;
   for(double S:P.SampleDistances) Chain.Add(BalloonSplinePlanning::PositionAt(P,S)+BalloonBorderPlanning::RightAt(P,S)*(Side*BalloonSplinePlanning::WidthAt(P,S)*.5));
   Chains.Add(MoveTemp(Chain));
  }
  if(Line) for(const auto& Chain:Chains)
  {
   if(Chain.Num()<2) return Fail(RI,0,TEXT("field border requires at least two finite points"));
   for(const auto& Point:Chain) if(Point.ContainsNaN()) return Fail(RI,0,TEXT("nonfinite field border"));
  }
  int32 ChainIndex=0, ChainEdge=1;
  FVector ChainCursor=Line?Chains[0][0]:FVector::ZeroVector;
  const double Cell=C.StreetCellMeters*100., Road=C.StreetRoadMeters*100., Depth=C.StreetLotDepthMeters*100.;
  TArray<TPair<FVector,double>> Lots;
  if(Street)
  {
   FBox B(ForceInit); for(const FVector& V:P.SplineSamples) B+=V;
   B=B.ExpandBy(MaxEdgeWidth);
   const int32 NX=FMath::CeilToInt((B.Max.X-B.Min.X)/Cell)+2, NY=FMath::CeilToInt((B.Max.Y-B.Min.Y)/Cell)+2;
   if(double(NX)*NY>100000) return Fail(RI,0,TEXT("street grid cell budget exceeded"));
   for(double X=FMath::FloorToDouble(B.Min.X/Cell)*Cell;X<=B.Max.X;X+=Cell)
    for(double Y=FMath::FloorToDouble(B.Min.Y/Cell)*Cell;Y<=B.Max.Y;Y+=Cell)
    {
     if(M.StreetLamp)
     {
      for(double T=0.;T<Cell;T+=C.StreetLampIntervalMeters*100.)
       for(int32 Side:{-1,1}) {Lots.Emplace(FVector(X+T,Y+Side*Road*.5,0.),0.); Lots.Emplace(FVector(X+Side*Road*.5,Y+T,0.),90.); if(++Work>200000) return Fail(RI,0,TEXT("street lamp budget exceeded"));}
     }
     else
     {
      // Paired lots share a rear boundary at block midline; front faces opposite parallel streets.
      const double Half=Cell*.5;
      Lots.Emplace(FVector(X+Half,Y+Half-Depth*.5,0.),270.);
      Lots.Emplace(FVector(X+Half,Y+Half+Depth*.5,0.),90.);
     }
    }
   if(!M.StreetLamp)
   {
    TArray<TPair<FVector,double>> Eligible;
    const double Margin=FVector2D(M.SizeMeters.X,M.SizeMeters.Y).Size()*50.*R.ScaleMax+Depth*.5;
    for(int32 I=0;I+1<Lots.Num();I+=2)
     if(Inside(P,Lots[I].Key,Margin) && Inside(P,Lots[I+1].Key,Margin)) {Eligible.Add(Lots[I]);Eligible.Add(Lots[I+1]);}
    Lots=MoveTemp(Eligible);
   }
   // Seeded partial Fisher-Yates: building pairs remain adjacent.
   const int32 Unit=M.StreetLamp?1:2;
   for(int32 I=Lots.Num()/Unit-1;I>0;--I) {const int32 J=Layout.RandRange(0,I);for(int32 K=0;K<Unit;++K) Swap(Lots[I*Unit+K],Lots[J*Unit+K]);}
  }
  int32 Lot=0;
  for(int32 I=0;I<Target;++I)
  {
   FRandomStream Item(Seed(P,E.RowName.ToString()+TEXT("|")+R.Rule.ToString()+FString::Printf(TEXT("|%d"),I)));
   const double Scale=Item.FRandRange(R.ScaleMin,R.ScaleMax);
   const double Length=M.SizeMeters.X*100.*Scale;
   const double Footprint=FVector2D(M.SizeMeters.X,M.SizeMeters.Y).Size()*50.*Scale;
   FVector V; double Yaw=0., S=0.; bool Found=false;
   FIntVector StreetKey=FIntVector::ZeroValue;
   for(int32 Attempt=0;Attempt<256;++Attempt)
   {
    if(++Work>500000) return Fail(RI,I,TEXT("bounded planning work exhausted"));
    S=Item.FRandRange(0.f,float(P.LengthCm));
    if(Ring)
    {
     const double Angle=Phase+2.*PI*I/Target;
     V=Center+FVector(FMath::Cos(Angle),FMath::Sin(Angle),0.)*Radius;
     Yaw=FMath::RadiansToDegrees(Angle)+180.;
     // Mesh Y is silhouette width when X faces inward. Require overlap even at minimum scale.
     if(2.*Radius*FMath::Tan(PI/Target)>=M.SizeMeters.Y*100.*R.ScaleMin)
      return Fail(RI,I,TEXT("uncovered horizon: increase density or silhouette width; annulus unchanged"));
     for(const auto& Segment:P.Segments)
      if(Segment.BaseBounds.ExpandBy(FVector(Footprint,Footprint,0.)).IsInsideOrOn(V))
       return Fail(RI,I,TEXT("annulus silhouette intersects conservative playable bounds"));
    }
    else if(Line)
    {
     if(!Chains.IsValidIndex(ChainIndex)) return Fail(RI,I,TEXT("chain count exceeds field borders"));
     const auto& Chain=Chains[ChainIndex];
     FVector Begin=ChainCursor, End=Chain.Last(); bool Hit=false;
     const int32 StartEdge=ChainEdge;
     // First forward intersection of the border polyline with a circle of one mesh length.
     for(;ChainEdge<Chain.Num();++ChainEdge)
     {
      FVector A=ChainEdge==StartEdge?ChainCursor:Chain[ChainEdge-1];
      FVector Delta=Chain[ChainEdge]-A; Delta.Z=0.; FVector Offset=A-Begin; Offset.Z=0.;
      const double AA=Delta.SizeSquared(), BB=2.*FVector::DotProduct(Offset,Delta), CC=Offset.SizeSquared()-Length*Length;
      const double Disc=BB*BB-4.*AA*CC;
      if(AA>0. && Disc>=0.)
      {
       const double T=(-BB+FMath::Sqrt(Disc))/(2.*AA);
       if(T>=0. && T<=1.) {End=A+Delta*T;Hit=true;break;}
      }
     }
     FVector Direction=(End-Begin).GetSafeNormal2D();
     if(Direction.IsNearlyZero()) return Fail(RI,I,TEXT("zero-length field border remainder"));
     if(!Hit) Begin=End-Direction*Length; // Full-scale overlapping final remainder, never a gap.
     V=(Begin+End)*.5; Yaw=Direction.Rotation().Yaw; ChainCursor=End;
     if(!Hit || End.Equals(Chain.Last(),.001))
     {++ChainIndex;ChainEdge=1;if(Chains.IsValidIndex(ChainIndex)) ChainCursor=Chains[ChainIndex][0];}
    }
    else if(Street)
    {
     if(!Lots.IsValidIndex(Lot)) return Fail(RI,I,TEXT("insufficient grid lots/edges for density or spacing"));
     V=Lots[Lot].Key; Yaw=Lots[Lot++].Value;
     if(!M.StreetLamp)
     {
      StreetKey=FIntVector(FMath::FloorToInt(V.X/Cell),FMath::FloorToInt(V.Y/Cell),Yaw==90.?1:0);
      if(OccupiedStreetLots.Contains(StreetKey)) continue;
      if(M.SizeMeters.Y*100.*Scale>Cell-Road || M.SizeMeters.X*100.*Scale>Depth) return Fail(RI,I,TEXT("mesh exceeds lot footprint"));
      // X is the authored front axis. Each rear face touches the shared block midline.
      V.Y+=(Yaw==90.?1.:-1.)*(M.SizeMeters.X*100.*Scale-Depth)*.5;
     }
     if(!Inside(P,V,Footprint)) continue;
    }
    else
    {
     V=BalloonSplinePlanning::PositionAt(P,S)+BalloonBorderPlanning::RightAt(P,S)*Item.FRandRange(float(-MaxEdgeWidth*.5),float(MaxEdgeWidth*.5));
     if(I%R.ClusterSize==0 || Flock) ClusterAnchor=V;
     else {const double A=Item.FRandRange(0.f,2.f*PI), L=FMath::Sqrt(Item.FRand())*C.ClusterRadiusMeters*100.; V=ClusterAnchor+FVector(FMath::Cos(A),FMath::Sin(A),0.)*L;}
     if(!Inside(P,V)) continue;
     Yaw=R.Rule==TEXT("wind_aligned")?P.PrevailingWindYaw+Item.FRandRange(-10.f,10.f):(R.RandomYaw?Item.FRandRange(0.f,360.f):0.);
    }
    bool Spaced=true;
    if(!Ring)
    {
     const FIntPoint Key=CellAt(V);
     for(int32 DX=-1;DX<=1;++DX) for(int32 DY=-1;DY<=1;++DY)
      if(const auto* Bucket=Reservations.Find(Key+FIntPoint(DX,DY))) for(int32 J:*Bucket)
      {
       const double Gap=FMath::Max(Spacing,ReservedSpacing[J]);
       if(++Work>500000) return Fail(RI,I,TEXT("reservation work limit exceeded"));
       if(FVector::DistSquared2D(V,Output[J].Transform.GetLocation())<Gap*Gap-0.001) Spaced=false;
      }
    }
    if(!Spaced) {if(Line) return Fail(RI,I,TEXT("line spacing conflicts with end-to-end mesh length"));continue;}
    // Keep the preflight route available, including its start/finish reservations.
    // Use transformed collision bounds at terrain height, rather than mesh origins:
    // low flight and narrow corridors must not turn valid geometry into a wall.
    if (R.Collision && Res.LocalCollisionBounds.IsValid && !ReservedRoute.IsEmpty())
    {
     FSurface ProposedSurface; ProposedSurface.Z=C.PlacementGroundZMeters*100.;
     if (Ground && !Ring && !Ground(V,ProposedSurface)) continue;
     if (!FMath::IsFinite(ProposedSurface.Z) || ProposedSurface.Normal.ContainsNaN() ||
         ProposedSurface.Normal.Z<=0. || ProposedSurface.Normal.IsNearlyZero()) continue;
     FVector Location=V; Location.Z=ProposedSurface.Z;
     FQuat ProposedRotation=FRotator(0.,Yaw,0.).Quaternion();
     if (R.AlignToSlope && !Street)
     {
      FVector Forward=FRotator(0.,Yaw,0.).Vector();
      Forward.Z=-(ProposedSurface.Normal.X*Forward.X+ProposedSurface.Normal.Y*Forward.Y)/ProposedSurface.Normal.Z;
      ProposedRotation=FRotationMatrix::MakeFromZX(ProposedSurface.Normal,Forward).ToQuat();
     }
     const FBox Bounds=Res.LocalCollisionBounds.TransformBy(FTransform(ProposedRotation,Location,FVector(Scale)))
      .ExpandBy(FVector(P.BalloonRadiusCm,P.BalloonRadiusCm,P.BalloonHalfHeightCm));
     bool BlocksRoute=Bounds.IsInsideOrOn(ReservedRoute[0]);
     for (int32 RouteIndex=1; !BlocksRoute && RouteIndex<ReservedRoute.Num(); ++RouteIndex)
     {
      if (++Work>500000) return Fail(RI,I,TEXT("route reservation work limit exceeded"));
      const FVector& Begin=ReservedRoute[RouteIndex-1];
      const FVector& End=ReservedRoute[RouteIndex];
      BlocksRoute=FMath::LineBoxIntersection(Bounds,Begin,End,End-Begin);
     }
     if (BlocksRoute)
     {
      if (Line) return Fail(RI,I,TEXT("field border intersects reserved flight route"));
      continue;
     }
    }
    Found=true; break;
   }
   if(!Found) return Fail(RI,I,TEXT("256 attempts exhausted: spacing/cluster/eligible bounds"));
   FSurface Surface; Surface.Z=C.PlacementGroundZMeters*100.;
   // Horizon rings are outside the playable terrain footprint and retain their authored datum.
   if(Ground && !Ring && !Ground(V,Surface)) return Fail(RI,I,TEXT("terrain sampler rejected location"));
   if(!FMath::IsFinite(Surface.Z) || Surface.Normal.ContainsNaN() || Surface.Normal.Z<=0. || Surface.Normal.IsNearlyZero()) return Fail(RI,I,TEXT("invalid terrain height/normal"));
   V.Z=Surface.Z;
   FQuat Rotation=FRotator(0.,Yaw,0.).Quaternion();
   if(R.AlignToSlope && !Street)
   {
    FVector Forward=FRotator(0.,Yaw,0.).Vector();
    Forward.Z=-(Surface.Normal.X*Forward.X+Surface.Normal.Y*Forward.Y)/Surface.Normal.Z;
    Rotation=FRotationMatrix::MakeFromZX(Surface.Normal,Forward).ToQuat();
   }
   FBalloonPlacementDescriptor Desc;
   Desc.Biome=P.BiomeId; Desc.RowName=E.RowName; Desc.Rule=R.Rule; Desc.InstanceIndex=I;
   int32 Edge=INDEX_NONE; S=BalloonSplinePlanning::NearestDistance(P,V,Edge,true);
   Desc.SegmentId=BalloonSplinePlanning::SegmentAt(P,S); Desc.ClusterId=Flock?I:I/R.ClusterSize; Desc.BirdsPerFlock=R.ClusterSize;
   Desc.Asset=R.Asset; Desc.Transform=FTransform(Rotation,V,FVector(Scale)); Desc.Collision=R.Collision; Desc.InteriorEnemy=M.InteriorEnemy;
   if(Res.LocalCollisionBounds.IsValid) Desc.CollisionBounds=Res.LocalCollisionBounds.TransformBy(Desc.Transform);
   Desc.Representation=Res.BlueprintActor?EBalloonPlacementRepresentation::BlueprintActor:(Group(Res)?EBalloonPlacementRepresentation::MeshActor:EBalloonPlacementRepresentation::HISM);
   Desc.CullEndCm=Group(Res)||Ring?0:(R.Rule==TEXT("wind_aligned") || Street?80000:M.CullEndCm);
   if(Desc.Representation==EBalloonPlacementRepresentation::HISM)
   {
    if(const int32* Existing=CullByMesh.Find(R.Asset)) if(*Existing!=Desc.CullEndCm) return Fail(RI,I,TEXT("same mesh has conflicting HISM cull profiles"));
    CullByMesh.Add(R.Asset,Desc.CullEndCm);
   }
   if(!Ring) Reservations.FindOrAdd(CellAt(V)).Add(Output.Num());
   if(Street && !M.StreetLamp) OccupiedStreetLots.Add(StreetKey);
   ReservedSpacing.Add(Spacing);
   Output.Add(Desc);
  }
  if(Line && ChainIndex<Chains.Num()) return Fail(RI,Target,TEXT("density budget leaves uncovered field border"));
  D.Add(FString::Printf(TEXT("Table %s row %s rule %s: target=%d planned=%d region=%s"),*E.SourceTable,*E.RowName.ToString(),*R.Rule.ToString(),Target,Target,*P.BiomeId.ToString()));
 }
 P.Placements=MoveTemp(Output); return true;
}
