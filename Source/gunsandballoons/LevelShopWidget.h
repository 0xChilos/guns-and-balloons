#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Fonts/SlateFontInfo.h"
#include "MainMenuWidget.h"
#include "LevelShopWidget.generated.h"

class UAudioComponent;
class UButton;
class UCanvasPanel;
class UImage;
class UTextBlock;
class UTexture2D;
class UVerticalBox;
class UWidget;

/** What a shop row sells. Each kind has its own buy/refill rules in ULevelShopWidget::GetOffer. */
UENUM(BlueprintType)
enum class ELevelShopItemKind : uint8
{
	/** The fire extinguisher: bought full (100/100) for Price; once owned, RefillUnits for RefillPrice. */
	Extinguisher
};

/** One row of the shop. Prices are TBD placeholders, tune them here (or in a Blueprint child of the widget). */
USTRUCT(BlueprintType)
struct FLevelShopItem
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop")
	ELevelShopItemKind Kind = ELevelShopItemKind::Extinguisher;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop")
	FText Name;

	/** Optional. Empty = a built-in pixel icon for Kind. Shown 16x16 pixels of the filter's frame. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop")
	TSoftObjectPtr<UTexture2D> Icon;

	/** Gold to buy it (it comes full). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop", meta = (ClampMin = "0"))
	int32 Price = 10;

	/** Gold for one refill once owned. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop", meta = (ClampMin = "0"))
	int32 RefillPrice = 1;

	/** Contents one refill adds (up to full). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop", meta = (ClampMin = "1.0"))
	float RefillUnits = 10.f;
};

/** UButton::OnClicked has no parameters, so each shop row reports through one of these. */
UCLASS()
class ULevelShopClickRelay : public UObject
{
	GENERATED_BODY()

public:
	TWeakObjectPtr<class ULevelShopWidget> Shop;

	/** Index into Items; INDEX_NONE = "Continue the travel". */
	int32 Index = INDEX_NONE;

	UFUNCTION()
	void HandleClicked();
};

/**
 * The shop between levels (docs/features/safe-zone.md), opened by UBalloonRunSubsystem::CompleteLevel and drawn on
 * ALevelShopScreen's panel in front of the camera, so the game's filter applies to it. Built in code with the main
 * menu's look (UMainMenuWidget): no box, a soft dark haze from the left edge, muted parchment text that warms to the
 * stove colour on hover with a lamp marker, a self-drawing underline and a slide to the right, a pixel cursor, the
 * clock tick/tock sounds and a fade-in.
 *
 * Layout, top to bottom on the left: title, the player's gold, one row per item (icon, name, what a click does,
 * price), and "Continue the travel" at the bottom, which opens the next level. A row the player can't use (gold
 * short, already full) is dimmed; a price the player can't afford is shown in ShortColor.
 *
 * Widget pixels at DrawHeight 1080: 4 of them are one pixel of the filter's frame. Values are first-pass, TBD.
 */
