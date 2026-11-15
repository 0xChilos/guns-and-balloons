#include "BalloonPlayerController.h"

#include "FirstPersonCameraPawn.h"

void ABalloonPlayerController::BeginPlay()
{
	Super::BeginPlay();
	UseGameplayInput();
}

void ABalloonPlayerController::UseGameplayInput()
{
	bGameplayInputEnabled = true;
	if (IsLocalController())
	{
		SetInputMode(FInputModeGameOnly());
		bShowMouseCursor = false;
	}
}

void ABalloonPlayerController::UseMenuInput()
{
	bGameplayInputEnabled = false;
	if (AFirstPersonCameraPawn* PlayerPawn = Cast<AFirstPersonCameraPawn>(GetPawn()))
	{
		PlayerPawn->ReleaseGameplayInput();
	}
	if (IsLocalController())
	{
		SetInputMode(FInputModeUIOnly());
		bShowMouseCursor = true;
	}
}
