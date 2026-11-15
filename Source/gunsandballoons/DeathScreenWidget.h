#pragma once

#include "CoreMinimal.h"
#include "PauseMenuWidget.h"
#include "DeathScreenWidget.generated.h"

class UImage;
class UTextBlock;
class UTexture2D;

/**
 * The death screen (docs/features/ui-menus.md), shown by ADeathScreen when the engine is destroyed
 * (UEngineHealthComponent). It has the menus' font, rows, hover, sounds and pixel cursor, and shows
 * three things: the title ENGINE DESTROYED, what destroyed it (UEngineHealthComponent::GetDeathCause,
 * via ABalloonPlayerController), and two rows: RETURN TO CHECKPOINT (for now the start of the current level,
 * there is no checkpoint system yet) and MAIN MENU. The rows only take a click after InputDelay, so panicked
 * clicking from the fight can't press one by accident.
 *
 * Look (picked by the team from three, 2026-11-15): a silent-film title card, a double line border
 * around a dark field, over a sepia, grainy frame of the frozen game. ADeathScreen's
 * masked, dithered panel material turns its transparency into a PS1 dither. Values are first-pass, TBD.
 */
UCLASS(Blueprintable)
class GUNSANDBALLOONS_API UDeathScreenWidget : public UPauseMenuWidget
{
	GENERATED_BODY()

public:
	UDeathScreenWidget(const FObjectInitializer& ObjectInitializer);

	virtual void ScaleColors(float Brightness) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Death|Look")
	FSlateFontInfo CauseFont;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Death|Look")
	FLinearColor CauseColor = FLinearColor(0.70f, 0.66f, 0.52f, 1.f);

	/** Seconds before the rows take a click. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Death", meta = (ClampMin = "0.0"))
	float InputDelay = 1.f;

	// --- Title card ----------------------------------------------------------------------------

	/** Colour of the card's lines and text (old film white). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Death|Card")
	FLinearColor CardInkColor = FLinearColor(0.80f, 0.74f, 0.58f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Death|Card")
	FLinearColor CardFillColor = FLinearColor(0.008f, 0.006f, 0.004f, 0.96f);

	/** Sepia wash over the frozen game behind the card. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Death|Card")
	FLinearColor SepiaColor = FLinearColor(0.10f, 0.06f, 0.02f, 0.55f);

	/** Film grain opacity, and how often the grain jumps (per second). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Death|Card", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float GrainOpacity = 0.18f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Death|Card", meta = (ClampMin = "1.0"))
	float FilmFrameRate = 6.f; // was 12, too busy (2026-11-15). TBD

protected:
	virtual void BuildTree() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	/** The cause as the player controller reports it (a fallback text outside the game). */
	FText GetCauseText() const;
	void SetRowsClickable(bool bClickable);

	float ShownTime = 0.f;
	bool bRowsClickable = true;
	float FilmClock = 0.f;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> GrainTexture;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> VignetteTexture;

	UPROPERTY(Transient)
	TObjectPtr<UImage> Grain;

	UPROPERTY(Transient)
	TObjectPtr<UWidget> Card;
};
