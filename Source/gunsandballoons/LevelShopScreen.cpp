#include "LevelShopScreen.h"

#include "Components/WidgetComponent.h"
#include "Components/WidgetInteractionComponent.h"
#include "BalloonRunSubsystem.h"
#include "LevelShopWidget.h"
#include "VictoryWidget.h"

ALevelShopScreen::ALevelShopScreen()
{
	// The main menu's widget is not wanted here; BeginPlay puts the shop on the panel instead.
	MenuClass = nullptr;
	ShopClass = ULevelShopWidget::StaticClass();
	VictoryClass = UVictoryWidget::StaticClass();
	// As the main menu's screen in MainMenu.umap.
	Brightness = 0.45f;
}

void ALevelShopScreen::BeginPlay()
{
	Super::BeginPlay();

	const UBalloonRunSubsystem* Run = UBalloonRunSubsystem::Get(this);
	if (Run && Run->IsRunWon() && VictoryClass)
	{
		// The run is over: the victory screen, drawn like the main menu (AMainMenuScreen::Tick moves its cursor).
		if (UMainMenuWidget* Victory = CreateWidget<UMainMenuWidget>(GetWorld(), VictoryClass))
		{
			Victory->ScaleColors(Brightness);
			Screen->SetWidget(Victory);
		}
		return;
	}
	if (ShopClass)
	{
		if (ULevelShopWidget* Shop = CreateWidget<ULevelShopWidget>(GetWorld(), ShopClass))
		{
			// Before the widget builds its tree, as AMainMenuScreen does for the menu.
			Shop->ApplyBrightness(Brightness);
			Screen->SetWidget(Shop);
		}
	}
}

void ALevelShopScreen::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (ULevelShopWidget* Shop = GetShop())
	{
		const bool bOverPanel = Pointer->GetHoveredWidgetComponent() == Screen;
		Shop->SetCursor(Pointer->Get2DHitLocation(), bOverPanel);
	}
}

ULevelShopWidget* ALevelShopScreen::GetShop() const
{
	return Cast<ULevelShopWidget>(Screen->GetUserWidgetObject());
}
