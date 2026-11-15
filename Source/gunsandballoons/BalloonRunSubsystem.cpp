#include "BalloonRunSubsystem.h"

#include "AmmoSupplyComponent.h"
#include "BalloonLevel/BalloonLevelStartOptions.h"
#include "BalloonPlayerController.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "ExtinguisherComponent.h"
#include "FirstPersonCameraPawn.h"
#include "FuelSupplyComponent.h"
#include "HookComponent.h"
#include "Kismet/GameplayStatics.h"
#include "ResourceLedger.h"
#include "RifleComponent.h"
#include "ShotgunComponent.h"
#include "StoveFuelType.h"
#include "UObject/UObjectIterator.h"
#include "WeaponRackComponent.h"

namespace
{
	AFirstPersonCameraPawn* FindPlayerPawn(UWorld* World)
	{
		const APlayerController* Player = World ? World->GetFirstPlayerController() : nullptr;
		return Player ? Cast<AFirstPersonCameraPawn>(Player->GetPawn()) : nullptr;
	}

	/**
	 * Whether Component is a live part of World. TObjectIterator also finds stale copies left by Blueprint recompiles
	 * (TRASH_*, unregistered but still outered to the level); filling those first used up the run's supplies.
	 */
	bool IsLiveIn(const UActorComponent* Component, const UWorld* World)
	{
		return World && IsValid(Component) && Component->IsRegistered() && Component->GetWorld() == World
			&& IsValid(Component->GetOwner());
	}

	/** Calls Visit for every rack in World that holds the hand item of ItemClass. */
	void ForEachRack(UWorld* World, const UClass* ItemClass, TFunctionRef<void(UWeaponRackComponent&)> Visit)
	{
		for (TObjectIterator<UWeaponRackComponent> It; It; ++It)
		{
			if (IsLiveIn(*It, World) && It->ItemClass && ItemClass && It->ItemClass->IsChildOf(ItemClass))
			{
				Visit(**It);
			}
		}
	}

	void ForEachExtinguisherRack(UWorld* World, TFunctionRef<void(UWeaponRackComponent&)> Visit)
	{
		ForEachRack(World, UExtinguisherComponent::StaticClass(), Visit);
	}

	/** Every ammo supply in World (the crate). */
	TArray<UAmmoSupplyComponent*> FindAmmoSupplies(UWorld* World)
	{
		TArray<UAmmoSupplyComponent*> Found;
		for (TObjectIterator<UAmmoSupplyComponent> It; It; ++It)
		{
			if (IsLiveIn(*It, World))
			{
				Found.Add(*It);
			}
		}
		return Found;
	}

	/** Calls Visit for every fuel bucket in World that holds Fuel. */
	void ForEachBucket(UWorld* World, const UStoveFuelType* Fuel, TFunctionRef<void(UFuelSupplyComponent&)> Visit)
	{
		for (TObjectIterator<UFuelSupplyComponent> It; It; ++It)
		{
			if (Fuel && IsLiveIn(*It, World) && It->Fuel == Fuel)
			{
				Visit(**It);
			}
		}
	}
}

