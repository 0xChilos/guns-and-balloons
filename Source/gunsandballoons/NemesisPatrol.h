#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NemesisPatrol.generated.h"

class ANemesisPlane;
class USplineComponent;

/**
 * Lava-biome patrol (AI ворогів, section 6). A group of planes (bombers or crop dusters) flies a
 * closed route (spline) at the route's altitude. When the balloon comes within AggroDistance of a
 * plane, the group attacks by its type's normal rules (if fewer than the max patrol groups are
 * attacking). When the balloon is DisengageDistance away from the route, the group returns to it.
 * Planes keep their altitude (they never move in Z). Shot-down planes are not replaced.
 */
UCLASS()
class GUNSANDBALLOONS_API ANemesisPatrol : public AActor
{
	GENERATED_BODY()

public:
	ANemesisPatrol();

	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintPure, Category = "Patrol")
	bool IsEngaged() const { return bEngaged; }

	UFUNCTION(BlueprintImplementableEvent, Category = "Patrol")
	void OnEngagedChanged(bool bNowEngaged);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

	UPROPERTY(VisibleAnywhere, Category = "Patrol")
	TObjectPtr<USceneComponent> Root;

	/** Closed route; edit its points in the level. The planes fly at the height of each point. */
	UPROPERTY(VisibleAnywhere, Category = "Patrol")
	TObjectPtr<USplineComponent> Route;

	/** ANemesisBomber or ANemesisCropDuster. */
	UPROPERTY(EditAnywhere, Category = "Patrol")
	TSubclassOf<ANemesisPlane> PlaneClass;

	/** Team decision 2026-11-15: one crop duster per patrol. */
	UPROPERTY(EditAnywhere, Category = "Patrol", meta = (ClampMin = "1", ClampMax = "4"))
	int32 GroupSize = 1;

	/** Spacing between planes along the route, cm. */
	UPROPERTY(EditAnywhere, Category = "Patrol", meta = (ClampMin = "0"))
	float Spacing = 2500.f;

	/** cm/s along the route. TBD */
	UPROPERTY(EditAnywhere, Category = "Patrol", meta = (ClampMin = "1"))
	float RouteSpeed = 1500.f;

	/** Balloon this close to any plane: attack, cm (200 m). */
	UPROPERTY(EditAnywhere, Category = "Patrol", meta = (ClampMin = "0"))
	float AggroDistance = 20000.f;

	/** Balloon this far from the route: back to patrolling, cm (400 m). */
	UPROPERTY(EditAnywhere, Category = "Patrol", meta = (ClampMin = "0"))
	float DisengageDistance = 40000.f;

	/** Two patrol bombers fly side by side: ± this from the balloon's course, cm. */
	UPROPERTY(EditAnywhere, Category = "Patrol", meta = (ClampMin = "0"))
	float BomberSideSpacing = 1200.f;

private:
	void SpawnGroup();
	void SetEngaged(bool bNowEngaged);

	TArray<TWeakObjectPtr<ANemesisPlane>> Planes;
	float RouteDistance = 0.f;
	bool bEngaged = false;
};
