#include "MainMenuWidget.h"

#include "BalloonPlayerController.h"
#include "BalloonRunSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/AudioComponent.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/SizeBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WidgetSwitcher.h"
#include "Engine/Texture2D.h"
#include "TextureResource.h"
#include "GeneralProjectSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Sound/SoundBase.h"
#include "Styling/CoreStyle.h"

#define LOCTEXT_NAMESPACE "MainMenu"

namespace
{
	FMainMenuItem MakeItem(const FText& Label, EMainMenuAction Action)
	{
		FMainMenuItem Item;
		Item.Label = Label;
		Item.Action = Action;
		return Item;
	}

	FMainMenuToggle MakeToggle(const FText& Label, EGameOption Option, const FText& OnText, const FText& OffText)
	{
		FMainMenuToggle Toggle;
		Toggle.Label = Label;
		Toggle.Option = Option;
		Toggle.OnText = OnText;
		Toggle.OffText = OffText;
		return Toggle;
	}

	FSlateFontInfo MakeFont(int32 Size, int32 LetterSpacing)
	{
		FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Bold", Size);
		Font.LetterSpacing = LetterSpacing;
		return Font;
	}

	/** A plain box of one colour (a brush with no image draws a solid quad). */
	FSlateBrush SolidBrush(const FVector2D& Size)
	{
		FSlateBrush Brush;
		Brush.ImageSize = Size;
		return Brush;
	}

	FSlateBrush TextureBrush(UTexture2D* Texture, const FVector2D& Size)
	{
		FSlateBrush Brush;
		Brush.SetResourceObject(Texture);
		Brush.ImageSize = Size;
		return Brush;
	}

	/** The arrow cursor: 'O' outline, 'X' fill, '.' clear. Hotspot = top-left pixel. */
	const TCHAR* const CursorRows[] = {
		TEXT("O......"),
		TEXT("OO....."),
		TEXT("OXO...."),
		TEXT("OXXO..."),
		TEXT("OXXXO.."),
		TEXT("OXXXXO."),
		TEXT("OXXXXXO"),
		TEXT("OXXOOOO"),
		TEXT("OXO...."),
		TEXT("OO....."),
	};
	constexpr int32 CursorWidth = 7;
	constexpr int32 CursorHeight = UE_ARRAY_COUNT(CursorRows);
}

UTexture2D* UMainMenuWidget::MakeRuntimeTexture(int32 Width, int32 Height, TextureFilter Filter, TFunctionRef<FColor(int32, int32)> Pixel)
{
	UTexture2D* Texture = UTexture2D::CreateTransient(Width, Height, PF_B8G8R8A8);
	if (!Texture)
	{
		return nullptr;
	}
	Texture->Filter = Filter;
	Texture->SRGB = true;
	FTexture2DMipMap& Mip = Texture->GetPlatformData()->Mips[0];
	FColor* Pixels = static_cast<FColor*>(Mip.BulkData.Lock(LOCK_READ_WRITE));
	for (int32 Y = 0; Y < Height; ++Y)
	{
		for (int32 X = 0; X < Width; ++X)
		{
			Pixels[Y * Width + X] = Pixel(X, Y);
		}
	}
	Mip.BulkData.Unlock();
	Texture->UpdateResource();
	return Texture;
}

void UMainMenuClickRelay::HandleClicked()
{
	if (UMainMenuWidget* Owner = Menu.Get())
	{
		switch (Kind)
		{
		case EMainMenuRowKind::Item: Owner->ActivateItem(Index); break;
		case EMainMenuRowKind::Back: Owner->ShowItemList(); break;
		case EMainMenuRowKind::Toggle: Owner->ToggleOption(Index); break;
		}
		// After the action, so switching SFX off is silent and switching it on clicks.
		Owner->PlayClick();
	}
}