UBalloonRunSubsystem::UBalloonRunSubsystem()
{
	for (int32& Supply : Supplies)
	{
		Supply = INDEX_NONE;
	}
	FuelTypes.Add(EResourceType::Wood, TSoftObjectPtr<UStoveFuelType>(FSoftObjectPath(TEXT("/Game/Data/Stove/DA_Fuel_Wood.DA_Fuel_Wood"))));
	FuelTypes.Add(EResourceType::Coal, TSoftObjectPtr<UStoveFuelType>(FSoftObjectPath(TEXT("/Game/Data/Stove/DA_Fuel_Coal.DA_Fuel_Coal"))));
	FuelTypes.Add(EResourceType::Crystal, TSoftObjectPtr<UStoveFuelType>(FSoftObjectPath(TEXT("/Game/Data/Stove/DA_Fuel_Crystal.DA_Fuel_Crystal"))));
	ShopLevel = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/Scenes/Shop.Shop")));
	SoldGear = { UHookComponent::StaticClass(), URifleComponent::StaticClass(), UShotgunComponent::StaticClass() };
	// New Run: no hook, rifle or shotgun; 4 of each fuel and 40 rounds.
	StartingGear.Reset();
	StartingSupplies.Add(EResourceType::Wood, 4);
	StartingSupplies.Add(EResourceType::Coal, 4);
	StartingSupplies.Add(EResourceType::Crystal, 4);
	StartingSupplies.Add(EResourceType::Ammo, 40);
	RunLevel = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/Scenes/MainLevel.MainLevel")));
	FirstBiome = TEXT("Forest");
	BiomePoolSource = { TEXT("City"), TEXT("Desert"), TEXT("Field") };
	for (int32& Supply : Checkpoint.Supplies)
	{
		Supply = INDEX_NONE;
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
	for (int32& Supply : Supplies)
	{
		Supply = INDEX_NONE;
	}
	for (const auto& Pair : StartingSupplies)
	{
		const int32 Max = GetSupplyMax(Pair.Key);
		if (Max > 0)
		{
			Supplies[ResourceTypes::ToIndex(Pair.Key)] = FMath::Clamp(Pair.Value, 0, Max);
		}
	}
	OwnedGear.Reset();
	for (const TSubclassOf<UGunComponent>& Gear : StartingGear)
	{
		if (Gear && SoldGear.Contains(Gear))
		{
			OwnedGear.Add(*Gear);
		}
	}
	bRunWon = false;
	LevelsCompleted = 0;
	CurrentBiome = NAME_None;
	RetryBiome = NAME_None;
	ResetBiomePool();
	// Dying in the first level goes back to the shop with what the run started with.
	SaveCheckpoint();
	OnChanged.Broadcast();
}

void UBalloonRunSubsystem::ResetBiomePool()
{
	BiomePool.Reset();
	for (const FName& Biome : BiomePoolSource)
	{
		if (!Biome.IsNone() && Biome != FirstBiome)
		{
			BiomePool.AddUnique(Biome);
		}
	}
}

bool UBalloonRunSubsystem::StartLevel(const UObject* WorldContextObject)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	UBalloonLevelLaunchSubsystem* Launch = World && World->GetGameInstance() ? World->GetGameInstance()->GetSubsystem<UBalloonLevelLaunchSubsystem>() : nullptr;
	if (!Launch || bRunWon)
	{
		return false;
	}

	// The first level is always FirstBiome; later ones come out of the pool (the failed one again after a death).
	FName Biome = FirstBiome;
	if (LevelsCompleted > 0)
	{
		if (!RetryBiome.IsNone() && BiomePool.Contains(RetryBiome))
		{
			Biome = RetryBiome;
		}
		else if (BiomePool.Num() > 0)
		{
			Biome = BiomePool[FMath::RandRange(0, BiomePool.Num() - 1)];
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("BalloonRun: the biome pool is empty; playing %s."), *FirstBiome.ToString());
		}
	}

	FBalloonLevelStartOptions Options = LevelOptions;
	Options.Biome = Biome;
	FText Error;
	if (!Launch->StartLevel(Options, RunLevel, Error))
	{
		UE_LOG(LogTemp, Warning, TEXT("BalloonRun: could not start the %s level: %s"), *Biome.ToString(), *Error.ToString());
		return false;
	}
	BiomePool.Remove(Biome);
	CurrentBiome = Biome;
	RetryBiome = NAME_None;
	bShopOpen = false;
	OnChanged.Broadcast();
	return true;
}

void UBalloonRunSubsystem::CompleteLevel(const UObject* WorldContextObject)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World || bShopOpen)
	{
		return;
	}

	// Without a run (a level played straight from the editor), the run starts from what the basket holds now.
	ReadExtinguisher(World, !bRunActive);
	ReadSupplies(World);
	if (!bRunActive)
	{
		ReadGear(World);
		bRunWon = false;
		LevelsCompleted = 0;
		CurrentBiome = FirstBiome;
		ResetBiomePool();
	}
	bRunActive = true;
	AddCollectedMoney(World);
	Gold += LevelCompleteGold;
	++LevelsCompleted;
	bRunWon = LevelsCompleted >= LevelsPerRun;
	SaveCheckpoint();
	OnChanged.Broadcast();
	OpenShop(World);
}

void UBalloonRunSubsystem::OpenShop(UWorld* World)
{
	bShopOpen = true;
	if (ShopLevel.IsNull())
	{
		UE_LOG(LogTemp, Warning, TEXT("BalloonRun: ShopLevel is not set; going straight to the next level."));
		ContinueTravel(World);
		return;
	}
	if (ABalloonPlayerController* BalloonPlayer = Cast<ABalloonPlayerController>(World->GetFirstPlayerController()))
	{
		// Lets go of anything held (the trigger, a grab) before the level changes.
		BalloonPlayer->UseMenuInput();
	}
	// Opening a level is deferred to the end of the frame.
	UGameplayStatics::OpenLevelBySoftObjectPtr(World, ShopLevel);
}

