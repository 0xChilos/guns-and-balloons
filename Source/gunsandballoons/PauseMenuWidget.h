#pragma once

#include "CoreMinimal.h"
#include "MainMenuWidget.h"
#include "PauseMenuWidget.generated.h"

class UImage;
class UTexture2D;

/** One layer of the pause menu's smoke: a tiling noise texture drifting across the screen. */
USTRUCT(BlueprintType)
struct FPauseSmokeLayer
{
	GENERATED_BODY()

	/** Size of one noise tile (widget pixels). Bigger = larger, slower-looking billows. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Smoke", meta = (ClampMin = "64.0"))
	float Tile = 1000.f;

	/** Drift (widget pixels per second); negative Y rises. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Smoke")
	FVector2D Drift = FVector2D(0.f, -10.f);

	/** Opacity where this layer is densest. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Smoke", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Opacity = 0.3f;

	/** Picks the layer's noise pattern. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Smoke")
	int32 Seed = 1;
};

/**
 * The pause menu (docs/features/ui-menus.md): the main menu's font, rows, hover, sounds and cursor,
 * with less on screen. A PAUSE MENU title at the top and three centred rows below it: RETURN TO GAME,
 * QUIT TO MAIN MENU, LEAVE THE GAME.
 *
 * Behind them the whole screen is smoke, but see-through: a few layers of swirling noise (made at
 * runtime, tiling, never fully thin) drift and rise at different speeds over the frozen game, a
 * little denser towards the edges. Shown by APauseMenuScreen on a filtered panel in front of the
 * player's camera. Values are first-pass, TBD.
 */
UCLASS(Blueprintable)
class GUNSANDBALLOONS_API UPauseMenuWidget : public UMainMenuWidget
{
	GENERATED_BODY()

public:
	UPauseMenuWidget(const FObjectInitializer& ObjectInitializer);

	virtual void ScaleColors(float Brightness) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pause")
	FText Title;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pause|Look")
	FSlateFontInfo TitleFont;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pause|Look")
	FLinearColor TitleColor = FLinearColor(0.78f, 0.74f, 0.58f, 1.f);

	/** A light flat tint under the smoke, so the text reads. Its alpha is kept low: the game must show. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pause|Smoke")
	FLinearColor VeilColor = FLinearColor(0.004f, 0.008f, 0.008f, 0.22f);

	/** Colour of the smoke (its alpha scales every layer). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pause|Smoke")
	FLinearColor SmokeColor = FLinearColor(0.16f, 0.17f, 0.16f, 1.f);

	/** The layers, back to front. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pause|Smoke")
	TArray<FPauseSmokeLayer> SmokeLayers;

	/** Thinnest smoke as a fraction of the densest, so no part of the screen is ever clear. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pause|Smoke", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SmokeFloor = 0.4f;

	/** Extra smoke towards the screen's edges (opacity at the corners), framing the menu. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pause|Smoke", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float EdgeSmoke = 0.45f;

	/** The smoke slowly thickens and thins by this fraction, over this period (s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pause|Smoke", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SmokeBreath = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pause|Smoke", meta = (ClampMin = "0.1"))
	float SmokeBreathPeriod = 7.f;

	/** Top of the title, from the top of the panel (widget pixels; the panel is 1080 high). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pause|Layout")
	float TitleTop = 230.f;

	/** A thin rule under the title (widget pixels); 0 hides it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pause|Layout", meta = (ClampMin = "0.0"))
	float TitleRuleWidth = 440.f;

	/** Top of the first row. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pause|Layout")
	float ListTop = 470.f;

protected:
	virtual void BuildTree() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void MoveSmoke();

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTexture2D>> SmokeTextures;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> SmokeImages;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> EdgeTexture;

	float SmokeTime = 0.f;
};

/**
 * A small hint in the bottom-right corner during play: a key cap with the pause key and the word
 * PAUSE. Bright when the level starts, then it dims but stays. Shown by APauseHintScreen.
 */
UCLASS(Blueprintable)
class GUNSANDBALLOONS_API UPauseHintWidget : public UMainMenuWidget
{
	GENERATED_BODY()

public:
	UPauseHintWidget(const FObjectInitializer& ObjectInitializer);

	virtual void ScaleColors(float Brightness) override;

	/** Shown on the key cap. P: in the editor Esc stops Play In Editor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hint")
	FText KeyLabel;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hint")
	FText Label;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hint")
	FSlateFontInfo HintFont;

	/** Fill behind the key's letter. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hint")
	FLinearColor KeyFillColor = FLinearColor(0.004f, 0.006f, 0.006f, 0.6f);

	/** Gap between the hint and the bottom-right corner (widget pixels). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hint")
	FVector2D Margin = FVector2D(64.f, 48.f);

	/** How long the hint stays bright before it dims (s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hint", meta = (ClampMin = "0.0"))
	float BrightTime = 10.f;

	/** Opacity once dimmed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hint", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DimmedOpacity = 0.4f;

protected:
	virtual void BuildTree() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	float ShownTime = 0.f;
};
