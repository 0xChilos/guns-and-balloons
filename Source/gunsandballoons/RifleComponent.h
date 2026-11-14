#pragma once

#include "CoreMinimal.h"
#include "GunComponent.h"
#include "RifleComponent.generated.h"

class UAnimSequenceBase;

UENUM(BlueprintType)
enum class ERifleState : uint8
{
	Idle,
	Firing,
	/** Lever down: the block drops. A fired case stays in the chamber. */
	Opening,
	/** The extractor pulls the fired case out and throws it (on an RMB click). */
	Ejecting,
	/** Breech open, waiting for a click: RMB on ammo pulls the fired case out or loads a round; LMB closes. */
	Open,
	/** One round slides into the chamber. */
	Inserting,
	/** Lever up: the block rises and closes the breech. */
	Closing,
};

/**
 * Single-shot falling-block rifle (SK_MartiniHenry), driving UGunAnimInstance on its own mesh.
 *
 * Firing (Fire): the trigger releases the striker and the shot leaves; the chamber keeps a spent case
 * (chamber_spent) until the breech is opened. An empty or spent chamber just clicks.
 * Reloading has no key; every step is one RMB click on an ammo supply:
 * - Aiming at an ammo supply with no live round in the chamber opens the breech: Reload_Open plays up
 *   to ExtractStartTime (lever down, block dropped). A fired case stays in the chamber.
 * - While the chamber holds a fired case, RMB pulls it out (EjectCase: the rest of Reload_Open, the
 *   case on case_eject, then a physics casing). This takes nothing from the supply.
 * - With the chamber empty, RMB loads the round (UAmmoSupplyComponent -> LoadRound: Reload_Insert)
 *   and closes the breech (Reload_Close). So after a shot it is two clicks: case out, round in.
 * - The breech stays open, even after looking away, until a round is in or LMB
 *   (bCloseWhenAimLeavesAmmo turns on the revolver's close-on-look-away instead).
 *   LMB closes the breech without firing (a round already in hand is loaded first).
 * - A loaded chamber refuses the round.
 */
UCLASS(ClassGroup = (Weapon), meta = (BlueprintSpawnableComponent))
class GUNSANDBALLOONS_API URifleComponent : public UGunComponent
{
	GENERATED_BODY()

public:
	URifleComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Fire (or click on an empty/spent chamber). With the breech open, closes it instead. */
	virtual void PullTrigger() override;

	/** Take one round: opens the breech if needed, loads it and closes. False if a round is chambered or on its way. */
	virtual bool LoadRound() override;

	virtual bool CanLoadRound() const override;

	/** Pull the fired case out (opening the breech if needed). False if there is none, or it is already coming out. */
	virtual bool EjectCase() override;

	/** Aiming at an ammo supply opens the breech ahead of the click (if the chamber is not loaded). */
	virtual void SetAimingAtAmmo(bool bAiming) override;

	/** True from the lever going down until the breech is closed again. */
	virtual bool IsReloading() const override;

	virtual int32 GetLiveRounds() const override { return Chamber == EGunChamber::Live ? 1 : 0; }

	/** Stop waiting for ammo: a round in hand is still loaded, then the breech closes. */
	virtual void PrepareToHolster() override;

	virtual bool IsReadyToHolster() const override;

	UFUNCTION(BlueprintPure, Category = "Rifle")
	ERifleState GetState() const { return State; }

	UFUNCTION(BlueprintPure, Category = "Rifle")
	EGunChamber GetChamber() const { return Chamber; }

protected:
	virtual void BeginPlay() override;

	// --- Animations (SK_MartiniHenry_Skeleton) ---------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rifle|Animation")
	TObjectPtr<UAnimSequenceBase> IdleAnim;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rifle|Animation")
	TObjectPtr<UAnimSequenceBase> FireAnim;

