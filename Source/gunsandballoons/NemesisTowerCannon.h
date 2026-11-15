#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NemesisFx.h"
#include "NemesisTowerCannon.generated.h"

class ANemesisBullet;
class UBoxComponent;
class UPointLightComponent;
class UStaticMeshComponent;

UENUM(BlueprintType)
enum class ETowerCannonState : uint8
{
	Idle,
	Aiming,
	Reloading,
	Destroyed,
};

/**
 * Desert tower with a cannon on top (enemy AI, 7.1; team decision 2026-11-15). Uses the team's
 * SM_TurretTower model when present, placeholder cubes otherwise. On it stand our steampunk cannon and
 * fuel tank (Content/Assets/Enemies/TowerGun.blend: SM_TowerGun_Mount / _Barrel / _FuelTank); without
 * them, the team's head, twin guns and boiler (the boiler is then the fuel tank, the weak spot).
 * The tower is solid level geometry. With the basket inside Range and in line of sight (a sandstorm
 * blocks it) the cannon turns to the basket and aims for AimTime (a glow at the muzzle warns the
 * player), fires one shell straight at the basket (no drop), then reloads for ReloadTime.
 * Two ways to destroy it (team decision 2026-11-15): one hit from any weapon on the fuel tank, which
 * with our model hangs under the deck where it can be seen from every side; or enough hits on the
 * cannon itself (revolver 4, shotgun 3, rifle 2, like a plane's fuselage). Either way the tank and
 * the cannon blow up and the tower stays standing with an empty, smoking deck. Shots pass through
 * the lattice tower (it ignores the Visibility channel) so the tank under the deck can be hit.
 */
UCLASS()
class GUNSANDBALLOONS_API ANemesisTowerCannon : public AActor
{
	GENERATED_BODY()

public:
	ANemesisTowerCannon();

	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintPure, Category = "Tower")
	ETowerCannonState GetState() const { return State; }

	/** Aiming started (muzzle glow on) / stopped. Hook for a sound. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Tower")
	void OnAimChanged(bool bAiming);

	UFUNCTION(BlueprintImplementableEvent, Category = "Tower")
	void OnShot();

	/** The fuel tank was shot: the cannon and the tower are gone. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Tower")
	void OnTowerDestroyed();

protected:
	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;

	UPROPERTY(VisibleAnywhere, Category = "Tower")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "Tower")
	TObjectPtr<UStaticMeshComponent> Tower;

	UPROPERTY(VisibleAnywhere, Category = "Tower")
	TObjectPtr<UStaticMeshComponent> Platform;

	/** Turns the cannon (yaw). */
	UPROPERTY(VisibleAnywhere, Category = "Tower")
	TObjectPtr<USceneComponent> CannonYaw;

	UPROPERTY(VisibleAnywhere, Category = "Tower")
	TObjectPtr<UStaticMeshComponent> Carriage;

	/** Raises the barrel (pitch). */
	UPROPERTY(VisibleAnywhere, Category = "Tower")
	TObjectPtr<USceneComponent> CannonPitch;

	UPROPERTY(VisibleAnywhere, Category = "Tower")
	TObjectPtr<UStaticMeshComponent> Barrel;

	/** Glows at the muzzle while aiming, so the player sees the shot coming. */
	UPROPERTY(VisibleAnywhere, Category = "Tower")
	TObjectPtr<UPointLightComponent> MuzzleGlow;

	UPROPERTY(VisibleAnywhere, Category = "Tower")
	TObjectPtr<UStaticMeshComponent> FuelTank;

	/** Invisible hit zone around the tank, a bit bigger than the model so it can be hit at range. */
	UPROPERTY(VisibleAnywhere, Category = "Tower")
	TObjectPtr<UBoxComponent> TankHitBox;

	/** TankHitBox half size, cm. TBD */
	UPROPERTY(EditAnywhere, Category = "Tower")
	FVector TankHitBoxExtent = FVector(110.f, 110.f, 130.f);

	/** At BeginPlay, stretch the tower (the model) or raise it (the cubes) so the gun deck is at the
	 *  player's height (basket + DeckHeightOffset). Off: the model's own 20 m deck / TowerHeight.
	 *  Team decision 2026-11-15. */
	UPROPERTY(EditAnywhere, Category = "Tower")
	bool bDeckAtPlayerHeight = true;

	/** Deck height relative to the basket centre, cm (with bDeckAtPlayerHeight). TBD */
	UPROPERTY(EditAnywhere, Category = "Tower")
	float DeckHeightOffset = -150.f;

	/** Tower height, cm (the cannon stands on top). */
	UPROPERTY(EditAnywhere, Category = "Tower", meta = (ClampMin = "100"))
	float TowerHeight = 2500.f;

	/** cm (180 m). TBD */
	UPROPERTY(EditAnywhere, Category = "Tower", meta = (ClampMin = "0"))
	float Range = 18000.f;

	UPROPERTY(EditAnywhere, Category = "Tower", meta = (ClampMin = "0"))
	float AimTime = 2.f;

	UPROPERTY(EditAnywhere, Category = "Tower", meta = (ClampMin = "0"))
	float ReloadTime = 3.f; // with AimTime: one shot every 5 s (team damage table 2026-11-15). TBD

	/** Cannon turn speed, degrees/s. */
	UPROPERTY(EditAnywhere, Category = "Tower", meta = (ClampMin = "0"))
	float TurnSpeed = 60.f;

	/** Balloon damage per shell. TBD */
	UPROPERTY(EditAnywhere, Category = "Tower", meta = (ClampMin = "0"))
	float Damage = 12.f;

	/** cm/s (250 m/s). */
	UPROPERTY(EditAnywhere, Category = "Tower", meta = (ClampMin = "1"))
	float ShellSpeed = 25000.f;

	UPROPERTY(EditAnywhere, Category = "Tower")
	TSubclassOf<ANemesisBullet> ShellClass;

	// Hits on the cannon (mount or barrel) that destroy it, per weapon. One shotgun shot is one hit. TBD
	UPROPERTY(EditAnywhere, Category = "Tower|Damage", meta = (ClampMin = "1"))
	int32 RevolverCannonHits = 4;

	UPROPERTY(EditAnywhere, Category = "Tower|Damage", meta = (ClampMin = "1"))
	int32 ShotgunCannonHits = 3;

	UPROPERTY(EditAnywhere, Category = "Tower|Damage", meta = (ClampMin = "1"))
	int32 RifleCannonHits = 2;

	/** Anything else that deals point damage. TBD */
	UPROPERTY(EditAnywhere, Category = "Tower|Damage", meta = (ClampMin = "1"))
	int32 OtherCannonHits = 3;

	/** Pellets of one shotgun shot landing within this time count as one hit, s. */
	UPROPERTY(EditAnywhere, Category = "Tower|Damage", meta = (ClampMin = "0"))
	float ShotgunPelletWindow = 0.15f;

	/** The empty deck keeps smoking this long after the blast, s. */
	UPROPERTY(EditAnywhere, Category = "Tower", meta = (ClampMin = "0"))
	float WreckSmokeTime = 8.f;

