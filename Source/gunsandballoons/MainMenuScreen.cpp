#include "MainMenuScreen.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/WidgetComponent.h"
#include "Components/WidgetInteractionComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "MainMenuWidget.h"

AMainMenuScreen::AMainMenuScreen()
{
	PrimaryActorTick.bCanEverTick = true;
	// Place the panel after the camera has moved this frame.
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;

	Screen = CreateDefaultSubobject<UWidgetComponent>(TEXT("Screen"));
	RootComponent = Screen;
	Screen->SetWidgetSpace(EWidgetSpace::World);
	Screen->SetDrawSize(FVector2D(1920.f, 1080.f));
	// Translucent, so the haze and the fades are soft; the filter still applies on top.
	Screen->SetBlendMode(EWidgetBlendMode::Transparent);
	Screen->SetTwoSided(false);
	Screen->SetCastShadow(false);
	Screen->SetTickWhenOffscreen(true);
	Screen->SetWindowFocusable(false);
	// Only the menu pointer's trace (Visibility) hits the panel.
	Screen->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Screen->SetCollisionResponseToAllChannels(ECR_Ignore);
	Screen->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	Pointer = CreateDefaultSubobject<UWidgetInteractionComponent>(TEXT("Pointer"));
	Pointer->SetupAttachment(Screen);
	Pointer->InteractionSource = EWidgetInteractionSource::Mouse;
	Pointer->TraceChannel = ECC_Visibility;
	Pointer->bShowDebug = false;

	MenuClass = UMainMenuWidget::StaticClass();
}

void AMainMenuScreen::BeginPlay()
{
	Super::BeginPlay();

	Pointer->InteractionDistance = Distance * 4.f;
	if (MenuClass)
	{
		// Brightness scales the menu's own colours before its tree is built. (The component's tint
		// can't do it: the translucent widget material dims the opacity with it too.)
		if (UMainMenuWidget* Menu = CreateWidget<UMainMenuWidget>(GetWorld(), MenuClass))
		{
			for (FLinearColor* Color : {&Menu->TextColor, &Menu->SelectedColor, &Menu->DisabledTextColor, &Menu->CursorColor})
			{
				*Color = FLinearColor(Color->R * Brightness, Color->G * Brightness, Color->B * Brightness, Color->A);
			}
			Screen->SetWidget(Menu);
		}
	}
	FitToView();
}

void AMainMenuScreen::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	FitToView();

	if (UMainMenuWidget* Menu = GetMenu())
	{
		const bool bOverPanel = Pointer->GetHoveredWidgetComponent() == Screen;
		Menu->SetCursor(Pointer->Get2DHitLocation(), bOverPanel);
	}
}

void AMainMenuScreen::FitToView()
{
	const APlayerCameraManager* Camera = UGameplayStatics::GetPlayerCameraManager(this, 0);
	if (!Camera)
	{
		return;
	}

	const FRotator CameraRotation = Camera->GetCameraRotation();
	const FRotationMatrix CameraAxes(CameraRotation);

	// Size of the view at Distance. Deprojecting the top-left corner of the viewport gives it exactly,
	// whatever the aspect and whichever FOV axis the player's aspect ratio constraint keeps.
	float HalfWidth = Distance * FMath::Tan(FMath::DegreesToRadians(Camera->GetFOVAngle() * 0.5f));
	float HalfHeight = HalfWidth * 9.f / 16.f;
	const FVector2D ViewSize = UWidgetLayoutLibrary::GetViewportSize(this);
	FVector Origin, CornerDirection;
	const APlayerController* Player = UGameplayStatics::GetPlayerController(this, 0);
	if (Player && ViewSize.X > 0.f && ViewSize.Y > 0.f
		&& Player->DeprojectScreenPositionToWorld(0.f, 0.f, Origin, CornerDirection))
	{
		const float Along = FVector::DotProduct(CornerDirection, CameraAxes.GetUnitAxis(EAxis::X));
		if (Along > KINDA_SMALL_NUMBER)
		{
			const FVector Corner = CornerDirection * (Distance / Along);
			HalfWidth = FMath::Abs(FVector::DotProduct(Corner, CameraAxes.GetUnitAxis(EAxis::Y)));
			HalfHeight = FMath::Abs(FVector::DotProduct(Corner, CameraAxes.GetUnitAxis(EAxis::Z)));
		}
	}

	const FVector2D DrawSize(FMath::RoundToFloat(DrawHeight * HalfWidth / HalfHeight), DrawHeight);
	if (!Screen->GetDrawSize().Equals(DrawSize))
	{
		Screen->SetDrawSize(DrawSize);
	}
	const float Scale = 2.f * HalfHeight / DrawHeight;
	// A widget component shows its front along +X, so it faces back toward the camera.
	SetActorLocationAndRotation(Camera->GetCameraLocation() + CameraRotation.Vector() * Distance,
		(CameraRotation.Quaternion() * FQuat(FVector::UpVector, PI)).Rotator());
	SetActorScale3D(FVector(Scale));
}

void AMainMenuScreen::PressPointer()
{
	Pointer->PressPointerKey(EKeys::LeftMouseButton);
}

void AMainMenuScreen::ReleasePointer()
{
	Pointer->ReleasePointerKey(EKeys::LeftMouseButton);
}

UMainMenuWidget* AMainMenuScreen::GetMenu() const
{
	return Cast<UMainMenuWidget>(Screen->GetUserWidgetObject());
}
