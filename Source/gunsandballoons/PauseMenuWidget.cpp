#include "PauseMenuWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "Styling/CoreStyle.h"

#define LOCTEXT_NAMESPACE "PauseMenu"

namespace
{
	constexpr int32 SmokeTextureSize = 256;
	constexpr int32 EdgeTextureSize = 64;

	FMainMenuItem MakePauseItem(const FText& Label, EMainMenuAction Action)
	{
		FMainMenuItem Item;
		Item.Label = Label;
		Item.Action = Action;
		return Item;
	}

	FPauseSmokeLayer MakeSmokeLayer(float Tile, const FVector2D& Drift, float Opacity, int32 Seed)
	{
		FPauseSmokeLayer Layer;
		Layer.Tile = Tile;
		Layer.Drift = Drift;
		Layer.Opacity = Opacity;
		Layer.Seed = Seed;
		return Layer;
	}

	FSlateFontInfo MakePauseFont(int32 Size, int32 LetterSpacing)
	{
		FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Bold", Size);
		Font.LetterSpacing = LetterSpacing;
		return Font;
	}

	float LatticeValue(int32 X, int32 Y, uint32 Seed)
	{
		uint32 Hash = uint32(X) * 374761393u + uint32(Y) * 668265263u + Seed * 2246822519u;
		Hash = (Hash ^ (Hash >> 13)) * 1274126177u;
		Hash ^= Hash >> 16;
		return float(Hash & 0xFFFFFFu) / float(0xFFFFFFu);
	}

	/** Value noise that repeats every Period cells (for any U, V), so a texture of it tiles without seams. */
	float PeriodicNoise(float U, float V, int32 Period, uint32 Seed)
	{
		const float X = U * Period;
		const float Y = V * Period;
		const int32 X0 = FMath::FloorToInt(X);
		const int32 Y0 = FMath::FloorToInt(Y);
		const float FX = FMath::SmoothStep(0.f, 1.f, X - X0);
		const float FY = FMath::SmoothStep(0.f, 1.f, Y - Y0);
		const auto Cell = [Period, Seed](int32 CX, int32 CY)
		{
			return LatticeValue(((CX % Period) + Period) % Period, ((CY % Period) + Period) % Period, Seed);
		};
		const float Top = FMath::Lerp(Cell(X0, Y0), Cell(X0 + 1, Y0), FX);
		const float Bottom = FMath::Lerp(Cell(X0, Y0 + 1), Cell(X0 + 1, Y0 + 1), FX);
		return FMath::Lerp(Top, Bottom, FY);
	}

	/** Octaves of periodic noise, 0..1. */
	float PeriodicFbm(float U, float V, int32 BasePeriod, int32 Octaves, uint32 Seed)
	{
		float Sum = 0.f, Total = 0.f, Weight = 0.5f;
		int32 Period = BasePeriod;
		for (int32 Octave = 0; Octave < Octaves; ++Octave)
		{
			Sum += PeriodicNoise(U, V, Period, Seed + 31u * Octave) * Weight;
			Total += Weight;
			Weight *= 0.5f;
			Period *= 2;
		}
		return Sum / Total;
	}

	/**
	 * Smoke: noise whose coordinates are pushed around by more noise, which curls the clumps into
	 * swirls. Never fully clear: the thinnest smoke keeps Floor of the densest. Tiles seamlessly,
	 * because every noise in it repeats over the texture.
	 */
	FColor SmokePixel(int32 X, int32 Y, uint32 Seed, float Floor)
	{
		const float U = X / float(SmokeTextureSize);
		const float V = Y / float(SmokeTextureSize);
		const float WarpU = PeriodicFbm(U, V, 2, 3, Seed + 101u) - 0.5f;
		const float WarpV = PeriodicFbm(U, V, 2, 3, Seed + 202u) - 0.5f;
		const float Noise = PeriodicFbm(U + 0.5f * WarpU, V + 0.5f * WarpV, 3, 5, Seed);
		const float Density = FMath::Lerp(Floor, 1.f, FMath::SmoothStep(0.22f, 0.82f, Noise));
		return FColor(255, 255, 255, uint8(FMath::RoundToInt(255.f * Density)));
	}

