#include "LevelShopWidget.h"

#include "BalloonRunSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/AudioComponent.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/SizeBoxSlot.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "GameOptions.h"
#include "HookComponent.h"
#include "Kismet/GameplayStatics.h"
#include "RifleComponent.h"
#include "ShotgunComponent.h"
#include "Sound/SoundBase.h"
#include "Styling/CoreStyle.h"

#define LOCTEXT_NAMESPACE "LevelShop"

namespace
{
	FSlateFontInfo MakeFont(int32 Size, int32 LetterSpacing)
	{
		FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Bold", Size);
		Font.LetterSpacing = LetterSpacing;
		return Font;
	}

	/** An icon rendered from the item's model, /Game/UI/Shop/Icons/T_ShopIcon_<Name>. */
	TSoftObjectPtr<UTexture2D> ShopIcon(const TCHAR* Name)
	{
		return TSoftObjectPtr<UTexture2D>(FSoftObjectPath(FString::Printf(TEXT("/Game/UI/Shop/Icons/T_ShopIcon_%s.T_ShopIcon_%s"), Name, Name)));
	}

	FLevelShopItem MakeItem(ELevelShopItemKind Kind, const FText& Name, const TCHAR* IconName, int32 Price)
	{
		FLevelShopItem Item;
		Item.Kind = Kind;
		Item.Name = Name;
		Item.Icon = ShopIcon(IconName);
		Item.Price = Price;
		return Item;
	}

	FLevelShopItem MakeSupply(EResourceType Resource, const FText& Name, const TCHAR* IconName, int32 Price)
	{
		FLevelShopItem Item = MakeItem(ELevelShopItemKind::Supply, Name, IconName, Price);
		Item.Resource = Resource;
		Item.Units = 1;
		return Item;
	}

	FLevelShopItem MakeGear(TSubclassOf<UGunComponent> GearClass, const FText& Name, const TCHAR* IconName, int32 Price)
	{
		FLevelShopItem Item = MakeItem(ELevelShopItemKind::Gear, Name, IconName, Price);
		Item.GearClass = GearClass;
		return Item;
	}

