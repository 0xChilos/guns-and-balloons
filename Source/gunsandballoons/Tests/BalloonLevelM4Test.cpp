#include "../BalloonLevel/BalloonPlacementPlanner.h"
#include "../BalloonLevel/BalloonSplinePlanner.h"
#include "../BalloonLevel/BalloonAssetCatalog.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBalloonLevelM4Test,"GunsAndBalloons.BalloonLevel.M4",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FBalloonLevelM4Test::RunTest(const FString& Parameters)
{
 using namespace BalloonPlacementPlanning;
 auto* C=NewObject<UBalloonLevelConfig>(); C->MaxHeadingDegrees=0.;
 FBalloonLevelPlan Base; TArray<FString> D;
 TestTrue(TEXT("M2 geometry"),BalloonSplinePlanning::Build(*C,Base,D));
 FResolvedRow R; R.Entry.RowName=TEXT("Test"); R.Entry.SourceTable=TEXT("TestTable");
 auto& Row=R.Entry.Row; Row.Asset=FSoftObjectPath(TEXT("/Engine/BasicShapes/Cube.Cube"));
 Row.Category=TEXT("Filler"); Row.Rule=TEXT("scatter"); Row.DensityPerKm2=20.; Row.ScaleMin=.8f; Row.ScaleMax=1.2f;
 R.Metadata.SizeMeters=FVector(1.); Row.MinSpacingM=5.; Row.ClusterSize=2;
 auto Run=[&](FName Biome,const TArray<FResolvedRow>& Rows,FBalloonLevelPlan& P)
 {
  P=Base;P.BiomeId=Biome; D.Reset();const bool Result=Build(*C,P,Rows,D);
  if(Result)
  {
   FBalloonLevelPlan Repeat=Base;Repeat.BiomeId=Biome;TArray<FString> RepeatD;
   TestTrue(TEXT("all rule repeat succeeds"),Build(*C,Repeat,Rows,RepeatD));
   TestEqual(TEXT("all rule repeat count"),P.Placements.Num(),Repeat.Placements.Num());
   for(int32 I=0;I<FMath::Min(P.Placements.Num(),Repeat.Placements.Num());++I)
    TestTrue(TEXT("all rule deterministic descriptor"),P.Placements[I].Transform.Equals(Repeat.Placements[I].Transform) && P.Placements[I].RowName==Repeat.Placements[I].RowName);
  }
  return Result;
 };
 FBalloonLevelPlan A,B;
 TestTrue(TEXT("scatter"),Run(TEXT("Forest"),{R},A));
 TestEqual(TEXT("density km2"),A.Placements.Num(),18);
 TestTrue(TEXT("scatter repeat"),Run(TEXT("Forest"),{R},B));
 for(int32 I=0;I<A.Placements.Num();++I)
 {
  TestTrue(TEXT("repeat transform"),A.Placements[I].Transform.Equals(B.Placements[I].Transform));
  TestTrue(TEXT("scale interval"),A.Placements[I].Transform.GetScale3D().X>=.8 && A.Placements[I].Transform.GetScale3D().X<=1.2);
  TestEqual(TEXT("cluster ID"),A.Placements[I].ClusterId,I/2);
  if(I%2) TestTrue(TEXT("cluster radius"),FVector::Dist2D(A.Placements[I].Transform.GetLocation(),A.Placements[I-1].Transform.GetLocation())<=3000.001);
  for(int32 J=0;J<I;++J) TestTrue(TEXT("spacing"),FVector::Dist2D(A.Placements[I].Transform.GetLocation(),A.Placements[J].Transform.GetLocation())>=499.99);
 }
 Row.AlignToSlope=true; D.Reset();B=Base;
 TestTrue(TEXT("ground slope sampler"),Build(*C,B,{R},D,[](const FVector&,FSurface& S){S.Z=123.;S.Normal=FVector(0.,.2,1.).GetSafeNormal();return true;}));
 if(B.Placements.Num()) {TestEqual(TEXT("ground Z separate from flight"),B.Placements[0].Transform.GetLocation().Z,123.);TestTrue(TEXT("asset up normal"),B.Placements[0].Transform.GetRotation().GetUpVector().Equals(FVector(0.,.2,1.).GetSafeNormal(),.0001));}
 Row.AlignToSlope=false; Row.ClusterSize=1;
 Row.Rule=TEXT("ring");Row.Category=TEXT("Background");Row.DensityPerKm2=4.; R.Metadata.SizeMeters=FVector(50.,600.,50.); Row.MinSpacingM=100000.;
 TestTrue(TEXT("ring overlap exemption"),Run(TEXT("Forest"),{R},A));
 const FVector Center=BalloonSplinePlanning::PositionAt(A,A.LengthCm*.5);
 for(const auto& X:A.Placements)
 {
  const double Radius=FVector::Dist2D(Center,X.Transform.GetLocation());
  TestTrue(TEXT("annulus"),Radius>=150000. && Radius<=300000.);
  TestTrue(TEXT("inward"),FVector::DotProduct(X.Transform.GetRotation().GetForwardVector(),(Center-X.Transform.GetLocation()).GetSafeNormal2D())>.999);
  TestEqual(TEXT("mountains no cull"),X.CullEndCm,0);
 }
 R.Metadata.SizeMeters.Y=1.; TestFalse(TEXT("uncovered ring diagnostic"),Run(TEXT("Forest"),{R},A));
 TestTrue(TEXT("rule table row and count diagnostic"),D.Num() && D.Last().Contains(TEXT("TestTable row Test rule ring: target=")));
 Row.Rule=TEXT("line");Row.Category=TEXT("Filler"); Row.MinSpacingM=0.;Row.ScaleMin=Row.ScaleMax=1.;
 R.Metadata.SizeMeters=FVector(100.,1.,1.); Row.DensityPerKm2=30.f/.9f;
 TestTrue(TEXT("line covers both borders"),Run(TEXT("Field"),{R},A));
 TestEqual(TEXT("chain count"),A.Placements.Num(),30);
 for(int32 I=1;I<15 && I<A.Placements.Num();++I) TestTrue(TEXT("end-to-end chain"),FMath::IsNearlyEqual(FVector::Dist2D(A.Placements[I].Transform.GetLocation(),A.Placements[I-1].Transform.GetLocation()),10000.,.1));
 Row.DensityPerKm2=1.; TestFalse(TEXT("line uncovered fails"),Run(TEXT("Field"),{R},B));
 Row.DensityPerKm2=3.f/.9f;B=Base;B.BiomeId=TEXT("Field");D.Reset();
 TestTrue(TEXT("supplied polyline and overlapping final remainder"),Build(*C,B,{R},D,FGroundSampler(),{{FVector(0.,0.,0.),FVector(10000.,0.,0.),FVector(25000.,0.,0.)}}));
 if(TestEqual(TEXT("short chain three pieces"),B.Placements.Num(),3)) TestTrue(TEXT("final remainder overlaps full-scale mesh"),B.Placements[2].Transform.GetLocation().Equals(FVector(20000.,0.,0.),.01));
 Row.Rule=TEXT("street");Row.DensityPerKm2=20.;R.Metadata.SizeMeters=FVector(10.);
 TestTrue(TEXT("street buildings"),Run(TEXT("City"),{R},A));
 for(const auto& X:A.Placements) TestTrue(TEXT("street quantized yaw"),FMath::IsNearlyZero(FMath::Fmod(FMath::Abs(X.Transform.Rotator().Yaw),90.),.001));
 if(A.Placements.Num()>1) TestTrue(TEXT("back-to-back pair depth"),FMath::IsNearlyEqual(FVector::Dist2D(A.Placements[0].Transform.GetLocation(),A.Placements[1].Transform.GetLocation()),1000.,.01));
 FResolvedRow OtherBuilding=R;OtherBuilding.Entry.RowName=TEXT("OtherBuilding");
 TestTrue(TEXT("shared street lot exclusions across rows"),Run(TEXT("City"),{R,OtherBuilding},B));
 for(int32 I=0;I<B.Placements.Num();++I) for(int32 J=0;J<I;++J)
  TestFalse(TEXT("building lots never reused"),B.Placements[I].Transform.GetLocation().Equals(B.Placements[J].Transform.GetLocation(),.01));
 R.Metadata.StreetLamp=true;TestTrue(TEXT("street lamps"),Run(TEXT("City"),{R},A));
 for(const auto& X:A.Placements) {const auto V=X.Transform.GetLocation();auto Edge=[](double Q){const double T=FMath::Abs(FMath::Fmod(Q,10000.));return FMath::IsNearlyEqual(FMath::Min(T,10000.-T),1000.,.01);};TestTrue(TEXT("lamp at grid street edge"),Edge(V.X)||Edge(V.Y));}
 R.Metadata.StreetLamp=false;Row.Rule=TEXT("wind_aligned");Row.RandomYaw=true;
 TestTrue(TEXT("wind dunes"),Run(TEXT("Desert"),{R},A));
 for(const auto& X:A.Placements) TestTrue(TEXT("common wind plus +/-10"),FMath::Abs(FMath::FindDeltaAngleDegrees(A.PrevailingWindYaw,X.Transform.Rotator().Yaw))<=10.001);
 Row.Rule=TEXT("flock");R.BlueprintActor=true;Row.DensityPerKm2=9999.; Row.ClusterSize=50;
 // Pure planner consumes resolved class kind; actual BP_BirdFlock asset validation is covered by resolver/row tests.
 TestTrue(TEXT("Field flock"),Run(TEXT("Field"),{R},A));
 TestTrue(TEXT("3-6 actors independent density and cluster"),A.Placements.Num()>=3 && A.Placements.Num()<=6);
 for(const auto& X:A.Placements) TestTrue(TEXT("flock Actor exception"),X.Representation==EBalloonPlacementRepresentation::BlueprintActor && !X.Collision && X.BirdsPerFlock==50);
 FResolvedRow Second=R;Second.Entry.RowName=TEXT("Other");
 TestTrue(TEXT("multiple flock rows"),Run(TEXT("Field"),{Second,R},B));TestEqual(TEXT("global flock budget"),A.Placements.Num(),B.Placements.Num());
 TestTrue(TEXT("flock other biome disabled"),Run(TEXT("Forest"),{R},B));TestEqual(TEXT("no Forest flock"),B.Placements.Num(),0);
 Row.Rule=TEXT("scatter");Row.Category=TEXT("Obstacle");Row.Collision=true;Row.DensityPerKm2=20.;Row.ClusterSize=1;R.BlueprintActor=false;
 C->ObstaclesEnabled=true;C->ObstacleExactCount=7;
 Row.DensityPerKm2=1000000.; // Exact count replaces density before any density-derived count limit.
 FResolvedRow Enemy=R;Enemy.Entry.RowName=TEXT("Enemy");Enemy.Entry.Row.Category=TEXT("Hazard");Enemy.Metadata.InteriorEnemy=true;Enemy.BlueprintActor=true;
 C->EnemiesEnabled=true;C->EnemyExactCount=3;
 TestTrue(TEXT("independent gameplay overrides"),Run(TEXT("Forest"),{R,Enemy},A)); TestEqual(TEXT("total counts"),A.Placements.Num(),10);
 for(const auto& X:A.Placements) TestTrue(TEXT("colliding Actors never cull"),X.Collision && X.CullEndCm==0 && X.Representation!=EBalloonPlacementRepresentation::HISM);
 C->ObstaclesEnabled=false;TestTrue(TEXT("obstacles off"),Run(TEXT("Forest"),{R,Enemy},B));TestEqual(TEXT("enemies stay"),B.Placements.Num(),3);
 for(const auto& X:B.Placements) TestTrue(TEXT("enemy stable"),A.Placements.ContainsByPredicate([&](const auto& Y){return Y.RowName==X.RowName&&Y.InstanceIndex==X.InstanceIndex&&Y.Transform.Equals(X.Transform);}));
 C->EnemyExactCount=0;TestTrue(TEXT("exact zero"),Run(TEXT("Forest"),{R,Enemy},B));TestEqual(TEXT("zero"),B.Placements.Num(),0);
 C->EnemyExactCount=3;TestFalse(TEXT("positive missing entries"),Run(TEXT("Forest"),{},B));
 C->EnemiesEnabled=false;C->ObstaclesEnabled=true;Row.MinSpacingM=100000.;TestFalse(TEXT("impossible packing bounded"),Run(TEXT("Forest"),{R},B));TestEqual(TEXT("no partial result"),B.Placements.Num(),0);
 C->ObstaclesEnabled=false;Row.Category=TEXT("Filler");Row.Collision=false;Row.MinSpacingM=0.;Row.DensityPerKm2=20.;
 FResolvedRow Conflicting=R;Conflicting.Entry.RowName=TEXT("Conflicting");Conflicting.Metadata.CullEndCm=30000;
 TestFalse(TEXT("same mesh conflicting cull profiles rejected"),Run(TEXT("Forest"),{R,Conflicting},B));
 TestTrue(TEXT("cull error is specific"),D.Num() && D.Last().Contains(TEXT("conflicting HISM cull profiles")));
 C->StreetRoadMeters=C->StreetCellMeters;TestFalse(TEXT("invalid street dimensions"),Run(TEXT("City"),{R},B));C->StreetRoadMeters=20.;
 // All six rule names plus wrong rule-specific categories use the real asset validator.
 for(const TCHAR* Rule:{TEXT("scatter"),TEXT("ring"),TEXT("line"),TEXT("street"),TEXT("wind_aligned"),TEXT("flock")})
 {FBalloonCatalogEntry E=R.Entry;E.Row.Rule=Rule;E.Row.Category=TEXT("Filler");E.Row.Collision=false;E.Row.MinSpacingM=0.;D.Reset(); const bool Valid=BalloonAssetCatalog::ValidateRow(E,D);TestEqual(TEXT("row-specific rule validation"),Valid,FName(Rule)!=TEXT("ring")&&FName(Rule)!=TEXT("flock"));}
 return true;
}
#endif
