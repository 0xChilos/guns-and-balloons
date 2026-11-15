#include "BalloonPlayerController.h"

#include "Components/InputComponent.h"
#include "FirstPersonCameraPawn.h"
#include "GameFramework/HUD.h"
#include "PauseMenuScreen.h"
#include "TimerManager.h"

void ABalloonPlayerController::BeginPlay()
{
	Super::BeginPlay();
	UseGameplayInput();

	// Only for a real local player (not in automation worlds without a viewport).
	if (bShowPauseHint && GetLocalPlayer() && GetWorld() && GetWorld()->IsGameWorld())
	{
		FActorSpawnParameters Params;
		Params.Owner = this;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		const TSubclassOf<APauseHintScreen> HintClass = PauseHintClass ? PauseHintClass : TSubclassOf<APauseHintScreen>(APauseHintScreen::StaticClass());
		PauseHint = GetWorld()->SpawnActor<APauseHintScreen>(HintClass, FTransform::Identity, Params);
	}
}

void ABalloonPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	for (const FKey& Key : PauseKeys)
	{
		InputComponent->BindKey(Key, IE_Pressed, this, &ABalloonPlayerController::TogglePause).bExecuteWhenPaused = true;
	}
	// The pawn polls the mouse buttons itself, so these must not consume them.
	FInputKeyBinding& Press = InputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &ABalloonPlayerController::PausePointerPressed);
	Press.bExecuteWhenPaused = true;
	Press.bConsumeInput = false;
	FInputKeyBinding& Release = InputComponent->BindKey(EKeys::LeftMouseButton, IE_Released, this, &ABalloonPlayerController::PausePointerReleased);
	Release.bExecuteWhenPaused = true;
	Release.bConsumeInput = false;
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

void ABalloonPlayerController::TogglePause()
{
	if (PauseMenu)
	{
		ResumeGame();
	}
	else
	{
		PauseGame();
	}
}

void ABalloonPlayerController::PauseGame()
{
	if (PauseMenu || !IsLocalController() || !SetPause(true))
	{
		return;
	}

	UseMenuInput();
	// The menu is a panel in the scene, not Slate on the viewport: input stays with this controller,
	// which forwards LMB to the panel's pointer. The menu draws its own cursor, so the system one is hidden.
	FInputModeGameAndUI Mode;
	Mode.SetHideCursorDuringCapture(false);
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::LockAlways);
	SetInputMode(Mode);
	DefaultMouseCursor = EMouseCursor::None;
	CurrentMouseCursor = EMouseCursor::None;
	int32 ViewWidth = 0, ViewHeight = 0;
	GetViewportSize(ViewWidth, ViewHeight);
	SetMouseLocation(ViewWidth / 2, ViewHeight / 2);

	if (MyHUD)
	{
		MyHUD->bShowHUD = false;
	}
	if (PauseHint)
	{
		PauseHint->SetActorHiddenInGame(true);
	}

	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const TSubclassOf<APauseMenuScreen> ScreenClass = PauseMenuClass ? PauseMenuClass : TSubclassOf<APauseMenuScreen>(APauseMenuScreen::StaticClass());
	PauseMenu = GetWorld()->SpawnActor<APauseMenuScreen>(ScreenClass, FTransform::Identity, Params);
}

void ABalloonPlayerController::ResumeGame()
{
	if (!PauseMenu)
	{
		return;
	}

	// RETURN TO GAME calls this from inside the panel's own click: hide the panel now, destroy it next frame.
	APauseMenuScreen* Closing = PauseMenu;
	PauseMenu = nullptr;
	Closing->SetActorHiddenInGame(true);
	Closing->SetActorTickEnabled(false);
	GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(Closing, [Closing]()
	{
		Closing->Destroy();
	}));

	SetPause(false);
	if (MyHUD)
	{
		MyHUD->bShowHUD = true;
	}
	if (PauseHint)
	{
		PauseHint->SetActorHiddenInGame(false);
	}
	DefaultMouseCursor = EMouseCursor::Default;
	CurrentMouseCursor = EMouseCursor::Default;
	UseGameplayInput();
}

void ABalloonPlayerController::PausePointerPressed()
{
	if (PauseMenu)
	{
		PauseMenu->PressPointer();
	}
}

void ABalloonPlayerController::PausePointerReleased()
{
	if (PauseMenu)
	{
		PauseMenu->ReleasePointer();
	}
}
