#include "VictoryWidget.h"

#define LOCTEXT_NAMESPACE "VictoryScreen"

UVictoryWidget::UVictoryWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	FMainMenuItem MainMenu;
	MainMenu.Label = LOCTEXT("MainMenu", "MAIN MENU");
	MainMenu.Action = EMainMenuAction::QuitToMenu;
	Items = { MainMenu };
	GameTitle = LOCTEXT("Title", "VICTORY");
	GameSubtitle = LOCTEXT("Subtitle", "THE JOURNEY IS OVER");
}

#undef LOCTEXT_NAMESPACE
