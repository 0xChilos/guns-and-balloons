#include "BalloonKeyboardControlComponent.h"

#include "BalloonFlightComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

UBalloonKeyboardControlComponent::UBalloonKeyboardControlComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UBalloonKeyboardControlComponent::BeginPlay()
{
	Super::BeginPlay();

	Flight = GetOwner() ? GetOwner()->FindComponentByClass<UBalloonFlightComponent>() : nullptr;
	if (!Flight)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: BalloonKeyboardControlComponent has no BalloonFlightComponent on the owner, disabling."),
			*GetNameSafe(GetOwner()));
		SetComponentTickEnabled(false);
		return;
	}

	// Let this component tick before flight, so the input read here applies to the same frame.
	Flight->AddTickPrerequisiteComponent(this);
}

void UBalloonKeyboardControlComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!Flight || TickType == LEVELTICK_PauseTick)
	{
		return;
	}

	if (!bEnabled)
	{
		Flight->SetTurnInput(0.f);
		TurnTimer = 0.f;
		return;
	}

	const APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!PC)
	{
		return;
	}

	const bool bSpeedUp = PC->IsInputKeyDown(SpeedUpKey);
	const bool bSpeedDown = PC->IsInputKeyDown(SpeedDownKey);
	const bool bTurnLeft = PC->IsInputKeyDown(TurnLeftKey);
	const bool bTurnRight = PC->IsInputKeyDown(TurnRightKey);

	// Speed changes on key press (click), not while held.
	if (bSpeedUp && !bPrevSpeedUp)
	{
		Flight->SetTargetSpeed(Flight->GetTargetSpeed() + SpeedStep);
	}
	if (bSpeedDown && !bPrevSpeedDown)
	{
		Flight->SetTargetSpeed(Flight->GetTargetSpeed() - SpeedStep);
	}

	// Turn: while a turn key is held the turn input stays active (hold-to-turn);
	// releasing the key stops the turn.
	if (bTurnLeft)
	{
		Flight->SetTurnInput(-1.f);
	}
	else if (bTurnRight)
	{
		Flight->SetTurnInput(1.f);
	}
	else
	{
		Flight->SetTurnInput(0.f);
	}

	bPrevSpeedUp = bSpeedUp;
	bPrevSpeedDown = bSpeedDown;
	bPrevTurnLeft = bTurnLeft;
	bPrevTurnRight = bTurnRight;
}
