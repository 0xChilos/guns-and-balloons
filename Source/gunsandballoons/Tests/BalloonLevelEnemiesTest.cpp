#include "../BalloonLevel/BalloonLevelGenerator.h"
#include "../BalloonLevel/BalloonLevelRealizer.h"
#include "../BalloonFlightComponent.h"
#include "../NemesisAirship.h"
#include "../NemesisFlak.h"
#include "../NemesisBirdFlock.h"
#include "../NemesisBeacon.h"
#include "../NemesisTowerCannon.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "Materials/Material.h"
#include "MaterialDomain.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBalloonLevelEnemiesTest, "GunsAndBalloons.BalloonLevel.Enemies",
 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBalloonLevelEnemiesTest::RunTest(const FString& Parameters)
{
 auto* World = UWorld::CreateWorld(EWorldType::Game, false);
 auto* Pawn = World->SpawnActor<AActor>();
 auto* Hull = NewObject<UBoxComponent>(Pawn);
 Pawn->SetRootComponent(Hull); Pawn->AddInstanceComponent(Hull);
 Hull->SetBoxExtent(FVector(100., 100., 400.)); Hull->SetCollisionProfileName(TEXT("BlockAllDynamic")); Hull->RegisterComponent();
 auto* Flight = NewObject<UBalloonFlightComponent>(Pawn); Pawn->AddInstanceComponent(Flight); Flight->RegisterComponent();
 auto* G = World->SpawnActor<ABalloonLevelGenerator>();
 auto* Config = NewObject<UBalloonLevelConfig>();
 Config->LengthMeters = 400.f; Config->MaxHeadingDegrees = 0.f;
 Config->TerrainMode = EBalloonTerrainMode::Flat;
 Config->TerrainMaterial = UMaterial::GetDefaultMaterial(MD_Surface);
 G->LevelConfig = Config; G->RoutePawn = Pawn; G->ManageEnvironment = false;
 G->EdgeAACount = 2; G->BirdFlockCount = 2; G->DesertCannonCount = 2; G->DesertBeaconCount = 1;
 auto Count = [&](UClass* Class)
 {
  int32 Result = 0;
  for (const auto& Ref : G->Realizer->Actors) if (Ref.IsValid() && Ref->IsA(Class)) ++Result;
  return Result;
 };
 Config->BiomeId = TEXT("Field");
 TestTrue(TEXT("field generation"), G->GenerateLevel());
 TestEqual(TEXT("shared airship"), Count(ANemesisAirship::StaticClass()), 1);
 TestEqual(TEXT("shared AA"), Count(ANemesisFlak::StaticClass()), 2);
 TestEqual(TEXT("field birds"), Count(ANemesisBirdFlock::StaticClass()), 2);
 const auto BeforeFailure = G->Realizer->Actors;
 Config->LengthMeters = 0.f;
 TestFalse(TEXT("invalid regeneration fails"), G->RegenerateLevel());
 TestTrue(TEXT("failure preserves generated enemies"), G->Realizer->Actors == BeforeFailure);
 for (const auto& Ref : BeforeFailure) TestTrue(TEXT("previous enemy collision restored"), Ref.IsValid() && Ref->GetActorEnableCollision());
 Config->LengthMeters = 400.f;
 const auto Previous = G->Realizer->Actors;
 TArray<FTransform> Transforms;
 for (const auto& Ref : Previous) Transforms.Add(Ref->GetActorTransform());
 TestTrue(TEXT("regenerate field"), G->RegenerateLevel());
 TestEqual(TEXT("no accumulation"), G->Realizer->Actors.Num(), Previous.Num());
 for (int32 I = 0; I < Previous.Num(); ++I)
 {
  TestFalse(TEXT("old generated actors removed"), Previous[I].IsValid());
  TestTrue(TEXT("seed reproduces enemies"), G->Realizer->Actors[I]->GetActorTransform().Equals(Transforms[I]));
 }
 Config->BiomeId = TEXT("Desert");
 TestTrue(TEXT("desert generation"), G->RegenerateLevel());
 TestEqual(TEXT("desert excludes field birds"), Count(ANemesisBirdFlock::StaticClass()), 0);
 TestEqual(TEXT("desert cannons grounded"), Count(ANemesisTowerCannon::StaticClass()), 2);
 TestEqual(TEXT("desert beacons grounded"), Count(ANemesisBeacon::StaticClass()), 1);
 G->SpawnSharedEnemies = false;
 TestTrue(TEXT("shared disabled generation"), G->RegenerateLevel());
 TestEqual(TEXT("shared disabled"), Count(ANemesisAirship::StaticClass()) + Count(ANemesisFlak::StaticClass()), 0);
 TestEqual(TEXT("biome still enabled"), Count(ANemesisBeacon::StaticClass()), 1);
 G->SpawnSharedEnemies = true; G->SpawnBiomeEnemies = false;
 TestTrue(TEXT("biome disabled generation"), G->RegenerateLevel());
 TestEqual(TEXT("biome disabled"), Count(ANemesisBeacon::StaticClass()) + Count(ANemesisTowerCannon::StaticClass()), 0);
 G->SpawnEnemies = false;
 TestTrue(TEXT("all disabled generation"), G->RegenerateLevel());
 TestEqual(TEXT("only terrain remains"), G->Realizer->Actors.Num(), 1);
 G->SpawnEnemies = true; G->SpawnBiomeEnemies = true;
 for (const FName Biome : {FName(TEXT("Forest")), FName(TEXT("City"))})
 {
  Config->BiomeId = Biome;
  TestTrue(TEXT("biome without unique enemies generates"), G->RegenerateLevel());
  TestEqual(TEXT("shared-only biome has no unique enemies"), Count(ANemesisBirdFlock::StaticClass()) +
      Count(ANemesisBeacon::StaticClass()) + Count(ANemesisTowerCannon::StaticClass()), 0);
 }
 auto* AuthoredAirship = World->SpawnActor<ANemesisAirship>();
 TestTrue(TEXT("authored airship level generates"), G->RegenerateLevel());
 TestEqual(TEXT("authored airship is reused rather than duplicated"), Count(ANemesisAirship::StaticClass()), 0);
 G->ClearGeneratedLevel();
 TestEqual(TEXT("clear removes generated actors"), G->Realizer->Actors.Num(), 0);
 TestTrue(TEXT("player survives cleanup"), IsValid(Pawn));
 TestTrue(TEXT("authored airship survives cleanup"), IsValid(AuthoredAirship));
 World->DestroyWorld(false);
 return true;
}
#endif