	FLevelShopItem MakeExtinguisher(const FText& Name, const TCHAR* IconName, int32 Price, int32 RefillPrice, float RefillUnits)
	{
		FLevelShopItem Item = MakeItem(ELevelShopItemKind::Extinguisher, Name, IconName, Price);
		Item.RefillPrice = RefillPrice;
		Item.RefillUnits = RefillUnits;
		return Item;
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

	UTexture2D* MakeTexture(int32 Width, int32 Height, TextureFilter Filter, TFunctionRef<FColor(int32, int32)> Pixel)
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

	/** Pixel art from rows of characters; Palette maps a character to its colour, anything else is clear. */
	template <int32 RowCount>
	UTexture2D* MakePixelArt(const TCHAR* const (&Rows)[RowCount], TFunctionRef<FColor(TCHAR)> Palette)
	{
		const int32 Width = FCString::Strlen(Rows[0]);
		return MakeTexture(Width, RowCount, TF_Nearest, [&Rows, &Palette](int32 X, int32 Y) { return Palette(Rows[Y][X]); });
	}

	/** The arrow cursor, as in the main menu: 'O' outline, 'X' fill. Hotspot = top-left pixel. */
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

	/** Fire extinguisher, 16x16: red cylinder with a metal band, valve, lever and hose. */
	const TCHAR* const ExtinguisherRows[] = {
		TEXT("......mmmm......"),
		TEXT(".....mMMMMmm...."),
		TEXT("......mMMm..mB.."),
		TEXT(".....OOOOOO..B.."),
		TEXT("....ORRRRRrO.B.."),
		TEXT("....ORHRRRrO.B.."),
		TEXT("....ORHRRRrO.B.."),
		TEXT("....ORHMMMrO.B.."),
		TEXT("....ORHMMMrO.B.."),
		TEXT("....ORHRRRrO..B."),
		TEXT("....ORHRRRrO..B."),
		TEXT("....ORRRRRrO...."),
		TEXT("....ORRRRRrO...."),
		TEXT("....ORRRRrrO...."),
		TEXT("....OmmmmmmO...."),
		TEXT(".....OOOOOO....."),
	};

	/** Gold coin, 8x8. */
	const TCHAR* const CoinRows[] = {
		TEXT("..OOOO.."),
		TEXT(".OYYYYO."),
		TEXT("OYHYYyYO"),
		TEXT("OYHYYyYO"),
		TEXT("OYYYYyYO"),
		TEXT("OYYYyyYO"),
		TEXT(".OyyyyO."),
		TEXT("..OOOO.."),
	};

	FColor ItemPalette(TCHAR C)
	{
		switch (C)
		{
		case 'O': return FColor(10, 10, 10);
		case 'R': return FColor(170, 40, 30);
		case 'r': return FColor(110, 25, 20);
		case 'H': return FColor(225, 120, 90);
		case 'M': return FColor(175, 175, 165);
		case 'm': return FColor(95, 95, 90);
		case 'B': return FColor(25, 25, 25);
		case 'Y': return FColor(230, 180, 50);
		case 'y': return FColor(160, 110, 25);
		default: return FColor(0, 0, 0, 0);
		}
	}

	FColor CoinPalette(TCHAR C)
	{
		return C == 'H' ? FColor(255, 240, 160) : ItemPalette(C);
	}
}

void ULevelShopClickRelay::HandleClicked()
{
	if (ULevelShopWidget* Owner = Shop.Get())
	{
		Owner->PlayClick();
		if (Index == INDEX_NONE)
		{
			Owner->ContinueTravel();
		}
		else
		{
			Owner->ActivateItem(Index);
		}
	}
}

ULevelShopWidget::ULevelShopWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Fuel and ammo: one piece / round per click. Extinguisher: bought full for 10 gold, then +10 for 1 gold.
	// Hook, rifle, shotgun: bought once.
	Items = {
		MakeSupply(EResourceType::Wood, LOCTEXT("Wood", "WOOD"), TEXT("Wood"), 2),
		MakeSupply(EResourceType::Coal, LOCTEXT("Coal", "COAL"), TEXT("Coal"), 3),
		MakeSupply(EResourceType::Crystal, LOCTEXT("Crystal", "CRYSTALS"), TEXT("Crystal"), 5),
		MakeSupply(EResourceType::Ammo, LOCTEXT("Ammo", "AMMO"), TEXT("Ammo"), 1),
		MakeExtinguisher(LOCTEXT("Extinguisher", "FIRE EXTINGUISHER"), TEXT("Extinguisher"), 10, 1, 10.f),
		MakeGear(UHookComponent::StaticClass(), LOCTEXT("Hook", "HOOK"), TEXT("Hook"), 10),
		MakeGear(URifleComponent::StaticClass(), LOCTEXT("Rifle", "RIFLE"), TEXT("Rifle"), 30),
		MakeGear(UShotgunComponent::StaticClass(), LOCTEXT("Shotgun", "SHOTGUN"), TEXT("Shotgun"), 20),
	};

	Title = LOCTEXT("Title", "SHOP");
	Subtitle = LOCTEXT("Subtitle", "LEVEL CLEARED");
	GoldFormat = LOCTEXT("GoldFormat", "GOLD {0}");
	PriceFormat = LOCTEXT("PriceFormat", "{0} GOLD");
	CountFormat = LOCTEXT("CountFormat", "{0}/{1}");
	OwnedLabel = LOCTEXT("Owned", "OWNED");
	ContinueLabel = LOCTEXT("Continue", "CONTINUE THE TRAVEL");

	TitleFont = MakeFont(60, 80);
	ItemFont = MakeFont(44, 60);
	DetailFont = MakeFont(30, 40);
	RowFont = MakeFont(36, 40);
	OwnedFont = MakeFont(20, 0);