UMainMenuWidget::UMainMenuWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Items = {
		MakeItem(LOCTEXT("Continue", "CONTINUE"), EMainMenuAction::Continue),
		MakeItem(LOCTEXT("NewRun", "NEW RUN"), EMainMenuAction::NewRun),
		MakeItem(LOCTEXT("Credits", "CREDITS"), EMainMenuAction::Credits),
		MakeItem(LOCTEXT("Options", "OPTIONS"), EMainMenuAction::Options),
		MakeItem(LOCTEXT("Quit", "QUIT"), EMainMenuAction::Quit),
	};
	const TSoftObjectPtr<USoundBase> ClockLoop(FSoftObjectPath(TEXT(
		"/Game/Audio/Main_Menu_Button_Click/daviddumaisaudio-steampunk-loopable-clock-12-188049."
		"daviddumaisaudio-steampunk-loopable-clock-12-188049")));
	ClickSound.Sound = ClockLoop;
	ClickSound.Start = 0.09f;
	ClickSound.Length = 0.34f;
	HoverSound.Sound = ClockLoop;
	HoverSound.Start = 1.12f;
	HoverSound.Length = 0.24f;
	NewRunLevel = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/Scenes/Movement_Scene1.Movement_Scene1")));
	MainMenuLevel = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/Scenes/MainMenu.MainMenu")));

	CreditsTitle = LOCTEXT("CreditsTitle", "CREDITS");
	CreditsText = LOCTEXT("CreditsText", "OLEKSII NAHORNIAK\nIVAN LUZHANSKIY\nIVAN PETRENKO\nROSTYSLAV IVLEV");
	OptionsTitle = LOCTEXT("OptionsTitle", "OPTIONS");
	const FText On = LOCTEXT("On", "ON");
	const FText Off = LOCTEXT("Off", "OFF");
	OptionToggles = {
		MakeToggle(LOCTEXT("Music", "MUSIC"), EGameOption::Music, On, Off),
		MakeToggle(LOCTEXT("Sfx", "SFX"), EGameOption::Sfx, On, Off),
		MakeToggle(LOCTEXT("Tutorial", "TUTORIAL"), EGameOption::Tutorial, LOCTEXT("Yes", "YES"), LOCTEXT("No", "NO")),
	};
	BackLabel = LOCTEXT("Back", "BACK");
	VersionSuffix = LOCTEXT("VersionSuffix", "RELEASE");
	GameTitle = LOCTEXT("GameTitle", "GUNS AND\nBALLOONS");
	GameSubtitle = LOCTEXT("GameSubtitle", "DIRECTOR'S CUT");

	ItemFont = MakeFont(44, 60);
	PageFont = MakeFont(30, 40);
	VersionFont = MakeFont(28, 80);
	GameTitleFont = MakeFont(92, 80);
	GameSubtitleFont = MakeFont(34, 260);
}

TSharedRef<SWidget> UMainMenuWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildTree();
	}
	return Super::RebuildWidget();
}

void UMainMenuWidget::MakeTextures()
{
	// Haze: alpha falls off smoothly from the left edge; HazeColor tints it.
	constexpr int32 HazeSteps = 256;
	HazeTexture = MakeRuntimeTexture(HazeSteps, 4, TF_Bilinear, [](int32 X, int32)
	{
		const float U = X / float(HazeSteps - 1);
		const float Fade = 1.f - FMath::SmoothStep(0.f, 1.f, U);
		return FColor(255, 255, 255, uint8(FMath::RoundToInt(255.f * FMath::Pow(Fade, 1.6f))));
	});

	const FColor Fill = CursorColor.ToFColor(true);
	const FColor Outline = CursorOutlineColor.ToFColor(true);
	CursorTexture = MakeRuntimeTexture(CursorWidth, CursorHeight, TF_Nearest, [Fill, Outline](int32 X, int32 Y)
	{
		switch (CursorRows[Y][X])
		{
		case 'O': return Outline;
		case 'X': return Fill;
		default: return FColor(0, 0, 0, 0);
		}
	});
}

UTextBlock* UMainMenuWidget::MakeText(const FText& Text, const FSlateFontInfo& Font, const FLinearColor& Color)
{
	UTextBlock* Block = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Block->SetText(Text);
	Block->SetFont(Font);
	Block->SetColorAndOpacity(FSlateColor(Color));
	Block->SetShadowOffset(ShadowOffset);
	Block->SetShadowColorAndOpacity(ShadowColor);
	return Block;
}