private:
	UFUNCTION()
	void HandlePointDamage(AActor* DamagedActor, float InDamage, AController* InstigatedBy, FVector HitLocation,
		UPrimitiveComponent* HitComponent, FName BoneName, FVector ShotFromDirection, const UDamageType* DamageType, AActor* DamageCauser);

	bool CanSeeBasket(FVector& OutBasket) const;
	void SetState(ETowerCannonState NewState);
	void Fire(const FVector& Basket);
	void BlowUp();
	void TickWreck(float DeltaSeconds);
	/** Cannon hits the weapon behind this damage needs; 0 = another pellet of the same shotgun shot. */
	int32 CannonHitsFor(AActor* DamageCauser);
	void ApplyLayout();
	FVector DeckLocation() const;

	ETowerCannonState State = ETowerCannonState::Idle;
	float StateTime = 0.f;
	float Yaw = 0.f;
	float Pitch = 0.f;
	/** The team's model is in use (not the cubes). */
	bool bModel = false;
	/** Our TowerGun cannon and fuel tank stand on the team's tower (instead of its head / guns / boiler). */
	bool bOwnGun = false;
	/** Height of the gun deck, cm (the explosions run down from it). */
	float DeckHeight = 0.f;
	/** The model tower's vertical stretch (bDeckAtPlayerHeight). */
	float TowerZScale = 1.f;
	/** 1 = intact; each cannon hit takes 1 / (hits for that weapon). */
	float CannonHealth = 1.f;
	double LastShotgunHitTime = -1.0;

	/** Alternates the two muzzles of the model's twin guns. */
	bool bSecondMuzzle = false;

	// After the blast.
	float WreckTime = 0.f;
	NemesisFx::FStream WreckFire;
	NemesisFx::FStream WreckSmoke;
};