	const TSoftObjectPtr<USoundBase> ClockLoop(FSoftObjectPath(TEXT(
		"/Game/Audio/Main_Menu_Button_Click/daviddumaisaudio-steampunk-loopable-clock-12-188049."
		"daviddumaisaudio-steampunk-loopable-clock-12-188049")));
	ClickSound.Sound = ClockLoop;
	ClickSound.Start = 0.09f;
	ClickSound.Length = 0.34f;
	HoverSound.Sound = ClockLoop;
	HoverSound.Start = 1.12f;
	HoverSound.Length = 0.24f;
}

void ULevelShopWidget::ApplyBrightness(float Brightness)
{
	for (FLinearColor* Color : {&TextColor, &SelectedColor, &DisabledTextColor, &ShortColor, &CursorColor, &IconTint, &OwnedIconTint, &BackgroundTint})
	{
		*Color = FLinearColor(Color->R * Brightness, Color->G * Brightness, Color->B * Brightness, Color->A);
	}
}

TSharedRef<SWidget> ULevelShopWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildTree();
	}
	return Super::RebuildWidget();
}

void ULevelShopWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (UBalloonRunSubsystem* Run = UBalloonRunSubsystem::Get(this))
	{
		Run->OnChanged.AddUniqueDynamic(this, &ULevelShopWidget::Refresh);
	}
	Refresh();
}

void ULevelShopWidget::NativeDestruct()
{
	if (UBalloonRunSubsystem* Run = UBalloonRunSubsystem::Get(this))
	{
		Run->OnChanged.RemoveDynamic(this, &ULevelShopWidget::Refresh);
	}
	Super::NativeDestruct();
}

void ULevelShopWidget::MakeTextures()
{
	constexpr int32 HazeSteps = 256;
	HazeTexture = MakeTexture(HazeSteps, 4, TF_Bilinear, [](int32 X, int32)
	{
		const float U = X / float(HazeSteps - 1);
		const float Fade = 1.f - FMath::SmoothStep(0.f, 1.f, U);
		return FColor(255, 255, 255, uint8(FMath::RoundToInt(255.f * FMath::Pow(Fade, 1.6f))));
	});

	// The built-in backdrop: dark teal (the main menu's night), lighter toward the bottom, a warm stove glow on the
	// right where the list doesn't reach, a vignette and a little grain. Fully opaque.
	constexpr int32 BackWidth = 320;
	constexpr int32 BackHeight = 180;
	PaintedBackground = MakeTexture(BackWidth, BackHeight, TF_Bilinear, [](int32 X, int32 Y)
	{
		const float U = X / float(BackWidth - 1);
		const float V = Y / float(BackHeight - 1);
		const FVector3f Top(12.f, 20.f, 21.f);
		const FVector3f Bottom(24.f, 30.f, 27.f);
		FVector3f Color = Top + (Bottom - Top) * V;

		const float GlowX = (U - 0.80f) * 16.f / 9.f;
		const float GlowY = V - 0.62f;
		const float Glow = FMath::Square(FMath::Max(0.f, 1.f - FMath::Sqrt(GlowX * GlowX + GlowY * GlowY) / 0.75f));
		Color += FVector3f(70.f, 40.f, 16.f) * Glow;

		const float EdgeX = (U - 0.5f) * 16.f / 9.f;
		const float EdgeY = V - 0.5f;
		const float Edge = FMath::SmoothStep(0.45f, 1.05f, FMath::Sqrt(EdgeX * EdgeX + EdgeY * EdgeY));
		Color *= 1.f - 0.6f * Edge;

		const uint32 Hash = (uint32(X) * 73856093u) ^ (uint32(Y) * 19349663u);
		const float Grain = float((Hash >> 7) % 7) - 3.f;
		auto Channel = [Grain](float C) { return uint8(FMath::Clamp(FMath::RoundToInt(C + Grain), 0, 255)); };
		return FColor(Channel(Color.X), Channel(Color.Y), Channel(Color.Z), 255);
	});

	const FColor Fill = CursorColor.ToFColor(true);
	const FColor Outline = CursorOutlineColor.ToFColor(true);
	CursorTexture = MakePixelArt(CursorRows, [Fill, Outline](TCHAR C)
	{
		return C == 'O' ? Outline : C == 'X' ? Fill : FColor(0, 0, 0, 0);
	});
	CoinTexture = MakePixelArt(CoinRows, CoinPalette);
	KindIcons = { MakePixelArt(ExtinguisherRows, ItemPalette) };
}

