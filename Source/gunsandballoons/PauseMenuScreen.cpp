#include "PauseMenuScreen.h"

#include "Components/WidgetComponent.h"
#include "Components/WidgetInteractionComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "PauseMenuWidget.h"

APauseMenuScreen::APauseMenuScreen()
{
	SetTickableWhenPaused(true);
	Screen->PrimaryComponentTick.bTickEvenWhenPaused = true;
	Pointer->PrimaryComponentTick.bTickEvenWhenPaused = true;
	Pointer->InteractionSource = EWidgetInteractionSource::Custom;
	// Drawn after the scene's other translucency (smoke, fire) and the hint, on top of everything.
	Screen->SetTranslucentSortPriority(1000);
	// Nothing traces against it: the pointer is aimed by hand.
	Screen->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	MenuClass = UPauseMenuWidget::StaticClass();
	Distance = GameplayPanel::Distance;
	PanelMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(GameplayPanel::MaterialPath));
}

void APauseMenuScreen::BeginPlay()
{
	Super::BeginPlay();
	AimPointer();
}

void APauseMenuScreen::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	// After the panel has been fitted to the view this frame; the pointer uses it on its next tick.
	AimPointer();
}

void APauseMenuScreen::AimPointer()
{
	FHitResult Hit;
	const APlayerController* Player = UGameplayStatics::GetPlayerController(this, 0);
	float MouseX = 0.f, MouseY = 0.f;
	FVector Origin, Direction;
	if (Player && Player->GetMousePosition(MouseX, MouseY)
		&& Player->DeprojectScreenPositionToWorld(MouseX, MouseY, Origin, Direction))
	{
		// The mouse ray meets the panel's plane (the panel faces the camera along its +X).
		const FVector Normal = GetActorForwardVector();
		const float Along = FVector::DotProduct(Direction, Normal);
		if (!FMath::IsNearlyZero(Along))
		{
			const float HitDistance = FVector::DotProduct(GetActorLocation() - Origin, Normal) / Along;
			if (HitDistance > 0.f)
			{
				Hit.bBlockingHit = true;
				Hit.Component = Screen;
				Hit.Distance = HitDistance;
				Hit.TraceStart = Origin;
				Hit.TraceEnd = Origin + Direction * (HitDistance * 2.f);
				Hit.Location = Hit.ImpactPoint = Origin + Direction * HitDistance;
				Hit.Normal = Hit.ImpactNormal = Normal;
			}
		}
	}
	Pointer->SetCustomHitResult(Hit);
}

APauseHintScreen::APauseHintScreen()
{
	Screen->SetTranslucentSortPriority(900);
	// Must not block the player's aim and interaction traces.
	Screen->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	// A still label: a few redraws a second are enough for its fade.
	Screen->SetRedrawTime(1.f / 15.f);
	Pointer->PrimaryComponentTick.bStartWithTickEnabled = false;
	Pointer->SetAutoActivate(false);

	MenuClass = UPauseHintWidget::StaticClass();
	Distance = GameplayPanel::Distance;
	PanelMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(GameplayPanel::MaterialPath));
}
