#pragma once

#include "CoreMinimal.h"
#include "GunComponent.h"
#include "GunSmoke.h"
#include "ExtinguisherComponent.generated.h"

class UAnimSequenceBase;

/** What the extinguisher itself is animating. */
UENUM(BlueprintType)
enum class EExtinguisherAction : uint8
{
	/** Coming up into view (Equip clip). */
	Equip,
	Idle,
	/** The lever going down (Spray_Start clip); the foam is already flowing. */
	SprayStart,
	/** Spraying (Spray_Loop clip, looped) for as long as LMB is held. */
	Spray,
	/** The lever coming back up (Spray_Stop clip). */
	SprayStop,
	/** Pressing the lever with nothing left: a dry sputter (Empty clip). */
	Empty,
	/** Going down out of view (Unequip clip); holds its last frame until the swap. */
	Unequip,
};

/**
 * Fire extinguisher (SK_Extinguisher), key 5. A tool, not a weapon: no bullets, no recoil.
 *
 * - Its contents (Amount, 0..100) show on the mechanical counter on the tank: three number drums
 *   (digit_100, digit_10, digit_1) that roll like an odometer. It comes full (100).
 * - LMB held sprays foam out of the nozzle (Spray_Start, then Spray_Loop while held, Spray_Stop on release),
 *   using UsePerSecond (5) of the amount every second, so a full tank lasts 20 s. The foam is a heavy white
 *   jet that falls in an arc and stays where it lands for a while (AExtinguisherFoam, PS1-style sprites).
 * - Running out while spraying, or pressing LMB when empty, plays the Empty clip (a dry sputter); no foam.
 * - It brings itself into view and takes itself out with its own Equip/Unequip clips (AnimatesOwnSwap).
 *
 * The counter drums are posed from Counter_Ref_000 and turned DigitStep (36) degrees per digit about their own
 * +Y; Counter_Ref_073 and _100 are the artist's references for that (checked by the automation test).
 */
UCLASS(ClassGroup = (Weapon), meta = (BlueprintSpawnableComponent))
class GUNSANDBALLOONS_API UExtinguisherComponent : public UGunComponent
{
	GENERATED_BODY()

public:
	UExtinguisherComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** The click: with nothing left, a dry sputter (Empty clip, OnDryFire). Spraying itself follows SetTriggerHeld. */
	virtual void PullTrigger() override;
	virtual bool ShouldUseInsteadOfInteract(const FHitResult& Hit) const override;

	/** LMB held: spray while there is something left. */
	virtual void SetTriggerHeld(bool bHeld) override;

	virtual void SetEquipped(bool bInEquipped) override;
	virtual void PrepareToHolster() override;
	virtual void CancelHolster() override;

	/** Once the Unequip clip has taken it out of view. */
	virtual bool IsReadyToHolster() const override;
	virtual bool AnimatesOwnSwap() const override { return true; }

	/** 1 while there is something left to spray. */
	virtual int32 GetLiveRounds() const override { return HasAmount() ? 1 : 0; }

	UFUNCTION(BlueprintPure, Category = "Extinguisher")
	float GetAmount() const { return Amount; }

	UFUNCTION(BlueprintPure, Category = "Extinguisher")
	bool HasAmount() const { return Amount > 0.f; }

	/** Set the contents directly (0..100), e.g. a refill. The counter rolls to it. */
	UFUNCTION(BlueprintCallable, Category = "Extinguisher")
	void SetAmount(float NewAmount);

	UFUNCTION(BlueprintPure, Category = "Extinguisher")
	EExtinguisherAction GetAction() const { return Action; }

	/** Foam is coming out right now. */
	UFUNCTION(BlueprintPure, Category = "Extinguisher")
	bool IsSpraying() const;

	/** TBD: prototype cooling rate and reach; tune in Blueprint. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Extinguisher|Cooling", meta=(ClampMin="0"))
	float StoveCoolingPerSecond = 20.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Extinguisher|Cooling", meta=(ClampMin="0", Units="cm"))
	float StoveCoolingRange = 400.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Extinguisher|Cooling")
	TEnumAsByte<ECollisionChannel> CoolingTraceChannel = ECC_Visibility;

	/** Uses the first blocking hit along the nozzle; visual puff counts do not affect cooling. */
	void ApplySprayCooling(const FVector& Tip, const FVector& Direction, float DeltaTime);

	/** The value the counter shows (rolls toward the whole number the amount is at). */
	UFUNCTION(BlueprintPure, Category = "Extinguisher")
	float GetDisplayedCount() const { return DisplayedCount; }

