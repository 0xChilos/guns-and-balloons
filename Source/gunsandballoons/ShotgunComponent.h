#pragma once

#include "CoreMinimal.h"
#include "GunComponent.h"
#include "ShotgunComponent.generated.h"

class UAnimSequenceBase;

UENUM(BlueprintType)
enum class EShotgunState : uint8
{
	Idle,
	/** One trigger is pulled: that barrel's hammer falls, or, on an empty/spent barrel, the trigger just clicks. */
	Firing,
	/** The barrels drop open on the hinge. */
	Opening,
	/** A fired case is pulled out of one barrel and thrown clear. */
	Ejecting,
	/** Open, waiting for a click: RMB on ammo pulls a fired case out or loads a shell; LMB closes. Looking away keeps it open. */
	Open,
	/** One shell slides into a barrel, then that barrel's hammer is cocked. */
	Inserting,
	/** The barrels swing shut. */
	Closing,
};

/** The front trigger fires the right barrel, so the right barrel goes first everywhere. */
UENUM(BlueprintType)
enum class EShotgunBarrel : uint8
{
	Right,
	Left,
};

/**
 * Double-barrel break-action shotgun with external hammers (SK_Homestead78), driving UGunAnimInstance
 * on its own mesh.
 *
 * Firing (PullTrigger): fires the right barrel if it is live, otherwise the left (Fire_Right / Fire_Left:
 * the hammer falls and the shot leaves). The barrel keeps a spent shell (shell_R/L_spent) until it is
 * opened. With no live barrel the front trigger just clicks (Trigger_Dry_Right).
 * Reloading has no key; every step is one RMB click on an ammo supply:
 * - Aiming at an ammo supply with a barrel that is not live breaks the gun open (Reload_Open). Fired
 *   cases stay in the barrels.
 * - While a barrel holds a fired case, RMB pulls it out and throws it (EjectCase: Reload_EjectRight/Left
 *   on case_eject_R/L, then a physics casing). This takes nothing from the supply.
 * - With no fired case left, RMB loads one shell into the first empty barrel (UAmmoSupplyComponent ->
 *   LoadRound: Reload_InsertRight/Left, which also cocks that barrel's hammer). Once both barrels are
 *   live the gun closes (Reload_Close). So two fired barrels take four clicks: eject, eject, load, load.
 * - Clicks during a step queue the next one. The gun stays open, even after looking away, until it is
 *   full or LMB (bCloseWhenAimLeavesAmmo turns on the revolver's close-on-look-away instead).
 * - LMB while open closes the gun without firing; LMB mid-reload lets the step in hand finish, drops the
 *   rest of the queue (a shell already in hand still goes in), then closes.
 * - Two live barrels refuse the shell.
 *
 * The clips key both hammers down unless they animate one, so the hammer of every live barrel is held
 * cocked from code (pose taken from the first frame of that barrel's fire clip).
 */
UCLASS(ClassGroup = (Weapon), meta = (BlueprintSpawnableComponent))
class GUNSANDBALLOONS_API UShotgunComponent : public UGunComponent
{
	GENERATED_BODY()

public:
	UShotgunComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Fire the next live barrel (or click). With the gun open, closes it instead. */
	virtual void PullTrigger() override;

	/** Take one shell: opens the gun if needed, loads it and closes once full. False if both barrels are live or on their way. */
	virtual bool LoadRound() override;

	/** Pull one fired case out (opening the gun if needed). False if no fired case is left that is not already coming out. */
	virtual bool EjectCase() override;

	virtual bool CanLoadRound() const override;

	/** Aiming at an ammo supply breaks the gun open ahead of the click (if a barrel is not live). */
	virtual void SetAimingAtAmmo(bool bAiming) override;

	/** True from the barrels dropping open until they are shut again. */
	virtual bool IsReloading() const override;

	virtual int32 GetLiveRounds() const override;

	/** Stop waiting for ammo: the shell in hand still goes in, the rest of the queue is dropped, then the gun closes. */
	virtual void PrepareToHolster() override;

	virtual bool IsReadyToHolster() const override;

	UFUNCTION(BlueprintPure, Category = "Shotgun")
	EShotgunState GetState() const { return State; }

	UFUNCTION(BlueprintPure, Category = "Shotgun")
	EGunChamber GetBarrel(EShotgunBarrel Barrel) const { return Barrels[static_cast<int32>(Barrel)]; }

	/** Shells handed over but not in a barrel yet (includes the one sliding in). */
	UFUNCTION(BlueprintPure, Category = "Shotgun")
	int32 GetQueuedRounds() const { return QueuedRounds; }

protected:
	virtual void BeginPlay() override;

	// --- Animations (SK_Homestead78_Skeleton) ----------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shotgun|Animation")
	TObjectPtr<UAnimSequenceBase> IdleAnim;

