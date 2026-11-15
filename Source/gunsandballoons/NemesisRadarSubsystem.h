#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "NemesisRadarSubsystem.generated.h"

/**
 * Puts every enemy on the balloon's radar (docs/features/ui-menus.md, Radar).
 *
 * The radar itself is the team's: BP_Balloon reads every actor with a BPC_RadarBlip component
 * (/Game/Assets/Balloon) into MPC_Radar, which M_Display draws; BP_JammerTower writes RadarJam into
 * the same collection. This subsystem only makes sure the enemies have that component: when a game
 * world begins play, and whenever an actor spawns, an enemy without one gets a BPC_RadarBlip with the
 * blip kind and warning-lamp radii for its type (see RadarBlips in the .cpp). The C++ enemies
 * (ANemesis*) and the team's Blueprint enemies the level generator places (BP_TurretTower,
 * BP_Lighthouse, BP_BirdFlock, BP_RocketBalloon) are covered. No Blueprint is edited.
 * An enemy leaves the radar as soon as it stops being a threat (RemoveFromRadar), not when its actor
 * goes: a shot-down plane still falls for seconds, a blown-up tower keeps standing.
 */
UCLASS()
class GUNSANDBALLOONS_API UNemesisRadarSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	/** Gives Actor a radar blip if it is an enemy without one. Returns true if it added one. */
	bool RegisterEnemy(AActor* Actor);

	/** Takes Actor off the radar now (a plane shot down, a tower or beacon destroyed, a flock scared off). */
	static void RemoveFromRadar(AActor* Actor);

private:
	void HandleActorSpawned(AActor* Actor);

	UPROPERTY(Transient)
	TObjectPtr<UClass> BlipClass;

	FDelegateHandle SpawnHandle;
};
