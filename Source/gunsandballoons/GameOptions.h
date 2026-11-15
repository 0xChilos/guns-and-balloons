#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "GameOptions.generated.h"

/** A player-facing on/off switch from the main menu's Options page. */
UENUM(BlueprintType)
enum class EGameOption : uint8
{
	Music,
	Sfx,
	Tutorial
};

/**
 * The player's options (docs/features/ui-menus.md), saved per user in GameUserSettings.ini so they
 * survive restarts. Set from the main menu's Options page; read them anywhere with IsOn.
 *
 * So far the main menu's click honours Sfx, and the main menu and gameplay music honour Music (they
 * listen to OnChanged); the game has no other sound effects or tutorial yet. When those are added,
 * play music and SFX through their own Sound Classes and silence a class while its switch is off
 * (e.g. a Sound Mix that sets its volume to 0), and skip the tutorial when Tutorial is off.
 */
UCLASS(Config = GameUserSettings)
class GUNSANDBALLOONS_API UGameOptions : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Options")
	static bool IsOn(EGameOption Option);

	/** Changes a switch and saves it at once. */
	UFUNCTION(BlueprintCallable, Category = "Options")
	static void SetOn(EGameOption Option, bool bOn);

	/** Broadcast by SetOn after a switch changes (option, new value). */
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnOptionChanged, EGameOption, bool);
	static FOnOptionChanged& OnChanged();

private:
	UPROPERTY(Config)
	bool bMusic = true;

	UPROPERTY(Config)
	bool bSfx = true;

	UPROPERTY(Config)
	bool bTutorial = true;

	bool& Field(EGameOption Option);
};