	/**
	 * Where each drum of a mechanical counter stands for Value (X = ones, Y = tens, Z = hundreds), in digits:
	 * the ones drum follows Value, and a higher drum rolls on to its next digit only while the drums below it
	 * pass from 9 to 0. 73 -> (3, 7, 0); 99.5 -> (9.5, 9.5, 0.5).
	 */
	static FVector GetDrumDigits(float Value);

	/** The spray started (foam is coming out). */
	UPROPERTY(BlueprintAssignable, Category = "Extinguisher")
	FOnGunEvent OnSprayStarted;

	/** The spray stopped (released, ran out or put away). */
	UPROPERTY(BlueprintAssignable, Category = "Extinguisher")
	FOnGunEvent OnSprayStopped;

	/** The contents reached 0 while spraying. */
	UPROPERTY(BlueprintAssignable, Category = "Extinguisher")
	FOnGunEvent OnEmptied;

protected:
	virtual void BeginPlay() override;

	// --- Contents -------------------------------------------------------------------------

	/** Contents when the game starts: the counter shows this on equip. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Extinguisher|Contents", meta = (ClampMin = "0.0", ClampMax = "100.0"))
	float InitialAmount = 100.f;

	/** Contents used per second of spraying. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Extinguisher|Contents", meta = (ClampMin = "0.0"))
	float UsePerSecond = 5.f;

	// --- Foam -----------------------------------------------------------------------------

	/**
	 * The foam jet out of the nozzle tip, along the nozzle, while spraying. Amount = puffs per second (a steady
	 * stream, Duration and Delay unused). First-pass values; tune together with AExtinguisherFoam.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Extinguisher|Foam")
	FGunSmokePuffs FoamSpray;

	/**
	 * The cloud billowing around the jet: wider, slower, lighter puffs that spread out big and thin away
	 * instead of landing. Amount = puffs per second while spraying. 0 = off.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Extinguisher|Foam")
	FGunSmokePuffs FoamMist;

	/** Bone the foam leaves from, and the bone behind it that gives the nozzle's direction. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Extinguisher|Foam")
	FName NozzleTipBone = TEXT("nozzle_tip");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Extinguisher|Foam")
	FName NozzleBone = TEXT("nozzle");

	// --- Animations (SK_Extinguisher_Skeleton) --------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Extinguisher|Animation")
	TObjectPtr<UAnimSequenceBase> EquipAnim;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Extinguisher|Animation")
	TObjectPtr<UAnimSequenceBase> UnequipAnim;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Extinguisher|Animation")
	TObjectPtr<UAnimSequenceBase> IdleAnim;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Extinguisher|Animation")
	TObjectPtr<UAnimSequenceBase> SprayStartAnim;

	/** Looped while spraying. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Extinguisher|Animation")
	TObjectPtr<UAnimSequenceBase> SprayLoopAnim;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Extinguisher|Animation")
	TObjectPtr<UAnimSequenceBase> SprayStopAnim;

	/** The dry sputter with nothing left. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Extinguisher|Animation")
	TObjectPtr<UAnimSequenceBase> EmptyAnim;

	/** All three counter drums at 0 (Counter_Ref_000). Each turns DigitStep degrees per digit from here. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Extinguisher|Animation")
	TObjectPtr<UAnimSequenceBase> CounterZeroAnim;

	/** Degrees a counter drum turns (about its own +Y) from one digit to the next. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Extinguisher|Animation")
	float DigitStep = 36.f;

	/** How fast the counter rolls to a new number (counts per second). Higher = snappier ticks. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Extinguisher|Animation", meta = (ClampMin = "1.0"))
	float CounterRollSpeed = 12.f;

private:
	void SetAction(EExtinguisherAction NewAction, UAnimSequenceBase* Anim);
	bool IsActionDone() const;
	void UpdateFlow(float DeltaTime);
	void EmitFoam(float DeltaTime);
	void UpdatePose();

	/** The whole number the counter settles on for the current amount (a part-used unit still counts). */
	float GetCounterTarget() const;

	EExtinguisherAction Action = EExtinguisherAction::Idle;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequenceBase> ActionAnim;

	/** Time in ActionAnim (s). */
	float ActionTime = 0.f;

	float Amount = 100.f;
	float DisplayedCount = 100.f;

	bool bTriggerHeld = false;
	bool bHolstering = false;
	bool bWasSpraying = false;

	/** Puffs owed but not emitted yet (fractions carry over): jet, mist. */
	float FoamOwed[2] = { 0.f, 0.f };
	FVector LastTipLocation = FVector::ZeroVector;
	bool bHasLastTip = false;
};
