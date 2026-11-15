#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "BalloonPlayerController.generated.h"

class APauseHintScreen;
class APauseMenuScreen;

/**
 * Owns gameplay/UI input policy; the possessed pawn routes gameplay actions to equipment.
 *
 * Also owns the pause menu (docs/features/ui-menus.md): a PauseKeys press pauses the world and
 * spawns an APauseMenuScreen in front of the camera; another press, or RETURN TO GAME, resumes.
 * While paused the mouse is free, the system cursor hidden (the menu draws its own), the crosshair
 * HUD hidden, and LMB goes to the menu's pointer. During play a small APauseHintScreen shows the key.
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

private:
	void PausePointerPressed();
	void PausePointerReleased();

	bool bGameplayInputEnabled = true;

	UPROPERTY(Transient)
	TObjectPtr<APauseMenuScreen> PauseMenu;

	UPROPERTY(Transient)
	TObjectPtr<APauseHintScreen> PauseHint;
};