UTexture2D* ULevelShopWidget::GetIcon(const FLevelShopItem& Item)
{
	if (UTexture2D* Custom = Item.Icon.LoadSynchronous())
	{
		return Custom;
	}
	// Only the extinguisher has a built-in pixel icon.
	return Item.Kind == ELevelShopItemKind::Extinguisher && KindIcons.Num() > 0 ? KindIcons[0] : nullptr;
}

UTextBlock* ULevelShopWidget::MakeText(const FText& Text, const FSlateFontInfo& Font, const FLinearColor& Color)
{
	UTextBlock* Block = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Block->SetText(Text);
	Block->SetFont(Font);
	Block->SetColorAndOpacity(FSlateColor(Color));
	Block->SetShadowOffset(ShadowOffset);
	Block->SetShadowColorAndOpacity(ShadowColor);
	return Block;
}

void ULevelShopWidget::BuildTree()
{
	ItemRows.Reset();
	Relays.Reset();
	MakeTextures();

	Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	// The shop's own background over the whole view, fully opaque; it doesn't fade.
	UTexture2D* BackTexture = BackgroundTexture.LoadSynchronous();
	UImage* Background = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Background"));
	Background->SetBrush(TextureBrush(BackTexture ? BackTexture : PaintedBackground.Get(), FVector2D(1920.f, 1080.f)));
	Background->SetColorAndOpacity(FLinearColor(BackgroundTint.R, BackgroundTint.G, BackgroundTint.B, 1.f));
	Background->SetVisibility(ESlateVisibility::HitTestInvisible);
	UCanvasPanelSlot* BackgroundSlot = Root->AddChildToCanvas(Background);
	BackgroundSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
	BackgroundSlot->SetOffsets(FMargin(0.f));

	// Everything else, faded in over it.
	Content = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Content"));
	UCanvasPanelSlot* ContentSlot = Root->AddChildToCanvas(Content);
	ContentSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
	ContentSlot->SetOffsets(FMargin(0.f));

	// The main menu's haze from the left edge, behind the list.
	UImage* Haze = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Haze"));
	Haze->SetBrush(TextureBrush(HazeTexture, FVector2D(HazeWidth, 4.f)));
	Haze->SetColorAndOpacity(HazeColor);
	Haze->SetVisibility(ESlateVisibility::HitTestInvisible);
	UCanvasPanelSlot* HazeSlot = Content->AddChildToCanvas(Haze);
	HazeSlot->SetAnchors(FAnchors(0.f, 0.f, 0.f, 1.f));
	HazeSlot->SetOffsets(FMargin(0.f, 0.f, HazeWidth, 0.f));

	// The gold counter, in the top left corner.
	UHorizontalBox* GoldLine = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("Gold"));
	UImage* Coin = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	Coin->SetBrush(TextureBrush(CoinTexture, FVector2D(32.f)));
	Coin->SetColorAndOpacity(IconTint);
	UHorizontalBoxSlot* CoinSlot = GoldLine->AddChildToHorizontalBox(Coin);
	CoinSlot->SetVerticalAlignment(VAlign_Center);
	CoinSlot->SetPadding(FMargin(0.f, 0.f, 24.f, 0.f));
	GoldText = MakeText(FText::GetEmpty(), ItemFont, SelectedColor);
	GoldLine->AddChildToHorizontalBox(GoldText)->SetVerticalAlignment(VAlign_Center);
	UCanvasPanelSlot* GoldSlot = Content->AddChildToCanvas(GoldLine);
	GoldSlot->SetAutoSize(true);
	GoldSlot->SetPosition(GoldPosition);

	// Title and the items.
	USizeBox* ColumnBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("Column"));
	ColumnBox->SetWidthOverride(ColumnWidth);
	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ColumnList"));
	ColumnBox->SetContent(Column);

	Column->AddChildToVerticalBox(MakeText(Title, TitleFont, SelectedColor));
	Column->AddChildToVerticalBox(MakeText(Subtitle, DetailFont, TextColor))->SetPadding(FMargin(0.f, 4.f, 0.f, 24.f));

	for (int32 Index = 0; Index < Items.Num(); ++Index)
	{
		FRow Row = MakeRow(Items[Index].Name, Index, ItemRowHeight);
		Column->AddChildToVerticalBox(Row.Button->GetParent());
		ItemRows.Add(Row);
	}

	UCanvasPanelSlot* ColumnSlot = Content->AddChildToCanvas(ColumnBox);
	ColumnSlot->SetAutoSize(true);
	ColumnSlot->SetPosition(ColumnPosition);

	// "Continue the travel", at the bottom.
	ContinueRow = MakeRow(ContinueLabel, INDEX_NONE, ContinueRowHeight);
	USizeBox* ContinueBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("ContinueBox"));
	ContinueBox->SetWidthOverride(ColumnWidth);
	ContinueBox->SetContent(ContinueRow.Button->GetParent());
	UCanvasPanelSlot* ContinueSlot = Content->AddChildToCanvas(ContinueBox);
	ContinueSlot->SetAutoSize(true);
	ContinueSlot->SetAnchors(FAnchors(0.f, 1.f));
	ContinueSlot->SetAlignment(FVector2D(0.f, 1.f));
	ContinueSlot->SetPosition(FVector2D(ColumnPosition.X, -BottomMargin));

	ArrowCursor = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Cursor"));
	ArrowCursor->SetBrush(TextureBrush(CursorTexture, FVector2D(CursorTexture ? CursorTexture->GetSizeX() : 7, CursorTexture ? CursorTexture->GetSizeY() : 10) * CursorPixelSize));
	ArrowCursor->SetVisibility(ESlateVisibility::Collapsed);
	Root->AddChildToCanvas(ArrowCursor)->SetAutoSize(true);

	OpenTime = 0.f;
	Content->SetRenderOpacity(FadeInTime > 0.f ? 0.f : 1.f);
}