	/**
	 * Lever down, block drops (up to ExtractStartTime), then the case is extracted (the rest). Opening
	 * plays the first part, an eject click the second; ExtractStartTime is held while the breech is open.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rifle|Animation")
	TObjectPtr<UAnimSequenceBase> OpenAnim;

	/** Slides one round into the open chamber. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rifle|Animation")
	TObjectPtr<UAnimSequenceBase> InsertAnim;

	/** Lever up, block rises. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rifle|Animation")
	TObjectPtr<UAnimSequenceBase> CloseAnim;

	/** Speed of the fire animation. Also sets the trigger-to-shot delay. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rifle|Animation", meta = (ClampMin = "0.1"))
	float FirePlayRate = 1.f;

	/** Speed of the open, insert and close clips, when ReloadTime is 0. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rifle|Animation", meta = (ClampMin = "0.1"))
	float ReloadPlayRate = 1.5f;

	/**
	 * Seconds of animation for a full reload after a shot: lever down, case out, round in, lever up (the
	 * clicks' own pace comes on top). Sets the play rate of every reload clip. 0 = use ReloadPlayRate.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rifle", meta = (ClampMin = "0.0", Units = "Seconds"))
	float ReloadTime = 0.f;

	/** Time in FireAnim (at play rate 1) when the trigger is fully back and the shot leaves. */
	UPROPERTY(EditAnywhere, Category = "Rifle|Animation|Timing", meta = (ClampMin = "0.0"))
	float ShotTime = 0.067f;

	/** Time in OpenAnim (at play rate 1) when the lever is fully down and the extractor starts pulling the case. */
	UPROPERTY(EditAnywhere, Category = "Rifle|Animation|Timing", meta = (ClampMin = "0.0"))
	float ExtractStartTime = 0.367f;

	/** Time in OpenAnim (at play rate 1) when the extracted case clears the receiver and becomes a physics casing. */
	UPROPERTY(EditAnywhere, Category = "Rifle|Animation|Timing", meta = (ClampMin = "0.0"))
	float CaseReleaseTime = 0.73f;

	/**
	 * If true, the breech closes once the aim has left the ammo supply for GateHoldGraceTime (the
	 * revolver's behaviour). If false it stays open until a round arrives or the trigger is pulled.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rifle")
	bool bCloseWhenAimLeavesAmmo = false;

	/** Seconds the breech stays open after the aim leaves the supply, when bCloseWhenAimLeavesAmmo is set. */
	UPROPERTY(EditAnywhere, Category = "Rifle", meta = (ClampMin = "0.0", EditCondition = "bCloseWhenAimLeavesAmmo"))
	float GateHoldGraceTime = 0.3f;

	/** Start with a live round in the chamber. Needs tuning: design value is TBD. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rifle")
	bool bStartLoaded = true;

private:
	void SetState(ERifleState NewState, UAnimSequenceBase* Anim, float Rate, float StartTime = 0.f);
	/** Play rate of the reload clips: from ReloadTime, or ReloadPlayRate. */
	float ReloadRate() const;
	/** Go idle, or reopen the breech straight away if a round was handed over meanwhile. */
	void EnterIdle();
	void StartOpening();
	/** After the breech has opened (or a click arrived while it was open): eject, load, wait, or close. */
	void ContinueReload();
	void Shoot();
	void UpdatePose();

	/** Keep the breech open for a round: the chamber can take one and nothing asked it to close. */
	bool ShouldStayOpen() const;
	/** The player is aiming at a supply now (or, with bCloseWhenAimLeavesAmmo, within the grace time). */
	bool IsAimingAtAmmo() const;

	EGunChamber Chamber = EGunChamber::Empty;

	/** A round handed over by LoadRound that is not in the chamber yet (includes the one sliding in). */
	bool bRoundQueued = false;

	/** A click (or a weapon swap) asked the open breech to close. */
	bool bCloseRequested = false;

	/** A click asked for the fired case to come out, and it has not started yet. */
	bool bEjectQueued = false;
	bool bCasingReleased = false;

	/** Last value passed to SetAimingAtAmmo, and seconds since it went false. */
	bool bAimRequested = false;
	float AimReleasedTime = TNumericLimits<float>::Max();

	ERifleState State = ERifleState::Idle;
	TObjectPtr<UAnimSequenceBase> StateAnim;
	float StateRate = 1.f;
	/** Time in StateAnim, at play rate 1. */
	float StateTime = 0.f;

	bool bShotDone = false;
};
