#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Fonts/SlateFontInfo.h"
#include "GameOptions.h"
#include "BalloonLevel/BalloonLevelStartOptions.h"
#include "MainMenuWidget.generated.h"

class UAudioComponent;
class UButton;
class UCanvasPanel;
class UImage;
class USoundBase;
class UTextBlock;
class UTexture2D;
class UVerticalBox;
class UWidget;
class UWidgetSwitcher;
class UComboBoxString;
class USpinBox;
class USizeBox;

/** What a main menu item does when clicked. */
UENUM(BlueprintType)
enum class EMainMenuAction : uint8
{
	Continue,
	NewRun,
	Credits,
	Options,
	/** Leave the game (the main menu's QUIT, the pause menu's LEAVE THE GAME). */
	Quit,
	/** Pause menu: close it and unpause. */
	Resume,
	/** Pause menu: open MainMenuLevel; the run is lost. */
	QuitToMenu
};

/** One row of the main menu. */
USTRUCT(BlueprintType)
struct FMainMenuItem
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu")
	FText Label;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu")
	EMainMenuAction Action = EMainMenuAction::NewRun;
};

/**
 * A menu sound effect: a slice of a sound asset, so one longer recording can give several short
 * effects. Silent while the SFX switch is off.
 */
USTRUCT(BlueprintType)
struct FMainMenuSound
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu")
	TSoftObjectPtr<USoundBase> Sound;

	/** Where the slice starts in Sound (s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu", meta = (ClampMin = "0.0"))
	float Start = 0.f;

	/** How long the slice plays before it fades out (s); 0 plays the sound to its end. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu", meta = (ClampMin = "0.0"))
	float Length = 0.f;

	/** Fade at the end of the slice so the cut does not pop (s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu", meta = (ClampMin = "0.0"))
	float FadeOut = 0.04f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu", meta = (ClampMin = "0.0"))
	float Volume = 1.f;
};

/** One on/off row of the Options page. */
USTRUCT(BlueprintType)
struct FMainMenuToggle
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu")
	FText Label;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu")
	EGameOption Option = EGameOption::Music;

	/** Shown on the right while the option is on / off (ON/OFF, YES/NO). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu")
	FText OnText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu")
	FText OffText;
};

/** What a clicked row is: a main item, a page's Back row, or an Options switch. */
enum class EMainMenuRowKind : uint8
{
	Item,
	Back,
	Toggle,
 StartLevel
};

/** UButton::OnClicked has no parameters, so each row's button reports through one of these. */
UCLASS()
class UMainMenuClickRelay : public UObject
{
	GENERATED_BODY()

public:
	TWeakObjectPtr<class UMainMenuWidget> Menu;

	EMainMenuRowKind Kind = EMainMenuRowKind::Item;

	/** Index into Items or OptionToggles. */
	int32 Index = INDEX_NONE;

	UFUNCTION()
	void HandleClicked();
};

/**
 * The main menu (docs/features/ui-menus.md), built in code so it needs no Widget Blueprint.
 *
 * Drawn on AMainMenuScreen's panel in the 3D scene, so the game's filter (MI_OilyDepths) applies to
 * it too. No box: the items float over the scene on a soft dark haze that fades in from the left
 * edge. The game's name (GUNS AND BALLOONS) and a subtitle (DIRECTOR'S CUT) sit top left, above the
 * items; a small version label sits bottom right.
 *
 * Mouse only: hovering a row selects it, LMB confirms. The selected row eases to the right, warms
 * to the colour of the stove and searchlights, a small lamp marker fades in and an underline draws
 * itself under the word; everything eases rather than snaps. The menu fades in when it opens. A
 * pixel arrow cursor follows the mouse. Continue stays disabled until there are saves
 * (bContinueAvailable). Credits opens a text page; Options opens the on/off switches
 * (UGameOptions: Music, SFX, Tutorial), each flipped by a click and saved at once. Both have Back.
 *
 * Layout in widget pixels: the panel is DrawHeight = 1080 rows high and as wide as the view, so 4
 * widget pixels make one pixel of the filter's frame. Make a Blueprint child to tune the look and
 * the texts. Values are first-pass, TBD.
 */