UCLASS(Blueprintable)
class GUNSANDBALLOONS_API ULevelShopWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	ULevelShopWidget(const FObjectInitializer& ObjectInitializer);

	/** Moves the pixel cursor to a point in widget pixels; hides it when bVisible is false. */
	void SetCursor(const FVector2D& WidgetPosition, bool bVisible);

	/** Multiplies the text and cursor colours, to balance the unlit panel against the scene's exposure. Call before it is built. */
	void ApplyBrightness(float Brightness);

	/** Buys or refills item Index (as a click on it does). */
	UFUNCTION(BlueprintCallable, Category = "Shop")
	void ActivateItem(int32 Index);

	/** Leaves the shop for the next level. */
	UFUNCTION(BlueprintCallable, Category = "Shop")
	void ContinueTravel();

	UFUNCTION(BlueprintPure, Category = "Shop")
	bool IsItemEnabled(int32 Index) const;

	/** Plays the click (does nothing while SFX is off). */
	void PlayClick();

	// --- Content ----------------------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop")
	TArray<FLevelShopItem> Items;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Text")
	FText Title;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Text")
	FText Subtitle;

	/** "GOLD {0}". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Text")
	FText GoldFormat;

	/** "{0} GOLD". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Text")
	FText PriceFormat;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Text")
	FText ContinueLabel;

	// --- Look (defaults match UMainMenuWidget) ----------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Look")
	FSlateFontInfo TitleFont;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Look")
	FSlateFontInfo ItemFont;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Look")
	FSlateFontInfo DetailFont;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Look")
	FLinearColor TextColor = FLinearColor(0.50f, 0.50f, 0.40f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Look")
	FLinearColor SelectedColor = FLinearColor(0.95f, 0.72f, 0.36f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Look")
	FLinearColor DisabledTextColor = FLinearColor(0.20f, 0.22f, 0.19f, 1.f);

	/** A price the player can't afford. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Look")
	FLinearColor ShortColor = FLinearColor(0.55f, 0.16f, 0.10f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Look")
	FLinearColor ShadowColor = FLinearColor(0.f, 0.005f, 0.005f, 0.85f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Look")
	FVector2D ShadowOffset = FVector2D(4.f, 4.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Look")
	FLinearColor HazeColor = FLinearColor(0.002f, 0.006f, 0.006f, 0.72f);

	/** Laid over the whole (paused) scene so the shop reads over a bright level. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Look")
	FLinearColor BackdropColor = FLinearColor(0.002f, 0.006f, 0.006f, 0.35f);

	/** The dark square behind each icon. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Look")
	FLinearColor IconBackColor = FLinearColor(0.002f, 0.006f, 0.006f, 0.55f);

	/** Multiplies the item and coin icons. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Look")
	FLinearColor IconTint = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Look")
	FLinearColor CursorColor =FLinearColor(0.85f, 0.80f, 0.62f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Look")
	FLinearColor CursorOutlineColor = FLinearColor(0.004f, 0.006f, 0.006f, 1.f);

	// --- Layout (widget pixels; 4 of them are one pixel of the filter's 270-row frame) --------

	/** Top-left corner of the title. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Layout")
	FVector2D ColumnPosition = FVector2D(136.f, 112.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Layout", meta = (ClampMin = "100.0"))
	float ColumnWidth = 1040.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Layout", meta = (ClampMin = "16.0"))
	float ItemRowHeight = 112.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Layout", meta = (ClampMin = "16.0"))
	float ContinueRowHeight = 80.f;

	/** Icon size: 16 frame pixels. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Layout", meta = (ClampMin = "8.0"))
	float IconSize = 64.f;

	/** Gap between "Continue the travel" and the bottom edge. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Layout", meta = (ClampMin = "0.0"))
	float BottomMargin = 112.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Layout", meta = (ClampMin = "0.0"))
	float SelectedSlide = 20.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Layout", meta = (ClampMin = "0.0"))
	float MarkerSize = 12.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Layout", meta = (ClampMin = "0.0"))
	float UnderlineThickness = 4.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Layout", meta = (ClampMin = "0.0"))
	float HazeWidth = 1500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Layout", meta = (ClampMin = "1.0"))
	float CursorPixelSize = 4.f;

	// --- Motion and sound ------------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Motion", meta = (ClampMin = "0.1"))
	float EaseSpeed = 9.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Motion", meta = (ClampMin = "0.0"))
	float FadeInTime = 0.8f;

	/** Same slices of the clock recording as the main menu. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Sound")
	FMainMenuSound ClickSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Sound")
	FMainMenuSound HoverSound;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	/** What a click on an item row would do right now. */
	struct FOffer
	{
		FText Detail;
		int32 Price = 0;
		bool bAvailable = false;
		bool bAffordable = false;
	};

	struct FRow
	{
		TObjectPtr<UButton> Button;
		TObjectPtr<UWidget> Line;
		TObjectPtr<UImage> Marker;
		TObjectPtr<UTextBlock> Text;
		TObjectPtr<UImage> Underline;
		/** Item rows only: what a click does, and the price. */
		TObjectPtr<UTextBlock> Detail;
		TObjectPtr<UTextBlock> Price;
		float Emphasis = 0.f;
	};

	FOffer GetOffer(int32 Index) const;
	void BuildTree();
	void MakeTextures();
	UTexture2D* GetIcon(const FLevelShopItem& Item);
	FRow MakeRow(const FText& Label, int32 Index, float Height);
	UTextBlock* MakeText(const FText& Text, const FSlateFontInfo& Font, const FLinearColor& Color);
	void EaseRow(FRow& Row, bool bHovered, bool bEnabled, float DeltaTime) const;
	void PlaySlice(const FMainMenuSound& Effect, TObjectPtr<UAudioComponent>& Audio, float& TimeLeft);
	void TickSlice(const FMainMenuSound& Effect, UAudioComponent* Audio, float& TimeLeft, float DeltaTime) const;

	/** Rewrites gold, details and prices from the run. */
	UFUNCTION()
	void Refresh();

	TArray<FRow> ItemRows;
	FRow ContinueRow;

	UPROPERTY(Transient)
	TArray<TObjectPtr<ULevelShopClickRelay>> Relays;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> HazeTexture;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> CursorTexture;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> CoinTexture;

	/** Built-in icons, one per ELevelShopItemKind. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTexture2D>> KindIcons;

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> Root;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> GoldText;

	UPROPERTY(Transient)
	TObjectPtr<UImage> ArrowCursor;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> ClickAudio;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> HoverAudio;

	float ClickTimeLeft = 0.f;
	float HoverTimeLeft = 0.f;
	TWeakObjectPtr<UButton> LastHovered;
	float OpenTime = 0.f;
	bool bLeaving = false;
};
