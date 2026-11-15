#include "DeathScreenPreview.h"

#include "BalloonPlayerController.h"
#include "Engine/World.h"
#include "TimerManager.h"

ADeathScreenPreview::ADeathScreenPreview()
{
	PrimaryActorTick.bCanEverTick = false;
	Cause = NSLOCTEXT("DeathScreenPreview", "Cause", "SHOT DOWN BY A TOWER CANNON");
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

void ADeathScreenPreview::BeginPlay()
{
	Super::BeginPlay();
	GetWorldTimerManager().SetTimer(Timer, this, &ADeathScreenPreview::ShowDeath, FMath::Max(0.01f, Delay), false);
}

void ADeathScreenPreview::ShowDeath()
{
	if (ABalloonPlayerController* Player = Cast<ABalloonPlayerController>(GetWorld()->GetFirstPlayerController()))
	{
		Player->ShowDeath(Cause);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: the player controller is not an ABalloonPlayerController, no death screen"), *GetName());
	}
}