void UMainMenuWidget::BuildTree()
{
	Rows.Reset();
	ToggleRows.Reset();
	Relays.Reset();
	MakeTextures();

	Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	// Haze behind the items, full height, fading out to the right.
	UImage* Haze = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Haze"));
	Haze->SetBrush(TextureBrush(HazeTexture, FVector2D(HazeWidth, 4.f)));
	Haze->SetColorAndOpacity(HazeColor);
	Haze->SetVisibility(ESlateVisibility::HitTestInvisible);
	UCanvasPanelSlot* HazeSlot = Root->AddChildToCanvas(Haze);
	HazeSlot->SetAnchors(FAnchors(0.f, 0.f, 0.f, 1.f));
	HazeSlot->SetOffsets(FMargin(0.f, 0.f, HazeWidth, 0.f));

	// The item list, the Credits page and the Options page share one spot.
	USizeBox* Width = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("ListWidth"));
	Width->SetWidthOverride(ListWidth);
	Pages = WidgetTree->ConstructWidget<UWidgetSwitcher>(UWidgetSwitcher::StaticClass(), TEXT("Pages"));
	Width->SetContent(Pages);

	UVerticalBox* List = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ItemList"));
	Pages->AddChild(List);
	for (int32 Index = 0; Index < Items.Num(); ++Index)
	{
		Rows.Add(MakeRow(List, Items[Index].Label, EMainMenuRowKind::Item, Index));
	}

	UTextBlock* CreditsHeading = nullptr;
	UVerticalBox* Page = MakePage(TEXT("TextPage"), CreditsHeading);
	PageTitle = CreditsHeading;
	PageBody = MakeText(FText::GetEmpty(), PageFont, TextColor);
	PageBody->SetAutoWrapText(true);
	Page->AddChildToVerticalBox(PageBody)->SetPadding(FMargin(0.f, 0.f, 0.f, 28.f));
	BackRow = MakeRow(Page, BackLabel, EMainMenuRowKind::Back, INDEX_NONE);

	UTextBlock* OptionsHeading = nullptr;
	UVerticalBox* OptionsPage = MakePage(TEXT("OptionsPage"), OptionsHeading);
	OptionsPageTitle = OptionsHeading;
	OptionsPageTitle->SetText(OptionsTitle);
	for (int32 Index = 0; Index < OptionToggles.Num(); ++Index)
	{
		ToggleRows.Add(MakeRow(OptionsPage, OptionToggles[Index].Label, EMainMenuRowKind::Toggle, Index));
		RefreshToggleValue(Index);
	}
	UVerticalBox* Gap = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	OptionsPage->AddChildToVerticalBox(Gap)->SetPadding(FMargin(0.f, 0.f, 0.f, 28.f));
	OptionsBackRow = MakeRow(OptionsPage, BackLabel, EMainMenuRowKind::Back, INDEX_NONE);

	UCanvasPanelSlot* ListSlot = Root->AddChildToCanvas(Width);
	ListSlot->SetAutoSize(true);
	ListSlot->SetPosition(ListPosition);

	// The game's name and the subtitle under it, top left above the items.
	UVerticalBox* TitleBlock = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("GameTitle"));
	TitleBlock->SetVisibility(ESlateVisibility::HitTestInvisible);
	UTextBlock* TitleText = MakeText(GameTitle, GameTitleFont, GameTitleColor);
	TitleText->SetLineHeightPercentage(0.9f);
	TitleBlock->AddChildToVerticalBox(TitleText);
	TitleBlock->AddChildToVerticalBox(MakeText(GameSubtitle, GameSubtitleFont, GameSubtitleColor))
		->SetPadding(FMargin(4.f, GameSubtitleGap, 0.f, 0.f));
	UCanvasPanelSlot* TitleSlot = Root->AddChildToCanvas(TitleBlock);
	TitleSlot->SetAutoSize(true);
	TitleSlot->SetPosition(GameTitlePosition);

	// Version label, bottom right: "V<project version> RELEASE".
	const FString ProjectVersion = GetDefault<UGeneralProjectSettings>()->ProjectVersion;
	UTextBlock* Version = MakeText(FText::Format(LOCTEXT("VersionFormat", "V{0} {1}"), FText::FromString(ProjectVersion), VersionSuffix),
		VersionFont, TextColor);
	UCanvasPanelSlot* VersionSlot = Root->AddChildToCanvas(Version);
	VersionSlot->SetAutoSize(true);
	VersionSlot->SetAnchors(FAnchors(1.f, 1.f));
	VersionSlot->SetAlignment(FVector2D(1.f, 1.f));
	VersionSlot->SetPosition(FVector2D(-VersionMargin.X, -VersionMargin.Y));

	AddCursor();
	ResetOpenState();
}