UCLASS(Blueprintable)
class GUNSANDBALLOONS_API UMainMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UMainMenuWidget(const FObjectInitializer& ObjectInitializer);

	/** Moves the pixel cursor to a point in widget pixels; hides it when bVisible is false. */
	void SetCursor(const FVector2D& WidgetPosition, bool bVisible);

	/** Runs the action of row Index (as a click on it does). */
	UFUNCTION(BlueprintCallable, Category = "Menu")
	void ActivateItem(int32 Index);
 UFUNCTION(BlueprintCallable, Category="Menu") void StartConfiguredLevel();
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Menu|Level Start") FBalloonLevelStartOptions LevelStartOptions;

	/** Leaves a Credits/Options page for the item list. */
	UFUNCTION(BlueprintCallable, Category = "Menu")
	void ShowItemList();

	/** Flips Options switch Index (as a click on it does) and saves it. */
	UFUNCTION(BlueprintCallable, Category = "Menu")
	void ToggleOption(int32 Index);

	UFUNCTION(BlueprintPure, Category = "Menu")
	int32 GetSelectedIndex() const { return Selected; }

	UFUNCTION(BlueprintPure, Category = "Menu")
	bool IsItemEnabled(int32 Index) const;

	// --- Content ----------------------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu")
	TArray<FMainMenuItem> Items;

	/** Level New Run opens. TBD: the real first level of a run. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu")
	TSoftObjectPtr<UWorld> NewRunLevel;

	/** Level Quit to Menu opens. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu")
	TSoftObjectPtr<UWorld> MainMenuLevel;

	/** Continue is clickable only when true. There is no save system yet. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu")
	bool bContinueAvailable = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Pages")
	FText CreditsTitle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Pages", meta = (MultiLine = "true"))
	FText CreditsText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Pages")
	FText OptionsTitle;

	/** The switches on the Options page. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Pages")
	TArray<FMainMenuToggle> OptionToggles;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Pages")
	FText BackLabel;

	/** The game's name, top left above the item list. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Title", meta = (MultiLine = "true"))
	FText GameTitle;

	/** Smaller line under the game's name. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Title")
	FText GameSubtitle;

	/** Shown after the project version (Project Settings > Description > Project Version). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Version")
	FText VersionSuffix;

	// --- Look -------------------------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Look")
	FSlateFontInfo ItemFont;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Look")
	FSlateFontInfo PageFont;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Look")
	FSlateFontInfo VersionFont;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Look")
	FSlateFontInfo GameTitleFont;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Look")
	FSlateFontInfo GameSubtitleFont;

	/** The game's name: the items' muted parchment, a touch brighter so it reads as the title. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Look")
	FLinearColor GameTitleColor = FLinearColor(0.56f, 0.55f, 0.44f, 1.f);

	/** The subtitle: a dim brass, the selected warmth turned down. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Look")
	FLinearColor GameSubtitleColor = FLinearColor(0.52f, 0.43f, 0.27f, 1.f);

	/** Resting item text: a muted parchment that sits in the fog. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Look")
	FLinearColor TextColor = FLinearColor(0.50f, 0.50f, 0.40f, 1.f);

	/** The selected row: the warm light of the stove and searchlights. Also the marker and underline. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Look")
	FLinearColor SelectedColor = FLinearColor(0.95f, 0.72f, 0.36f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Look")
	FLinearColor DisabledTextColor = FLinearColor(0.20f, 0.22f, 0.19f, 1.f);

	/** One-pixel drop shadow under all text, so it holds against the fog. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Look")
	FLinearColor ShadowColor = FLinearColor(0.f, 0.005f, 0.005f, 0.85f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Look")
	FVector2D ShadowOffset = FVector2D(4.f, 4.f);

	/** The haze behind the items: darkest at the left edge, gone by HazeWidth. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Look")
	FLinearColor HazeColor = FLinearColor(0.002f, 0.006f, 0.006f, 0.72f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Look")
	FLinearColor CursorColor = FLinearColor(0.85f, 0.80f, 0.62f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Look")
	FLinearColor CursorOutlineColor = FLinearColor(0.004f, 0.006f, 0.006f, 1.f);

	// --- Layout (widget pixels; 4 of them are one pixel of the filter's 270-row frame) --------

	/** Top-left corner of the item list. Everything above it stays empty for the title. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Layout")
	FVector2D ListPosition = FVector2D(136.f, 480.f);

	/** Top-left corner of the title block (the game's name and the subtitle under it). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Layout")
	FVector2D GameTitlePosition = FVector2D(136.f, 72.f);

	/** Gap between the game's name and the subtitle. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Layout", meta = (ClampMin = "0.0"))
	float GameSubtitleGap = 12.f;

	/** Width of the item list and of the pages' text. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Layout", meta = (ClampMin = "100.0"))
	float ListWidth = 640.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Layout", meta = (ClampMin = "16.0"))
	float RowHeight = 80.f;

	/** How far the selected row eases to the right. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Layout", meta = (ClampMin = "0.0"))
	float SelectedSlide = 20.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Layout", meta = (ClampMin = "0.0"))
	float MarkerSize = 12.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Layout", meta = (ClampMin = "0.0"))
	float UnderlineThickness = 4.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Layout", meta = (ClampMin = "0.0"))
	float HazeWidth = 1150.f;

	/** Gap between the version label and the bottom-right corner. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Layout")
	FVector2D VersionMargin = FVector2D(64.f, 48.f);

	/** Widget pixels per pixel of the arrow cursor (4 = one pixel of the filter's frame). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Layout", meta = (ClampMin = "1.0"))
	float CursorPixelSize = 4.f;

	// --- Motion -----------------------------------------------------------------------------

	/** How quickly a row eases into and out of the selected look (1/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Motion", meta = (ClampMin = "0.1"))
	float EaseSpeed = 9.f;

	/** How long the menu takes to fade in when it opens (s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Motion", meta = (ClampMin = "0.0"))
	float FadeInTime = 1.2f;

	// --- Sound ------------------------------------------------------------------------------

	/**
	 * Played when a row is clicked. Defaults to the high tick of the 20 s ticking-clock loop in
	 * /Game/Audio/Main_Menu_Button_Click (about 0.16-0.29 s). TBD, tune by ear.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Sound")
	FMainMenuSound ClickSound;

	/**
	 * Played when the cursor moves onto a row. Defaults to the lower, quieter tock of the same
	 * loop (about 1.18-1.30 s). TBD, tune by ear.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Sound")
	FMainMenuSound HoverSound;

	/** Plays the click (does nothing while SFX is off). */
	UFUNCTION(BlueprintCallable, Category = "Menu")
	void PlayClick();

	/** Multiplies the menu's colours (not their opacity). Call before the widget is built. */
	virtual void ScaleColors(float Brightness);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void NativeDestruct() override;

	/** The widgets of one clickable row, and how far it has eased toward the selected look (0..1). */
	struct FRow
	{
		TObjectPtr<UButton> Button;
		TObjectPtr<UWidget> Line;
		TObjectPtr<UImage> Marker;
		TObjectPtr<UTextBlock> Text;
		TObjectPtr<UImage> Underline;
		/** Right-aligned value of an Options switch (ON/OFF); null on other rows. */
		TObjectPtr<UTextBlock> Value;
		float Emphasis = 0.f;
	};

	/** Builds the whole widget tree. A menu with another layout (UPauseMenuWidget) overrides it. */
	virtual void BuildTree();
	/** Makes HazeTexture and CursorTexture. */
	void MakeTextures();
	FRow MakeRow(UVerticalBox* Parent, const FText& Label, EMainMenuRowKind Kind, int32 Index);
	UTextBlock* MakeText(const FText& Text, const FSlateFontInfo& Font, const FLinearColor& Color);
	/** Adds the pixel arrow cursor to Root. Call last, so it is drawn on top. */
	void AddCursor();
	/** Selects the first enabled row and restarts the fade-in. Call at the end of BuildTree. */
	void ResetOpenState();

	/** A small runtime texture, filled pixel by pixel. */
	static UTexture2D* MakeRuntimeTexture(int32 Width, int32 Height, TextureFilter Filter, TFunctionRef<FColor(int32, int32)> Pixel);

	/** Where each row's marker and word sit in the row: left (main menu) or centred (pause menu). */
	TEnumAsByte<EHorizontalAlignment> RowAlignment = HAlign_Fill;

	TArray<FRow> Rows;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMainMenuClickRelay>> Relays;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> HazeTexture;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> CursorTexture;

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> Root;

	UPROPERTY(Transient)
	TObjectPtr<UImage> ArrowCursor;

	int32 Selected = INDEX_NONE;
	float OpenTime = 0.f;

