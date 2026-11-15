#include "BalloonRunSubsystem.h"

#include "BalloonPlayerController.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "ExtinguisherComponent.h"
#include "FirstPersonCameraPawn.h"
#include "Kismet/GameplayStatics.h"
#include "LevelShopScreen.h"
#include "ResourceLedger.h"
#include "UObject/UObjectIterator.h"
#include "WeaponRackComponent.h"

namespace
{
	AFirstPersonCameraPawn* FindPlayerPawn(UWorld* World)
	{
		const APlayerController* Player = World ? World->GetFirstPlayerController() : nullptr;
		return Player ? Cast<AFirstPersonCameraPawn>(Player->GetPawn()) : nullptr;
	}

	/** Calls Visit for every rack in World that holds the extinguisher. */
	void ForEachExtinguisherRack(UWorld* World, TFunctionRef<void(UWeaponRackComponent&)> Visit)
	{
		for (TObjectIterator<UWeaponRackComponent> It; It; ++It)
		{
			if (It->GetWorld() == World && It->ItemClass && It->ItemClass->IsChildOf(UExtinguisherComponent::StaticClass()))
			{
				Visit(**It);
			}
		}
	}
}

UBalloonRunSubsystem* UBalloonRunSubsystem::Get(const UObject* WorldContextObject)
{
	const UGameInstance* GameInstance = UGameplayStatics::GetGameInstance(WorldContextObject);
	return GameInstance ? GameInstance->GetSubsystem<UBalloonRunSubsystem>() : nullptr;
}

void UBalloonRunSubsystem::StartNewRun()
{
	bRunActive = true;
	bShopOpen = false;
	Gold = 0;
	bHasExtinguisher = false;
	ExtinguisherAmount = 0.f;
	PendingLevel.Reset();
	OnChanged.Broadcast();
}

void UBalloonRunSubsystem::CompleteLevel(const UObject* WorldContextObject, TSoftObjectPtr<UWorld> NextLevel)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World || bShopOpen)
	{
		return;
	}

	// Without a run (a level played straight from the editor), the run starts from what the basket holds now.
	ReadExtinguisher(World, !bRunActive);
	bRunActive = true;
	if (const UResourceLedger* Ledger = World->GetSubsystem<UResourceLedger>())
	{
		Gold += Ledger->GetCount(EResourceType::Money);
	}
	PendingLevel = NextLevel;
	bShopOpen = true;

	if (APlayerController* Player = World->GetFirstPlayerController())
	{
		if (ABalloonPlayerController* BalloonPlayer = Cast<ABalloonPlayerController>(Player))
		{
			// Lets go of anything held (the trigger, a grab) before the menu takes the mouse.
			BalloonPlayer->UseMenuInput();
		}
		// The mouse moves freely; the shop draws its own pixel cursor and takes LMB itself.
		FInputModeGameAndUI Mode;
		Mode.SetHideCursorDuringCapture(false);
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		Player->SetInputMode(Mode);
		Player->bShowMouseCursor = true;
		Player->DefaultMouseCursor = EMouseCursor::None;
		Player->CurrentMouseCursor = EMouseCursor::None;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	World->SpawnActor<ALevelShopScreen>(ShopScreenClass ? *ShopScreenClass : ALevelShopScreen::StaticClass(), FTransform::Identity, Params);
	UGameplayStatics::SetGamePaused(World, true);
	OnChanged.Broadcast();
}

void UBalloonRunSubsystem::ContinueTravel(const UObject* WorldContextObject)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World || !bShopOpen)
	{
		return;
	}
	bShopOpen = false;
	UGameplayStatics::SetGamePaused(World, false);
	if (PendingLevel.IsNull())
	{
		UGameplayStatics::OpenLevel(World, FName(*UGameplayStatics::GetCurrentLevelName(World)));
	}
	else
	{
		UGameplayStatics::OpenLevelBySoftObjectPtr(World, PendingLevel);
	}
}

void UBalloonRunSubsystem::AddGold(int32 Amount)
{
	if (Amount > 0)
	{
		Gold += Amount;
		OnChanged.Broadcast();
	}
}

bool UBalloonRunSubsystem::SpendGold(int32 Price)
{
	if (Price < 0 || Gold < Price)
	{
		return false;
	}
	Gold -= Price;
	OnChanged.Broadcast();
	return true;
}

bool UBalloonRunSubsystem::BuyExtinguisher(int32 Price)
{
	if (bHasExtinguisher || !SpendGold(Price))
	{
		return false;
	}
	bHasExtinguisher = true;
	ExtinguisherAmount = ExtinguisherFull;
	OnChanged.Broadcast();
	return true;
}

bool UBalloonRunSubsystem::RefillExtinguisher(int32 Price, float Units)
{
	if (!bHasExtinguisher || Units <= 0.f || ExtinguisherAmount >= ExtinguisherFull || !SpendGold(Price))
	{
		return false;
	}
	ExtinguisherAmount = FMath::Min(ExtinguisherAmount + Units, ExtinguisherFull);
	OnChanged.Broadcast();
	return true;
}

void UBalloonRunSubsystem::ReadExtinguisher(UWorld* World, bool bReadOwnership)
{
	const AFirstPersonCameraPawn* Pawn = FindPlayerPawn(World);
	const UExtinguisherComponent* Extinguisher = Pawn ? Pawn->GetExtinguisher() : nullptr;
	if (bReadOwnership)
	{
		// The player has it if it is in hand or on its rack.
		bool bOwned = Extinguisher && Pawn->GetActiveGun() == Extinguisher;
		ForEachExtinguisherRack(World, [&bOwned](UWeaponRackComponent& Rack) { bOwned |= Rack.HasWeapon(); });
		bHasExtinguisher = bOwned;
	}
	ExtinguisherAmount = bHasExtinguisher && Extinguisher ? Extinguisher->GetAmount() : 0.f;
}

void UBalloonRunSubsystem::ApplyToWorld(UWorld* World) const
{
	if (!bRunActive || !World)
	{
		return;
	}
	ForEachExtinguisherRack(World, [this](UWeaponRackComponent& Rack) { Rack.SetHasWeapon(bHasExtinguisher); });
	if (AFirstPersonCameraPawn* Pawn = FindPlayerPawn(World))
	{
		if (UExtinguisherComponent* Extinguisher = Pawn->GetExtinguisher())
		{
			Extinguisher->SetAmount(bHasExtinguisher ? ExtinguisherAmount : 0.f);
		}
	}
}

bool UBalloonRunWorldHook::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UBalloonRunWorldHook::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	// This runs before the actors' BeginPlay (racks reset themselves there); the world's delegate runs after.
	InWorld.OnWorldBeginPlay.AddUObject(this, &UBalloonRunWorldHook::HandleActorsBegunPlay);
}

void UBalloonRunWorldHook::HandleActorsBegunPlay()
{
	if (const UBalloonRunSubsystem* Run = UBalloonRunSubsystem::Get(this))
	{
		Run->ApplyToWorld(GetWorld());
	}
}