void UBalloonRunSubsystem::ContinueTravel(const UObject* WorldContextObject)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World || !bShopOpen || bRunWon)
	{
		return;
	}
	if (!bRunActive)
	{
		// The shop level played on its own (no run): nowhere to go.
		UE_LOG(LogTemp, Warning, TEXT("BalloonRun: no run to continue."));
		return;
	}
	StartLevel(World);
}

bool UBalloonRunSubsystem::FailLevel(const UObject* WorldContextObject)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World || !bRunActive || bShopOpen)
	{
		return false;
	}
	const FName Failed = CurrentBiome;
	RestoreCheckpoint();
	RetryBiome = Failed;
	OnChanged.Broadcast();
	OpenShop(World);
	return true;
}

void UBalloonRunSubsystem::SaveCheckpoint()
{
	Checkpoint.Gold = Gold;
	Checkpoint.bHasExtinguisher = bHasExtinguisher;
	Checkpoint.ExtinguisherAmount = ExtinguisherAmount;
	FMemory::Memcpy(Checkpoint.Supplies, Supplies, sizeof(Supplies));
	Checkpoint.OwnedGear = OwnedGear;
	Checkpoint.BiomePool = BiomePool;
	Checkpoint.LevelsCompleted = LevelsCompleted;
}

void UBalloonRunSubsystem::RestoreCheckpoint()
{
	Gold = Checkpoint.Gold;
	bHasExtinguisher = Checkpoint.bHasExtinguisher;
	ExtinguisherAmount = Checkpoint.ExtinguisherAmount;
	FMemory::Memcpy(Supplies, Checkpoint.Supplies, sizeof(Supplies));
	OwnedGear = Checkpoint.OwnedGear;
	BiomePool = Checkpoint.BiomePool;
	LevelsCompleted = Checkpoint.LevelsCompleted;
	bRunWon = false;
}

void UBalloonRunSubsystem::AddCollectedMoney(UWorld* World)
{
	// Money has no cap. Hooked ammo and fuel are already in the basket (StoreCollected), read by ReadSupplies.
	if (const UResourceLedger* Ledger = World ? World->GetSubsystem<UResourceLedger>() : nullptr)
	{
		Gold += Ledger->GetCount(EResourceType::Money);
	}
}

