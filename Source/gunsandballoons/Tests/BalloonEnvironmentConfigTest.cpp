#include "../BalloonLevel/BalloonEnvironmentPresetController.h"
#include "../BalloonLevel/BalloonLevelGenerator.h"
#include "Engine/World.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "../BalloonFlightComponent.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBalloonEnvironmentConfigTest,
 "GunsAndBalloons.BalloonLevel.EnvironmentConfig",
 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBalloonEnvironmentConfigTest::RunTest(const FString& Parameters)
{
 UWorld* World = UWorld::CreateWorld(EWorldType::Editor, false);
 auto* Generator = World->SpawnActor<ABalloonLevelGenerator>();
 auto* Environment = World->SpawnActor<ABalloonEnvironmentPresetController>();
 Environment->Generator = Generator;
 Environment->FallbackBiomeId = TEXT("Forest");
 auto* Config = NewObject<UBalloonLevelConfig>(Generator);
 Generator->LevelConfig = Config;
 Config->BiomeId = TEXT("Desert");
 Config->Preset = EBalloonLightingPreset::Night;
 TestTrue(TEXT("Preview reads selected config before generation"), Environment->ApplyFromGenerator());
 TestEqual(TEXT("Biome comes from config, not fallback"), Environment->GetAppliedBiome(), FName(TEXT("Desert")));
 TestTrue(TEXT("Night comes from config"), Environment->GetSelectedLightingPreset() == EBalloonLightingPreset::Night);
 TestNotNull(TEXT("Night sky material loaded"), Environment->SkyDome->GetMaterial(0));
 TestEqual(TEXT("Night sky matches biome"), Environment->SkyDome->GetMaterial(0)->GetName(), FString(TEXT("MI_Desert_NightSky")));
 TestTrue(TEXT("Fallback dome visible without authored sky"), Environment->SkyDome->IsVisible());
 TestTrue(TEXT("Sky never blocks flight"), Environment->SkyDome->GetCollisionEnabled() == ECollisionEnabled::NoCollision);
 Config->BiomeId = TEXT("City");
 Config->Preset = EBalloonLightingPreset::Day;
 Environment->PreviewEnvironment();
 TestEqual(TEXT("Preview follows edited config"), Environment->GetAppliedBiome(), FName(TEXT("City")));
 TestTrue(TEXT("Preview follows edited preset"), Environment->GetSelectedLightingPreset() == EBalloonLightingPreset::Day);
 TestEqual(TEXT("Preview switches sky with biome and lighting"), Environment->SkyDome->GetMaterial(0)->GetName(), FString(TEXT("MI_City_DaySky")));
 Generator->LevelConfig = nullptr;
 TestFalse(TEXT("Missing config does not silently use fallback"), Environment->ApplyFromGenerator());
 Generator->BiomeSelection = EBalloonBiomeSelection::Random;
 TestFalse(TEXT("Random waits for successful generation"), Environment->ApplyFromGenerator());
 // The shared generation API must apply the environment without a separate preview call.
 auto* Pawn = World->SpawnActor<AActor>();
 auto* Hull = NewObject<UBoxComponent>(Pawn);
 Pawn->SetRootComponent(Hull);
 Pawn->AddInstanceComponent(Hull);
 Hull->SetBoxExtent(FVector(100., 100., 400.));
 Hull->SetCollisionProfileName(TEXT("BlockAllDynamic"));
 Hull->RegisterComponent();
 Pawn->SetActorLocation(FVector(0., 0., 10000.));
 auto* Flight = NewObject<UBalloonFlightComponent>(Pawn);
 Pawn->AddInstanceComponent(Flight);
 Flight->RegisterComponent();
 Generator->RoutePawn = Pawn;
 Config->StartTransform.SetLocation(Pawn->GetActorLocation());
 Config->LengthMeters = 100.;
 Config->MaxHeadingDegrees = 0.;
 Config->SceneryEnabled = false;
 Config->BiomeId = TEXT("Field");
 Config->Preset = EBalloonLightingPreset::Night;
 Generator->RandomBiomeConfigs = {Config};
 TestTrue(TEXT("Direct generation succeeds"), Generator->GenerateLevel());
 TestEqual(TEXT("Generation automatically applies random-selected biome"), Environment->GetAppliedBiome(), FName(TEXT("Field")));
 TestTrue(TEXT("Generation automatically applies Night"), Environment->GetSelectedLightingPreset() == EBalloonLightingPreset::Night);
 Config->BiomeId = TEXT("Forest");
 Config->Preset = EBalloonLightingPreset::Day;
 TestTrue(TEXT("Direct regeneration succeeds"), Generator->RegenerateLevel());
 TestEqual(TEXT("Regeneration refreshes editor environment"), Environment->GetAppliedBiome(), FName(TEXT("Forest")));
 Config->BiomeId = TEXT("Desert");
 Config->LengthMeters = 0.;
 TestFalse(TEXT("Invalid regeneration rejected"), Generator->RegenerateLevel());
 TestEqual(TEXT("Failed regeneration preserves environment"), Environment->GetAppliedBiome(), FName(TEXT("Forest")));
 World->DestroyWorld(false);
 return true;
}
#endif
