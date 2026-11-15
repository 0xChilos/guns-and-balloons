#pragma once

#include "CoreMinimal.h"
#include "MainMenuScreen.h"
#include "LevelShopScreen.generated.h"

class ULevelShopWidget;

/**
 * Shows the shop between levels (ULevelShopWidget) the way AMainMenuScreen shows the main menu: on an unlit,
 * translucent panel in front of the camera, sized to fill the view, so the game's filter applies to it.
 * Spawned by UBalloonRunSubsystem::CompleteLevel while the game is paused, so it ticks and takes LMB while paused.
 * It floats closer than the item in hand (Distance 15 cm) so nothing in the basket covers it.
 */
UCLASS(Blueprintable)
class GUNSANDBALLOONS_API ALevelShopScreen : public AMainMenuScreen
{
	GENERATED_BODY()

public:
	ALevelShopScreen();

	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintPure, Category = "Shop")
	ULevelShopWidget* GetShop() const;

protected:
	virtual void BeginPlay() override;

	/** The shop to show (ULevelShopWidget or a Blueprint child of it). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shop")
	TSubclassOf<ULevelShopWidget> ShopClass;

private:
	void PointerPressed();
	void PointerReleased();
};
