#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NemesisAirship.generated.h"

class ANemesisRocket;
class ANemesisBomber;
class ANemesisCropDuster;
class ANemesisMine;
class UBalloonFlightComponent;
class UEngineHealthComponent;
class UStaticMeshComponent;
class USceneComponent;

/** Attack band picked by distance to the balloon (AI ворогів, section 1). */
UENUM(BlueprintType)
enum class ENemesisBand : uint8
{
	Turrets, // closer than KillDistance: turrets kill the player
	Rockets, // D1
	Bombers, // D2 (not implemented yet)
	CropDusters, // D3 (not implemented yet)
};

/**
 * The nemesis airship. It cannot be destroyed. It follows the balloon's trail at the balloon's
 * slowest (no-fuel) speed, picks an attack by distance and kills the player with its turrets if
 * it gets close. All distances are in cm; every default is a prototype value that needs tuning.
 */
UCLASS()
class GUNSANDBALLOONS_API ANemesisAirship : public AActor
{
	GENERATED_BODY()

public:
	ANemesisAirship();

	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintPure, Category = "Nemesis")
	float GetDistanceToBalloon() const { return DistanceToBalloon; }

	UFUNCTION(BlueprintPure, Category = "Nemesis")
	ENemesisBand GetBand() const { return Band; }

	UFUNCTION(BlueprintPure, Category = "Nemesis")
	AActor* GetBalloon() const { return Balloon.Get(); }

	/** Where the rockets aim and what they must cross to hit: the middle of the basket. */
	FVector GetBasketLocation() const;

	/** Balloon velocity along its heading, for rocket intercept. */
	FVector GetBalloonVelocity() const;

	/** Rocket hit: −SlowdownPercent speed for SlowdownDuration; a new hit restarts the timer. */
	void ApplyRocketSlowdown();

	void OnRocketGone(ANemesisRocket* Rocket);

	/** Level scripting / testing: use Band's attack regardless of distance (bForce false = by distance). */
	void SetForcedBand(bool bForce, ENemesisBand NewBand) { bForceBand = bForce; ForcedBand = NewBand; }

	/** Before BeginPlay (deferred spawn): how far behind the balloon it starts, cm. */
	void SetStartDistance(float Distance) { StartDistance = Distance; }

	/** Testing: balloon damage is logged but not applied. */
	void SetInvulnerableBalloon(bool bInvulnerable) { bDebugInvulnerableBalloon = bInvulnerable; }

	/** Beacon: launch one crop duster for it now from the hangar. It doesn't count toward the
	 *  airship's own MaxCropDusters; only the map-wide limit (UNemesisTargetSubsystem::
	 *  MaxAttackingCropDusters) applies. False if there's no room or the airship is down. */
	bool RequestCropDuster();

	/** Is another live crop duster (not Except) holding that side (+1 right, −1 left)? */
	bool IsCropDusterSideTaken(int32 Side, const ANemesisCropDuster* Except) const;
	/** Own count under MaxCropDusters and room under the map-wide limit. */
	bool CanLaunchOwnCropDuster();

	/** Turrets start turning to the balloon (inside TurretWarnDistance). Hook sound/horn here. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Nemesis")
	void OnTurretWarning(bool bWarning);

	/** Turrets fired and killed the player. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Nemesis")
	void OnTurretsFired();

	UFUNCTION(BlueprintImplementableEvent, Category = "Nemesis")
	void OnRocketLaunched(ANemesisRocket* Rocket);

protected:
	virtual void BeginPlay() override;

	// --- Components ---
	UPROPERTY(VisibleAnywhere, Category = "Nemesis")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "Nemesis")
	TObjectPtr<UStaticMeshComponent> Body;

	UPROPERTY(VisibleAnywhere, Category = "Nemesis")
	TArray<TObjectPtr<UStaticMeshComponent>> Turrets;

	UPROPERTY(VisibleAnywhere, Category = "Nemesis")
	TArray<TObjectPtr<UStaticMeshComponent>> Props;

	UPROPERTY(VisibleAnywhere, Category = "Nemesis")
	TObjectPtr<USceneComponent> RocketMuzzle;

	/** Planes leave from the hangar under the hull (XY only: they start at their own altitude). */
	UPROPERTY(VisibleAnywhere, Category = "Nemesis")
	TObjectPtr<USceneComponent> HangarMouth;

	// --- Movement ---
	/** 0 = fly at the balloon's MinSpeed (design). >0 overrides it, e.g. to test the turrets. */
	UPROPERTY(EditAnywhere, Category = "Nemesis|Movement", meta = (ClampMin = "0"))
	float SpeedOverride = 0.f;

	/** At BeginPlay, the airship centre jumps this far behind the balloon (0 = stay where placed).
	 *  8000 puts the nose ~60 m from the basket: inside D1, so rockets start right away. TBD */
	UPROPERTY(EditAnywhere, Category = "Nemesis|Movement", meta = (ClampMin = "0"))
	float StartDistance = 8000.f;

	/** The airship flies this much higher than the balloon's trail. TBD */
	UPROPERTY(EditAnywhere, Category = "Nemesis|Movement")
	float HeightOffset = 1500.f;

	/** How often a trail point is recorded, s. */
	UPROPERTY(EditAnywhere, Category = "Nemesis|Movement", meta = (ClampMin = "0.05"))
	float TrailSampleInterval = 0.5f;

	UPROPERTY(EditAnywhere, Category = "Nemesis|Movement", meta = (ClampMin = "0"))
	float TurnInterpSpeed = 1.f;

	UPROPERTY(EditAnywhere, Category = "Nemesis|Movement")
	float PropSpinSpeed = 540.f;

	/** Basket centre relative to the balloon actor (rocket target and hit point). */
	UPROPERTY(EditAnywhere, Category = "Nemesis|Distance")
	FVector BasketOffset = FVector(0.f, 0.f, 60.f);

	// --- Distances (cm). Measured from DistanceReference (the nose) to the basket. ---
	UPROPERTY(EditAnywhere, Category = "Nemesis|Distance")
	FVector DistanceReference = FVector(2400.f, 0.f, 0.f);

	UPROPERTY(EditAnywhere, Category = "Nemesis|Distance")
	float KillDistance = 3000.f;

	UPROPERTY(EditAnywhere, Category = "Nemesis|Distance")
	float TurretWarnDistance = 5000.f;

	/** D1 rockets: KillDistance .. RocketBandMax. */
	UPROPERTY(EditAnywhere, Category = "Nemesis|Distance")
	float RocketBandMax = 10000.f;

	/** D2 bombers: RocketBandMax .. BomberBandMax; beyond it D3 crop dusters. */
	UPROPERTY(EditAnywhere, Category = "Nemesis|Distance")
	float BomberBandMax = 22000.f;

	// --- Rockets ---
	UPROPERTY(EditAnywhere, Category = "Nemesis|Rockets")
	TSubclassOf<ANemesisRocket> RocketClass;

	UPROPERTY(EditAnywhere, Category = "Nemesis|Rockets", meta = (ClampMin = "0"))
	float RocketCooldown = 10.f; // team decision 2026-11-15 (was 7 s). TBD

	UPROPERTY(EditAnywhere, Category = "Nemesis|Rockets", meta = (ClampMin = "0"))
	int32 MaxRockets = 2;

	UPROPERTY(EditAnywhere, Category = "Nemesis|Rockets", meta = (ClampMin = "0", ClampMax = "1"))
	float SlowdownPercent = 0.2f;

	UPROPERTY(EditAnywhere, Category = "Nemesis|Rockets", meta = (ClampMin = "0"))
	float SlowdownDuration = 3.f;

	// --- Planes (D2 bomber, D3 crop dusters) ---
	UPROPERTY(EditAnywhere, Category = "Nemesis|Planes")
	TSubclassOf<ANemesisBomber> BomberClass;

	UPROPERTY(EditAnywhere, Category = "Nemesis|Planes", meta = (ClampMin = "0"))
	float BomberCooldown = 18.f;

	UPROPERTY(EditAnywhere, Category = "Nemesis|Planes", meta = (ClampMin = "0"))
	int32 MaxBombers = 1;

	UPROPERTY(EditAnywhere, Category = "Nemesis|Planes", meta = (ClampMin = "0"))
	int32 MaxMines = 3; // playtest 2026-11-14: 6 was too many. TBD

	UPROPERTY(EditAnywhere, Category = "Nemesis|Planes")
	TSubclassOf<ANemesisCropDuster> CropDusterClass;

	UPROPERTY(EditAnywhere, Category = "Nemesis|Planes", meta = (ClampMin = "0"))
	float CropDusterCooldown = 15.f;

	/** The airship's own crop dusters in the air at once (beacons' ones don't count); one launch per
	 *  CropDusterCooldown, and never above the map-wide limit of attacking crop dusters. */
	UPROPERTY(EditAnywhere, Category = "Nemesis|Planes", meta = (ClampMin = "0"))
	int32 MaxCropDusters = 2;

	// --- Turrets ---
	UPROPERTY(EditAnywhere, Category = "Nemesis|Turrets", meta = (ClampMin = "0"))
	float TurretTurnSpeed = 3.f;

	UPROPERTY(EditAnywhere, Category = "Nemesis|Debug")
	bool bDebugDraw = false;

	/** Testing: use ForcedBand's attack regardless of distance (turrets still kill by distance). */
	UPROPERTY(EditAnywhere, Category = "Nemesis|Debug")
	bool bForceBand = false;

	UPROPERTY(EditAnywhere, Category = "Nemesis|Debug", meta = (EditCondition = "bForceBand"))
	ENemesisBand ForcedBand = ENemesisBand::Rockets;

	/** Demo: rockets, bomber and crop dusters all at once (each on its own cooldown and limit). */
	UPROPERTY(EditAnywhere, Category = "Nemesis|Debug")
	bool bDebugAllAttacks = false;

	/** Demo: the balloon's engine takes no damage and the turrets can't kill. */
	UPROPERTY(EditAnywhere, Category = "Nemesis|Debug")
	bool bDebugInvulnerableBalloon = false;