ULevelShopWidget::FRow ULevelShopWidget::MakeRow(const FText& Label, int32 Index, float Height)
{
	FRow Row;

	// The button only takes the clicks; the row draws itself.
	FSlateBrush NoBrush;
	NoBrush.DrawAs = ESlateBrushDrawType::NoDrawType;
	FButtonStyle Style;
	Style.SetNormal(NoBrush).SetHovered(NoBrush).SetPressed(NoBrush).SetDisabled(NoBrush);
	Style.SetNormalPadding(FMargin(0.f)).SetPressedPadding(FMargin(0.f));

	USizeBox* HeightBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	HeightBox->SetHeightOverride(Height);
	Row.Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	Row.Button->SetStyle(Style);
	HeightBox->SetContent(Row.Button);

	UHorizontalBox* Line = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Row.Line = Line;
	if (UButtonSlot* ContentSlot = Cast<UButtonSlot>(Row.Button->SetContent(Line)))
	{
		ContentSlot->SetHorizontalAlignment(HAlign_Fill);
		ContentSlot->SetVerticalAlignment(VAlign_Center);
		ContentSlot->SetPadding(FMargin(0.f));
	}

	// Lamp marker.
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

	const bool bItem = Items.IsValidIndex(Index);
	if (bItem)
	{
		// Icon on a dark square; gear already owned gets OwnedLabel across it.
		UBorder* IconBack = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		IconBack->SetBrush(SolidBrush(FVector2D(4.f)));
		IconBack->SetBrushColor(IconBackColor);
		IconBack->SetPadding(FMargin(8.f));
		UOverlay* IconStack = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
		Row.Icon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		Row.Icon->SetBrush(TextureBrush(GetIcon(Items[Index]), FVector2D(IconSize)));
		Row.Icon->SetColorAndOpacity(IconTint);
		IconStack->AddChildToOverlay(Row.Icon);
		Row.Owned = MakeText(OwnedLabel, OwnedFont, TextColor);
		Row.Owned->SetVisibility(ESlateVisibility::Collapsed);
		UOverlaySlot* OwnedSlot = IconStack->AddChildToOverlay(Row.Owned);
		OwnedSlot->SetHorizontalAlignment(HAlign_Center);
		OwnedSlot->SetVerticalAlignment(VAlign_Center);
		IconBack->SetContent(IconStack);
		UHorizontalBoxSlot* IconSlot = Line->AddChildToHorizontalBox(IconBack);
		IconSlot->SetVerticalAlignment(VAlign_Center);
		IconSlot->SetPadding(FMargin(0.f, 0.f, 32.f, 0.f));
	}

	// Name with its underline.
	UVerticalBox* Info = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	UVerticalBox* Word = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Row.Text = MakeText(Label, bItem ? RowFont : ItemFont, TextColor);
	Word->AddChildToVerticalBox(Row.Text);
	Row.Underline = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	Row.Underline->SetBrush(SolidBrush(FVector2D(0.f, UnderlineThickness)));
	Row.Underline->SetColorAndOpacity(FLinearColor::Transparent);
	UVerticalBoxSlot* UnderlineSlot = Word->AddChildToVerticalBox(Row.Underline);
	UnderlineSlot->SetHorizontalAlignment(HAlign_Left);
	UnderlineSlot->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));
	Info->AddChildToVerticalBox(Word)->SetHorizontalAlignment(HAlign_Left);
	UHorizontalBoxSlot* InfoSlot = Line->AddChildToHorizontalBox(Info);
	InfoSlot->SetVerticalAlignment(VAlign_Center);
	InfoSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	if (bItem)
	{
		// How much the player has, then the price on the right.
		USizeBox* CountBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		CountBox->SetWidthOverride(CountWidth);
		Row.Count = MakeText(FText::GetEmpty(), DetailFont, TextColor);
		if (USizeBoxSlot* CountSlot = Cast<USizeBoxSlot>(CountBox->SetContent(Row.Count)))
		{
			CountSlot->SetHorizontalAlignment(HAlign_Left);
			CountSlot->SetVerticalAlignment(VAlign_Center);
		}
		Line->AddChildToHorizontalBox(CountBox)->SetVerticalAlignment(VAlign_Center);

		Row.Price = MakeText(FText::GetEmpty(), RowFont, TextColor);
		UHorizontalBoxSlot* PriceSlot = Line->AddChildToHorizontalBox(Row.Price);
		PriceSlot->SetVerticalAlignment(VAlign_Center);
		PriceSlot->SetHorizontalAlignment(HAlign_Right);
	}

	ULevelShopClickRelay* Relay = NewObject<ULevelShopClickRelay>(this);
	Relay->Shop = this;
	Relay->Index = Index;
	Row.Button->OnClicked.AddDynamic(Relay, &ULevelShopClickRelay::HandleClicked);
	Relays.Add(Relay);
	return Row;
}