private:
 void BuildLevelStartPage();
 void ShowLevelStartPage();
 UFUNCTION() UWidget* MakeLevelChoiceText(FString Value);
 UPROPERTY(Transient) TObjectPtr<UComboBoxString> BiomeChoice;
 UPROPERTY(Transient) TObjectPtr<UComboBoxString> CycleChoice;
 UPROPERTY(Transient) TArray<TObjectPtr<USpinBox>> LevelNumbers;
 UPROPERTY(Transient) TObjectPtr<UTextBlock> LevelStartError;
 UPROPERTY(Transient) TObjectPtr<USizeBox> PageWidth;
 FRow StartLevelRow;
 FRow LevelBackRow;
	UVerticalBox* MakePage(const FName& Name, UTextBlock*& OutTitle);
	void ShowOptions();
	void RefreshToggleValue(int32 Index);
	void ShowPage(const FText& Title, const FText& Body);
	void EaseRow(FRow& Row, bool bSelected, bool bEnabled, float DeltaTime) const;
	UButton* FindHoveredButton() const;
	void PlaySlice(const FMainMenuSound& Effect, TObjectPtr<UAudioComponent>& Audio, float& TimeLeft);
	void TickSlice(const FMainMenuSound& Effect, UAudioComponent* Audio, float& TimeLeft, float DeltaTime) const;

	TArray<FRow> ToggleRows;
	FRow BackRow;
	FRow OptionsBackRow;

	UPROPERTY(Transient)
	TObjectPtr<UWidgetSwitcher> Pages;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PageTitle;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PageBody;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> OptionsPageTitle;

	/** Click and hover play on their own audio components, so a hover never cuts a click short. */
	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> ClickAudio;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> HoverAudio;

	/** Time until each playing slice fades out (s); 0 when none is cut short. */
	float ClickTimeLeft = 0.f;
	float HoverTimeLeft = 0.f;

	/** The row button under the cursor last frame, to play the hover sound once per row entered. */
	TWeakObjectPtr<UButton> LastHovered;
};
