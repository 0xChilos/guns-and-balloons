#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NemesisBirdFlock.generated.h"

class UAnimSequence;
class UPrimitiveComponent;
class USkeletalMesh;
class UStaticMeshComponent;

/**
 * Field-biome bird flock (AI ворогів, 7.3). Place it along the route: when the balloon comes within
 * ActivationDistance (and no other flock is active), 8–15 birds appear to one side and cross the
 * balloon's path with boids flocking, timed to meet it. Each bird that reaches the basket deals
 * DamagePerBird (at most MaxDamage per flock) and is gone. Shooting a bird kills it; a hit or a
 * shot that passes near the flock scares it for good: the birds scatter away from the shot and the
 * balloon, climb, and never strike (team decision 2026-11-15). When the birds are past, scared off
 * or dead the flock removes itself. Birds are the team's SK_Bird (Content/Assets/Locations/Field)
 * flapping its wings (A_Bird_Flap), 2026-11-15; placeholder cubes if the model is missing.
 */
UCLASS()
class GUNSANDBALLOONS_API ANemesisBirdFlock : public AActor
{
	GENERATED_BODY()

public:
	ANemesisBirdFlock();

	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintImplementableEvent, Category = "Birds")
	void OnFlockStarted(int32 NumBirds);

	UFUNCTION(BlueprintImplementableEvent, Category = "Birds")
	void OnScattered();

	UFUNCTION(BlueprintImplementableEvent, Category = "Birds")
	void OnBirdHitBalloon(float Damage);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

	UPROPERTY(VisibleAnywhere, Category = "Birds")
	TObjectPtr<USceneComponent> Root;

	/** The bird model (faces −Y), flapping BirdFlapAnim in a loop. */
	UPROPERTY(EditAnywhere, Category = "Birds")
	TObjectPtr<USkeletalMesh> BirdModel;

	UPROPERTY(EditAnywhere, Category = "Birds")
	TObjectPtr<UAnimSequence> BirdFlapAnim;

	/** Wingspan of a bird, cm: the model (~1 m) is scaled to it. TBD */
	UPROPERTY(EditAnywhere, Category = "Birds", meta = (ClampMin = "1"))
	float BirdWingspan = 150.f;

	/** Placeholder when BirdModel is missing. */
	UPROPERTY(EditAnywhere, Category = "Birds")
	TObjectPtr<UStaticMesh> BirdMesh;

	UPROPERTY(EditAnywhere, Category = "Birds")
	TObjectPtr<UMaterialInterface> BirdMaterial;

	/** Balloon this close to the flock actor starts the crossing, cm. */
	UPROPERTY(EditAnywhere, Category = "Birds", meta = (ClampMin = "0"))
	float ActivationDistance = 15000.f;

	UPROPERTY(EditAnywhere, Category = "Birds", meta = (ClampMin = "1"))
	int32 MinBirds = 8;

	UPROPERTY(EditAnywhere, Category = "Birds", meta = (ClampMin = "1"))
	int32 MaxBirds = 15;

	/** cm/s (8 m/s). */
	UPROPERTY(EditAnywhere, Category = "Birds", meta = (ClampMin = "1"))
	float BirdSpeed = 800.f;

	/** The flock starts this far to the side of the balloon's course, cm. */
	UPROPERTY(EditAnywhere, Category = "Birds", meta = (ClampMin = "0"))
	float StartSideDistance = 6000.f;

	UPROPERTY(EditAnywhere, Category = "Birds", meta = (ClampMin = "0"))
	float DamagePerBird = 3.f;

	UPROPERTY(EditAnywhere, Category = "Birds", meta = (ClampMin = "0"))
	float MaxDamage = 15.f;

	/** The flock flies this high above the basket centre — a little above the player's head
	 *  (eye ≈ 1 m above it). Birds strike the balloon around this point. cm. TBD */
	UPROPERTY(EditAnywhere, Category = "Birds")
	float FlightHeight = 180.f;

	/** A bird this close to the strike point hits the balloon, cm. */
	UPROPERTY(EditAnywhere, Category = "Birds", meta = (ClampMin = "0"))
	float BirdHitRadius = 200.f;

	/** A player's shot passing this close to the flock centre scatters it, cm. */
	UPROPERTY(EditAnywhere, Category = "Birds", meta = (ClampMin = "0"))
	float ScareRadius = 1000.f;


	/** Flock removed once its birds are this far from the balloon after crossing, cm. */
	UPROPERTY(EditAnywhere, Category = "Birds", meta = (ClampMin = "0"))
	float DespawnDistance = 25000.f;

	// Boids weights.
	UPROPERTY(EditAnywhere, Category = "Birds|Boids") float SeparationRadius = 150.f;
	UPROPERTY(EditAnywhere, Category = "Birds|Boids") float SeparationWeight = 3.f;
	UPROPERTY(EditAnywhere, Category = "Birds|Boids") float CohesionWeight = 0.6f;
	UPROPERTY(EditAnywhere, Category = "Birds|Boids") float AlignmentWeight = 0.8f;
	UPROPERTY(EditAnywhere, Category = "Birds|Boids") float GoalWeight = 1.2f;
	UPROPERTY(EditAnywhere, Category = "Birds|Boids") float TurnRate = 4.f;

private:
	struct FBird
	{
		TObjectPtr<UPrimitiveComponent> Mesh;
		FVector Velocity = FVector::ZeroVector;
		bool bAlive = true;
		float FallTime = 0.f;
	};

	UFUNCTION()
	void HandlePointDamage(AActor* DamagedActor, float InDamage, AController* InstigatedBy, FVector HitLocation,
		UPrimitiveComponent* HitComponent, FName BoneName, FVector ShotFromDirection, const UDamageType* DamageType, AActor* DamageCauser);

	UFUNCTION()
	void HandlePlayerFired();

	void StartCrossing();
	void TickBirds(float DeltaSeconds);
	void Scatter(const FVector& From);
	void BindToPlayerGuns(bool bBind);

	TArray<FBird> Birds;
	FVector Goal = FVector::ZeroVector;
	FVector ScareFrom = FVector::ZeroVector;
	/** Scared off for good: fleeing, no more strikes. */
	bool bScared = false;
	float DamageDealt = 0.f;
	bool bActive = false;
	bool bCrossed = false;
};