void UMainMenuWidget::AddCursor()
{
	// Pixel arrow cursor, drawn last so it is on top. Never hit-tested, so it can't block clicks.
	ArrowCursor = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Cursor"));
	ArrowCursor->SetBrush(TextureBrush(CursorTexture, FVector2D(CursorWidth, CursorHeight) * CursorPixelSize));
	ArrowCursor->SetVisibility(ESlateVisibility::Collapsed);
	Root->AddChildToCanvas(ArrowCursor)->SetAutoSize(true);
}

void UMainMenuWidget::ResetOpenState()
{
	Selected = INDEX_NONE;
	for (int32 Index = 0; Selected == INDEX_NONE && Index < Items.Num(); ++Index)
	{
		if (IsItemEnabled(Index))
		{
			Selected = Index;
		}
	}
	OpenTime = 0.f;
	Root->SetRenderOpacity(FadeInTime > 0.f ? 0.f : 1.f);
}

UVerticalBox* UMainMenuWidget::MakePage(const FName& Name, UTextBlock*& OutTitle)
{
	UVerticalBox* Page = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), Name);
	Pages->AddChild(Page);
	OutTitle = MakeText(FText::GetEmpty(), ItemFont, SelectedColor);
	Page->AddChildToVerticalBox(OutTitle)->SetPadding(FMargin(0.f, 0.f, 0.f, 20.f));
	return Page;
}

UMainMenuWidget::FRow UMainMenuWidget::MakeRow(UVerticalBox* Parent, const FText& Label, EMainMenuRowKind Kind, int32 Index)
{
	FRow Row;

	// The button only takes the clicks; the row draws itself.
	FSlateBrush NoBrush;
	NoBrush.DrawAs = ESlateBrushDrawType::NoDrawType;
	FButtonStyle Style;
	Style.SetNormal(NoBrush).SetHovered(NoBrush).SetPressed(NoBrush).SetDisabled(NoBrush);
	Style.SetNormalPadding(FMargin(0.f)).SetPressedPadding(FMargin(0.f));

	USizeBox* Height = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	Height->SetHeightOverride(RowHeight);
	Row.Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	Row.Button->SetStyle(Style);
	Height->SetContent(Row.Button);

	// Marker | text with its underline. The whole line eases right when selected.
	UHorizontalBox* Line = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Row.Line = Line;
	if (UButtonSlot* ContentSlot = Cast<UButtonSlot>(Row.Button->SetContent(Line)))
	{
		ContentSlot->SetHorizontalAlignment(RowAlignment);
		ContentSlot->SetVerticalAlignment(VAlign_Center);
		ContentSlot->SetPadding(FMargin(0.f));
	}

	USizeBox* MarkerColumn = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	MarkerColumn->SetWidthOverride(MarkerSize + 28.f);
	Row.Marker = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	Row.Marker->SetBrush(SolidBrush(FVector2D(MarkerSize)));
	Row.Marker->SetColorAndOpacity(FLinearColor::Transparent);
	if (USizeBoxSlot* MarkerSlot = Cast<USizeBoxSlot>(MarkerColumn->SetContent(Row.Marker)))
	{
		MarkerSlot->SetHorizontalAlignment(HAlign_Left);
		MarkerSlot->SetVerticalAlignment(VAlign_Center);
	}
	Line->AddChildToHorizontalBox(MarkerColumn)->SetVerticalAlignment(VAlign_Center);

	UVerticalBox* Word = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Row.Text = MakeText(Label, ItemFont, TextColor);
	Word->AddChildToVerticalBox(Row.Text);
	Row.Underline = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	Row.Underline->SetBrush(SolidBrush(FVector2D(0.f, UnderlineThickness)));
	Row.Underline->SetColorAndOpacity(FLinearColor::Transparent);
	UVerticalBoxSlot* UnderlineSlot = Word->AddChildToVerticalBox(Row.Underline);
	UnderlineSlot->SetHorizontalAlignment(HAlign_Left);
	UnderlineSlot->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));
	UHorizontalBoxSlot* WordSlot = Line->AddChildToHorizontalBox(Word);
	WordSlot->SetVerticalAlignment(VAlign_Center);

	if (Kind == EMainMenuRowKind::Toggle)
	{
		// Switch rows: label on the left, its value (ON/OFF) on the right.
		WordSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		Row.Value = MakeText(FText::GetEmpty(), ItemFont, TextColor);
		UHorizontalBoxSlot* ValueSlot = Line->AddChildToHorizontalBox(Row.Value);
		ValueSlot->SetVerticalAlignment(VAlign_Top);
		ValueSlot->SetHorizontalAlignment(HAlign_Right);
	}

	Parent->AddChildToVerticalBox(Height);

	UMainMenuClickRelay* Relay = NewObject<UMainMenuClickRelay>(this);
	Relay->Menu = this;
	Relay->Kind = Kind;
	Relay->Index = Index;
	Row.Button->OnClicked.AddDynamic(Relay, &UMainMenuClickRelay::HandleClicked);
	Relays.Add(Relay);

	if (Kind == EMainMenuRowKind::Item)
	{
		Row.Button->SetIsEnabled(IsItemEnabled(Index));
		Row.Text->SetColorAndOpacity(FSlateColor(IsItemEnabled(Index) ? TextColor : DisabledTextColor));
	}
	return Row;
}

void UMainMenuWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (Root && FadeInTime > 0.f && OpenTime < FadeInTime)
	{
		OpenTime += InDeltaTime;
		Root->SetRenderOpacity(FMath::SmoothStep(0.f, 1.f, FMath::Min(OpenTime / FadeInTime, 1.f)));
	}

	TickSlice(ClickSound, ClickAudio, ClickTimeLeft, InDeltaTime);
	TickSlice(HoverSound, HoverAudio, HoverTimeLeft, InDeltaTime);

	// One hover tock each time the cursor moves onto a row.
	UButton* Hovered = FindHoveredButton();
	if (Hovered != LastHovered.Get())
	{
		LastHovered = Hovered;
		if (Hovered)
		{
			PlaySlice(HoverSound, HoverAudio, HoverTimeLeft);
		}
	}

	// Hover selects; the selection stays on the last hovered row.
	for (int32 Index = 0; Index < Rows.Num(); ++Index)
	{
		if (Rows[Index].Button && Rows[Index].Button->IsHovered() && IsItemEnabled(Index))
		{
			Selected = Index;
		}
	}
	for (int32 Index = 0; Index < Rows.Num(); ++Index)
	{
		EaseRow(Rows[Index], Index == Selected, IsItemEnabled(Index), InDeltaTime);
	}

	// Page rows light up only while hovered.
	for (FRow* Row : {&BackRow, &OptionsBackRow})
	{
		if (Row->Button)
		{
			EaseRow(*Row, Row->Button->IsHovered(), true, InDeltaTime);
		}
	}
	for (int32 Index = 0; Index < ToggleRows.Num(); ++Index)
	{
		FRow& Row = ToggleRows[Index];
		EaseRow(Row, Row.Button->IsHovered(), true, InDeltaTime);
		// The value follows the row's warmth when on, and stays dim when off.
		const float Ease = FMath::SmoothStep(0.f, 1.f, Row.Emphasis);
		const bool bOn = UGameOptions::IsOn(OptionToggles[Index].Option);
		Row.Value->SetColorAndOpacity(FSlateColor(bOn ? FMath::Lerp(TextColor, SelectedColor, Ease)
			: FMath::Lerp(DisabledTextColor, TextColor, Ease * 0.5f)));
	}
}

void UMainMenuWidget::EaseRow(FRow& Row, bool bSelected, bool bEnabled, float DeltaTime) const
{
	Row.Emphasis = FMath::FInterpTo(Row.Emphasis, bSelected && bEnabled ? 1.f : 0.f, DeltaTime, EaseSpeed);
	const float Ease = FMath::SmoothStep(0.f, 1.f, Row.Emphasis);

	Row.Text->SetColorAndOpacity(FSlateColor(bEnabled ? FMath::Lerp(TextColor, SelectedColor, Ease) : DisabledTextColor));
	Row.Line->SetRenderTranslation(FVector2D(SelectedSlide * Ease, 0.f));

	FLinearColor Accent = SelectedColor;
	Accent.A *= Ease;
	Row.Marker->SetColorAndOpacity(Accent);
	Accent.A *= 0.8f;
	Row.Underline->SetColorAndOpacity(Accent);
	// The underline draws itself from the left under the word.
	Row.Underline->SetDesiredSizeOverride(FVector2D(Row.Text->GetDesiredSize().X * Ease, UnderlineThickness));
}