	/** Front trigger, right hammer falls. Its first frame is also the cocked pose of hammer_R. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shotgun|Animation")
	TObjectPtr<UAnimSequenceBase> FireRightAnim;

	/** Back trigger, left hammer falls. Its first frame is also the cocked pose of hammer_L. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shotgun|Animation")
	TObjectPtr<UAnimSequenceBase> FireLeftAnim;

	/** Front trigger pulled with nothing to fire. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shotgun|Animation")
	TObjectPtr<UAnimSequenceBase> DryFireRightAnim;

	/** Back trigger pulled with nothing to fire (not used by the default firing order; kept for tuning). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shotgun|Animation")
	TObjectPtr<UAnimSequenceBase> DryFireLeftAnim;

	/** Barrels drop open. Its last frame is held while the gun is open. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shotgun|Animation")
	TObjectPtr<UAnimSequenceBase> OpenAnim;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shotgun|Animation")
	TObjectPtr<UAnimSequenceBase> EjectRightAnim;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shotgun|Animation")
	TObjectPtr<UAnimSequenceBase> EjectLeftAnim;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shotgun|Animation")
	TObjectPtr<UAnimSequenceBase> InsertRightAnim;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shotgun|Animation")
	TObjectPtr<UAnimSequenceBase> InsertLeftAnim;

	/** Barrels swing shut. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shotgun|Animation")
	TObjectPtr<UAnimSequenceBase> CloseAnim;

	/** Speed of the fire and dry-fire clips. Also sets the trigger-to-shot delay. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shotgun|Animation", meta = (ClampMin = "0.1"))
	float FirePlayRate = 1.5f;

	/** Speed of the open, eject, insert and close clips, when ReloadTime is 0. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shotgun|Animation", meta = (ClampMin = "0.1"))
	float ReloadPlayRate = 1.5f;

	/**
	 * Seconds of animation for a full reload of two fired barrels: open, two ejects, two inserts, close
	 * (the clicks' own pace comes on top). Sets the play rate of every reload clip. 0 = use ReloadPlayRate.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shotgun", meta = (ClampMin = "0.0", Units = "Seconds"))
	float ReloadTime = 0.f;

	/** Time in the fire clips (at play rate 1) when the hammer strikes and the shot leaves. */
	UPROPERTY(EditAnywhere, Category = "Shotgun|Animation|Timing", meta = (ClampMin = "0.0"))
	float ShotTime = 0.133f;

	/** Time in the dry-fire clips (at play rate 1) when the trigger is fully back and clicks. */
	UPROPERTY(EditAnywhere, Category = "Shotgun|Animation|Timing", meta = (ClampMin = "0.0"))
	float DryClickTime = 0.067f;

	/** Time in the eject clips (at play rate 1) when the pulled case is clear of the barrel and becomes a physics casing. */
	UPROPERTY(EditAnywhere, Category = "Shotgun|Animation|Timing", meta = (ClampMin = "0.0"))
	float CaseReleaseTime = 0.4f;

	/**
	 * If true, the gun closes once the aim has left the ammo supply for GateHoldGraceTime (the
	 * revolver's behaviour). If false it stays open until it is full, a shell arrives or the trigger is pulled.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shotgun")
	bool bCloseWhenAimLeavesAmmo = false;

	/** Seconds the gun stays open after the aim leaves the supply, when bCloseWhenAimLeavesAmmo is set. */
	UPROPERTY(EditAnywhere, Category = "Shotgun", meta = (ClampMin = "0.0", EditCondition = "bCloseWhenAimLeavesAmmo"))
	float GateHoldGraceTime = 0.3f;

	/** Live shells at the start (right barrel first). Needs tuning: design value is TBD. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shotgun", meta = (ClampMin = "0", ClampMax = "2"))
	int32 InitialRounds = 2;

private:
	static constexpr int32 NumBarrels = 2;

	void SetState(EShotgunState NewState, UAnimSequenceBase* Anim, float Rate);
	/** Go idle, or open straight away if a shell was handed over meanwhile. */
	void EnterIdle();
	void StartFiring();
	void StartOpening();
	/** Whenever the gun is open and free: eject a case or load a shell (as clicked), wait, or close. */
	void ContinueReload();
	void UpdatePose();

	/** First barrel (right, then left) holding Content, or INDEX_NONE. */
	int32 FindBarrel(EGunChamber Content) const;

	/** Keep the gun open for a shell: a barrel can take one and nothing asked it to close. */
	bool ShouldStayOpen() const;
	/** The player is aiming at a supply now (or, with bCloseWhenAimLeavesAmmo, within the grace time). */
	bool IsAimingAtAmmo() const;

	/** Play rate of the reload clips: from ReloadTime, or ReloadPlayRate. */
	float ReloadRate() const;

	/** Per-barrel clips (0 = right, 1 = left). */
	UAnimSequenceBase* FireAnimFor(int32 Barrel) const;
	UAnimSequenceBase* DryFireAnimFor(int32 Barrel) const;
	UAnimSequenceBase* EjectAnimFor(int32 Barrel) const;
	UAnimSequenceBase* InsertAnimFor(int32 Barrel) const;

	/** Index 0 = right, 1 = left (EShotgunBarrel). */
	EGunChamber Barrels[NumBarrels] = { EGunChamber::Empty, EGunChamber::Empty };

	/** Shells handed over by LoadRound that are not in a barrel yet (includes the one sliding in). */
	int32 QueuedRounds = 0;

	/** Clicks that asked for a fired case to come out and have not started yet. */
	int32 QueuedEjects = 0;

	/** A click (or a weapon swap) asked the open gun to close. */
	bool bCloseRequested = false;

	/** The barrel being fired, ejected or loaded; INDEX_NONE otherwise. */
	int32 ActiveBarrel = INDEX_NONE;

	/** The trigger pulled in Firing has nothing to fire. */
	bool bDryFire = false;
	bool bShotDone = false;
	/** Real seconds since the trigger pull that started the current firing cycle. */
	float FiringElapsed = 0.f;

	bool bCasingReleased = false;

	/** Last value passed to SetAimingAtAmmo, and seconds since it went false. */
	bool bAimRequested = false;
	float AimReleasedTime = TNumericLimits<float>::Max();

	EShotgunState State = EShotgunState::Idle;
	TObjectPtr<UAnimSequenceBase> StateAnim;
	float StateRate = 1.f;
	/** Time in StateAnim, at play rate 1. */
	float StateTime = 0.f;
};
