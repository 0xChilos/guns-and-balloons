#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DeathScreenPreview.generated.h"

/**
 * Test helper for /Game/Scenes/DeathScreen_Test (docs/features/ui-menus.md): Delay seconds after the
 * level starts it plays the death sequence and shows the death screen with Cause, through
 * ABalloonPlayerController::ShowDeath, so the screen can be seen and tuned without dying.
 * RETURN TO CHECKPOINT restarts the level, which shows it again.
 */
UCLASS()
class GUNSANDBALLOONS_API ADeathScreenPreview : public AActor
{
	GENERATED_BODY()

public:
	ADeathScreenPreview();

	/** Seconds of play before the death. */
	UPROPERTY(EditAnywhere, Category = "Death Screen Preview", meta = (ClampMin = "0.0"))
	float Delay = 6.f;

	/** The cause line to show (as UEngineHealthComponent::GetDeathCause would give it). */
	UPROPERTY(EditAnywhere, Category = "Death Screen Preview")
	FText Cause;

protected:
	virtual void BeginPlay() override;

private:
	void ShowDeath();

	FTimerHandle Timer;
};