	/** Clear in the middle, denser towards the edges and corners. */
	FColor EdgePixel(int32 X, int32 Y)
	{
		const float U = (X + 0.5f) / EdgeTextureSize * 2.f - 1.f;
		const float V = (Y + 0.5f) / EdgeTextureSize * 2.f - 1.f;
		const float Density = FMath::SmoothStep(0.45f, 1.35f, FMath::Sqrt(U * U + V * V));
		return FColor(255, 255, 255, uint8(FMath::RoundToInt(255.f * Density)));
	}

	FLinearColor Scaled(const FLinearColor& Color, float Brightness)
	{
		return FLinearColor(Color.R * Brightness, Color.G * Brightness, Color.B * Brightness, Color.A);
	}

	/** A plain box of one colour (a brush with no image draws a solid quad). */
	FSlateBrush PauseSolidBrush()
	{
		return FSlateBrush();
	}
}

// --- Pause menu ------------------------------------------------------------------------------------

UPauseMenuWidget::UPauseMenuWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Items = {
		MakePauseItem(LOCTEXT("Resume", "RETURN TO GAME"), EMainMenuAction::Resume),
		MakePauseItem(LOCTEXT("QuitToMenu", "QUIT TO MAIN MENU"), EMainMenuAction::QuitToMenu),
		MakePauseItem(LOCTEXT("Leave", "LEAVE THE GAME"), EMainMenuAction::Quit),
	};
	Title = LOCTEXT("Title", "PAUSE MENU");
	TitleFont = MakePauseFont(64, 160);

	// Back to front: big slow billows, then smaller, quicker wisps rising over them.
	SmokeLayers = {
		MakeSmokeLayer(1700.f, FVector2D(-14.f, -6.f), 0.30f, 17),
		MakeSmokeLayer(1050.f, FVector2D(22.f, -14.f), 0.26f, 53),
		MakeSmokeLayer(640.f, FVector2D(-30.f, -24.f), 0.20f, 91),
	};

	ListWidth = 900.f;
	// The world has already stopped: show the menu quickly.
	FadeInTime = 0.25f;
	RowAlignment = HAlign_Center;
}

void UPauseMenuWidget::ScaleColors(float Brightness)
{
	Super::ScaleColors(Brightness);
	TitleColor = Scaled(TitleColor, Brightness);
	VeilColor = Scaled(VeilColor, Brightness);
	SmokeColor = Scaled(SmokeColor, Brightness);
}

