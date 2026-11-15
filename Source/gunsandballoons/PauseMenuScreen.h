#pragma once

#include "CoreMinimal.h"
#include "MainMenuScreen.h"
#include "PauseMenuScreen.generated.h"

/** Where the gameplay menu panels live, and the material that keeps them on top. */
namespace GameplayPanel
{
	/** Just past the default near clip plane (10 cm); with the panel material nearer things can't cover it anyway. */
	constexpr float Distance = 15.f;
	inline const TCHAR* const MaterialPath = TEXT("/Game/UI/PauseMenu/M_PauseMenuPanel.M_PauseMenuPanel");
}

/**
 * The pause menu's panel (docs/features/ui-menus.md). ABalloonPlayerController spawns it in the
 * gameplay level when the game pauses and destroys it on resume, so the frozen frame stays behind it.
 *
 * Like AMainMenuScreen it fills the camera's view and is filtered with the rest of the scene, but:
 * - it keeps ticking while the world is paused (panel, widget and pointer);
 * - it sits just past the near clip plane and uses M_PauseMenuPanel (depth test off), so the gun in
 *   the player's hands and the basket never cover it;
 * - the pointer is aimed at the panel directly (Custom source), not traced through the scene, so
 *   nothing in the level can block a click.
 */
UCLASS()
class GUNSANDBALLOONS_API APauseMenuScreen : public AMainMenuScreen
{
	GENERATED_BODY()

public:
	APauseMenuScreen();

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

private:
	void AimPointer();
};

/**
 * The small "[P] PAUSE" hint in the bottom-right corner during play (UPauseHintWidget), on the same
 * kind of filtered panel. Spawned by ABalloonPlayerController, hidden while the pause menu is open.
 * Takes no input: no collision, no pointer, and its picture is redrawn only a few times a second.
 */
UCLASS()
class GUNSANDBALLOONS_API APauseHintScreen : public AMainMenuScreen
{
	GENERATED_BODY()

public:
	APauseHintScreen();
};