ULevelShopWidget::FOffer ULevelShopWidget::GetOffer(int32 Index) const
{
	FOffer Offer;
	const UBalloonRunSubsystem* Run = UBalloonRunSubsystem::Get(this);
	if (!Run || !Items.IsValidIndex(Index))
	{
		return Offer;
	}
	const FLevelShopItem& Item = Items[Index];
	switch (Item.Kind)
	{
	case ELevelShopItemKind::Extinguisher:
	{
		// Not owned: bought full for Price. Owned: refills for RefillPrice. Like the counter on the tank, a
		// part-used unit still counts.
		const int32 Full = FMath::RoundToInt(UBalloonRunSubsystem::ExtinguisherFull);
		const int32 Amount = Run->HasExtinguisher() ? FMath::CeilToInt(Run->GetExtinguisherAmount() - KINDA_SMALL_NUMBER) : 0;
		Offer.Price = Run->HasExtinguisher() ? Item.RefillPrice : Item.Price;
		Offer.bAvailable = !Run->HasExtinguisher() || Run->GetExtinguisherAmount() < UBalloonRunSubsystem::ExtinguisherFull;
		Offer.Count = FText::Format(CountFormat, Amount, Full);
		break;
	}
	case ELevelShopItemKind::Supply:
	{
		const int32 Have = Run->GetSupply(Item.Resource);
		const int32 Max = UBalloonRunSubsystem::GetSupplyMax(Item.Resource);
		Offer.Price = Item.Price;
		Offer.bAvailable = Have >= 0 && Have < Max;
		Offer.Count = FText::Format(CountFormat, FMath::Max(Have, 0), Max);
		break;
	}
	case ELevelShopItemKind::Gear:
		Offer.Price = Item.Price;
		Offer.bOwned = Run->OwnsGear(Item.GearClass);
		Offer.bAvailable = !Offer.bOwned;
		break;
	}
	Offer.bAffordable = Run->GetGold() >= Offer.Price;
	return Offer;
}

