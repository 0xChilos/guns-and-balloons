#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "GameOptions.h"
#include "MainMenuPlayerController.generated.h"

class UAudioComponent;
class USoundBase;

/**
 * Player controller of the main menu level: mouse only (docs/features/ui-menus.md).
 *
 * The mouse moves freely over the view with the system cursor hidden (the menu draws its own pixel
 * cursor, so the filter applies to it too). LMB is forwarded to every AMainMenuScreen in the level.
 * The view comes from the level's camera (its Auto Activate for Player 0); there is no pawn.
 *
 * Also plays the menu music, looped, quietly, and only while the Music option is on.
 */
UCLASS(Blueprintable)
class GUNSANDBALLOONS_API AMainMenuPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AMainMenuPlayerController();

	/** Looped for as long as the menu is open. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Music")
	TSoftObjectPtr<USoundBase> Music;

	/** Volume multiplier. The track is mastered loud (about -14 LUFS); 0.25 keeps it in the background. TBD, tune by ear. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Music", meta = (ClampMin = "0", ClampMax = "1"))
	float MusicVolume = 0.25f;

	/** Seconds to fade in when the menu opens or Music is switched on. TBD */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Music", meta = (ClampMin = "0"))
	float MusicFadeIn = 2.f;

	/** Seconds to fade out when Music is switched off. TBD */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Music", meta = (ClampMin = "0"))
	float MusicFadeOut = 0.75f;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void SetupInputComponent() override;

private:
	void PointerPressed();
	void PointerReleased();

	void StartMusic();
	void StopMusic();
	void OnOptionChanged(EGameOption Option, bool bOn);

	/** Restarts the track when it ends, so it loops even if the sound asset itself isn't set to loop. */
	UFUNCTION()
	void OnMusicFinished();

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> MusicAudio;

	bool bMusicWanted = false;
	FDelegateHandle OptionChangedHandle;
};
