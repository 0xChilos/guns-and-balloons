#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NemesisFx.h"
#include "NemesisPlane.generated.h"

class ANemesisAirship;
class UStaticMeshComponent;
class UMaterialInterface;
class UPointLightComponent;

/**
 * Base for the airship's planes (AI ворогів, sections 3–5). Placeholder cube body.
 * - Never changes altitude: Z is fixed at launch (except when falling after a downed engine).
 * - Holds a slot relative to the balloon (subclass picks it) with inertia.
 * - Never flies through the balloon, other enemies (airship, towers, beacons, AA guns) or another
 *   plane (2026-11-15): at an obstacle's height it slides around its bounds box, and planes push each
 *   other apart; a hard limit keeps them out even if steering lags.
 * - Two hit zones: Engine (incl. propeller) and Fuselage, each with HP. Damage per hit comes
 *   from the weapon (ApplyPointDamage amount). Engine destroyed → loses thrust and falls;
 *   fuselage destroyed → explodes.
 */
UCLASS(Abstract)
class GUNSANDBALLOONS_API ANemesisPlane : public AActor
{
	GENERATED_BODY()

public:
	ANemesisPlane();

	/** Called right after spawning. Z of the spawn location becomes the fixed altitude.
	 *  InAirship is null for planes that don't come from the airship (patrols). */
	virtual void Launch(ANemesisAirship* InAirship);

	/** Patrol: fly toward this point (XY, at the fixed altitude) instead of attacking. */
	void SetRouteTarget(const FVector& Point);
	/** Patrol over: back to the normal slot and attack. */
	void ClearRouteTarget();
	bool IsOnRoute() const { return bOnRoute; }

	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintPure, Category = "Plane")
	bool IsDown() const { return bDown; }

	/** 1 = intact, 0 = destroyed. Any hit on an engine zone destroys it. */
	UFUNCTION(BlueprintPure, Category = "Plane")
	float GetEngineHealth() const { return EngineHealth; }

	/** 1 = intact, 0 = destroyed. Each fuselage hit takes 1 / (hits for that weapon). */
	UFUNCTION(BlueprintPure, Category = "Plane")
	float GetFuselageHealth() const { return FuselageHealth; }

	/** bEngine: engine destroyed (falls trailing black smoke); false: fuselage destroyed (falls on fire). */
	UFUNCTION(BlueprintImplementableEvent, Category = "Plane")
	void OnPlaneDowned(bool bEngine);