bool ULevelShopWidget::IsItemEnabled(int32 Index) const
{
	const FOffer Offer = GetOffer(Index);
	return !bLeaving && Offer.bAvailable && Offer.bAffordable;
}

void ULevelShopWidget::ActivateItem(int32 Index)
{
	UBalloonRunSubsystem* Run = UBalloonRunSubsystem::Get(this);
	if (!Run || !IsItemEnabled(Index))
	{
		return;
	}
	const FLevelShopItem& Item = Items[Index];
	switch (Item.Kind)
	{
	case ELevelShopItemKind::Extinguisher:
		if (Run->HasExtinguisher())
		{
			Run->RefillExtinguisher(Item.RefillPrice, Item.RefillUnits);
		}
		else
		{
			Run->BuyExtinguisher(Item.Price);
		}
		break;
	case ELevelShopItemKind::Supply:
		Run->BuySupply(Item.Resource, Item.Price, Item.Units);
		break;
	case ELevelShopItemKind::Gear:
		Run->BuyGear(Item.GearClass, Item.Price);
		break;
	}
}

void ULevelShopWidget::ContinueTravel()
{
	if (bLeaving)
	{
		return;
	}
	if (UBalloonRunSubsystem* Run = UBalloonRunSubsystem::Get(this))
	{
		bLeaving = true;
		Run->ContinueTravel(this);
	}
}

void ULevelShopWidget::Refresh()
{
	const UBalloonRunSubsystem* Run = UBalloonRunSubsystem::Get(this);
	if (GoldText)
	{
		GoldText->SetText(FText::Format(GoldFormat, Run ? Run->GetGold() : 0));
	}
	for (int32 Index = 0; Index < ItemRows.Num(); ++Index)
	{
		const FOffer Offer = GetOffer(Index);
		FRow& Row = ItemRows[Index];
		Row.Count->SetText(Offer.Count);
		Row.Price->SetText(Offer.bOwned ? FText::GetEmpty() : FText::Format(PriceFormat, Offer.Price));
		Row.Owned->SetVisibility(Offer.bOwned ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		Row.Icon->SetColorAndOpacity(Offer.bOwned ? OwnedIconTint : IconTint);
		Row.Button->SetIsEnabled(IsItemEnabled(Index));
	}
}

void ULevelShopWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (Content && FadeInTime > 0.f && OpenTime < FadeInTime)
	{
		OpenTime += InDeltaTime;
		Content->SetRenderOpacity(FMath::SmoothStep(0.f, 1.f, FMath::Min(OpenTime / FadeInTime, 1.f)));
	}

	TickSlice(ClickSound, ClickAudio, ClickTimeLeft, InDeltaTime);
	TickSlice(HoverSound, HoverAudio, HoverTimeLeft, InDeltaTime);

	// One hover tock each time the cursor moves onto a usable row.
	UButton* Hovered = nullptr;
	for (int32 Index = 0; Index < ItemRows.Num() && !Hovered; ++Index)
	{
		if (ItemRows[Index].Button->IsHovered() && IsItemEnabled(Index))
		{
			Hovered = ItemRows[Index].Button;
		}
	}
	if (!Hovered && ContinueRow.Button && ContinueRow.Button->IsHovered() && !bLeaving)
	{
		Hovered = ContinueRow.Button;
	}
	if (Hovered != LastHovered.Get())
	{
		LastHovered = Hovered;
		if (Hovered)
		{
			PlaySlice(HoverSound, HoverAudio, HoverTimeLeft);
		}
	}

	for (int32 Index = 0; Index < ItemRows.Num(); ++Index)
	{
		FRow& Row = ItemRows[Index];
		const FOffer Offer = GetOffer(Index);
		const bool bEnabled = IsItemEnabled(Index);
		EaseRow(Row, Row.Button == Hovered, bEnabled, InDeltaTime);
		const float Ease = FMath::SmoothStep(0.f, 1.f, Row.Emphasis);
		// A row that can't be used keeps a readable name (it is still for sale) and count; owned gear is greyed out.
		if (Offer.bOwned)
		{
			Row.Text->SetColorAndOpacity(FSlateColor(DisabledTextColor));
		}
		else if (!bEnabled)
		{
			Row.Text->SetColorAndOpacity(FSlateColor(FMath::Lerp(DisabledTextColor, TextColor, 0.5f)));
		}
		Row.Count->SetColorAndOpacity(FSlateColor(bEnabled ? FMath::Lerp(TextColor, SelectedColor, Ease * 0.6f) : FMath::Lerp(DisabledTextColor, TextColor, 0.5f)));
		Row.Price->SetColorAndOpacity(FSlateColor(!Offer.bAvailable ? DisabledTextColor
			: !Offer.bAffordable ? ShortColor
			: FMath::Lerp(TextColor, SelectedColor, Ease)));
	}
	if (ContinueRow.Button)
	{
		EaseRow(ContinueRow, ContinueRow.Button == Hovered, true, InDeltaTime);
	}
}

