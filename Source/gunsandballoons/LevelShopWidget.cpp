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
#include "Components/SizeBox.h"
#include "Components/SizeBoxSlot.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "GameOptions.h"
#include "Kismet/GameplayStatics.h"
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

	FLevelShopItem MakeItem(ELevelShopItemKind Kind, const FText& Name, int32 Price, int32 RefillPrice, float RefillUnits)
	{
		FLevelShopItem Item;
		Item.Kind = Kind;
		Item.Name = Name;
		Item.Price = Price;
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
	// TBD prices: bought full for 10 gold; each refill adds 10 units for 1 gold.
	Items = { MakeItem(ELevelShopItemKind::Extinguisher, LOCTEXT("Extinguisher", "EXTINGUISHER"), 10, 1, 10.f) };

	Title = LOCTEXT("Title", "SHOP");
	Subtitle = LOCTEXT("Subtitle", "LEVEL CLEARED");
	GoldFormat = LOCTEXT("GoldFormat", "GOLD {0}");
	PriceFormat = LOCTEXT("PriceFormat", "{0} GOLD");
	ContinueLabel = LOCTEXT("Continue", "CONTINUE THE TRAVEL");

	TitleFont = MakeFont(60, 80);
	ItemFont = MakeFont(44, 60);
	DetailFont = MakeFont(30, 40);

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
	for (FLinearColor* Color : {&TextColor, &SelectedColor, &DisabledTextColor, &ShortColor, &CursorColor, &IconTint})
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
	const int32 Kind = int32(Item.Kind);
	return KindIcons.IsValidIndex(Kind) ? KindIcons[Kind] : nullptr;
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

	// A light veil over the paused level, then the main menu's haze from the left edge.
	UImage* Backdrop = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Backdrop"));
	Backdrop->SetBrush(SolidBrush(FVector2D(4.f)));
	Backdrop->SetColorAndOpacity(BackdropColor);
	Backdrop->SetVisibility(ESlateVisibility::HitTestInvisible);
	UCanvasPanelSlot* BackdropSlot = Root->AddChildToCanvas(Backdrop);
	BackdropSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
	BackdropSlot->SetOffsets(FMargin(0.f));

	UImage* Haze = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Haze"));
	Haze->SetBrush(TextureBrush(HazeTexture, FVector2D(HazeWidth, 4.f)));
	Haze->SetColorAndOpacity(HazeColor);
	Haze->SetVisibility(ESlateVisibility::HitTestInvisible);
	UCanvasPanelSlot* HazeSlot = Root->AddChildToCanvas(Haze);
	HazeSlot->SetAnchors(FAnchors(0.f, 0.f, 0.f, 1.f));
	HazeSlot->SetOffsets(FMargin(0.f, 0.f, HazeWidth, 0.f));

	// Title, gold and the items, from the top left.
	USizeBox* ColumnBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("Column"));
	ColumnBox->SetWidthOverride(ColumnWidth);
	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ColumnList"));
	ColumnBox->SetContent(Column);

	Column->AddChildToVerticalBox(MakeText(Title, TitleFont, SelectedColor));
	Column->AddChildToVerticalBox(MakeText(Subtitle, DetailFont, TextColor))->SetPadding(FMargin(0.f, 4.f, 0.f, 36.f));

	UHorizontalBox* GoldLine = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("Gold"));
	UImage* Coin = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	Coin->SetBrush(TextureBrush(CoinTexture, FVector2D(32.f)));
	Coin->SetColorAndOpacity(IconTint);
	UHorizontalBoxSlot* CoinSlot = GoldLine->AddChildToHorizontalBox(Coin);
	CoinSlot->SetVerticalAlignment(VAlign_Center);
	CoinSlot->SetPadding(FMargin(0.f, 0.f, 24.f, 0.f));
	GoldText = MakeText(FText::GetEmpty(), ItemFont, SelectedColor);
	GoldLine->AddChildToHorizontalBox(GoldText)->SetVerticalAlignment(VAlign_Center);
	Column->AddChildToVerticalBox(GoldLine)->SetPadding(FMargin(0.f, 0.f, 0.f, 40.f));

	for (int32 Index = 0; Index < Items.Num(); ++Index)
	{
		FRow Row = MakeRow(Items[Index].Name, Index, ItemRowHeight);
		Column->AddChildToVerticalBox(Row.Button->GetParent());
		ItemRows.Add(Row);
	}

	UCanvasPanelSlot* ColumnSlot = Root->AddChildToCanvas(ColumnBox);
	ColumnSlot->SetAutoSize(true);
	ColumnSlot->SetPosition(ColumnPosition);

	// "Continue the travel", at the bottom.
	ContinueRow = MakeRow(ContinueLabel, INDEX_NONE, ContinueRowHeight);
	USizeBox* ContinueBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("ContinueBox"));
	ContinueBox->SetWidthOverride(ColumnWidth);
	ContinueBox->SetContent(ContinueRow.Button->GetParent());
	UCanvasPanelSlot* ContinueSlot = Root->AddChildToCanvas(ContinueBox);
	ContinueSlot->SetAutoSize(true);
	ContinueSlot->SetAnchors(FAnchors(0.f, 1.f));
	ContinueSlot->SetAlignment(FVector2D(0.f, 1.f));
	ContinueSlot->SetPosition(FVector2D(ColumnPosition.X, -BottomMargin));

	ArrowCursor = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Cursor"));
	ArrowCursor->SetBrush(TextureBrush(CursorTexture, FVector2D(CursorTexture ? CursorTexture->GetSizeX() : 7, CursorTexture ? CursorTexture->GetSizeY() : 10) * CursorPixelSize));
	ArrowCursor->SetVisibility(ESlateVisibility::Collapsed);
	Root->AddChildToCanvas(ArrowCursor)->SetAutoSize(true);

	OpenTime = 0.f;
	Root->SetRenderOpacity(FadeInTime > 0.f ? 0.f : 1.f);
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
		// Icon on a dark square.
		UBorder* IconBack = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		IconBack->SetBrush(SolidBrush(FVector2D(4.f)));
		IconBack->SetBrushColor(IconBackColor);
		IconBack->SetPadding(FMargin(8.f));
		UImage* Icon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		Icon->SetBrush(TextureBrush(GetIcon(Items[Index]), FVector2D(IconSize)));
		Icon->SetColorAndOpacity(IconTint);
		IconBack->SetContent(Icon);
		UHorizontalBoxSlot* IconSlot = Line->AddChildToHorizontalBox(IconBack);
		IconSlot->SetVerticalAlignment(VAlign_Center);
		IconSlot->SetPadding(FMargin(0.f, 0.f, 32.f, 0.f));
	}

	// Name with its underline; under it, what a click does (items only).
	UVerticalBox* Info = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	UVerticalBox* Word = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Row.Text = MakeText(Label, ItemFont, TextColor);
	Word->AddChildToVerticalBox(Row.Text);
	Row.Underline = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	Row.Underline->SetBrush(SolidBrush(FVector2D(0.f, UnderlineThickness)));
	Row.Underline->SetColorAndOpacity(FLinearColor::Transparent);
	UVerticalBoxSlot* UnderlineSlot = Word->AddChildToVerticalBox(Row.Underline);
	UnderlineSlot->SetHorizontalAlignment(HAlign_Left);
	UnderlineSlot->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));
	Info->AddChildToVerticalBox(Word)->SetHorizontalAlignment(HAlign_Left);
	if (bItem)
	{
		Row.Detail = MakeText(FText::GetEmpty(), DetailFont, TextColor);
		Info->AddChildToVerticalBox(Row.Detail)->SetPadding(FMargin(0.f, 6.f, 0.f, 0.f));
	}
	UHorizontalBoxSlot* InfoSlot = Line->AddChildToHorizontalBox(Info);
	InfoSlot->SetVerticalAlignment(VAlign_Center);
	InfoSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	if (bItem)
	{
		Row.Price = MakeText(FText::GetEmpty(), ItemFont, TextColor);
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
		const int32 Full = FMath::RoundToInt(UBalloonRunSubsystem::ExtinguisherFull);
		if (!Run->HasExtinguisher())
		{
			Offer.Price = Item.Price;
			Offer.bAvailable = true;
			Offer.Detail = FText::Format(LOCTEXT("ExtinguisherBuy", "BUY - COMES FULL {0}/{0}"), Full);
		}
		else
		{
			// Like the counter on the tank, a part-used unit still counts.
			const int32 Amount = FMath::CeilToInt(Run->GetExtinguisherAmount() - KINDA_SMALL_NUMBER);
			Offer.Price = Item.RefillPrice;
			Offer.bAvailable = Run->GetExtinguisherAmount() < UBalloonRunSubsystem::ExtinguisherFull;
			Offer.Detail = Offer.bAvailable
				? FText::Format(LOCTEXT("ExtinguisherRefill", "REFILL +{0} - {1}/{2}"), FMath::RoundToInt(Item.RefillUnits), Amount, Full)
				: FText::Format(LOCTEXT("ExtinguisherFull", "FULL - {0}/{0}"), Full);
		}
		break;
	}
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
		Row.Detail->SetText(Offer.Detail);
		Row.Price->SetText(FText::Format(PriceFormat, Offer.Price));
		Row.Button->SetIsEnabled(IsItemEnabled(Index));
	}
}

void ULevelShopWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (Root && FadeInTime > 0.f && OpenTime < FadeInTime)
	{
		OpenTime += InDeltaTime;
		Root->SetRenderOpacity(FMath::SmoothStep(0.f, 1.f, FMath::Min(OpenTime / FadeInTime, 1.f)));
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
		// A row that can't be used keeps a readable name (it is still for sale) but dims what a click would do.
		if (!bEnabled)
		{
			Row.Text->SetColorAndOpacity(FSlateColor(FMath::Lerp(DisabledTextColor, TextColor, 0.5f)));
		}
		Row.Detail->SetColorAndOpacity(FSlateColor(bEnabled ? FMath::Lerp(TextColor, SelectedColor, Ease * 0.6f) : DisabledTextColor));
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
