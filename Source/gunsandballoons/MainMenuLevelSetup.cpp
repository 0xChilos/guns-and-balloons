#include "MainMenuWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ComboBoxString.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/SpinBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WidgetSwitcher.h"
#include "Engine/GameInstance.h"

void UMainMenuWidget::BuildLevelStartPage()
{
 auto* Page = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("LevelStartPage"));
 Pages->AddChild(Page);
 FSlateFontInfo Font = PageFont; Font.Size = 32;
 auto* Title = MakeText(FText::FromString(TEXT("START LEVEL")), Font, TextColor);
 Page->AddChildToVerticalBox(Title)->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));
 auto AddField = [&](const TCHAR* Label, UWidget* Control)
 {
  auto* Height = WidgetTree->ConstructWidget<USizeBox>(); Height->SetHeightOverride(58.f);
  auto* Row = WidgetTree->ConstructWidget<UHorizontalBox>(); Height->SetContent(Row);
  auto* LabelSlot = Row->AddChildToHorizontalBox(MakeText(FText::FromString(Label), Font, TextColor));
  LabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); LabelSlot->SetVerticalAlignment(VAlign_Center);
  auto* ControlWidth = WidgetTree->ConstructWidget<USizeBox>(); ControlWidth->SetWidthOverride(350.f); ControlWidth->SetContent(Control);
  auto* ControlSlot = Row->AddChildToHorizontalBox(ControlWidth); ControlSlot->SetVerticalAlignment(VAlign_Center);
  Page->AddChildToVerticalBox(Height);
 };
 auto MakeChoice = [&]()
 {
  auto* Choice = WidgetTree->ConstructWidget<UComboBoxString>();
  Choice->OnGenerateWidgetEvent.BindDynamic(this, &UMainMenuWidget::MakeLevelChoiceText);
  Choice->SetMaxListHeight(300.f);
  auto Style = Choice->GetWidgetStyle();
  Style.ComboButtonStyle.ButtonStyle.Normal.TintColor = FSlateColor(FLinearColor(.025f, .035f, .03f, 1.f));
  Style.ComboButtonStyle.ButtonStyle.Hovered.TintColor = FSlateColor(FLinearColor(.08f, .07f, .04f, 1.f));
  Style.ComboButtonStyle.ButtonStyle.Pressed.TintColor = Style.ComboButtonStyle.ButtonStyle.Hovered.TintColor;
  Choice->SetWidgetStyle(Style);
  return Choice;
 };
 BiomeChoice = MakeChoice();
 for (const TCHAR* Biome : {TEXT("Forest"), TEXT("Field"), TEXT("City"), TEXT("Desert")}) BiomeChoice->AddOption(Biome);
 BiomeChoice->SetSelectedOption(LevelStartOptions.Biome.ToString()); AddField(TEXT("BIOME"), BiomeChoice);
 CycleChoice = MakeChoice(); CycleChoice->AddOption(TEXT("Day")); CycleChoice->AddOption(TEXT("Night"));
 CycleChoice->SetSelectedOption(LevelStartOptions.Cycle == EBalloonLightingPreset::Night ? TEXT("Night") : TEXT("Day"));
 AddField(TEXT("CYCLE"), CycleChoice);
 LevelNumbers.Reset();
 // Prototype UI drag ranges/steps require tuning. Values are validated again before launch/generation.
 auto AddNumber = [&](const TCHAR* Label, float Value, float Min, float Max, float Step)
 {
  auto* Number = WidgetTree->ConstructWidget<USpinBox>(); Number->SetFont(Font);
  auto Style = Number->GetWidgetStyle();
  Style.BackgroundBrush.TintColor = FSlateColor(FLinearColor(.025f, .035f, .03f, 1.f));
  Style.HoveredBackgroundBrush.TintColor = FSlateColor(FLinearColor(.08f, .07f, .04f, 1.f));
  Style.ActiveBackgroundBrush.TintColor = Style.HoveredBackgroundBrush.TintColor;
  Style.InactiveFillBrush.DrawAs = ESlateBrushDrawType::NoDrawType;
  Style.HoveredFillBrush.DrawAs = ESlateBrushDrawType::NoDrawType;
  Style.ActiveFillBrush.DrawAs = ESlateBrushDrawType::NoDrawType;
  Number->SetWidgetStyle(Style);
  Number->SetForegroundColor(TextColor); Number->SetMinValue(Min); Number->SetMaxValue(Max);
  Number->SetMinSliderValue(Min); Number->SetMaxSliderValue(Max); Number->SetDelta(Step);
  Number->SetMinFractionalDigits(0); Number->SetMaxFractionalDigits(3); Number->SetValue(Value);
  LevelNumbers.Add(Number); AddField(Label, Number);
 };
 AddNumber(TEXT("FLIGHT HEIGHT (m)"), LevelStartOptions.FlightHeight, 0.f, 1000.f, 10.f);
 AddNumber(TEXT("LEVEL WIDTH (m)"), LevelStartOptions.LevelWidth, 1.f, 2000.f, 10.f);
 AddNumber(TEXT("ENEMY DENSITY (x)"), LevelStartOptions.EnemyDensity, 0.f, 10.f, .25f);
 AddNumber(TEXT("OBSTACLE DENSITY (x)"), LevelStartOptions.ObstacleDensity, 0.f, 10.f, .25f);
 AddNumber(TEXT("FOG DENSITY"), LevelStartOptions.FogDensity, 0.f, 10.f, .005f);
 AddNumber(TEXT("DISTANCE TO NEMESIS (m)"), LevelStartOptions.DistanceToNemesis, 0.f, 5000.f, 25.f);
 AddNumber(TEXT("ITEM DENSITY (%)"), LevelStartOptions.ItemDensity * 100.f, 0.f, 100.f, 10.f);
 LevelStartError = MakeText(FText::GetEmpty(), Font, SelectedColor);
 LevelStartError->SetAutoWrapText(true); Page->AddChildToVerticalBox(LevelStartError);
 const float PreviousHeight = RowHeight; const auto PreviousFont = ItemFont;
 RowHeight = 58.f; ItemFont = Font;
 StartLevelRow = MakeRow(Page, FText::FromString(TEXT("START LEVEL")), EMainMenuRowKind::StartLevel, INDEX_NONE);
 LevelBackRow = MakeRow(Page, BackLabel, EMainMenuRowKind::Back, INDEX_NONE);
 RowHeight = PreviousHeight; ItemFont = PreviousFont;
}

