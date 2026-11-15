#include "../BalloonLevel/BalloonBorderPlanner.h"
#include "../BalloonLevel/BalloonSplinePlanner.h"
#include "../BalloonLevel/BalloonBorderThreat.h"
#include "Components/SphereComponent.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/World.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBalloonLevelM3Test,"GunsAndBalloons.BalloonLevel.M3",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FBalloonLevelM3Test::RunTest(const FString& Parameters)
{
 // Enemy density 1.5 turns the authored 8 borders into 12 on a 500m route.
 // Greedy random packing can jam despite enough room for six per side.
 auto* Dense=NewObject<UBalloonLevelConfig>();
 Dense->BordersEnabled=true; Dense->BorderExactCount=12; Dense->LengthMeters=500.f;
 Dense->BorderMinSpacingMeters=80.f; Dense->BorderAllowOverlap=false;
 for (int32 Seed : {12345,42,30068})
 {
  Dense->Seed=Seed; FBalloonLevelPlan DensePlan; TArray<FString> Messages;
  TestTrue(TEXT("dense border geometry"),BalloonSplinePlanning::Build(*Dense,DensePlan,Messages));
  TestTrue(TEXT("twelve borders fit without relaxing spacing"),BalloonBorderPlanning::Build(*Dense,DensePlan,Messages));
  TestEqual(TEXT("dense border count preserved"),DensePlan.Borders.Num(),12);
  for (int32 I=0;I<DensePlan.Borders.Num();++I) for(int32 J=0;J<I;++J)
   TestTrue(TEXT("dense borders retain minimum spacing and no overlaps"),
    FVector::DistXY(DensePlan.Borders[I].Center,DensePlan.Borders[J].Center)+.01>=
     FMath::Max(8000.,DensePlan.Borders[I].RadiusCm+DensePlan.Borders[J].RadiusCm));
 }
 auto* C=NewObject<UBalloonLevelConfig>();
 C->BordersEnabled=true; C->BorderExactCount=25; C->MaxHeadingDegrees=0.f;
 FBalloonLevelPlan P,Q; TArray<FString> D;
 TestTrue(TEXT("Geometry"),BalloonSplinePlanning::Build(*C,P,D));
 TestTrue(TEXT("Border plan"),BalloonBorderPlanning::Build(*C,P,D));
 TestEqual(TEXT("Exact odd count"),P.Borders.Num(),25);
 Q=P;
 TestTrue(TEXT("Repeat border plan"),BalloonBorderPlanning::Build(*C,Q,D));
 int32 Left=0,Right=0;
 for(int32 I=0;I<P.Borders.Num();++I)
 {
  const auto& T=P.Borders[I];
  TestTrue(TEXT("Deterministic descriptors"),T.Center==Q.Borders[I].Center && T.RadiusCm==Q.Borders[I].RadiusCm && T.InstanceId==I);
  TestTrue(TEXT("Variable radius bounds"),T.RadiusCm>=1200.f && T.RadiusCm<=3500.f);
  TestTrue(TEXT("Exact local intrusion"),FMath::IsNearlyEqual(double(T.IntrusionCm),double(T.RadiusCm)*0.5,0.02));
  TestEqual(TEXT("Fixed center Z"),T.Center.Z,10000.);
  T.Side<0?++Left:++Right;
 }
 TestEqual(TEXT("Stable balanced partition"),Left-Right,1);
 TestTrue(TEXT("Radii vary"),P.Borders[0].RadiusCm!=P.Borders[1].RadiusCm);
 C->CatalogVersion=TEXT("unrelated"); C->BorderExactCount=26;
 TestTrue(TEXT("Rebuild unrelated config"),BalloonBorderPlanning::Build(*C,Q,D));
 TestTrue(TEXT("Existing item stream unchanged by count/catalog"),P.Borders[0].Center==Q.Borders[0].Center);
 FBalloonBorderInfluence T; T.RadiusCm=100.; T.Center=FVector(0.,1050.,10000.); T.Policy=EBalloonBorderInfluencePolicy::Blocked;
 double A=0.,B=0.;
 TestTrue(TEXT("Circle line intersection"),BalloonBorderPlanning::CrossSection(T,FVector(0.,0.,10000.),FVector::RightVector,0.,0.,A,B));
 TestEqual(TEXT("Exact lower intersection"),A,950.);
 P.Borders={T}; P.BaseWidthCm=2000.; P.MinWidthCm=2000.;
 TestEqual(TEXT("Clipped intrusion"),BalloonBorderPlanning::UnsafeWidth(P,0.,0.,0.),50.);
 P.Borders.Add(T);
 TestEqual(TEXT("Overlaps union, never double count"),BalloonBorderPlanning::UnsafeWidth(P,0.,0.,0.),50.);
 P.Borders[0].RadiusCm=200.; P.Borders.SetNum(1);
 TestEqual(TEXT("Radius expansion deforms XY safe width"),BalloonBorderPlanning::UnsafeWidth(P,0.,0.,0.),150.);
 TestEqual(TEXT("Balloon footprint inflation"),BalloonBorderPlanning::UnsafeWidth(P,0.,0.,50.),200.);
 T.Center.Z=10200.;
 TestEqual(TEXT("Sphere misses flight envelope"),BalloonBorderPlanning::ProjectedRadius(T,10000.,0.),-1.);
 TestTrue(TEXT("Full collider Z extent intersects sphere"),BalloonBorderPlanning::ProjectedRadius(T,10000.,150.)>86.6);
 T.Policy=EBalloonBorderInfluencePolicy::VisualOnly;
 TestFalse(TEXT("Visual only never blocks"),BalloonBorderPlanning::CrossSection(T,FVector(0.,0.,10200.),FVector::RightVector,0.,0.,A,B));
 C->MaxHeadingDegrees=25.f; C->BorderExactCount=25;
 C->WidthProfile.GetRichCurve()->AddKey(0.f,1.f); C->WidthProfile.GetRichCurve()->AddKey(1.f,0.5f);
 TestTrue(TEXT("Curved variable width geometry"),BalloonSplinePlanning::Build(*C,Q,D));
 const auto Geometry=Q.SplineSamples;
 TestTrue(TEXT("Curved border plan"),BalloonBorderPlanning::Build(*C,Q,D));
 TestTrue(TEXT("Border stream leaves geometry untouched"),Q.SplineSamples==Geometry);
 for(const auto& Border:Q.Borders)
 {
  const FVector O=BalloonSplinePlanning::PositionAt(Q,Border.DistanceCm);
  const double Offset=FVector::DotProduct(Border.Center-O,BalloonBorderPlanning::RightAt(Q,Border.DistanceCm))*Border.Side;
  TestTrue(TEXT("Local offset uses actual curved width"),FMath::Abs(Offset-(BalloonSplinePlanning::WidthAt(Q,Border.DistanceCm)*0.5+Border.RadiusCm*0.5))<0.01);
 }
 C->BordersEnabled=false;
 TestTrue(TEXT("Disabled plan"),BalloonBorderPlanning::Build(*C,Q,D)); TestEqual(TEXT("Disabled zero"),Q.Borders.Num(),0);
 C->BordersEnabled=true; C->BorderExactCount=0;
 TestTrue(TEXT("Exact zero"),BalloonBorderPlanning::Build(*C,Q,D)); TestEqual(TEXT("Zero count"),Q.Borders.Num(),0);
 C->BorderExactCount=-1; C->BorderDensityPerKm=2.f;
 TestTrue(TEXT("Density mode"),BalloonBorderPlanning::Build(*C,Q,D)); TestEqual(TEXT("Both sides length density"),Q.Borders.Num(),6);
 C->BorderExactCount=2; C->BorderAllowOverlap=false; C->BorderRadiusMinMeters=100000.f; C->BorderRadiusMaxMeters=100000.f;
 TestFalse(TEXT("Disallowed influence overlap fails"),BalloonBorderPlanning::Build(*C,Q,D));
 C->BorderAllowOverlap=true; C->BorderRadiusMinMeters=12.f; C->BorderRadiusMaxMeters=35.f;
 C->BorderExactCount=2; C->BorderMinSpacingMeters=100000.f;
 TestFalse(TEXT("Impossible spacing fails bounded"),BalloonBorderPlanning::Build(*C,Q,D)); TestEqual(TEXT("No partial descriptors"),Q.Borders.Num(),0);
 UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);
 auto* Proxy=World->SpawnActor<ABalloonBorderThreat>();
 T.Center.Z=10000.;
 TestTrue(TEXT("Proxy initialization"),Proxy->InitializeInfluence(T));
 TestEqual(TEXT("Visual proxy no collision"),Proxy->InfluenceCollision->GetCollisionEnabled(),ECollisionEnabled::NoCollision);
 T.Policy=EBalloonBorderInfluencePolicy::Blocked; Proxy->InitializeInfluence(T);
 TestEqual(TEXT("Existing balloon WorldDynamic collision"),Proxy->InfluenceCollision->GetCollisionResponseToChannel(ECC_WorldDynamic),ECR_Block);
 TestEqual(TEXT("Blocking proxy"),Proxy->InfluenceCollision->GetCollisionResponseToChannel(ECC_Pawn),ECR_Block);
 T.Policy=EBalloonBorderInfluencePolicy::Damaging; Proxy->InitializeInfluence(T);
 TestEqual(TEXT("Damaging proxy overlap"),Proxy->InfluenceCollision->GetCollisionResponseToChannel(ECC_Pawn),ECR_Overlap);
 T.Policy=EBalloonBorderInfluencePolicy::Pushback; Proxy->InitializeInfluence(T);
 TestTrue(TEXT("Pushback generates overlaps"),Proxy->InfluenceCollision->GetGenerateOverlapEvents());
 World->DestroyWorld(false);
 return true;
}
#endif
