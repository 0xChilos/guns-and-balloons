#include "LevelShopScreen.h"

#include "Components/InputComponent.h"
#include "Components/WidgetComponent.h"
#include "Components/WidgetInteractionComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "LevelShopWidget.h"

ALevelShopScreen::ALevelShopScreen()
{
	// The main menu's widget is not wanted here; BeginPlay puts the shop on the panel instead.
	MenuClass = nullptr;
	ShopClass = ULevelShopWidget::StaticClass();
	// Closer than the item in hand (about 34 cm in front of the eyes) and the basket.
	Distance = 15.f;

	// The game is paused while the shop is open: the panel must still draw, follow the mouse and tick.
	PrimaryActorTick.bTickEvenWhenPaused = true;
	Screen->PrimaryComponentTick.bTickEvenWhenPaused = true;
	Pointer->PrimaryComponentTick.bTickEvenWhenPaused = true;
}

void ALevelShopScreen::BeginPlay()
{
	Super::BeginPlay();

	if (ShopClass)
	{
		if (ULevelShopWidget* Shop = CreateWidget<ULevelShopWidget>(GetWorld(), ShopClass))
		{
			// Before the widget builds its tree, as AMainMenuScreen does for the menu.
			Shop->ApplyBrightness(Brightness);
			Screen->SetWidget(Shop);
		}
	}

	if (APlayerController* Player = UGameplayStatics::GetPlayerController(this, 0))
	{
		EnableInput(Player);
		if (InputComponent)
		{
			InputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &ALevelShopScreen::PointerPressed).bExecuteWhenPaused = true;
			InputComponent->BindKey(EKeys::LeftMouseButton, IE_Released, this, &ALevelShopScreen::PointerReleased).bExecuteWhenPaused = true;
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

void ALevelShopScreen::PointerPressed()
{
	PressPointer();
}

void ALevelShopScreen::PointerReleased()
{
	ReleasePointer();
}