int32 UBalloonRunSubsystem::StoreCollected(UWorld* World, EResourceType Type, int32 Amount) const
{
	if (!World || Amount <= 0)
	{
		return 0;
	}
	if (Type == EResourceType::Ammo)
	{
		// All of it in the first crate (a basket has one); an unlimited crate (a level as placed) takes nothing.
		const TArray<UAmmoSupplyComponent*> Crates = FindAmmoSupplies(World);
		if (Crates.Num() == 0 || Crates[0]->GetRounds() < 0)
		{
			return 0;
		}
		int32 Total = 0;
		for (const UAmmoSupplyComponent* Crate : Crates)
		{
			Total += FMath::Max(0, Crate->GetRounds());
		}
		const int32 Put = FMath::Clamp(AmmoMax - Total, 0, Amount);
		Crates[0]->SetRounds(Crates[0]->GetRounds() + Put);
		return Put;
	}
	const TSoftObjectPtr<UStoveFuelType>* Fuel = FuelTypes.Find(Type);
	if (!Fuel || GetSupplyMax(Type) <= 0)
	{
		return 0;
	}
	const UStoveFuelType* FuelType = Fuel->LoadSynchronous();
	int32 Total = 0;
	ForEachBucket(World, FuelType, [&Total](UFuelSupplyComponent& Bucket) { Total += Bucket.GetCount(); });
	// The buckets of the type are filled in turn, each up to its capacity, all of them up to FuelMax.
	int32 Left = FMath::Clamp(FuelMax - Total, 0, Amount);
	const int32 Wanted = Left;
	ForEachBucket(World, FuelType, [&Left](UFuelSupplyComponent& Bucket) { Left -= Bucket.AddFuel(Left); });
	return Wanted - Left;
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

int32 UBalloonRunSubsystem::GetSupply(EResourceType Type) const
{
	return GetSupplyMax(Type) > 0 ? Supplies[ResourceTypes::ToIndex(Type)] : INDEX_NONE;
}

int32 UBalloonRunSubsystem::GetSupplyMax(EResourceType Type)
{
	switch (Type)
	{
	case EResourceType::Wood:
	case EResourceType::Coal:
	case EResourceType::Crystal:
		return FuelMax;
	case EResourceType::Ammo:
		return AmmoMax;
	default:
		return 0;
	}
}

bool UBalloonRunSubsystem::BuySupply(EResourceType Type, int32 Price, int32 Units)
{
	const int32 Have = GetSupply(Type);
	if (Have < 0 || Units <= 0 || Have >= GetSupplyMax(Type) || !SpendGold(Price))
	{
		return false;
	}
	Supplies[ResourceTypes::ToIndex(Type)] = FMath::Min(Have + Units, GetSupplyMax(Type));
	OnChanged.Broadcast();
	return true;
}

bool UBalloonRunSubsystem::OwnsGear(TSubclassOf<UGunComponent> GearClass) const
{
	return GearClass && OwnedGear.Contains(*GearClass);
}

bool UBalloonRunSubsystem::BuyGear(TSubclassOf<UGunComponent> GearClass, int32 Price)
{
	if (!GearClass || !SoldGear.Contains(GearClass) || OwnsGear(GearClass) || !SpendGold(Price))
	{
		return false;
	}
	OwnedGear.Add(*GearClass);
	OnChanged.Broadcast();
	return true;
}

void UBalloonRunSubsystem::ReadSupplies(UWorld* World)
{
	for (const auto& Pair : FuelTypes)
	{
		if (GetSupplyMax(Pair.Key) <= 0)
		{
			continue;
		}
		int32 Count = 0;
		bool bFound = false;
		ForEachBucket(World, Pair.Value.LoadSynchronous(), [&Count, &bFound](UFuelSupplyComponent& Bucket)
		{
			Count += Bucket.GetCount();
			bFound = true;
		});
		if (bFound)
		{
			Supplies[ResourceTypes::ToIndex(Pair.Key)] = FMath::Min(Count, FuelMax);
		}
	}

	// An unlimited crate (a level as placed) counts as full.
	const TArray<UAmmoSupplyComponent*> Crates = FindAmmoSupplies(World);
	if (Crates.Num() > 0)
	{
		int32 Rounds = 0;
		for (const UAmmoSupplyComponent* Crate : Crates)
		{
			Rounds = Crate->GetRounds() < 0 ? AmmoMax : Rounds + Crate->GetRounds();
		}
		Supplies[ResourceTypes::ToIndex(EResourceType::Ammo)] = FMath::Min(Rounds, AmmoMax);
	}
}

void UBalloonRunSubsystem::ReadGear(UWorld* World)
{
	const AFirstPersonCameraPawn* Pawn = FindPlayerPawn(World);
	const UGunComponent* InHand = Pawn ? Pawn->GetActiveGun() : nullptr;
	OwnedGear.Reset();
	for (const TSubclassOf<UGunComponent>& Gear : SoldGear)
	{
		if (!Gear)
		{
			continue;
		}
		bool bOwned = InHand && InHand->IsA(Gear);
		ForEachRack(World, Gear, [&bOwned](UWeaponRackComponent& Rack) { bOwned |= Rack.HasWeapon(); });
		if (bOwned)
		{
			OwnedGear.Add(*Gear);
		}
	}
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
	for (const TSubclassOf<UGunComponent>& Gear : SoldGear)
	{
		const bool bOwned = OwnsGear(Gear);
		ForEachRack(World, Gear, [bOwned](UWeaponRackComponent& Rack) { Rack.SetHasWeapon(bOwned); });
	}

	// Fuel: the buckets of a type are filled in turn, each up to its capacity.
	for (const auto& Pair : FuelTypes)
	{
		int32 Left = GetSupply(Pair.Key);
		if (Left >= 0)
		{
			ForEachBucket(World, Pair.Value.LoadSynchronous(), [&Left](UFuelSupplyComponent& Bucket)
			{
				const int32 Put = FMath::Min(Left, Bucket.GetCapacity());
				Bucket.SetCount(Put);
				Left -= Put;
			});
		}
	}

	// Ammo: all of it in the first crate (a basket has one).
	const int32 Rounds = GetSupply(EResourceType::Ammo);
	if (Rounds >= 0)
	{
		const TArray<UAmmoSupplyComponent*> Crates = FindAmmoSupplies(World);
		for (int32 Index = 0; Index < Crates.Num(); ++Index)
		{
			Crates[Index]->SetRounds(Index == 0 ? Rounds : 0);
		}
	}
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
	if (UResourceLedger* Ledger = GetWorld() ? GetWorld()->GetSubsystem<UResourceLedger>() : nullptr)
	{
		Ledger->OnCountChanged.AddUniqueDynamic(this, &UBalloonRunWorldHook::HandleCollected);
	}
}

void UBalloonRunWorldHook::HandleCollected(EResourceType Type, int32 NewCount, int32 Delta)
{
	// A hooked pickup arrived: its ammo or fuel goes into the basket (Money stays in the ledger until the level ends).
	if (const UBalloonRunSubsystem* Run = UBalloonRunSubsystem::Get(this))
	{
		Run->StoreCollected(GetWorld(), Type, Delta);
	}
}