void UPauseMenuWidget::BuildTree()
{
	Rows.Reset();
	Relays.Reset();
	SmokeTextures.Reset();
	SmokeImages.Reset();
	MakeTextures();

	Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	const auto AddFullScreen = [this](UWidget* Child, const FMargin& Offsets)
	{
		UCanvasPanelSlot* ChildSlot = Root->AddChildToCanvas(Child);
		ChildSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		ChildSlot->SetOffsets(Offsets);
	};

	// A light tint, so the text holds against bright parts of the frozen frame.
	UImage* Veil = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Veil"));
	Veil->SetColorAndOpacity(VeilColor);
	Veil->SetVisibility(ESlateVisibility::HitTestInvisible);
	AddFullScreen(Veil, FMargin(0.f));

	// The smoke layers, each one tile larger than the panel on every side so it can drift a whole tile and loop.
	for (int32 Index = 0; Index < SmokeLayers.Num(); ++Index)
	{
		const FPauseSmokeLayer& Layer = SmokeLayers[Index];
		const uint32 Seed = uint32(Layer.Seed);
		const float Floor = SmokeFloor;
		UTexture2D* Texture = MakeRuntimeTexture(SmokeTextureSize, SmokeTextureSize, TF_Bilinear,
			[Seed, Floor](int32 X, int32 Y) { return SmokePixel(X, Y, Seed, Floor); });
		SmokeTextures.Add(Texture);

		UImage* Image = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		FSlateBrush Brush;
		Brush.SetResourceObject(Texture);
		Brush.ImageSize = FVector2D(Layer.Tile);
		Brush.Tiling = ESlateBrushTileType::Both;
		Image->SetBrush(Brush);
		Image->SetVisibility(ESlateVisibility::HitTestInvisible);
		AddFullScreen(Image, FMargin(-Layer.Tile));
		SmokeImages.Add(Image);
	}

	// Thicker smoke towards the edges frames the menu and leaves the middle clearest.
	if (EdgeSmoke > 0.f)
	{
		EdgeTexture = MakeRuntimeTexture(EdgeTextureSize, EdgeTextureSize, TF_Bilinear, [](int32 X, int32 Y) { return EdgePixel(X, Y); });
		UImage* Edges = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("EdgeSmoke"));
		FSlateBrush EdgeBrush;
		EdgeBrush.SetResourceObject(EdgeTexture);
		EdgeBrush.ImageSize = FVector2D(EdgeTextureSize);
		Edges->SetBrush(EdgeBrush);
		FLinearColor EdgeColor = SmokeColor;
		EdgeColor.A *= EdgeSmoke;
		Edges->SetColorAndOpacity(EdgeColor);
		Edges->SetVisibility(ESlateVisibility::HitTestInvisible);
		AddFullScreen(Edges, FMargin(0.f));
	}

	// Title and a thin rule under it, centred at the top.
	UVerticalBox* Heading = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Heading"));
	Heading->AddChildToVerticalBox(MakeText(Title, TitleFont, TitleColor))->SetHorizontalAlignment(HAlign_Center);
	if (TitleRuleWidth > 0.f)
	{
		UImage* Rule = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("TitleRule"));
		FSlateBrush RuleBrush = PauseSolidBrush();
		RuleBrush.ImageSize = FVector2D(TitleRuleWidth, 4.f);
		Rule->SetBrush(RuleBrush);
		FLinearColor RuleColor = TextColor;
		RuleColor.A *= 0.45f;
		Rule->SetColorAndOpacity(RuleColor);
		UVerticalBoxSlot* RuleSlot = Heading->AddChildToVerticalBox(Rule);
		RuleSlot->SetHorizontalAlignment(HAlign_Center);
		RuleSlot->SetPadding(FMargin(0.f, 24.f, 0.f, 0.f));
	}
	UCanvasPanelSlot* HeadingSlot = Root->AddChildToCanvas(Heading);
	HeadingSlot->SetAutoSize(true);
	HeadingSlot->SetAnchors(FAnchors(0.5f, 0.f));
	HeadingSlot->SetAlignment(FVector2D(0.5f, 0.f));
	HeadingSlot->SetPosition(FVector2D(0.f, TitleTop));

	// The rows, centred under the title.
	USizeBox* Width = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("ListWidth"));
	Width->SetWidthOverride(ListWidth);
	UVerticalBox* List = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ItemList"));
	Width->SetContent(List);
	for (int32 Index = 0; Index < Items.Num(); ++Index)
	{
		Rows.Add(MakeRow(List, Items[Index].Label, EMainMenuRowKind::Item, Index));
	}
	UCanvasPanelSlot* ListSlot = Root->AddChildToCanvas(Width);
	ListSlot->SetAutoSize(true);
	ListSlot->SetAnchors(FAnchors(0.5f, 0.f));
	ListSlot->SetAlignment(FVector2D(0.5f, 0.f));
	ListSlot->SetPosition(FVector2D(0.f, ListTop));

	AddCursor();
	ResetOpenState();
	SmokeTime = 0.f;
	MoveSmoke();
}

