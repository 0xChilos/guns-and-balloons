#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "MainMenuPlayerController.generated.h"

/**
 * Player controller of the main menu level: mouse only (docs/features/ui-menus.md).
 *
 * The mouse moves freely over the view with the system cursor hidden (the menu draws its own pixel
 * cursor, so the filter applies to it too). LMB is forwarded to every AMainMenuScreen in the level.
 * The view comes from the level's camera (its Auto Activate for Player 0); there is no pawn.
 */
UCLASS(Blueprintable)
class GUNSANDBALLOONS_API AMainMenuPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AMainMenuPlayerController();

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

private:
	void PointerPressed();
	void PointerReleased();
};
