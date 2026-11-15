#pragma once

#include "CoreMinimal.h"
#include "ResourceType.h"
#include "BalloonLevel/BalloonLevelStartOptions.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Subsystems/WorldSubsystem.h"
#include "BalloonRunSubsystem.generated.h"

class UGunComponent;
class UStoveFuelType;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBalloonRunChanged);

/**
 * What the player carries from one level to the next during a run (docs/features/safe-zone.md): banked gold, the
 * fuel in the basket's buckets (wood, coal, crystal), the rounds in the ammo crate, the gear bought once (hook,
 * rifle, shotgun) and the fire extinguisher (owned or not, and how full). Lives as long as the game instance, so it
 * survives OpenLevel.
 *
 * - A run starts with New Run in the main menu (StartNewRun, then StartLevel): no gold, no extinguisher, no hook,
 *   rifle or shotgun (StartingGear); 4 wood, 4 coal, 4 crystals and 40 rounds (StartingSupplies). StartNewRun also
 *   fills the biome pool (BiomePoolSource: City, Desert, Field); the first level is always FirstBiome (Forest).
 * - StartLevel opens RunLevel (the generated MainLevel) with LevelOptions and the next biome: FirstBiome for the
 *   first level, then a random one from the pool, which is removed from the pool.
 * - What the hook brings in during a level goes straight into the basket (StoreCollected): fuel into its bucket, ammo
 *   into the crate, each up to its max; Money is banked as gold at the end of the level, without a cap.
 * - At the end of a level (ABalloonLevelGenerator::CompleteLevel calls CompleteLevel): the basket is read, the level's
 *   Money and LevelCompleteGold are added, a checkpoint is saved and the shop level (ShopLevel, with an ALevelShopScreen) opens. "Continue the
 *   travel" calls ContinueTravel, which calls StartLevel. After LevelsPerRun levels the shop level shows the victory
 *   screen instead (IsRunWon).
 * - When the engine is destroyed during a run (FailLevel), the run goes back to the last checkpoint and the shop
 *   level opens again; the failed level's biome is played again.
 * - When a level starts during a run, the basket is set up from the run (UBalloonRunWorldHook): racks of gear not
 *   owned are empty, the buckets and the ammo crate hold what the run has, a bought extinguisher keeps its contents.
 *
 * Playing a level straight from the editor starts no run, so the basket stays as placed; the first CompleteLevel
 * then starts a run from what the basket holds at that moment.
 */