void UPauseMenuWidget::MoveSmoke()
{
	for (int32 Index = 0; Index < SmokeImages.Num() && Index < SmokeLayers.Num(); ++Index)
	{
		const FPauseSmokeLayer& Layer = SmokeLayers[Index];
		UImage* Image = SmokeImages[Index];
		Image->SetRenderTranslation(FVector2D(FMath::Fmod(SmokeTime * Layer.Drift.X, Layer.Tile),
			FMath::Fmod(SmokeTime * Layer.Drift.Y, Layer.Tile)));
		// Each layer breathes out of step with the others, so the total stays even.
		const float Phase = 2.f * PI * Index / FMath::Max(SmokeLayers.Num(), 1);
		const float Breath = 1.f - SmokeBreath * 0.5f * (1.f - FMath::Sin(2.f * PI * SmokeTime / SmokeBreathPeriod + Phase));
		FLinearColor Color = SmokeColor;
		Color.A *= Layer.Opacity * Breath;
		Image->SetColorAndOpacity(Color);
	}
}

void UPauseMenuWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	SmokeTime += InDeltaTime;
	MoveSmoke();
}

// --- Pause hint ------------------------------------------------------------------------------------

UPauseHintWidget::UPauseHintWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Items.Reset();
	KeyLabel = LOCTEXT("HintKey", "P");
	Label = LOCTEXT("HintLabel", "PAUSE");
	HintFont = MakePauseFont(32, 80);
	FadeInTime = 1.f;
}

void UPauseHintWidget::ScaleColors(float Brightness)
{
	Super::ScaleColors(Brightness);
	KeyFillColor = Scaled(KeyFillColor, Brightness);
}

void UPauseHintWidget::BuildTree()
{
	Rows.Reset();
	Relays.Reset();
	MakeTextures();

	Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	// [P] PAUSE: a key cap (a one-filter-pixel outline around a dark fill) and the word.
	UHorizontalBox* Hint = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("Hint"));
	UBorder* Outline = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("KeyOutline"));
	Outline->SetBrush(PauseSolidBrush());
	Outline->SetBrushColor(TextColor);
	Outline->SetPadding(FMargin(4.f));
	UBorder* Fill = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("KeyFill"));
	Fill->SetBrush(PauseSolidBrush());
	Fill->SetBrushColor(KeyFillColor);
	Fill->SetPadding(FMargin(16.f, 4.f, 12.f, 4.f));
	Fill->SetContent(MakeText(KeyLabel, HintFont, SelectedColor));
	Outline->SetContent(Fill);
	Hint->AddChildToHorizontalBox(Outline)->SetVerticalAlignment(VAlign_Center);
	UHorizontalBoxSlot* WordSlot = Hint->AddChildToHorizontalBox(MakeText(Label, HintFont, TextColor));
	WordSlot->SetVerticalAlignment(VAlign_Center);
	WordSlot->SetPadding(FMargin(24.f, 0.f, 0.f, 0.f));

	UCanvasPanelSlot* HintSlot = Root->AddChildToCanvas(Hint);
	HintSlot->SetAutoSize(true);
	HintSlot->SetAnchors(FAnchors(1.f, 1.f));
	HintSlot->SetAlignment(FVector2D(1.f, 1.f));
	HintSlot->SetPosition(FVector2D(-Margin.X, -Margin.Y));
	Root->SetVisibility(ESlateVisibility::HitTestInvisible);

	ResetOpenState();
	ShownTime = 0.f;
}

void UPauseHintWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	// The base fades the hint in; after that it dims once BrightTime has passed.
	Super::NativeTick(MyGeometry, InDeltaTime);
	ShownTime += InDeltaTime;
	if (Root && ShownTime > FadeInTime)
	{
		const float Target = ShownTime < BrightTime ? 1.f : DimmedOpacity;
		Root->SetRenderOpacity(FMath::FInterpTo(Root->GetRenderOpacity(), Target, InDeltaTime, 1.5f));
	}
}

#undef LOCTEXT_NAMESPACE
