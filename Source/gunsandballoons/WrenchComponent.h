#pragma once

#include "CoreMinimal.h"
#include "GunComponent.h"
#include "WrenchComponent.generated.h"

class UAnimSequenceBase;

/** What the wrench itself is animating. */
UENUM(BlueprintType)
enum class EWrenchAction : uint8
{
	/** Coming up into view (Equip clip). */
	Equip,
	/** Held ready (Idle clip, looped). */
	Idle,
	/** Bringing the wrench onto the work (ToFix clip). */
	ToFix,
	/** Turning (Fix clip, looped) for as long as LMB is held. */
	Fix,
	/** LMB let go: the turn under way goes on to its rest pose (Fix clip). */
	FinishTurn,
	/** Back to the ready pose (ToFix played backwards). */
	FromFix,
	/** Going down out of view (Equip played backwards); holds its last frame until the swap. */
	Unequip,
};

/**
 * Wrench (SK_Wrench), a hand item like the extinguisher. A tool, not a weapon: no bullets, no recoil, no sights.
 *
 * - Taken from its place with LMB like every hand item; brings itself into view with Equip and takes itself out
 *   with Equip played backwards (AnimatesOwnSwap; the wrench has no Unequip clip).
 * - LMB held: ToFix brings it onto the work, then Fix (four ratchet turns) loops while held. Letting go finishes
 *   the turn under way (to the clip's next rest pose, found by sampling the clip), then ToFix plays backwards.
 * - What it repairs is not designed yet: it only animates. OnFixStarted / OnFixStopped mark the turning for
 *   the gameplay to hook into.
 *
 * SK_Wrench is a rigid mesh: its one bone (`wrench`) carries the whole wrench, and every clip keys it absolutely.
 */
UCLASS(ClassGroup = (Weapon), meta = (BlueprintSpawnableComponent))
class GUNSANDBALLOONS_API UWrenchComponent : public UGunComponent
{
	GENERATED_BODY()

public:
	UWrenchComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** LMB held: turn the wrench for as long as it stays down. */
	virtual void SetTriggerHeld(bool bHeld) override;

	virtual void SetEquipped(bool bInEquipped) override;
	virtual void PrepareToHolster() override;
	virtual void CancelHolster() override;

	/** Once it has gone down out of view (Equip backwards). */
	virtual bool IsReadyToHolster() const override;
	virtual bool AnimatesOwnSwap() const override { return true; }

	UFUNCTION(BlueprintPure, Category = "Wrench")
	EWrenchAction GetAction() const { return Action; }

	/** On the work and turning (ToFix done, LMB held). */
	UFUNCTION(BlueprintPure, Category = "Wrench")
	bool IsFixing() const { return Action == EWrenchAction::Fix; }

	/** Turning started (ToFix reached the work). */
	UPROPERTY(BlueprintAssignable, Category = "Wrench")
	FOnGunEvent OnFixStarted;

	/** Turning stopped (let go, or put away). */
	UPROPERTY(BlueprintAssignable, Category = "Wrench")
	FOnGunEvent OnFixStopped;

	/** Seconds into the Fix clip at which the turn under way at Time is back at rest (Time itself if it is). */
	float GetNextFixRestTime(float Time) const;

	/** Samples the Fix clip for its rest poses (BeginPlay does; GetNextFixRestTime needs it). */
	void FindFixRestTimes();

protected:
	virtual void BeginPlay() override;

	// --- Animations (SK_Wrench_Skeleton) ----------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wrench|Animation")
	TObjectPtr<UAnimSequenceBase> EquipAnim;

	/** Looped while held ready. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wrench|Animation")
	TObjectPtr<UAnimSequenceBase> IdleAnim;

	/** From the ready pose onto the work; played backwards to come off it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wrench|Animation")
	TObjectPtr<UAnimSequenceBase> ToFixAnim;

	/** Looped while LMB is held. Starts and ends at its rest pose, the end pose of ToFix. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wrench|Animation")
	TObjectPtr<UAnimSequenceBase> FixAnim;

	/** Bone that carries the whole wrench (every clip keys it). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wrench|Animation")
	FName WrenchBone = TEXT("wrench");

	/** How close (degrees, and cm) the Fix clip has to be to its first frame to count as at rest. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wrench|Animation", meta = (ClampMin = "0.0"))
	float FixRestTolerance = 0.1f;

private:
	void SetAction(EWrenchAction NewAction, UAnimSequenceBase* Anim, float Time = 0.f);
	bool IsActionDone() const;
	void UpdatePose();

	EWrenchAction Action = EWrenchAction::Idle;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequenceBase> ActionAnim;

	/** Time in ActionAnim (s). Counts down in FromFix and Unequip, which play their clips backwards. */
	float ActionTime = 0.f;

	/** In FinishTurn: the Fix time to stop at. */
	float FinishTime = 0.f;

	bool bTriggerHeld = false;
	bool bHolstering = false;

	/** Times in the Fix clip where it is at rest (its first frame's pose), in order; ends with its length. */
	TArray<float> FixRestTimes;
};
