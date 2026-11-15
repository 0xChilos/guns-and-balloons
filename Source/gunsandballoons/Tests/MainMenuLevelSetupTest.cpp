#include "../MainMenuWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/ComboBoxString.h"
#include "Components/SpinBox.h"
#include "Components/WidgetSwitcher.h"
#include "Engine/GameInstance.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMainMenuLevelSetupTest, "GunsAndBalloons.Menu.LevelSetup",
 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMainMenuLevelSetupTest::RunTest(const FString& Parameters)
{
 auto* Instance = NewObject<UGameInstance>();
 auto* Menu = NewObject<UMainMenuWidget>(Instance);
 TestTrue(TEXT("initialize menu"), Menu->Initialize());
 const auto SlateWidget = Menu->TakeWidget();
 auto* Pages = Cast<UWidgetSwitcher>(Menu->WidgetTree->FindWidget(TEXT("Pages")));
 if (!TestNotNull(TEXT("menu pages"), Pages)) return false;
 TestEqual(TEXT("setup page available"), Pages->GetNumWidgets(), 4);
 TestEqual(TEXT("first level"), Menu->NewRunLevel.ToSoftObjectPath().GetLongPackageName(), FString(TEXT("/Game/Scenes/MainLevel")));
 Menu->ActivateItem(1); TestEqual(TEXT("new run opens setup"), Pages->GetActiveWidgetIndex(), 3);
 TArray<UWidget*> Widgets; Menu->WidgetTree->GetAllWidgets(Widgets);
 int32 ComboCount = 0, NumberCount = 0;
 for (UWidget* Widget : Widgets)
 {
  if (auto* Combo = Cast<UComboBoxString>(Widget))
  {
   Combo->SetSelectedOption(ComboCount == 0 ? TEXT("Desert") : TEXT("Night")); ++ComboCount;
  }
  if (auto* Number = Cast<USpinBox>(Widget)) { Number->SetValue(NumberCount == 0 ? 150.f : NumberCount == 2 ? 350.f : Number->GetValue()); ++NumberCount; }
 }
 TestEqual(TEXT("biome and cycle controls"), ComboCount, 2); TestEqual(TEXT("three numeric controls"), NumberCount, 3);
 // This fixture has no running world, so the launch method reports an error after field collection.
 Menu->NewRunLevel.Reset(); Menu->StartConfiguredLevel();
 TestEqual(TEXT("reads chosen biome"), Menu->LevelStartOptions.Biome, FName(TEXT("Desert")));
 TestTrue(TEXT("reads fixed night"), Menu->LevelStartOptions.Cycle == EBalloonLightingPreset::Night);
 TestEqual(TEXT("reads numeric height"), Menu->LevelStartOptions.FlightHeight, 150.f);
 TestEqual(TEXT("reads nemesis distance"), Menu->LevelStartOptions.DistanceToNemesis, 350.f);
 Menu->ShowItemList(); TestEqual(TEXT("back restores list"), Pages->GetActiveWidgetIndex(), 0);
 Menu->ActivateItem(1); TestEqual(TEXT("reopen setup"), Pages->GetActiveWidgetIndex(), 3);
 Menu->ReleaseSlateResources(true);
 return true;
}
#endif
