#include "../BalloonLevel/BalloonLevelGenerator.h"
#include "Engine/World.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Engine/ExponentialHeightFog.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBalloonLevelControllerTest, "GunsAndBalloons.BalloonLevel.Controller",
 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBalloonLevelControllerTest::RunTest(const FString& Parameters)
{
 auto* World = UWorld::CreateWorld(EWorldType::Game, false);
 auto* Generator = World->SpawnActor<ABalloonLevelGenerator>();
 auto* ExistingSun = World->SpawnActor<ADirectionalLight>();
 Generator->SetupLevelController();
 TestTrue(TEXT("existing sun reused"), Generator->Sun == ExistingSun);
 TestNotNull(TEXT("environment created"), Generator->EnvironmentController.Get());
 TestNotNull(TEXT("sky created"), Generator->Sky.Get());
 TestNotNull(TEXT("fog created"), Generator->Fog.Get());
 const auto Environment = Generator->EnvironmentController;
 const auto Sky = Generator->Sky;
 const auto Fog = Generator->Fog;
 TestTrue(TEXT("sun grouped under controller"), ExistingSun->GetAttachParentActor() == Generator);
 TestTrue(TEXT("environment grouped under controller"), Environment->GetAttachParentActor() == Generator);
 Generator->SetupLevelController();
 TestTrue(TEXT("setup is idempotent"), Generator->EnvironmentController == Environment && Generator->Sky == Sky && Generator->Fog == Fog);
 auto* Config = NewObject<UBalloonLevelConfig>();
 Config->LengthMeters = 0.f; // Inspect the snapshot without needing a live route pawn.
 Generator->LevelConfig = Config;
 Generator->OverrideHeights = true; Generator->FlightHeightMeters = 120.f; Generator->GroundHeightMeters = -30.f;
 Generator->OverrideTerrain = true; Generator->TerrainHeightVariationMeters = 25.f;
 Generator->OverrideLightingPreset = true; Generator->LightingPreset = EBalloonLightingPreset::Night;
 Generator->GenerateLevel();
 TestEqual(TEXT("controller flight height applied"), Generator->ActiveConfig->FlightHeightMeters, 120.f);
 TestEqual(TEXT("controller ground height applied"), Generator->ActiveConfig->PlacementGroundZMeters, -30.f);
 TestEqual(TEXT("controller terrain applied"), Generator->ActiveConfig->TerrainHeightVariationMeters, 25.f);
 TestTrue(TEXT("controller preset applied"), Generator->ActiveConfig->Preset == EBalloonLightingPreset::Night);
 TestEqual(TEXT("source asset unchanged"), Config->FlightHeightMeters, 100.f);
 TestEqual(TEXT("source terrain unchanged"), Config->TerrainHeightVariationMeters, 15.f);
 Generator->Destroy();
 TestTrue(TEXT("authored light survives controller deletion"), IsValid(ExistingSun));
 TestFalse(TEXT("owned environment removed"), IsValid(Environment));
 TestFalse(TEXT("owned sky removed"), IsValid(Sky));
 TestFalse(TEXT("owned fog removed"), IsValid(Fog));
 World->DestroyWorld(false);
 return true;
}
#endif
