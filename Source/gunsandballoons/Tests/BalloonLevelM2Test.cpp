#include "../BalloonLevel/BalloonLevelGenerator.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "Components/SplineComponent.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBalloonLevelM2Test,"GunsAndBalloons.BalloonLevel.M2",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FBalloonLevelM2Test::RunTest(const FString& Parameters)
{
 auto* C=NewObject<UBalloonLevelConfig>();
 C->LengthMeters=1620.f;
 C->StartTransform=FTransform(FRotator(30.,70.,20.),FVector(123.,456.,99999.),FVector(3.));
 C->WidthProfile.GetRichCurve()->AddKey(0.f,1.f);
 C->WidthProfile.GetRichCurve()->AddKey(1.f,0.f);
 FBalloonLevelPlan P,Q; TArray<FString> Errors;
 TestTrue(TEXT("Build curved geometry"),BalloonSplinePlanning::Build(*C,P,Errors));
 TestTrue(TEXT("Repeat build"),BalloonSplinePlanning::Build(*C,Q,Errors));
 TestTrue(TEXT("Exact deterministic samples"),P.SplineSamples==Q.SplineSamples);
 double Length=0.; const FVector Forward=FRotator(0.,70.,0.).Vector();
 for(int32 I=1; I<P.SplineSamples.Num(); ++I)
 {
  const FVector D=P.SplineSamples[I]-P.SplineSamples[I-1]; Length+=D.Size();
  TestEqual(TEXT("Fixed Z only"),P.SplineSamples[I].Z,10000.);
  TestTrue(TEXT("Strict forward projection proves no self crossings"),FVector::DotProduct(D,Forward)>0.);
  if(I>1)
  {
   const FVector Prev=P.SplineSamples[I-1]-P.SplineSamples[I-2];
   const double Angle=FMath::Acos(FMath::Clamp(FVector::DotProduct(D.GetSafeNormal(),Prev.GetSafeNormal()),-1.,1.));
   TestTrue(TEXT("Discrete turn radius bound"),Angle/D.Size()<=1./(C->MinimumTurningRadiusMeters*100.)+1.e-9);
  }
 }
 TestTrue(TEXT("Requested arc length within 0.01cm"),FMath::Abs(Length-162000.)<0.01);
 TestEqual(TEXT("Partial final segment"),P.Segments.Last().EndS-P.Segments.Last().StartS,12000.);
 TestEqual(TEXT("Width minimum"),BalloonSplinePlanning::WidthAt(P,P.LengthCm),4000.);
 TestEqual(TEXT("Boundary belongs to next segment"),BalloonSplinePlanning::SegmentAt(P,25000.),1);
 TestEqual(TEXT("Finish belongs to final segment"),BalloonSplinePlanning::SegmentAt(P,P.LengthCm),P.Segments.Num()-1);
 TestEqual(TEXT("Base width"),BalloonSplinePlanning::WidthAt(P,0.),60000.);
 TestTrue(TEXT("Finish equals endpoint"),P.Finish.GetLocation().Equals(P.SplineSamples.Last()));
 C->Seed++; BalloonSplinePlanning::Build(*C,Q,Errors);
 TestFalse(TEXT("Seed changes shape"),P.SplineSamples==Q.SplineSamples);
 C->Seed--; C->FlightHeightMeters=250.f; C->SegmentTargetMeters=111.f; C->Preset=EBalloonLightingPreset::Night;
 BalloonSplinePlanning::Build(*C,Q,Errors);
 for(int32 I=0; I<P.SplineSamples.Num(); ++I)
 {
  TestEqual(TEXT("Other systems/datum do not change XY"),FVector2D(P.SplineSamples[I]),FVector2D(Q.SplineSamples[I]));
  TestEqual(TEXT("Authored fixed height"),Q.SplineSamples[I].Z,25000.);
 }
 FBalloonLevelProgress Progress;
 Progress.Update(P,BalloonSplinePlanning::PositionAt(P,80000.));
 Progress.Update(P,BalloonSplinePlanning::PositionAt(P,70000.)+FVector(0.,0.,900000.));
 TestTrue(TEXT("Backtracking current"),FMath::Abs(Progress.CurrentS-70000.)<0.01);
 TestTrue(TEXT("Monotonic maximum"),FMath::Abs(Progress.MaxReachedS-80000.)<0.01);
 Progress.Update(P,P.Finish.GetLocation(),true);
 TestEqual(TEXT("Teleport full search reaches finish"),Progress.CurrentS,P.LengthCm);
 Progress.Update(P,P.Start.GetLocation(),true);
 TestEqual(TEXT("Teleport back to start"),Progress.CurrentS,0.);
 TestEqual(TEXT("Maximum retained"),Progress.MaxReachedS,P.LengthCm);
 UWorld* World=GWorld;
 if(TestNotNull(TEXT("Editor world"),World))
 {
  auto* G=World->SpawnActor<ABalloonLevelGenerator>(); G->LevelConfig=C;
  TestFalse(TEXT("M5 rejects uninspected curved reference"),G->GenerateLevel());
  TestTrue(TEXT("No unproved reference spline"),G->Spline->GetNumberOfSplinePoints()==0);
  TestEqual(TEXT("Unproved finish has no progression"),G->GetNormalizedProgress(G->GetFinishTransform().GetLocation()),0.f);
  G->UpdateProgress(G->GetFinishTransform().GetLocation(),true); G->RegenerateLevel();
  TestEqual(TEXT("Regenerate resets progression"),G->GetMaxReachedProgressCm(),0.f);
  World->DestroyActor(G);
 }
 C->WidthProfile.GetRichCurve()->Reset();
 C->WidthProfile.GetRichCurve()->AddKey(0.f,1.f);
 C->WidthProfile.GetRichCurve()->AddKey(0.12345f,2.f);
 C->WidthProfile.GetRichCurve()->AddKey(1.f,1.f);
 BalloonSplinePlanning::Build(*C,Q,Errors);
 TestTrue(TEXT("Width evaluates authored curve between geometry samples"),FMath::Abs(BalloonSplinePlanning::WidthAt(Q,Q.LengthCm*0.12345)-120000.)<0.1);
 for(int32 Seed : {0,1,12345,-98765})
 {
  C->Seed=Seed; BalloonSplinePlanning::Build(*C,Q,Errors);
  FBalloonLevelPlan Repeat; BalloonSplinePlanning::Build(*C,Repeat,Errors);
  TestTrue(TEXT("Pinned seeds reproducible"),Q.SplineSamples==Repeat.SplineSamples);
 }
 C->MaxHeadingDegrees=46.f; Errors.Reset(); TestFalse(TEXT("Unsafe heading rejected"),C->Validate(Errors));
 C->MaxHeadingDegrees=0.f; C->MinimumTurningRadiusMeters=0.f; Errors.Reset(); TestFalse(TEXT("Invalid radius rejected"),C->Validate(Errors));
 return true;
}
#endif