void UMainMenuWidget::SetCursor(const FVector2D& WidgetPosition, bool bVisible)
{
	if (!ArrowCursor)
	{
		return;
	}
	ArrowCursor->SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (bVisible)
	{
		// The arrow's tip (top-left pixel) sits on the mouse point.
		const FVector2D Snapped(FMath::RoundToFloat(WidgetPosition.X), FMath::RoundToFloat(WidgetPosition.Y));
		Cast<UCanvasPanelSlot>(ArrowCursor->Slot)->SetPosition(Snapped);
	}
}

bool UMainMenuWidget::IsItemEnabled(int32 Index) const
{
	if (!Items.IsValidIndex(Index))
	{
		return false;
	}
	return Items[Index].Action != EMainMenuAction::Continue || bContinueAvailable;
}

void UMainMenuWidget::ActivateItem(int32 Index)
{
	if (!IsItemEnabled(Index))
	{
		return;
	}
	switch (Items[Index].Action)
	{
	case EMainMenuAction::Continue:
		UE_LOG(LogTemp, Warning, TEXT("MainMenu: Continue has no save system to load yet."));
		break;
	case EMainMenuAction::NewRun:
		if (NewRunLevel.IsNull())
		{
			UE_LOG(LogTemp, Warning, TEXT("MainMenu: NewRunLevel is not set."));
		}
		else
		{
			// Fresh gold and gear for the run (docs/features/safe-zone.md).
			if (UBalloonRunSubsystem* Run = UBalloonRunSubsystem::Get(this))
			{
				Run->StartNewRun();
			}
			UGameplayStatics::OpenLevelBySoftObjectPtr(this, NewRunLevel);
		}
		break;
	case EMainMenuAction::Credits:
		ShowPage(CreditsTitle, CreditsText);
		break;
	case EMainMenuAction::Options:
		ShowOptions();
		break;
	case EMainMenuAction::Quit:
		UKismetSystemLibrary::QuitGame(this, GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr, EQuitPreference::Quit, false);
		break;
	case EMainMenuAction::Resume:
		if (ABalloonPlayerController* Player = Cast<ABalloonPlayerController>(GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr))
		{
			Player->ResumeGame();
		}
		break;
	case EMainMenuAction::Retry:
		// Deferred to the end of the frame like any level change, so it works from a paused world.
		UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this)));
		break;
	case EMainMenuAction::QuitToMenu:
		if (MainMenuLevel.IsNull())
		{
			UE_LOG(LogTemp, Warning, TEXT("MainMenu: MainMenuLevel is not set."));
		}
		else
		{
			// Opening a level is deferred to the end of the frame, so it works from a paused world too.
			UGameplayStatics::OpenLevelBySoftObjectPtr(this, MainMenuLevel);
		}
		break;
	}
}

void UMainMenuWidget::ScaleColors(float Brightness)
{
	for (FLinearColor* Color : {&TextColor, &SelectedColor, &DisabledTextColor, &CursorColor, &GameTitleColor, &GameSubtitleColor})
	{
		*Color = FLinearColor(Color->R * Brightness, Color->G * Brightness, Color->B * Brightness, Color->A);
	}
}

void UMainMenuWidget::ShowPage(const FText& Title, const FText& Body)
{
	if (Pages && PageTitle && PageBody)
	{
		PageTitle->SetText(Title);
		PageBody->SetText(Body);
		BackRow.Emphasis = 0.f;
		Pages->SetActiveWidgetIndex(1);
	}
}

void UMainMenuWidget::ShowOptions()
{
	if (Pages)
	{
		OptionsBackRow.Emphasis = 0.f;
		for (int32 Index = 0; Index < ToggleRows.Num(); ++Index)
		{
			ToggleRows[Index].Emphasis = 0.f;
			RefreshToggleValue(Index);
		}
		Pages->SetActiveWidgetIndex(2);
	}
}