protected:
	/** Where the plane wants to be now (world; Z is ignored). */
	virtual FVector GetSlotLocation() const { return GetActorLocation(); }

	/** Attack logic; only called while the plane is flying and not leaving. */
	virtual void TickAttack(float DeltaSeconds) {}

	/** Fly away forward and disappear. */
	void Leave();

	bool IsInSlot(float Tolerance) const;
	FVector GetBalloonForward() const;
	ANemesisAirship* GetAirship() const { return Airship.Get(); }

	UStaticMeshComponent* MakeCube(const TCHAR* Name, const FVector& Location, const FVector& SizeM, UMaterialInterface* Material);

	UPROPERTY(VisibleAnywhere, Category = "Plane")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "Plane")
	TObjectPtr<UStaticMeshComponent> Fuselage;

	/** Flickering orange light while the plane burns. */
	UPROPERTY(VisibleAnywhere, Category = "Plane")
	TObjectPtr<UPointLightComponent> FireLight;

	UPROPERTY(VisibleAnywhere, Category = "Plane")
	TObjectPtr<UStaticMeshComponent> Wings;

	/** Engine + propeller hit zone. */
	UPROPERTY(VisibleAnywhere, Category = "Plane")
	TObjectPtr<UStaticMeshComponent> Engine;

	/** Propellers (spin around their local X; they count as part of the engine, design). */
	UPROPERTY(VisibleAnywhere, Category = "Plane")
	TArray<TObjectPtr<UStaticMeshComponent>> Props;

	UPROPERTY(EditAnywhere, Category = "Plane")
	float PropSpinSpeed = 1440.f;

	/** Model part on Root, shootable, with its pivot at Location (cm). Null mesh = no component. */
	UStaticMeshComponent* MakePart(const TCHAR* Name, UStaticMesh* Mesh, const FVector& Location);

	/** Every component that counts as the engine (Engine + e.g. the bomber's wing engines). */
	UPROPERTY(VisibleAnywhere, Category = "Plane")
	TArray<TObjectPtr<UStaticMeshComponent>> EngineZones;

	// Planes count hits, not damage (design 2026-11-14): one hit on an engine from any weapon
	// downs the plane; the fuselage needs a number of hits that depends on the weapon. One shotgun
	// shot is one hit however many pellets land.

	UPROPERTY(EditAnywhere, Category = "Plane|Damage", meta = (ClampMin = "1"))
	int32 RevolverFuselageHits = 4;

	UPROPERTY(EditAnywhere, Category = "Plane|Damage", meta = (ClampMin = "1"))
	int32 ShotgunFuselageHits = 3;

	UPROPERTY(EditAnywhere, Category = "Plane|Damage", meta = (ClampMin = "1"))
	int32 RifleFuselageHits = 2;

	/** Anything else that deals point damage (not one of the player's guns). TBD */
	UPROPERTY(EditAnywhere, Category = "Plane|Damage", meta = (ClampMin = "1"))
	int32 OtherFuselageHits = 3;

	/** Pellets of one shotgun shot landing within this time count as a single hit, s. */
	UPROPERTY(EditAnywhere, Category = "Plane|Damage", meta = (ClampMin = "0"))
	float ShotgunPelletWindow = 0.15f;

	/** cm/s. TBD */
	UPROPERTY(EditAnywhere, Category = "Plane|Flight", meta = (ClampMin = "0"))
	float MaxSpeed = 2500.f;

	/** cm/s². Inertia when matching the slot. TBD */
	UPROPERTY(EditAnywhere, Category = "Plane|Flight", meta = (ClampMin = "0"))
	float Acceleration = 600.f;

	/** How hard it pulls toward the slot: extra cm/s per cm of error. TBD */
	UPROPERTY(EditAnywhere, Category = "Plane|Flight", meta = (ClampMin = "0"))
	float SlotGain = 0.5f;

	/** Gap kept between the plane and an obstacle's bounds (the balloon, the airship, towers,
	 *  beacons, AA guns), cm. TBD */
	UPROPERTY(EditAnywhere, Category = "Plane|Avoidance", meta = (ClampMin = "0"))
	float ObstacleClearance = 400.f;

	/** Starts steering around an obstacle this far before the clearance, cm. TBD */
	UPROPERTY(EditAnywhere, Category = "Plane|Avoidance", meta = (ClampMin = "0"))
	float ObstacleAvoidDistance = 2000.f;

	/** Gap kept between two planes (their bounds), cm. TBD */
	UPROPERTY(EditAnywhere, Category = "Plane|Avoidance", meta = (ClampMin = "0"))
	float PlaneClearance = 400.f;

	/** Push away from an obstacle / another plane: cm/s per cm inside the zone. TBD */
	UPROPERTY(EditAnywhere, Category = "Plane|Avoidance", meta = (ClampMin = "0"))
	float AvoidGain = 1.5f;

	/** Seconds a plane with a dead engine falls before it is removed. */
	UPROPERTY(EditAnywhere, Category = "Plane|Damage", meta = (ClampMin = "0"))
	float FallTime = 6.f;

	/** Speed along a patrol route, cm/s. TBD */
	UPROPERTY(EditAnywhere, Category = "Plane|Flight", meta = (ClampMin = "0"))
	float PatrolSpeed = 1500.f;

	/** Seconds a leaving plane flies away before it is removed. */
	UPROPERTY(EditAnywhere, Category = "Plane|Flight", meta = (ClampMin = "0"))
	float LeaveTime = 10.f;

	FVector Velocity = FVector::ZeroVector;
	float FixedZ = 0.f;
	/** The plane's own size for avoidance (from its bounds at launch): XY radius and half height, cm. */
	float BodyRadius = 500.f;
	float BodyHalfHeight = 150.f;
	/** False while a plane that just left the airship's hangar is still inside the airship. */
	bool bClearOfAirship = true;

private:
	UFUNCTION()
	void HandlePointDamage(AActor* DamagedActor, float Damage, AController* InstigatedBy, FVector HitLocation,
		UPrimitiveComponent* HitComponent, FName BoneName, FVector ShotFromDirection, const UDamageType* DamageType, AActor* DamageCauser);

	void Fly(float DeltaSeconds);
	/** Steers DesiredVel around the balloon and away from other planes. */
	FVector Avoid(const FVector& Pos, FVector DesiredVel) const;
	/** Hard limit: moves NewPos out of the balloon and other planes. */
	FVector KeepClear(FVector NewPos) const;
	/** Bounds boxes (centre, half size) of everything planes keep out of: the balloon and the
	 *  airship, towers, beacons and AA guns. Pairs of (centre, extent). */
	void GetObstacles(TArray<TPair<FVector, FVector>>& Out) const;
	void Fall(float DeltaSeconds);
	void GoDown(bool bEngine);
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

	NemesisFx::FStream SmokeStream;
	NemesisFx::FStream FireStream;
	/** Fuselage hits the weapon behind this damage needs; 0 = ignore (extra pellet of one shot). */
	int32 FuselageHitsFor(AActor* DamageCauser);

	TWeakObjectPtr<ANemesisAirship> Airship;
	float EngineHealth = 1.f;
	float FuselageHealth = 1.f;
	double LastShotgunHitTime = -1.0;
	bool bDown = false;
	bool bOnRoute = false;
	FVector RouteTarget = FVector::ZeroVector;
	bool bLeaving = false;
	float StateTime = 0.f;
};
