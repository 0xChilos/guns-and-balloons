#pragma once

#include "CoreMinimal.h"
#include "MainMenuScreen.h"
#include "LevelShopScreen.generated.h"

class ULevelShopWidget;
class UMainMenuWidget;

/**
 * Shows the shop between levels (ULevelShopWidget) the way AMainMenuScreen shows the main menu: on an unlit panel
 * in front of the camera, sized to fill the view, so the game's filter applies to it.
 * Placed in the shop level (/Game/Scenes/Shop), which UBalloonRunSubsystem::CompleteLevel opens. That level uses
 * BP_MainMenuGameMode, whose AMainMenuPlayerController forwards LMB to this screen. Once the run is won
 * (UBalloonRunSubsystem::IsRunWon) it shows the victory screen (VictoryClass) instead of the shop.
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

	/** Shown instead of the shop after the last level of the run (UVictoryWidget or a Blueprint child of it). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shop")
	TSubclassOf<UMainMenuWidget> VictoryClass;
};
