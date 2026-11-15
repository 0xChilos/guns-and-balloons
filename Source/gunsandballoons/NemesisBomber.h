#pragma once

#include "CoreMinimal.h"
#include "NemesisPlane.h"
#include "NemesisBomber.generated.h"

class ANemesisMine;

/**
 * Bomber-miner (AI ворогів, section 3). One at a time. Overtakes the balloon, holds a slot
 * AheadDistance in front of it on its course, never turns back, and drops a parachute mine
 * behind its tail every 5–7 s (random per drop). Only drops mines, never shoots.
 */
UCLASS()
class GUNSANDBALLOONS_API ANemesisBomber : public ANemesisPlane
{
	GENERATED_BODY()

public:
	ANemesisBomber();

	virtual void Launch(ANemesisAirship* InAirship) override;

	/** Sideways shift of the slot from the balloon's course, cm (e.g. two patrol bombers side by side). */
	void SetSlotSideOffset(float Offset) { SlotSideOffset = Offset; }

	/** Altitude above the balloon at launch, cm (mines fall onto its path). */
	UPROPERTY(EditAnywhere, Category = "Bomber", meta = (ClampMin = "0"))
	float HeightAboveBalloon = 1200.f;

protected:
	virtual FVector GetSlotLocation() const override;
	virtual void TickAttack(float DeltaSeconds) override;

	/** TBD: 150 m in the AI doc was too far for the prototype balloon speeds. */
	UPROPERTY(EditAnywhere, Category = "Bomber", meta = (ClampMin = "0"))
	float AheadDistance = 4000.f;

	/** Playtest 2026-11-14: 5–7 s too frequent, 10–13 s too rare. TBD */
	UPROPERTY(EditAnywhere, Category = "Bomber", meta = (ClampMin = "0"))
	float DropIntervalMin = 7.f;

	UPROPERTY(EditAnywhere, Category = "Bomber", meta = (ClampMin = "0"))
	float DropIntervalMax = 10.f;

	/**
	 * The bomber holds its slot; mines are released from the left wing, the belly or the right wing
	 * (a different point each time), LaneOffset to the side of the balloon's course. cm. TBD
	 */
	UPROPERTY(EditAnywhere, Category = "Bomber", meta = (ClampMin = "0"))
	float LaneOffset = 500.f;

	/** Drops only when this close to its slot, cm. */
	UPROPERTY(EditAnywhere, Category = "Bomber", meta = (ClampMin = "0"))
	float SlotTolerance = 2500.f;

	/** Design (2026-11-14): the bomber never leaves; it holds its slot until shot down (0).
	 *  A positive value makes it fly away after that many seconds (testing only). */
	UPROPERTY(EditAnywhere, Category = "Bomber", meta = (ClampMin = "0"))
	float LifeTime = 0.f;

	UPROPERTY(EditAnywhere, Category = "Bomber")
	TSubclassOf<ANemesisMine> MineClass;

	UFUNCTION(BlueprintImplementableEvent, Category = "Bomber")
	void OnMineDropped(ANemesisMine* Mine);

private:
	void PickNextLane();

	float SlotSideOffset = 0.f;
	int32 Lane = 0; // release point: −1 left wing, 0 belly, +1 right wing
	float DropTimer = 0.f;
	float Age = 0.f;
};
