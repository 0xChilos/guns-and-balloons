#pragma once

#include "CoreMinimal.h"
#include "MainMenuWidget.h"
#include "VictoryWidget.generated.h"

/**
 * The victory screen at the end of a run (all UBalloonRunSubsystem::LevelsPerRun levels completed): the main menu's
 * look with a VICTORY title and one MAIN MENU row, which opens MainMenuLevel. Shown by ALevelShopScreen in the shop
 * level instead of the shop once the run is won.
 */
UCLASS(Blueprintable)
class GUNSANDBALLOONS_API UVictoryWidget : public UMainMenuWidget
{
	GENERATED_BODY()

public:
	UVictoryWidget(const FObjectInitializer& ObjectInitializer);
};
