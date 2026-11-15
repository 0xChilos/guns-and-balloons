#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "GameOptions.h"
#include "InputCoreTypes.h"
#include "BalloonPlayerController.generated.h"

class ADeathScreen;
class APauseHintScreen;
class APauseMenuScreen;
class UAudioComponent;
class USoundBase;

/**
 * Owns gameplay/UI input policy; the possessed pawn routes gameplay actions to equipment.
 *
 * Also owns the pause menu (docs/features/ui-menus.md): a PauseKeys press pauses the world and
 * spawns an APauseMenuScreen in front of the camera; another press, or RETURN TO GAME, resumes.
 * While paused the mouse is free, the system cursor hidden (the menu draws its own), the crosshair
 * HUD hidden, and LMB goes to the menu's pointer. During play a small APauseHintScreen shows the key.
 *
 * And the death screen (docs/features/ui-menus.md): when the engine is destroyed
 * (UEngineHealthComponent) the world slows down for DeathSlowMoTime with a blast at the basket, then
 * pauses under an ADeathScreen (ENGINE DESTROYED, the cause, TRY AGAIN). The console command
 * "DeathScreen" plays it at once, to see it without dying.
 *
 * And the gameplay music (docs/features/ui-menus.md): Music plays from the top when the level
 * starts (every map load: new run, TRY AGAIN, the next level) and loops. It pauses whenever a menu
 * takes the input (UseMenuInput: the pause menu, a future shop) and resumes where it stopped on
 * UseGameplayInput. It fades out for good on death, and follows the Music option.
 */
UCLASS(Blueprintable)
class GUNSANDBALLOONS_API ABalloonPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	/** Call when closing a menu. Does not change possession or pause the world. */
	UFUNCTION(BlueprintCallable, Category="Player|Input")
	void UseGameplayInput();

	/** Call after opening a menu. Stops held gameplay actions before giving input to UI. */
	UFUNCTION(BlueprintCallable, Category="Player|Input")
	void UseMenuInput();

	UFUNCTION(BlueprintPure, Category="Player|Input")
	bool IsGameplayInputEnabled() const { return bGameplayInputEnabled; }

	/** Pauses the world and opens the pause menu. Does nothing if it is already open. */
	UFUNCTION(BlueprintCallable, Category="Player|Pause")
	void PauseGame();

	/** Closes the pause menu and unpauses the world. */
	UFUNCTION(BlueprintCallable, Category="Player|Pause")
	void ResumeGame();

	UFUNCTION(BlueprintCallable, Category="Player|Pause")
	void TogglePause();

	UFUNCTION(BlueprintPure, Category="Player|Pause")
	bool IsPauseMenuOpen() const { return PauseMenu != nullptr; }

	/** The engine is destroyed: play the death sequence, then show the death screen. Cause: what
	 *  destroyed it (UEngineHealthComponent::GetDeathCause). Once per life. */
	void ShowDeath(const FText& Cause);

	UFUNCTION(BlueprintPure, Category="Player|Death")
	bool IsDead() const { return bDead; }

	UFUNCTION(BlueprintPure, Category="Player|Death")
	FText GetDeathCause() const { return DeathCause; }

	/** Console: "DeathScreen" plays the death sequence and shows the death screen now. */
	UFUNCTION(Exec)
	void DeathScreen();

	/** Plays the gameplay music from the top. Called at BeginPlay; call it if a new level ever starts without reloading the map. */
	UFUNCTION(BlueprintCallable, Category="Player|Music")
	void RestartMusic();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void SetupInputComponent() override;

	/** The gameplay soundtrack, played in order from its start on each level and looped. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Player|Music")
	TSoftObjectPtr<USoundBase> Music = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/Audio/Gameplay_OST/Gameplay_OST.Gameplay_OST")));

	/** Volume multiplier. TBD, tune by ear against the guns and the stove. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Player|Music", meta=(ClampMin="0.0", ClampMax="1.0"))
	float MusicVolume = 0.25f;

	/** Seconds to fade in when the level starts. TBD */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Player|Music", meta=(ClampMin="0.0"))
	float MusicFadeIn = 1.5f;

	/** Seconds to fade out when the engine is destroyed. TBD */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Player|Music", meta=(ClampMin="0.0"))
	float MusicFadeOut = 1.f;

	/**
	 * Keys that open and close the pause menu. In the editor, Esc stops Play In Editor before the game
	 * sees it, so use P there (or rebind the editor's Stop to Shift+Esc). TBD.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Player|Pause")
	TArray<FKey> PauseKeys = {EKeys::Escape, EKeys::P};

	/** The pause menu's panel (APauseMenuScreen or a Blueprint child of it). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Player|Pause")
	TSubclassOf<APauseMenuScreen> PauseMenuClass;

	/** Show the small "[P] PAUSE" hint in the corner during play. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Player|Pause")
	bool bShowPauseHint = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Player|Pause")
	TSubclassOf<APauseHintScreen> PauseHintClass;

	/** Real seconds of slow motion between the engine's death and the death screen. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Player|Death", meta=(ClampMin="0.0"))
	float DeathSlowMoTime = 2.f;

	/** How slow the world runs meanwhile (time dilation). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Player|Death", meta=(ClampMin="0.05", ClampMax="1.0"))
	float DeathTimeDilation = 0.3f;

private:
	/** Free mouse for a panel menu drawn in the scene (the pause menu, the death screen). */
	void EnterPanelMenuMode();
	void OpenDeathScreen();
	void PausePointerPressed();
	void PausePointerReleased();

	bool bGameplayInputEnabled = true;

	UPROPERTY(Transient)
	TObjectPtr<APauseMenuScreen> PauseMenu;

	UPROPERTY(Transient)
	TObjectPtr<APauseHintScreen> PauseHint;

	UPROPERTY(Transient)
	TObjectPtr<ADeathScreen> DeathMenu;

	bool bDead = false;
	FText DeathCause;
	FTimerHandle DeathTimer;

	/** Gameplay input on, no pause menu, alive, and the Music option on. */
	bool ShouldMusicPlay() const;
	/** Starts, pauses or resumes the music to match ShouldMusicPlay. */
	void UpdateMusic();
	void OnOptionChanged(EGameOption Option, bool bOn);

	/** Restarts the track when it ends, so it loops even if the sound asset isn't set to loop. */
	UFUNCTION()
	void OnMusicFinished();

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> MusicAudio;

	/** Played since the last RestartMusic (so resuming continues instead of starting over). */
	bool bMusicStarted = false;
	bool bMusicPaused = false;
	FDelegateHandle OptionChangedHandle;
};
