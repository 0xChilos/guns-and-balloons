#include "MainMenuPlayerController.h"
#include "Components/InputComponent.h"

#include "Components/AudioComponent.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "MainMenuScreen.h"
#include "Sound/SoundBase.h"

AMainMenuPlayerController::AMainMenuPlayerController()
{
	// Keep the mouse free over the view, but hide the system cursor: the menu draws its own.
	bShowMouseCursor = true;
	DefaultMouseCursor = EMouseCursor::None;

	Music = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/Audio/Main_Menu_OST/Main_Menu_OST.Main_Menu_OST")));
}

void AMainMenuPlayerController::BeginPlay()
{
	Super::BeginPlay();

	FInputModeGameAndUI Mode;
	Mode.SetHideCursorDuringCapture(false);
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(Mode);

	if (IsLocalController())
	{
		OptionChangedHandle = UGameOptions::OnChanged().AddUObject(this, &AMainMenuPlayerController::OnOptionChanged);
		if (UGameOptions::IsOn(EGameOption::Music))
		{
			StartMusic();
		}
	}
}

void AMainMenuPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UGameOptions::OnChanged().Remove(OptionChangedHandle);
	OptionChangedHandle.Reset();
	bMusicWanted = false;
	if (MusicAudio)
	{
		MusicAudio->Stop();
		MusicAudio = nullptr;
	}

	Super::EndPlay(EndPlayReason);
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

void AMainMenuPlayerController::StartMusic()
{
	bMusicWanted = true;
	if (!MusicAudio)
	{
		USoundBase* Sound = Music.LoadSynchronous();
		if (!Sound)
		{
			return;
		}
		// 2D, kept between plays (no auto destroy) so the loop and the Music switch can restart it.
		MusicAudio = UGameplayStatics::CreateSound2D(this, Sound, MusicVolume, 1.f, 0.f, nullptr, false, false);
		if (!MusicAudio)
		{
			return;
		}
		MusicAudio->OnAudioFinished.AddDynamic(this, &AMainMenuPlayerController::OnMusicFinished);
	}
	MusicAudio->SetVolumeMultiplier(MusicVolume);
	MusicAudio->FadeIn(MusicFadeIn, 1.f);
}

void AMainMenuPlayerController::StopMusic()
{
	bMusicWanted = false;
	if (MusicAudio && MusicAudio->IsPlaying())
	{
		MusicAudio->FadeOut(MusicFadeOut, 0.f);
	}
}

void AMainMenuPlayerController::OnOptionChanged(EGameOption Option, bool bOn)
{
	if (Option == EGameOption::Music)
	{
		bOn ? StartMusic() : StopMusic();
	}
}

void AMainMenuPlayerController::OnMusicFinished()
{
	// Also fires when a fade-out stops it; only loop while the music is meant to play.
	if (bMusicWanted && MusicAudio)
	{
		MusicAudio->Play(0.f);
	}
}