void UMainMenuWidget::ShowLevelStartPage()
{
 if (!Pages || !PageWidth) return;
 PageWidth->SetWidthOverride(1020.f);
 if (auto* PageSlot = Cast<UCanvasPanelSlot>(PageWidth->Slot)) PageSlot->SetPosition(FVector2D(ListPosition.X, 330.f));
 LevelStartError->SetText(FText::GetEmpty()); Pages->SetActiveWidgetIndex(3);
}

void UMainMenuWidget::StartConfiguredLevel()
{
 if (!BiomeChoice || !CycleChoice || LevelNumbers.Num() != 7) return;
 LevelStartOptions.Biome = FName(*BiomeChoice->GetSelectedOption());
 LevelStartOptions.Cycle = CycleChoice->GetSelectedOption() == TEXT("Night") ? EBalloonLightingPreset::Night : EBalloonLightingPreset::Day;
 LevelStartOptions.FlightHeight = LevelNumbers[0]->GetValue(); LevelStartOptions.LevelWidth = LevelNumbers[1]->GetValue();
 LevelStartOptions.EnemyDensity = LevelNumbers[2]->GetValue(); LevelStartOptions.ObstacleDensity = LevelNumbers[3]->GetValue();
 LevelStartOptions.FogDensity = LevelNumbers[4]->GetValue(); LevelStartOptions.DistanceToNemesis = LevelNumbers[5]->GetValue();
 LevelStartOptions.ItemDensity = LevelNumbers[6]->GetValue() / 100.f;
 auto* Launch = GetGameInstance() ? GetGameInstance()->GetSubsystem<UBalloonLevelLaunchSubsystem>() : nullptr;
 FText Error;
 if (!Launch) Error = FText::FromString(TEXT("Level launch is unavailable."));
 else Launch->StartLevel(LevelStartOptions, NewRunLevel, Error);
 LevelStartError->SetText(Error);
}

UWidget* UMainMenuWidget::MakeLevelChoiceText(FString Value)
{
 auto Font = PageFont; Font.Size = 32;
 return MakeText(FText::FromString(Value), Font, TextColor);
}