void ULevelShopWidget::EaseRow(FRow& Row, bool bHovered, bool bEnabled, float DeltaTime) const
{
	Row.Emphasis = FMath::FInterpTo(Row.Emphasis, bHovered && bEnabled ? 1.f : 0.f, DeltaTime, EaseSpeed);
	const float Ease = FMath::SmoothStep(0.f, 1.f, Row.Emphasis);

	Row.Text->SetColorAndOpacity(FSlateColor(bEnabled ? FMath::Lerp(TextColor, SelectedColor, Ease) : DisabledTextColor));
	Row.Line->SetRenderTranslation(FVector2D(SelectedSlide * Ease, 0.f));

	FLinearColor Accent = SelectedColor;
	Accent.A *= Ease;
	Row.Marker->SetColorAndOpacity(Accent);
	Accent.A *= 0.8f;
	Row.Underline->SetColorAndOpacity(Accent);
	Row.Underline->SetDesiredSizeOverride(FVector2D(Row.Text->GetDesiredSize().X * Ease, UnderlineThickness));
}

void ULevelShopWidget::SetCursor(const FVector2D& WidgetPosition, bool bVisible)
{
	if (!ArrowCursor)
	{
		return;
	}
	ArrowCursor->SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (bVisible)
	{
		const FVector2D Snapped(FMath::RoundToFloat(WidgetPosition.X), FMath::RoundToFloat(WidgetPosition.Y));
		Cast<UCanvasPanelSlot>(ArrowCursor->Slot)->SetPosition(Snapped);
	}
}

void ULevelShopWidget::PlayClick()
{
	PlaySlice(ClickSound, ClickAudio, ClickTimeLeft);
}

void ULevelShopWidget::PlaySlice(const FMainMenuSound& Effect, TObjectPtr<UAudioComponent>& Audio, float& TimeLeft)
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
		// The game is paused while the shop is open; UI sounds still play.
		Audio->bIsUISound = true;
	}
	Audio->SetSound(Sound);
	Audio->SetVolumeMultiplier(Effect.Volume);
	Audio->Play(Effect.Start);
	TimeLeft = Effect.Length;
}

void ULevelShopWidget::TickSlice(const FMainMenuSound& Effect, UAudioComponent* Audio, float& TimeLeft, float DeltaTime) const
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

#undef LOCTEXT_NAMESPACE