UCLASS()
class GUNSANDBALLOONS_API UBalloonRunSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static UBalloonRunSubsystem* Get(const UObject* WorldContextObject);

	/** A fresh run: no gold, no extinguisher. Called by the main menu's New Run. */
	UFUNCTION(BlueprintCallable, Category = "Run")
	void StartNewRun();

	UFUNCTION(BlueprintPure, Category = "Run")
	bool IsRunActive() const { return bRunActive; }

	/**
	 * Opens the next level of the run: RunLevel with LevelOptions and the next biome (FirstBiome for the first level,
	 * then one taken out of the biome pool). Returns whether the level was requested.
	 */
	UFUNCTION(BlueprintCallable, Category = "Run", meta = (WorldContext = "WorldContextObject"))
	bool StartLevel(const UObject* WorldContextObject);

	/**
	 * The level is done: keeps the basket (fuel, ammo, gear, extinguisher; hooked supplies are already in it) and
	 * banks the level's Money plus LevelCompleteGold as gold, saves a checkpoint and opens
	 * ShopLevel (the victory screen after the last level). Does nothing while the shop is already open.
	 */
	UFUNCTION(BlueprintCallable, Category = "Run", meta = (WorldContext = "WorldContextObject"))
	void CompleteLevel(const UObject* WorldContextObject);

	/** Leaves the shop level: StartLevel. Does nothing once the run is won. */
	UFUNCTION(BlueprintCallable, Category = "Run", meta = (WorldContext = "WorldContextObject"))
	void ContinueTravel(const UObject* WorldContextObject);

	/**
	 * The player died during a level of the run: goes back to the last checkpoint (the state at the last
	 * CompleteLevel, or the start of the run) and opens ShopLevel; the failed biome is played again. False when no
	 * run is active (the caller restarts the level instead).
	 */
	UFUNCTION(BlueprintCallable, Category = "Run", meta = (WorldContext = "WorldContextObject"))
	bool FailLevel(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "Run")
	bool IsShopOpen() const { return bShopOpen; }

	/** All LevelsPerRun levels are completed: the shop level shows the victory screen. */
	UFUNCTION(BlueprintPure, Category = "Run")
	bool IsRunWon() const { return bRunWon; }

	UFUNCTION(BlueprintPure, Category = "Run")
	int32 GetLevelsCompleted() const { return LevelsCompleted; }

	/** Biome of the level being played (or last played). */
	UFUNCTION(BlueprintPure, Category = "Run")
	FName GetCurrentBiome() const { return CurrentBiome; }

	/** Biomes still to come after the first level. */
	UFUNCTION(BlueprintPure, Category = "Run")
	TArray<FName> GetBiomePool() const { return BiomePool; }

	// --- Levels -----------------------------------------------------------------------------

	/** The level every run level opens (the procedural MainLevel, which generates the chosen biome). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Run|Levels")
	TSoftObjectPtr<UWorld> RunLevel;

	/** Settings of each run level; Biome is replaced by the biome StartLevel chooses. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Run|Levels")
	FBalloonLevelStartOptions LevelOptions;

	/** The first level of a run. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Run|Levels")
	FName FirstBiome;

	/** The pool New Run fills; each later level takes a random biome out of it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Run|Levels")
	TArray<FName> BiomePoolSource;

	/** Levels to complete to win the run (the first one plus the pool). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Run|Levels", meta = (ClampMin = "1"))
	int32 LevelsPerRun = 4;

	/** Gold added for each completed level. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Run|Gold", meta = (ClampMin = "0"))
	int32 LevelCompleteGold = 30;

	// --- Gold -------------------------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "Run|Gold")
	int32 GetGold() const { return Gold; }

	UFUNCTION(BlueprintCallable, Category = "Run|Gold")
	void AddGold(int32 Amount);

	/** Takes Price gold if there is enough. Returns whether it was paid. */
	UFUNCTION(BlueprintCallable, Category = "Run|Gold")
	bool SpendGold(int32 Price);

	// --- Extinguisher -----------------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "Run|Extinguisher")
	bool HasExtinguisher() const { return bHasExtinguisher; }

	/** Contents of the owned extinguisher, 0..100. */
	UFUNCTION(BlueprintPure, Category = "Run|Extinguisher")
	float GetExtinguisherAmount() const { return ExtinguisherAmount; }

	/** Buys the extinguisher for Price: it comes full (100). False if one is owned already or gold is short. */
	UFUNCTION(BlueprintCallable, Category = "Run|Extinguisher")
	bool BuyExtinguisher(int32 Price);

	/** Adds Units (up to 100) to the owned extinguisher for Price. False if none is owned, it is full or gold is short. */
	UFUNCTION(BlueprintCallable, Category = "Run|Extinguisher")
	bool RefillExtinguisher(int32 Price, float Units);

	/** Gold, ownership or contents changed (the shop redraws). */
	UPROPERTY(BlueprintAssignable, Category = "Run")
	FOnBalloonRunChanged OnChanged;

	/** The shop level CompleteLevel opens (/Game/Scenes/Shop: a camera, the filter and an ALevelShopScreen). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Run")
	TSoftObjectPtr<UWorld> ShopLevel;

	/** Sets up the basket of a level that has just begun from the run (see UBalloonRunWorldHook). */
	void ApplyToWorld(UWorld* World) const;

	/**
	 * Puts Amount of a hooked supply into the basket of World: wood, coal or crystals into their buckets, ammo into
	 * the crate, never past GetSupplyMax (nor a bucket's capacity). Returns how much went in. Money and None: 0.
	 */
	int32 StoreCollected(UWorld* World, EResourceType Type, int32 Amount) const;

	// --- Supplies (fuel buckets and the ammo crate) ------------------------------------------

	/** Wood, Coal, Crystal (pieces in its bucket) or Ammo (rounds in the crate). -1 = not read yet (as placed in the level). */
	UFUNCTION(BlueprintPure, Category = "Run|Supplies")
	int32 GetSupply(EResourceType Type) const;

	/** How much of Type the basket holds at most: 6 of each fuel, 60 rounds. 0 = not a supply. */
	UFUNCTION(BlueprintPure, Category = "Run|Supplies")
	static int32 GetSupplyMax(EResourceType Type);

	/** Adds Units of Type (up to its max) for Price. False if it is full, not a supply, not read yet or gold is short. */
	UFUNCTION(BlueprintCallable, Category = "Run|Supplies")
	bool BuySupply(EResourceType Type, int32 Price, int32 Units);

	/** Which bucket holds which fuel: a bucket counts as Type when its Fuel is FuelTypes[Type]. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Run|Supplies")
	TMap<EResourceType, TSoftObjectPtr<UStoveFuelType>> FuelTypes;

	/** What New Run starts with (clamped to GetSupplyMax). A supply not listed is read from the first level as placed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Run|Supplies")
	TMap<EResourceType, int32> StartingSupplies;

	// --- Gear (bought once) -----------------------------------------------------------------

	/** Whether the player owns the hand item of GearClass (one of SoldGear). */
	UFUNCTION(BlueprintPure, Category = "Run|Gear")
	bool OwnsGear(TSubclassOf<UGunComponent> GearClass) const;

	/** Buys the hand item of GearClass for Price. False if it is not in SoldGear, owned already or gold is short. */
	UFUNCTION(BlueprintCallable, Category = "Run|Gear")
	bool BuyGear(TSubclassOf<UGunComponent> GearClass, int32 Price);

	/** The hand items the run tracks (bought once in the shop): their racks are empty in a level until owned. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Run|Gear")
	TArray<TSubclassOf<UGunComponent>> SoldGear;

	/** Of SoldGear, what New Run starts with. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Run|Gear")
	TArray<TSubclassOf<UGunComponent>> StartingGear;

	static constexpr float ExtinguisherFull = 100.f;
	static constexpr int32 FuelMax = 6;
	static constexpr int32 AmmoMax = 60;

	UBalloonRunSubsystem();

private:
	/** Everything a checkpoint restores. */
	struct FCheckpoint
	{
		int32 Gold = 0;
		bool bHasExtinguisher = false;
		float ExtinguisherAmount = 0.f;
		int32 Supplies[ResourceTypes::Num];
		TSet<const UClass*> OwnedGear;
		TArray<FName> BiomePool;
		int32 LevelsCompleted = 0;
	};

	void SaveCheckpoint();
	void RestoreCheckpoint();
	/** Opens ShopLevel (the shop, or the victory screen once the run is won). */
	void OpenShop(UWorld* World);

	/** Banks the Money the hook brought in this level as gold (no cap). */
	void AddCollectedMoney(UWorld* World);

	/** Fills BiomePool from BiomePoolSource. */
	void ResetBiomePool();

	/** Reads the extinguisher from the level: whether the player has it and how full it is. */
	void ReadExtinguisher(UWorld* World, bool bReadOwnership);

	/** Reads the buckets and the ammo crate from the level. */
	void ReadSupplies(UWorld* World);

	/** Reads which of SoldGear the player has in the level (in hand or on its rack). */
	void ReadGear(UWorld* World);

	bool bRunActive = false;
	bool bShopOpen = false;
	int32 Gold = 0;
	bool bHasExtinguisher = false;
	float ExtinguisherAmount = 0.f;
	int32 Supplies[ResourceTypes::Num];
	/** Native classes (never unloaded), so no UPROPERTY needed. */
	TSet<const UClass*> OwnedGear;
	bool bRunWon = false;
	int32 LevelsCompleted = 0;
	TArray<FName> BiomePool;
	FName CurrentBiome;
	/** Set by FailLevel: the next StartLevel plays this biome again. */
	FName RetryBiome;
	FCheckpoint Checkpoint;
};

/**
 * Hands each game level to UBalloonRunSubsystem::ApplyToWorld once all its actors have begun play, and puts what the
 * hook brings in (UResourceLedger::OnCountChanged) into the basket (UBalloonRunSubsystem::StoreCollected).
 */
UCLASS()
class GUNSANDBALLOONS_API UBalloonRunWorldHook : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

private:
	void HandleActorsBegunPlay();

	UFUNCTION()
	void HandleCollected(EResourceType Type, int32 NewCount, int32 Delta);
};
