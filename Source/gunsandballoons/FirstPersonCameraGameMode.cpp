#include "FirstPersonCameraGameMode.h"

#include "EngineUtils.h"
#include "FirstPersonCameraPawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

AFirstPersonCameraGameMode::AFirstPersonCameraGameMode()
{
	DefaultPawnClass = AFirstPersonCameraPawn::StaticClass();
}

void AFirstPersonCameraGameMode::RestartPlayer(AController* NewPlayer)
{
	// Prefer a pawn placed in the level that is set to auto-possess this player,
	// so we don't spawn a second (default) pawn next to it.
	if (APlayerController* PC = Cast<APlayerController>(NewPlayer); PC && !PC->GetPawn())
	{
		for (APawn* Pawn : TActorRange<APawn>(GetWorld()))
		{
			if (Pawn->AutoPossessPlayer == EAutoReceiveInput::Disabled || Pawn->GetController() != nullptr)
			{
				continue;
			}

			const int32 PlayerIndex = int32(Pawn->AutoPossessPlayer.GetValue()) - 1;
			if (UGameplayStatics::GetPlayerController(this, PlayerIndex) == PC)
			{
				PC->Possess(Pawn);
				return;
			}
		}
	}

	Super::RestartPlayer(NewPlayer);
}
