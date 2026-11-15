#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "BalloonPlayerController.generated.h"

class ADeathScreen;
class APauseHintScreen;
class APauseMenuScreen;

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
 * pauses under an ADeathScreen (ENGINE DESTROYED, the cause, RETURN TO CHECKPOINT, MAIN MENU). The console command
 * "DeathScreen" plays it at once, to see it without dying.
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

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

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
};
