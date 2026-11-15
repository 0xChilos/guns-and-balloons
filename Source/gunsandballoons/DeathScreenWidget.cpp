#include "DeathScreenWidget.h"

#include "BalloonPlayerController.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Styling/CoreStyle.h"

#define LOCTEXT_NAMESPACE "DeathScreen"

namespace
{
	FSlateFontInfo DeathFont(int32 Size, int32 LetterSpacing)
	{
		FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Bold", Size);
		Font.LetterSpacing = LetterSpacing;
		return Font;
	}

	FLinearColor DeathScaled(const FLinearColor& Color, float Brightness)
	{
		return FLinearColor(Color.R * Brightness, Color.G * Brightness, Color.B * Brightness, Color.A);
	}

	uint32 GrainHash(uint32 X, uint32 Y)
	{
		uint32 H = X * 374761393u + Y * 668265263u + 0x9E3779B9u;
		H = (H ^ (H >> 13)) * 1274126177u;
		return H ^ (H >> 16);
	}
}

UDeathScreenWidget::UDeathScreenWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// There is no checkpoint system yet: the checkpoint is the start of the current level (EMainMenuAction::Retry).
	FMainMenuItem Checkpoint;
	Checkpoint.Label = LOCTEXT("Checkpoint", "RETURN TO CHECKPOINT");
	Checkpoint.Action = EMainMenuAction::Retry;
	FMainMenuItem Menu;
	Menu.Label = LOCTEXT("MainMenu", "MAIN MENU");
	Menu.Action = EMainMenuAction::QuitToMenu;
	Items = { Checkpoint, Menu };
	Title = LOCTEXT("Title", "ENGINE DESTROYED");
	TitleFont = DeathFont(64, 200);
	CauseFont = DeathFont(30, 100);
	ListWidth = 600.f;
	FadeInTime = 0.8f;
	// The rows in the card's ink.
	TextColor = FLinearColor(0.62f, 0.57f, 0.45f, 1.f);
	SmokeLayers.Reset();
}

void UDeathScreenWidget::ScaleColors(float Brightness)
{
	Super::ScaleColors(Brightness);
	CauseColor = DeathScaled(CauseColor, Brightness);
	CardInkColor = DeathScaled(CardInkColor, Brightness);
	CardFillColor = DeathScaled(CardFillColor, Brightness);
	SepiaColor = DeathScaled(SepiaColor, Brightness);
}

FText UDeathScreenWidget::GetCauseText() const
{
	if (const ABalloonPlayerController* Player = Cast<ABalloonPlayerController>(GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr))
	{
		return Player->GetDeathCause();
	}
	return LOCTEXT("NoCause", "THE ENGINE GAVE OUT");
}

