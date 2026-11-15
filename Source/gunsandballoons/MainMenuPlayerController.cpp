#include "MainMenuPlayerController.h"

#include "EngineUtils.h"
#include "MainMenuScreen.h"

AMainMenuPlayerController::AMainMenuPlayerController()
{
	// Keep the mouse free over the view, but hide the system cursor: the menu draws its own.
	bShowMouseCursor = true;
	DefaultMouseCursor = EMouseCursor::None;
}

void AMainMenuPlayerController::BeginPlay()
{
	Super::BeginPlay();

	FInputModeGameAndUI Mode;
	Mode.SetHideCursorDuringCapture(false);
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(Mode);
}

void AMainMenuPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	InputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &AMainMenuPlayerController::PointerPressed);
	InputComponent->BindKey(EKeys::LeftMouseButton, IE_Released, this, &AMainMenuPlayerController::PointerReleased);
}

void AMainMenuPlayerController::PointerPressed()
{
	for (TActorIterator<AMainMenuScreen> It(GetWorld()); It; ++It)
	{
		It->PressPointer();
	}
}

void AMainMenuPlayerController::PointerReleased()
{
	for (TActorIterator<AMainMenuScreen> It(GetWorld()); It; ++It)
	{
		It->ReleasePointer();
	}
}
