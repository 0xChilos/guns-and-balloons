#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NemesisAirship.h"
#include "NemesisWaveDirector.generated.h"

UENUM(BlueprintType)
enum class ENemesisWaveKind : uint8
{
	Message,        // just show Label on screen
	BirdFlock,
	Beacon,
	TowerCannon,
	Flak,
	Patrol,
	AirshipBand,    // force the airship's attack to Band
	AirshipByDistance, // back to picking the attack by distance
};

USTRUCT(BlueprintType)
struct FNemesisWaveEvent
{
	GENERATED_BODY()

	/** Seconds after the level starts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave")
	float Time = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave")
	ENemesisWaveKind Kind = ENemesisWaveKind::Message;

	/** Where to put the enemy, relative to the balloon at that moment: ahead along its course, cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave")
	float Ahead = 10000.f;

	/** ... and to the side, cm (+ = right, − = left). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave")
	float Side = 0.f;

	/** For AirshipBand. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave")
	ENemesisBand Band = ENemesisBand::CropDusters;

	/** Shown on screen when the event fires. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave")
	FString Label;
};

/**
 * Enemy test level director: runs a timeline of events. Each one spawns an enemy relative to the
 * balloon (ahead along its course + to the side; towers and beacons stand on the ground under that
 * point, the AA gun and patrols at the balloon's altitude, facing the balloon) or switches the nemesis
 * airship's attack. Spawns the airship at BeginPlay if the level has none. The default timeline shows
 * every enemy once in about a minute and a half; edit Events in the level to change it.
 */
UCLASS()
class GUNSANDBALLOONS_API ANemesisWaveDirector : public AActor
{
	GENERATED_BODY()

public:
	ANemesisWaveDirector();

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere, Category = "Waves")
	TArray<FNemesisWaveEvent> Events;

	/** Start the timeline again after the last event. */
	UPROPERTY(EditAnywhere, Category = "Waves")
	bool bLoop = false;

	/** Testing: hits are logged but don't hurt the balloon, so every wave can be seen. */
	UPROPERTY(EditAnywhere, Category = "Waves")
	bool bInvulnerableBalloon = true;

	/** The airship's start distance behind the balloon, cm. */
	UPROPERTY(EditAnywhere, Category = "Waves")
	float AirshipStartDistance = 25000.f;

	/** No ground under a spawn point: put towers this far below the basket, cm. */
	UPROPERTY(EditAnywhere, Category = "Waves", meta = (ClampMin = "0"))
	float FallbackGroundDepth = 5000.f;

	UPROPERTY(EditAnywhere, Category = "Waves")
	TSubclassOf<ANemesisAirship> AirshipClass;

private:
	void Fire(const FNemesisWaveEvent& Event);
	AActor* SpawnEnemy(UClass* Class, const FNemesisWaveEvent& Event, bool bOnGround);
	void Say(const FString& Text) const;

	float Elapsed = 0.f;
	int32 NextEvent = 0;
};
