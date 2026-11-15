#include "../BalloonLevel/BalloonLevelGenerator.h"
#include "Engine/World.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBalloonLevelModesTest, "GunsAndBalloons.BalloonLevel.GenerationModes",
 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBalloonLevelModesTest::RunTest(const FString& Parameters)
{
 UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
 auto* Generator = World->SpawnActor<ABalloonLevelGenerator>();
 for (const FName Button : {FName(TEXT("GenerateLevelInEditor")), FName(TEXT("RegenerateLevelInEditor"))})
 {
  UFunction* Function = Generator->FindFunction(Button);
  TestNotNull(TEXT("Editor button reflected"), Function);
  if (Function)
  {
   TestEqual(TEXT("Editor button has no parameter/return storage"), int32(Function->ParmsSize), 0);
#if WITH_METADATA
   TestTrue(TEXT("Editor button enabled"), Function->GetBoolMetaData(TEXT("CallInEditor")));
#endif
  }
 }
 auto* Forest = NewObject<UBalloonLevelConfig>();
 Forest->BiomeId = TEXT("Forest");
 // Deliberately invalid geometry: selection is tested without requiring a live balloon.
 Forest->LengthMeters = 0.f;
 Generator->LevelConfig = Forest;
 TestTrue(TEXT("Manual is the compatible default"), Generator->GenerationMode == EBalloonGenerationMode::Manual);
 Generator->GenerateLevel();
 TestNotNull(TEXT("Selected config copied"), Generator->ActiveConfig.Get());
 TestTrue(TEXT("Source asset not reused"), Generator->ActiveConfig != Forest);
 TestEqual(TEXT("Authored seed retained"), Generator->ActiveConfig->Seed, Forest->Seed);
 Generator->BiomeSelection = EBalloonBiomeSelection::Random;
 Generator->GenerateLevel();
 TestTrue(TEXT("Empty random list rejected"), Generator->GetGenerationStatus() == EBalloonLevelGenerationStatus::InvalidConfig);
 TestNull(TEXT("Stale active config cleared"), Generator->ActiveConfig.Get());
 auto* Desert = NewObject<UBalloonLevelConfig>();
 Desert->BiomeId = TEXT("Desert");
 Desert->LengthMeters = 0.f;
 Generator->RandomBiomeConfigs.Add(nullptr);
 Generator->RandomBiomeConfigs.Add(Desert);
 Generator->RandomBiomeConfigs.Add(Desert);
 Generator->RandomizeSeed = true;
 Generator->GenerateLevel();
 TestEqual(TEXT("Random selects pool config instead of fixed config"), Generator->ActiveConfig->BiomeId, Desert->BiomeId);
 TestEqual(TEXT("Randomization does not mutate source seed"), Desert->Seed, 12345);
 Generator->ClearGeneratedLevel();
 TestNull(TEXT("Clear releases snapshot"), Generator->ActiveConfig.Get());
 auto* Automatic = World->SpawnActor<ABalloonLevelGenerator>();
 Automatic->LevelConfig = Forest;
 Automatic->GenerationMode = EBalloonGenerationMode::OnLevelStart;
 Automatic->DispatchBeginPlay();
 TestNotNull(TEXT("Level start automatically attempts generation"), Automatic->ActiveConfig.Get());
 Generator->DispatchBeginPlay();
 TestTrue(TEXT("Manual level start leaves generation untouched"), Generator->GetGenerationStatus() == EBalloonLevelGenerationStatus::NotStarted);
 World->DestroyWorld(false);
 return true;
}
#endif