private:
	void FindBalloon();
	void RecordTrail();
	void Move(float DeltaSeconds);
	void UpdateBand();
	void UpdateTurrets(float DeltaSeconds);
	void UpdateAttacks(float DeltaSeconds);
	void LaunchRocket();
	void LaunchBomber();
	/** bOwn: counts toward MaxCropDusters (false: sent for a beacon). */
	void LaunchCropDuster(bool bOwn = true);
	float GetSpeed() const;

	TWeakObjectPtr<AActor> Balloon;
	TWeakObjectPtr<UBalloonFlightComponent> Flight;
	TWeakObjectPtr<UEngineHealthComponent> Health;

	TArray<FVector> Trail;
	TArray<FTransform> TurretRest;
	TArray<TWeakObjectPtr<ANemesisRocket>> ActiveRockets;
	TArray<TWeakObjectPtr<ANemesisBomber>> ActiveBombers;
	TArray<TWeakObjectPtr<ANemesisCropDuster>> ActiveCropDusters;

	float TrailTimer = 0.f;
	float RocketTimer = 0.f;
	float BomberTimer = 0.f;
	float CropDusterTimer = 0.f;
	float DistanceToBalloon = 0.f;
	ENemesisBand Band = ENemesisBand::CropDusters;
	bool bTurretWarning = false;
	bool bKilled = false;
	FTimerHandle SlowdownTimer;
};
