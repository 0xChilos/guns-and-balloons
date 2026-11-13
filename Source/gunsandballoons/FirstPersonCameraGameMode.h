#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "FirstPersonCameraGameMode.generated.h"

/**
 * GameMode for the first-person camera.
 * - If the level contains a pawn set to Auto Possess this player (e.g. a placed AFirstPersonCameraPawn),
 *   the player takes that pawn and nothing else is spawned.
 * - Otherwise it spawns AFirstPersonCameraPawn at the PlayerStart.
 */
UCLASS()
class GUNSANDBALLOONS_API AFirstPersonCameraGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AFirstPersonCameraGameMode();

	virtual void RestartPlayer(AController* NewPlayer) override;
};