void UDeathScreenWidget::BuildTree()
{
	Card = nullptr;
	Grain = nullptr;
	Rows.Reset();
	Relays.Reset();
	MakeTextures();

	Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;
	const auto AddFull = [this](UWidget* Child, const FMargin& Offsets)
	{
		UCanvasPanelSlot* ChildSlot = Root->AddChildToCanvas(Child);
		ChildSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		ChildSlot->SetOffsets(Offsets);
	};

	// Sepia wash over the frozen frame.
	UImage* Sepia = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Sepia"));
	Sepia->SetColorAndOpacity(SepiaColor);
	Sepia->SetVisibility(ESlateVisibility::HitTestInvisible);
	AddFull(Sepia, FMargin(0.f));

	// Old-film vignette: dark, burnt corners.
	constexpr int32 VignetteSize = 64;
	VignetteTexture = MakeRuntimeTexture(VignetteSize, VignetteSize, TF_Bilinear, [](int32 X, int32 Y)
	{
		const float U = (X + 0.5f) / VignetteSize * 2.f - 1.f;
		const float V = (Y + 0.5f) / VignetteSize * 2.f - 1.f;
		const float D = FMath::SmoothStep(0.55f, 1.4f, FMath::Sqrt(U * U + V * V));
		return FColor(255, 255, 255, uint8(FMath::RoundToInt(255.f * D)));
	});
	UImage* Vignette = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Vignette"));
	FSlateBrush VignetteBrush;
	VignetteBrush.SetResourceObject(VignetteTexture);
	Vignette->SetBrush(VignetteBrush);
	Vignette->SetColorAndOpacity(FLinearColor(0.01f, 0.006f, 0.002f, 0.9f));
	Vignette->SetVisibility(ESlateVisibility::HitTestInvisible);
	AddFull(Vignette, FMargin(0.f));

	// Film grain: a tiling speckle that jumps to a new spot every film frame.
	constexpr int32 GrainSize = 128;
	GrainTexture = MakeRuntimeTexture(GrainSize, GrainSize, TF_Nearest, [](int32 X, int32 Y)
	{
		const uint32 H = GrainHash(uint32(X), uint32(Y));
		const uint8 Alpha = (H & 0xFF) > 200 ? uint8(80 + (H >> 8) % 175) : 0;
		const uint8 Tone = ((H >> 16) & 1) ? 255 : 0; // light dust and dark specks
		return FColor(Tone, Tone, Tone, Alpha);
	});
	Grain = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Grain"));
	FSlateBrush GrainBrush;
	GrainBrush.SetResourceObject(GrainTexture);
	GrainBrush.ImageSize = FVector2D(512.f);
	GrainBrush.Tiling = ESlateBrushTileType::Both;
	Grain->SetBrush(GrainBrush);
	Grain->SetColorAndOpacity(FLinearColor(1.f, 0.92f, 0.75f, GrainOpacity));
	Grain->SetVisibility(ESlateVisibility::HitTestInvisible);
	AddFull(Grain, FMargin(-512.f));

	// The title card: a double line border around a dark field.
	const auto Frame = [this](const FName& Name, const FLinearColor& Color, const FMargin& Padding)
	{
		UBorder* B = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), Name);
		B->SetBrush(FSlateBrush());
		B->SetBrushColor(Color);
		B->SetPadding(Padding);
		return B;
	};
	UBorder* OuterLine = Frame(TEXT("CardOuterLine"), CardInkColor, FMargin(4.f));
	UBorder* Gap = Frame(TEXT("CardGap"), CardFillColor, FMargin(12.f));
	UBorder* InnerLine = Frame(TEXT("CardInnerLine"), CardInkColor, FMargin(4.f));
	UBorder* Field = Frame(TEXT("CardField"), CardFillColor, FMargin(96.f, 56.f, 96.f, 48.f));
	OuterLine->SetContent(Gap);
	Gap->SetContent(InnerLine);
	InnerLine->SetContent(Field);

	UVerticalBox* Content = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("CardContent"));
	Field->SetContent(Content);
	const auto AddCentred = [Content](UWidget* Child, float Top)
	{
		UVerticalBoxSlot* ChildSlot = Content->AddChildToVerticalBox(Child);
		ChildSlot->SetHorizontalAlignment(HAlign_Center);
		ChildSlot->SetPadding(FMargin(0.f, Top, 0.f, 0.f));
	};
	AddCentred(MakeText(Title, TitleFont, CardInkColor), 0.f);
	// A short rule with a dot in the middle, as on old title cards.
	const auto Rule = [this](float Width)
	{
		UImage* Line = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		FSlateBrush Brush;
		Brush.ImageSize = FVector2D(Width, 4.f);
		Line->SetBrush(Brush);
		FLinearColor Color = CardInkColor;
		Color.A *= 0.6f;
		Line->SetColorAndOpacity(Color);
		return Line;
	};
	AddCentred(Rule(360.f), 28.f);
	UTextBlock* Cause = MakeText(GetCauseText(), CauseFont, CardInkColor);
	Cause->SetJustification(ETextJustify::Center);
	AddCentred(Cause, 28.f);

	USizeBox* Width = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("ListWidth"));
	Width->SetWidthOverride(ListWidth);
	UVerticalBox* List = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ItemList"));
	Width->SetContent(List);
	for (int32 Index = 0; Index < Items.Num(); ++Index)
	{
		Rows.Add(MakeRow(List, Items[Index].Label, EMainMenuRowKind::Item, Index));
	}
	AddCentred(Width, 56.f);

	UCanvasPanelSlot* CardSlot = Root->AddChildToCanvas(OuterLine);
	CardSlot->SetAutoSize(true);
	CardSlot->SetAnchors(FAnchors(0.5f, 0.5f));
	CardSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	Card = OuterLine;

	AddCursor();
	ResetOpenState();
	FilmClock = 0.f;
	ShownTime = 0.f;
	SetRowsClickable(InputDelay <= 0.f);
}

void UDeathScreenWidget::SetRowsClickable(bool bClickable)
{
	bRowsClickable = bClickable;
	for (FRow& Row : Rows)
	{
		if (Row.Button) Row.Button->SetIsEnabled(bClickable);
	}
}

void UDeathScreenWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	ShownTime += InDeltaTime;
	if (!bRowsClickable && ShownTime >= InputDelay) SetRowsClickable(true);

	// Old film: the grain jumps a few times a second. The card itself stays steady (2026-11-15).
	FilmClock += InDeltaTime;
	const float FrameTime = 1.f / FilmFrameRate;
	if (FilmClock >= FrameTime)
	{
		FilmClock = FMath::Fmod(FilmClock, FrameTime);
		if (Grain) Grain->SetRenderTranslation(FVector2D(FMath::FRandRange(-512.f, 0.f), FMath::FRandRange(-512.f, 0.f)));
	}
}

#undef LOCTEXT_NAMESPACE
