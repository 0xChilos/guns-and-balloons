#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Subsystems/WorldSubsystem.h"
#include "BalloonRunSubsystem.generated.h"

class ALevelShopScreen;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBalloonRunChanged);

/**
 * What the player carries from one level to the next during a run (docs/features/safe-zone.md): banked gold and
 * the fire extinguisher (owned or not, and how full). Lives as long as the game instance, so it survives
 * OpenLevel.
 *
 * - A run starts with New Run in the main menu (StartNewRun): no gold, no extinguisher.
 * - At the end of a level, call CompleteLevel: the level's Money (UResourceLedger) is banked as gold, the game
 *   pauses and the shop (ALevelShopScreen) opens. "Continue the travel" calls ContinueTravel, which opens the
 *   next level.
 * - When a level starts during a run, the basket is set up from the run: the extinguisher's rack is empty until
 *   one has been bought, and a bought one keeps its contents (UBalloonRunWorldHook).
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
	 * The level is done: banks the level's Money as gold, keeps the extinguisher's contents, pauses the game and
	 * opens the shop. NextLevel is the level "Continue the travel" opens; leave it empty to open the current map
	 * again (a procedural level generates a new layout). Does nothing while the shop is already open.
	 */
	UFUNCTION(BlueprintCallable, Category = "Run", meta = (WorldContext = "WorldContextObject"))
	void CompleteLevel(const UObject* WorldContextObject, TSoftObjectPtr<UWorld> NextLevel);

	/** Leaves the shop: unpauses and opens the level given to CompleteLevel. */
	UFUNCTION(BlueprintCallable, Category = "Run", meta = (WorldContext = "WorldContextObject"))
	void ContinueTravel(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "Run")
	bool IsShopOpen() const { return bShopOpen; }

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

	/** The shop to open on CompleteLevel. TBD: a Blueprint child to tune its look. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Run")
	TSubclassOf<ALevelShopScreen> ShopScreenClass;

	/** Sets up the basket of a level that has just begun from the run (see UBalloonRunWorldHook). */
	void ApplyToWorld(UWorld* World) const;

	static constexpr float ExtinguisherFull = 100.f;

private:
	/** Reads the extinguisher from the level: whether the player has it and how full it is. */
	void ReadExtinguisher(UWorld* World, bool bReadOwnership);

	bool bRunActive = false;
	bool bShopOpen = false;
	int32 Gold = 0;
	bool bHasExtinguisher = false;
	float ExtinguisherAmount = 0.f;
	TSoftObjectPtr<UWorld> PendingLevel;
};

/** Hands each game level to UBalloonRunSubsystem::ApplyToWorld once all its actors have begun play. */
UCLASS()
class GUNSANDBALLOONS_API UBalloonRunWorldHook : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

private:
	void HandleActorsBegunPlay();
};
