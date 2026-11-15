#include "../BalloonLevel/BalloonLevelGenerator.h"
#include "../BalloonFlightComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBalloonLevelCompletionTest, "GunsAndBalloons.BalloonLevel.Completion",
 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBalloonLevelCompletionTest::RunTest(const FString& Parameters)
{
 auto* W = UWorld::CreateWorld(EWorldType::Game, false);
 auto* Balloon = W->SpawnActor<AActor>();
 auto* Hull = NewObject<UBoxComponent>(Balloon);
 Balloon->SetRootComponent(Hull); Balloon->AddInstanceComponent(Hull);
 Hull->SetBoxExtent(FVector(100., 100., 400.)); Hull->SetCollisionProfileName(TEXT("BlockAllDynamic")); Hull->RegisterComponent();
 auto* Flight = NewObject<UBalloonFlightComponent>(Balloon);
 Balloon->AddInstanceComponent(Flight); Flight->RegisterComponent();
 auto* G = W->SpawnActor<ABalloonLevelGenerator>();
 G->ManageEnvironment = false; G->SpawnEnemies = false; G->RoutePawn = Balloon;
 auto* C = NewObject<UBalloonLevelConfig>();
 C->LengthMeters = 100.f; C->MaxHeadingDegrees = 0.f;
 C->BaseWidthMeters = C->MinWidthMeters = 40.f;
 C->StartTransform.SetRotation(FRotator(0., 35., 0.).Quaternion()); G->LevelConfig = C;
 G->CompleteLevel(); TestFalse(TEXT("not ready"), G->LevelCompleted);
 if (!TestTrue(TEXT("generate"), G->GenerateLevel())) { W->DestroyWorld(false); return false; }
 TestTrue(TEXT("zone at finish"), G->CompletionZone->GetComponentLocation().Equals(G->GetFinishTransform().GetLocation()));
 TestTrue(TEXT("nonblocking zone"), G->CompletionZone->GetCollisionEnabled() == ECollisionEnabled::NoCollision);
 G->Tick(.016f); TestFalse(TEXT("start not complete"), G->LevelCompleted);
 Balloon->SetActorLocation(G->GetFinishTransform().TransformPosition(FVector(0., 3000., 0.)));
 G->Tick(.016f); TestFalse(TEXT("outside zone"), G->LevelCompleted);
 Balloon->SetActorLocation(G->GetFinishTransform().GetLocation());
 G->Tick(.016f); TestTrue(TEXT("finish completes"), G->LevelCompleted);
 G->CompleteLevel(); TestTrue(TEXT("duplicate remains complete"), G->LevelCompleted);
 C->LengthMeters = 0.f;
 TestFalse(TEXT("failed generation"), G->GenerateLevel()); TestTrue(TEXT("retains completion"), G->LevelCompleted);
 C->LengthMeters = 100.f;
 TestTrue(TEXT("regenerate"), G->GenerateLevel()); TestFalse(TEXT("reset completion"), G->LevelCompleted);
 G->ClearGeneratedLevel(); G->Tick(.016f); TestFalse(TEXT("clear disables zone"), G->LevelCompleted);
 W->DestroyWorld(false); return true;
}
#endif
