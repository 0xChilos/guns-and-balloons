#include "../BalloonLevel/BalloonLevelGenerator.h"
#include "../BalloonLevel/BalloonLevelRealizer.h"
#include "../BalloonLevel/BalloonPlacementPlanner.h"
#include "../BalloonFlightComponent.h"
#include "../NemesisAirship.h"
#include "../NemesisBirdFlock.h"
#include "Components/BoxComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Engine/Engine.h"
#include "Materials/Material.h"
#include "MaterialDomain.h"
#include <limits>
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBalloonLevelStartOptionsTest, "GunsAndBalloons.BalloonLevel.StartOptions",
 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBalloonLevelStartOptionsTest::RunTest(const FString& Parameters)
{
 FBalloonLevelStartOptions Options; TArray<FString> Errors;
 auto* Instance = NewObject<UGameInstance>();
 auto* Launch = NewObject<UBalloonLevelLaunchSubsystem>(Instance);
 FText LaunchError;
 TestFalse(TEXT("start rejects missing running world"), Launch->StartLevel(Options, TSoftObjectPtr<UWorld>(), LaunchError));
 TestFalse(TEXT("start reports failure"), LaunchError.IsEmpty());
 TestFalse(TEXT("rejected start does not queue travel"), Launch->Pending);
 TestTrue(TEXT("queue valid setup"), Launch->QueueLevel(Options));
 FBalloonLevelStartOptions Received;
 TestTrue(TEXT("consume launch"), Launch->Consume(Received)); TestFalse(TEXT("launch only once"), Launch->Consume(Received));
 TestEqual(TEXT("height survives launch handoff"),Received.FlightHeight,Options.FlightHeight);
 Options.LevelWidth = std::numeric_limits<float>::quiet_NaN();
 TestFalse(TEXT("start validates settings independently of menu"), Launch->StartLevel(Options, TSoftObjectPtr<UWorld>(), LaunchError));
 TestFalse(TEXT("reject NaN"), Launch->QueueLevel(Options));
 Options.LevelWidth = 180.f; Options.Biome = TEXT("Unknown"); TestFalse(TEXT("reject unknown biome"), Options.Validate(Errors));
 Options.Biome = TEXT("Field"); Options.Cycle = EBalloonLightingPreset::Night;
 Options.FlightHeight = 120.f;
 Options.DistanceToNemesis = 350.f;
 auto* W = UWorld::CreateWorld(EWorldType::Game, false);
 auto* Balloon = W->SpawnActor<AActor>(); auto* Hull = NewObject<UBoxComponent>(Balloon);
 Balloon->SetRootComponent(Hull); Balloon->AddInstanceComponent(Hull); Hull->SetBoxExtent(FVector(100.,100.,400.));
 Hull->SetCollisionProfileName(TEXT("BlockAllDynamic")); Hull->RegisterComponent();
 auto* Flight = NewObject<UBalloonFlightComponent>(Balloon); Balloon->AddInstanceComponent(Flight); Flight->RegisterComponent();
 auto* G = W->SpawnActor<ABalloonLevelGenerator>(); G->RoutePawn = Balloon;
 G->UseStartOptions = true; G->StartOptions = Options; G->EdgeAACount = G->BirdFlockCount = 2;
 G->OverrideHeights = true; G->FlightHeightMeters = 999.f; G->OverrideLightingPreset = true;
 auto* C = NewObject<UBalloonLevelConfig>(); C->BiomeId = TEXT("Field"); C->LengthMeters = 400.f;
 C->MaxHeadingDegrees = 0.f; C->TerrainMode = EBalloonTerrainMode::Flat; C->TerrainMaterial = UMaterial::GetDefaultMaterial(MD_Surface);
 auto* Empty = NewObject<UDataTable>(); Empty->RowStruct = FBalloonLevelAssetRow::StaticStruct();
 C->SharedAssetTable = C->BiomeAssetTable = Empty; G->RandomBiomeConfigs.Add(C);
 C->EnemiesEnabled=true; C->EnemyDensity=.5f; C->ObstacleDensity=.75f; C->ItemDensity=.5f;
 if (!TestTrue(TEXT("generate from options without selected config"), G->GenerateLevel())) { W->DestroyWorld(false); return false; }
 TestEqual(TEXT("biome"), G->GetPlan().BiomeId, Options.Biome);
 TestTrue(TEXT("fixed night wins over legacy override"), G->ActiveConfig->Preset == EBalloonLightingPreset::Night);
 TestEqual(TEXT("balloon height in cm"), Balloon->GetActorLocation().Z, 12000.);
 TestEqual(TEXT("width in cm"), G->GetWidthAtDistanceCm(0.f), 18000.f);
 TestEqual(TEXT("authored enemy density retained"), G->ActiveConfig->EnemyDensity, C->EnemyDensity);
 TestEqual(TEXT("authored obstacle density retained"), G->ActiveConfig->ObstacleDensity, C->ObstacleDensity);
 TestEqual(TEXT("source width unchanged"), C->BaseWidthMeters, 600.f);
 TestEqual(TEXT("source height unchanged"), C->FlightHeightMeters, 100.f);
 TestEqual(TEXT("item density applied to active config"), G->ActiveConfig->ItemDensity, .5f);
 TestEqual(TEXT("source item density unchanged"), C->ItemDensity, .5f);
 int32 Birds = 0; ANemesisAirship* Airship = nullptr;
 for (const auto& Ref : G->Realizer->Actors)
 {
  if (Ref.IsValid() && Ref->IsA<ANemesisBirdFlock>()) ++Birds;
  if (auto* Found = Cast<ANemesisAirship>(Ref.Get())) Airship = Found;
 }
 TestEqual(TEXT("scaled biome enemies"), Birds, 1);
 TestNotNull(TEXT("single nemesis retained at positive density"), Airship);
 if (Airship) TestTrue(TEXT("nemesis distance"), FMath::IsNearlyEqual(FVector::Dist2D(Airship->GetActorLocation(), G->GetStartTransform().GetLocation()), 35000., .1));
 const auto Previous = G->Realizer->Actors;
 G->StartOptions.LevelWidth = -1.f; TestFalse(TEXT("invalid start rejected"), G->RegenerateLevel());
 TestTrue(TEXT("failure preserves realized actors"), Previous == G->Realizer->Actors);
 G->StartOptions.LevelWidth = Options.LevelWidth; G->SpawnEnemies=false; TestTrue(TEXT("zero enemies"), G->RegenerateLevel());
 for (const auto& Ref : G->Realizer->Actors) if (Ref.IsValid()) TestFalse(TEXT("no generated enemies at zero"), Ref->ActorHasTag(TEXT("BalloonLevel.Enemy")));
 // Regression: low, narrow Start Options must still populate a colliding forest.
 auto* Forest = LoadObject<UBalloonLevelConfig>(nullptr,
  TEXT("/Game/Blueprints/Tests/Environment/DA_Environment_Forest_Day.DA_Environment_Forest_Day"));
 if (TestNotNull(TEXT("authored forest config exists"), Forest))
 {
  G->LevelConfig=Forest;
  G->StartOptions.Biome=TEXT("Forest"); G->StartOptions.FlightHeight=35.f;
  G->StartOptions.LevelWidth=100.f;
  G->SpawnEnemies=false;
  for (int32 Seed : {30068,12345,42})
  {
   auto* Snapshot=DuplicateObject<UBalloonLevelConfig>(Forest,G);
   Snapshot->Seed=Seed; G->LevelConfig=Snapshot;
   const bool Built=G->GenerateLevel();
   TestTrue(FString::Printf(TEXT("low narrow forest seed %d generates"),Seed),Built);
   if (!Built) for (const auto& Message:G->GetDiagnostics()) AddInfo(Message);
   if (Built) TestTrue(TEXT("forest contains scenery and obstacles"),G->GetPlan().Placements.Num()>0);
  }
 }
 // Test placement budgets separately from the generated enemy actors.
 BalloonPlacementPlanning::FResolvedRow Row;
 Row.Entry.RowName = TEXT("DensityTest"); Row.Entry.Row.Category = TEXT("Obstacle"); Row.Entry.Row.Rule = TEXT("scatter");
 Row.Entry.Row.DensityPerKm2 = 1.f; Row.Entry.Row.Asset = FSoftObjectPath(TEXT("/Engine/BasicShapes/Cube.Cube"));
 Row.Metadata.SizeMeters = FVector(1.);
 C->ObstaclesEnabled = true; C->ObstacleExactCount = 8; C->ObstacleDensity = .5f;
 auto Plan = G->GetPlan(); Errors.Reset();
 TestTrue(TEXT("scaled exact obstacle count"), BalloonPlacementPlanning::Build(*C, Plan, {Row}, Errors));
 TestEqual(TEXT("half obstacle budget"), Plan.Placements.Num(), 4);
 C->ObstacleDensity = 0.f; TestTrue(TEXT("disable obstacles"), BalloonPlacementPlanning::Build(*C, Plan, {Row}, Errors));
 TestEqual(TEXT("no obstacles"), Plan.Placements.Num(), 0);
 W->DestroyWorld(false); return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBalloonLevelLaunchMethodTest, "GunsAndBalloons.BalloonLevel.StartMethod",
 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBalloonLevelLaunchMethodTest::RunTest(const FString& Parameters)
{
 auto* Instance = NewObject<UGameInstance>(GEngine); Instance->InitializeStandalone();
 auto* Launch = Instance->GetSubsystem<UBalloonLevelLaunchSubsystem>();
 FBalloonLevelStartOptions Options; Options.Biome = TEXT("Desert"); Options.DistanceToNemesis = 400.f;
 FText Error;
 TestTrue(TEXT("method requests default map travel"), Launch->StartLevel(Options, TSoftObjectPtr<UWorld>(), Error));
 TestTrue(TEXT("success clears error"), Error.IsEmpty());
 TestTrue(TEXT("options queued"), Launch->Pending);
 TestEqual(TEXT("menu-independent options"), Launch->Options.DistanceToNemesis, 400.f);
 const FString TravelURL = Instance->GetWorldContext()->TravelURL;
 TestTrue(TEXT("default MainLevel target"), TravelURL.Contains(TEXT("/Game/Scenes/MainLevel")));
 TestFalse(TEXT("missing map rejected"), Launch->StartLevel(Options,
  TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/MissingLevelStartTestMap.MissingLevelStartTestMap"))), Error));
 TestFalse(TEXT("missing map error"), Error.IsEmpty());
 TestTrue(TEXT("rejected request preserves pending options"), Launch->Pending);
 TestEqual(TEXT("rejected request preserves travel"), Instance->GetWorldContext()->TravelURL, TravelURL);
 Instance->GetWorldContext()->TravelURL.Empty();
 auto* World = Instance->GetWorld(); Instance->Shutdown();
 GEngine->DestroyWorldContext(World); World->DestroyWorld(false);
 return true;
}
#endif
