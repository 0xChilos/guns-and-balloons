#include "BalloonEventTestObserver.h"
#include "../BalloonLevel/BalloonLevelGenerator.h"
#include "../BalloonLevel/BalloonBorderThreat.h"
#include "../BalloonLevel/BalloonLevelRealizer.h"
#include "../BalloonFlightComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/World.h"
#include <limits>
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBalloonLevelM6Test,"GunsAndBalloons.BalloonLevel.M6.Events",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FBalloonLevelM6Test::RunTest(const FString& Parameters)
{
 auto* M=NewObject<UBalloonLevelEventManager>();auto* O=NewObject<UBalloonEventTestObserver>();O->Manager=M;
 M->OnLevelEventTriggered.AddDynamic(O,&UBalloonEventTestObserver::Triggered);
 M->OnLevelEventRejected.AddDynamic(O,&UBalloonEventTestObserver::Reject);
 FBalloonLevelEventDefinition A;A.EventId=TEXT("late");A.Threshold=10.;
 auto B=A;B.EventId=TEXT("B");B.Threshold=2.;
 auto C=B;C.EventId=TEXT("A");auto D=B;D.EventId=TEXT("priority");D.Priority=-1;
 auto Zero=A;Zero.EventId=TEXT("zero");Zero.Threshold=0.;
 auto P=A;P.EventId=TEXT("progress");P.Trigger=EBalloonEventTrigger::NormalizedProgress;P.Threshold=.4;
 auto Dist=P;Dist.EventId=TEXT("meters");Dist.Trigger=EBalloonEventTrigger::AbsoluteProgress;Dist.Threshold=80.;
 auto Custom=A;Custom.EventId=TEXT("custom");Custom.Trigger=EBalloonEventTrigger::CustomSignal;Custom.SignalId=TEXT("boss");Custom.TargetTag=TEXT("finish");
 TestTrue(TEXT("register"),M->RegisterEventDefinitions({A,B,C,D,Zero,P,Dist,Custom}));
 M->UpdateElapsedGameTime(100.);TestEqual(TEXT("planning time excluded"),M->GetSnapshot().ElapsedSeconds,0.);
 M->StartEventTimeline();M->StartEventTimeline();TestEqual(TEXT("initial equality once"),O->Fired.Num(),1);
 M->PauseEventTimeline(true);M->UpdateElapsedGameTime(100.);M->UpdatePlayerProgress(1.,100.);M->EmitCustomSignal(TEXT("boss"),TEXT("finish"));
 TestEqual(TEXT("paused time frozen"),M->GetSnapshot().ElapsedSeconds,0.);TestEqual(TEXT("pause suppresses signals/progress"),O->Fired.Num(),1);
 M->PauseEventTimeline(false);M->UpdateElapsedGameTime(20.);
 TestEqual(TEXT("large time step no skips"),O->Fired.Num(),5);
 if(O->Fired.Num()==5) {TestEqual(TEXT("priority"),O->Fired[1],FName(TEXT("priority")));TestEqual(TEXT("stable ID"),O->Fired[2],FName(TEXT("A")));TestEqual(TEXT("threshold"),O->Fired[4],FName(TEXT("late")));}
 M->UpdatePlayerProgress(.9,90.);M->UpdatePlayerProgress(.1,10.);M->UpdatePlayerProgress(.9,90.);
 TestEqual(TEXT("backtracking once"),O->Fired.Num(),7);TestEqual(TEXT("watermark"),M->GetSnapshot().ProgressWatermark,.9);
 M->EmitCustomSignal(TEXT("boss"),TEXT("other"));TestEqual(TEXT("target filtered"),O->Fired.Num(),7);
 M->EmitCustomSignal(TEXT("boss"),TEXT("finish"));M->EmitCustomSignal(TEXT("boss"),TEXT("finish"));TestEqual(TEXT("custom once"),O->Fired.Num(),8);
 M->ResetEventTimeline();O->Fired.Reset();M->StartEventTimeline();M->UpdateElapsedGameTime(20.);TestEqual(TEXT("restart resets fired state"),O->Fired.Num(),5);
 M->ResetEventTimeline();auto Invalid=A;Invalid.RequestsLightingSwap=true;
 TestFalse(TEXT("Day/Night requests rejected"),M->RegisterEventDefinitions({Invalid}));Invalid.RequestsLightingSwap=false;Invalid.Action=static_cast<EBalloonEventAction>(255);
 TestFalse(TEXT("unknown imported transition rejected"),M->RegisterEventDefinitions({Invalid}));
 Invalid=A;Invalid.EventId=TEXT("TransitionToNight");TestFalse(TEXT("named transition rejected"),M->RegisterEventDefinitions({Invalid}));
 TestFalse(TEXT("duplicate ID rejected atomically"),M->RegisterEventDefinitions({A,A}));
 Invalid=A;Invalid.Threshold=std::numeric_limits<double>::quiet_NaN();TestFalse(TEXT("NaN rejected"),M->RegisterEventDefinitions({Invalid}));
 auto Repeat=P;Repeat.EventId=TEXT("repeat");Repeat.Repeat=EBalloonEventRepeat::RepeatAfterCooldown;Repeat.CooldownSeconds=2.;
 TestTrue(TEXT("cooldown registration"),M->RegisterEventDefinitions({Repeat}));O->Fired.Reset();M->StartEventTimeline();M->UpdatePlayerProgress(.8,80.);M->UpdateElapsedGameTime(1.);TestEqual(TEXT("cooldown holds"),O->Fired.Num(),1);
 M->UpdateElapsedGameTime(1.);TestEqual(TEXT("cooldown boundary"),O->Fired.Num(),2);M->UpdateElapsedGameTime(100.);TestEqual(TEXT("missed repeats coalesce to bounded single attempt"),O->Fired.Num(),3);
 M->ResetEventTimeline();Repeat.Repeat=EBalloonEventRepeat::OnEveryCrossing;Repeat.CooldownSeconds=0.;
 TestTrue(TEXT("crossing registration"),M->RegisterEventDefinitions({Repeat}));O->Fired.Reset();M->StartEventTimeline();M->UpdatePlayerProgress(.8,80.);M->UpdatePlayerProgress(.9,90.);M->UpdatePlayerProgress(.1,10.);M->UpdatePlayerProgress(.5,50.);TestEqual(TEXT("upward crossings only"),O->Fired.Num(),2);
 M->ResetEventTimeline();auto Nested=Custom;Nested.SignalId=TEXT("nested");TestTrue(TEXT("reentrancy registration"),M->RegisterEventDefinitions({A,B,Nested}));O->Fired.Reset();O->ResetOnFire=true;M->StartEventTimeline();M->UpdateElapsedGameTime(20.);
 TestEqual(TEXT("reset cancels pending batch; nested signals suppressed"),O->Fired.Num(),1);TestFalse(TEXT("callback reset stops timeline"),M->GetSnapshot().Running);
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBalloonLevelM6SafetyTest,"GunsAndBalloons.BalloonLevel.M6.RuntimeSafety",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FBalloonLevelM6SafetyTest::RunTest(const FString& Parameters)
{
 UWorld* W=UWorld::CreateWorld(EWorldType::Game,false);
 auto* Pawn=W->SpawnActor<AActor>();auto* Hull=NewObject<UBoxComponent>(Pawn);Pawn->SetRootComponent(Hull);Pawn->AddInstanceComponent(Hull);Hull->SetBoxExtent(FVector(100.,100.,400.));Hull->SetCollisionProfileName(TEXT("BlockAllDynamic"));Hull->RegisterComponent();
 auto* Flight=NewObject<UBalloonFlightComponent>(Pawn);Pawn->AddInstanceComponent(Flight);Flight->RegisterComponent();
 auto* G=W->SpawnActor<ABalloonLevelGenerator>();auto* C=NewObject<UBalloonLevelConfig>();C->MaxHeadingDegrees=0.;C->LengthMeters=100.;C->BaseWidthMeters=C->MinWidthMeters=40.;C->BordersEnabled=true;C->BorderExactCount=2;C->BorderRadiusMinMeters=C->BorderRadiusMaxMeters=1.;
 G->LevelConfig=C;G->RoutePawn=Pawn;TestTrue(TEXT("generate"),G->GenerateLevel());Pawn->SetActorLocation(G->GetPlan().Start.GetLocation()+FVector(1000.,0.,0.));
 if(G->GetPlan().Borders.Num()!=2) {AddError(TEXT("missing border fixtures"));W->DestroyWorld(false);return false;}
 const auto Old=G->GetPlan().Borders;
 auto* Proxy=Cast<ABalloonBorderThreat>(G->Realizer->Actors[0].Get());
 TestNotNull(TEXT("generation realizes border proxy"),Proxy);
 if(!Proxy) {W->DestroyWorld(false);return false;}
 Proxy->Tags.Add(TEXT("selected"));TestFalse(TEXT("generated proxy already registered"),G->RegisterBorderProxy(Proxy));
 FBalloonLevelEventDefinition E;E.EventId=TEXT("expand");E.Action=EBalloonEventAction::SetBorderRadius;E.Value=1000.;FString Reason;
 const auto Proof=G->GetPlan().RoutePoints;
 TestFalse(TEXT("unsafe whole-group expansion rejected"),G->TryEventBorderChange(E,Reason));TestTrue(TEXT("explicit safety result"),!Reason.IsEmpty());
 TestEqual(TEXT("descriptor rollback"),G->GetPlan().Borders[0].RadiusCm,Old[0].RadiusCm);TestEqual(TEXT("proxy rollback"),Proxy->InfluenceCollision->GetUnscaledSphereRadius(),Old[0].RadiusCm);TestTrue(TEXT("proof rollback"),G->GetPlan().RoutePoints==Proof);
 E.Action=EBalloonEventAction::ScaleBorderRadius;E.Value=.5;E.TargetTag=TEXT("selected");
 TestTrue(TEXT("safe tagged shrink"),G->TryEventBorderChange(E,Reason));TestEqual(TEXT("selected committed"),G->GetPlan().Borders[0].RadiusCm,Old[0].RadiusCm*.5f);TestEqual(TEXT("other untouched"),G->GetPlan().Borders[1].RadiusCm,Old[1].RadiusCm);
 TestEqual(TEXT("collision commit after proof"),Proxy->InfluenceCollision->GetUnscaledSphereRadius(),Old[0].RadiusCm*.5f);TestTrue(TEXT("proof starts from current Pawn"),G->GetPlan().RoutePoints[0].Equals(Pawn->GetActorLocation()));
 E.Action=EBalloonEventAction::AddBorderRadius;E.Value=-100.;TestFalse(TEXT("nonpositive radius rejected"),G->TryEventBorderChange(E,Reason));
 E.Value=1.;E.RequestsLightingSwap=true;TestFalse(TEXT("direct lighting swap rejected"),G->TryEventBorderChange(E,Reason));E.RequestsLightingSwap=false;
 const auto Preset=G->GetPlan().LightingPreset;
 auto* O=NewObject<UBalloonEventTestObserver>();G->EventManager->OnLevelEventRejected.AddDynamic(O,&UBalloonEventTestObserver::Reject);
 E.TargetTag=NAME_None;E.Action=EBalloonEventAction::SetBorderRadius;E.Value=1000.;TestTrue(TEXT("event registration"),G->EventManager->RegisterEventDefinitions({E}));G->EventManager->StartEventTimeline();G->EventManager->UpdateElapsedGameTime(1.);
 TestEqual(TEXT("unsafe event rejected once"),O->Rejected.Num(),1);TestEqual(TEXT("fixed preset survives events"),G->GetPlan().LightingPreset,Preset);
 G->ClearGeneratedLevel();TestFalse(TEXT("clear stops timeline"),G->EventManager->GetSnapshot().Running);TestTrue(TEXT("clear removes fired state"),!G->EventManager->GetSnapshot().States[0].Fired);
 W->DestroyWorld(false);return true;
}
#endif
