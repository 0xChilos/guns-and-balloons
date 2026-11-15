#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "BalloonPlayerController.generated.h"

/** Owns gameplay/UI input policy; the possessed pawn routes gameplay actions to equipment. */
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

protected:
	virtual void BeginPlay() override;

private:
	bool bGameplayInputEnabled = true;
};