void UMainMenuWidget::PlayClick()
{
	PlaySlice(ClickSound, ClickAudio, ClickTimeLeft);
}

UButton* UMainMenuWidget::FindHoveredButton() const
{
	// Only rows on the visible page count; a disabled row (Continue) makes no sound.
	TArray<const FRow*> Visible;
	switch (Pages ? Pages->GetActiveWidgetIndex() : 0)
	{
	case 0:
		for (int32 Index = 0; Index < Rows.Num(); ++Index)
		{
			if (IsItemEnabled(Index))
			{
				Visible.Add(&Rows[Index]);
			}
		}
		break;
	case 1:
		Visible.Add(&BackRow);
		break;
	default:
		for (const FRow& Row : ToggleRows)
		{
			Visible.Add(&Row);
		}
		Visible.Add(&OptionsBackRow);
		break;
	}
	for (const FRow* Row : Visible)
	{
		if (Row->Button && Row->Button->IsHovered())
		{
			return Row->Button;
		}
	}
	return nullptr;
}

void UMainMenuWidget::PlaySlice(const FMainMenuSound& Effect, TObjectPtr<UAudioComponent>& Audio, float& TimeLeft)
{
	if (!UGameOptions::IsOn(EGameOption::Sfx))
	{
		return;
	}
	USoundBase* Sound = Effect.Sound.LoadSynchronous();
	if (!Sound)
	{
		return;
	}
	if (!Audio)
	{
		Audio = UGameplayStatics::CreateSound2D(this, Sound, Effect.Volume, 1.f, 0.f, nullptr, false, false);
		if (!Audio)
		{
			return;
		}
	}
	Audio->SetSound(Sound);
	Audio->SetVolumeMultiplier(Effect.Volume);
	Audio->Play(Effect.Start);
	TimeLeft = Effect.Length;
	if (Effect.Length > 0.f)
	{
		// TickSlice fades the slice out, but only while this widget ticks. A pause-menu click hides the
		// panel at once, and the rest of the recording would play on into the game, so the audio
		// device also stops it on its own once the slice and its fade are over.
		Audio->StopDelayed(Effect.Length + Effect.FadeOut);
	}
}

void UMainMenuWidget::NativeDestruct()
{
	// The audio components belong to the world, not to this widget. Let a slice still playing finish
	// (it stops itself, see PlaySlice) and free them, so each opening of the pause menu leaves nothing behind.
	for (TObjectPtr<UAudioComponent>* Audio : {&ClickAudio, &HoverAudio})
	{
		if (UAudioComponent* Component = Audio->Get())
		{
			if (Component->IsPlaying())
			{
				Component->bAutoDestroy = true;
			}
			else
			{
				Component->DestroyComponent();
			}
		}
		*Audio = nullptr;
	}
	Super::NativeDestruct();
}

void UMainMenuWidget::TickSlice(const FMainMenuSound& Effect, UAudioComponent* Audio, float& TimeLeft, float DeltaTime) const
{
	if (TimeLeft > 0.f)
	{
		TimeLeft -= DeltaTime;
		if (TimeLeft <= 0.f && Audio)
		{
			Audio->FadeOut(Effect.FadeOut, 0.f);
		}
	}
}

void UMainMenuWidget::ToggleOption(int32 Index)
{
	if (OptionToggles.IsValidIndex(Index))
	{
		const EGameOption Option = OptionToggles[Index].Option;
		UGameOptions::SetOn(Option, !UGameOptions::IsOn(Option));
		RefreshToggleValue(Index);
	}
}

void UMainMenuWidget::RefreshToggleValue(int32 Index)
{
	if (ToggleRows.IsValidIndex(Index) && OptionToggles.IsValidIndex(Index) && ToggleRows[Index].Value)
	{
		const FMainMenuToggle& Toggle = OptionToggles[Index];
		ToggleRows[Index].Value->SetText(UGameOptions::IsOn(Toggle.Option) ? Toggle.OnText : Toggle.OffText);
	}
}

void UMainMenuWidget::ShowItemList()
{
	if (Pages)
	{
		Pages->SetActiveWidgetIndex(0);
	}
}

#undef LOCTEXT_NAMESPACE
