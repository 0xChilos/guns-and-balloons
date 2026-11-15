#pragma once

#include "CoreMinimal.h"
#include "GunComponent.h"
#include "CollisionQueryParams.h"
#include "HookComponent.generated.h"

class AHookPickup;
class UAnimSequenceBase;
class UMeshComponent;
class UNiagaraSystem;
class UStaticMeshComponent;
class UTexture;

/** Where the hook head is. */
UENUM(BlueprintType)
enum class EHookState : uint8
{
	/** On the gun. */
	Stowed,
	/** Flying out along the aim. */
	Out,
	/** Stopped on a pickup, claws closing on it. */
	Grabbing,
	/** Reeled back to the gun (with or without a pickup). */
	Returning,
};

/** What the gun itself is animating. */
UENUM(BlueprintType)
enum class EHookAction : uint8
{
	Idle,
	/** The kick of the shot (Throw clip). */
	Throw,
	/** One stroke of the pump lever (Pump clip). */
	Pump,
	/** The pressure has just reached 100 (Charged clip). */
	Charged,
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnHookPickup, AActor*, Pickup);

/**
 * Pneumatic grappling hook (SK_Hook), key 4. Not a combat weapon: it pulls resources (AHookPickup) in.
 *
 * - Pressure (Charge, 0..100) shows on the gauge on the gun (needle bone, 240 degree dial; the green
 *   zone is at 100). It starts full. The hook only fires at 100: LMB below that just clicks (OnDryFire).
 * - LMB at 100: the gun kicks (Throw clip), the pressure drops to 0 and the head flies out along the aim
 *   at ThrowSpeed, up to MaxRange. The rope is a straight line (SM_HK_Rope, no physics).
 * - The head hitting an AHookPickup grabs it (Grab clip: the claws close) and reels it back at
 *   ReturnSpeed; anything else, or running out of range, sends it back empty. A pickup is let go
 *   CatchDistance before the gun, so it drops in front of the player.
 * - Back on the gun the hook needs pumping: each stroke (RMB, hold to keep pumping; Pump clip) adds
 *   100 / PumpsToFullCharge. Strokes are quick at first; once the pressure passes HardPumpFrom the lever
 *   gets heavy and each stroke plays slower, down to HardPumpPlayRate on the last. Reaching 100 plays
 *   the Charged clip.
 *
 * The flying head is drawn by a second, world-space copy of SK_Hook (HeadMesh) whose body is shrunk to
 * nothing, so only the head and claws show; the head on the first-person gun is hidden meanwhile.
 */
UCLASS(ClassGroup = (Weapon), meta = (BlueprintSpawnableComponent))
class GUNSANDBALLOONS_API UHookComponent : public UGunComponent
{
	GENERATED_BODY()

public:
	UHookComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Fire the hook (only at full pressure and with the head on the gun); otherwise a dry click. */
	virtual void PullTrigger() override;

	/** RMB held: keep pumping while the head is on the gun and the pressure is below 100. */
	virtual void SetSecondaryHeld(bool bHeld) override;

	virtual void SetEquipped(bool bInEquipped) override;
	virtual void PrepareToHolster() override;

	/** Only with the head back on the gun and no pump stroke under way. */
	virtual bool IsReadyToHolster() const override;

	/** 1 when ready to fire. */
	virtual int32 GetLiveRounds() const override { return IsFullyCharged() ? 1 : 0; }

	UFUNCTION(BlueprintPure, Category = "Hook")
	float GetCharge() const { return Charge; }

	UFUNCTION(BlueprintPure, Category = "Hook")
	bool IsFullyCharged() const;

	UFUNCTION(BlueprintPure, Category = "Hook")
	EHookState GetState() const { return State; }

	UFUNCTION(BlueprintPure, Category = "Hook")
	EHookAction GetAction() const { return Action; }

	UFUNCTION(BlueprintPure, Category = "Hook")
	int32 GetPumpsToFullCharge() const { return PumpsToFullCharge; }

	UFUNCTION(BlueprintCallable, Category = "Hook")
	void SetPumpsToFullCharge(int32 Pumps) { PumpsToFullCharge = FMath::Clamp(Pumps, 1, 100); }

	/** Debug: set the pressure directly (0..100). */
	UFUNCTION(BlueprintCallable, Category = "Hook")
	void SetCharge(float NewCharge);

	// Tuning values, exposed for the debug menu (SHookDebugMenu).
	float GetMaxRange() const { return MaxRange; }
	void SetMaxRange(float Value) { MaxRange = FMath::Max(100.f, Value); }
	float GetThrowSpeed() const { return ThrowSpeed; }
	void SetThrowSpeed(float Value) { ThrowSpeed = FMath::Max(100.f, Value); }
	float GetReturnSpeed() const { return ReturnSpeed; }
	void SetReturnSpeed(float Value) { ReturnSpeed = FMath::Max(100.f, Value); }
	float GetHardPumpFrom() const { return HardPumpFrom; }
	void SetHardPumpFrom(float Value) { HardPumpFrom = FMath::Clamp(Value, 0.f, 100.f); }
	float GetHardPumpPlayRate() const { return HardPumpPlayRate; }
	void SetHardPumpPlayRate(float Value) { HardPumpPlayRate = FMath::Clamp(Value, 0.05f, 1.f); }

	/** Play rate of a pump stroke starting at FromCharge: PumpPlayRate below HardPumpFrom, heavier after. */
	float GetPumpStrokeRate(float FromCharge) const;

	/** The hook has grabbed a pickup (it is on its way back). */
	UPROPERTY(BlueprintAssignable, Category = "Hook")
	FOnHookPickup OnPickupGrabbed;

	/**
	 * A pickup reached the player and was let go in front of them (or, with bConsumeOnDelivery, is about to be
	 * destroyed). Not called for one dropped on the way (the hook put away mid-reel).
	 */
	UPROPERTY(BlueprintAssignable, Category = "Hook")
	FOnHookPickup OnPickupDelivered;

	/** The head is back on the gun. */
	UPROPERTY(BlueprintAssignable, Category = "Hook")
	FOnGunEvent OnHookReturned;

	/** One pump stroke has added its share of pressure. */
	UPROPERTY(BlueprintAssignable, Category = "Hook")
	FOnGunEvent OnPumped;

	/** The pressure reached 100. */
	UPROPERTY(BlueprintAssignable, Category = "Hook")
	FOnGunEvent OnFullyCharged;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// --- Animations (SK_Hook_Skeleton) ----------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hook|Animation")
	TObjectPtr<UAnimSequenceBase> IdleAnim;

	/** Kick of the shot; the claws open. Plays on the gun and on the flying head. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hook|Animation")
	TObjectPtr<UAnimSequenceBase> ThrowAnim;

	/** Claws close (on a pickup, or empty when the hook comes back with nothing). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hook|Animation")
	TObjectPtr<UAnimSequenceBase> GrabAnim;

	/** One stroke of the pump lever. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hook|Animation")
	TObjectPtr<UAnimSequenceBase> PumpAnim;

	/** Played when the pressure reaches 100. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hook|Animation")
	TObjectPtr<UAnimSequenceBase> ChargedAnim;

	/** The gauge needle at 0 (Needle_Ref_0). The needle turns NeedleSweep degrees from here to 100. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hook|Animation")
	TObjectPtr<UAnimSequenceBase> NeedleZeroAnim;

	/** Degrees the needle turns (about its own +Y) from 0 to 100; from Needle_Ref_0 -> Needle_Ref_100. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hook|Animation")
	float NeedleSweep = 240.f;

	/** How fast the needle follows the pressure (gauge units per second). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hook|Animation", meta = (ClampMin = "1.0"))
	float NeedleSpeed = 250.f;

	/** Speed of an easy pump stroke, below HardPumpFrom (1 = the clip's 0.37 s). TBD: needs tuning. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hook|Animation", meta = (ClampMin = "0.1"))
	float PumpPlayRate = 1.f;

	/**
	 * Pressure above which the lever gets heavy. A stroke that ends past it plays slower, down to
	 * HardPumpPlayRate for the stroke that reaches 100. TBD: needs tuning.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hook|Pressure", meta = (ClampMin = "0.0", ClampMax = "100.0"))
	float HardPumpFrom = 50.f;

	/** Speed of the last, heaviest stroke (fraction of PumpPlayRate). TBD: needs tuning. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hook|Pressure", meta = (ClampMin = "0.05", ClampMax = "1.0"))
	float HardPumpPlayRate = 0.4f;

	// --- Pressure -------------------------------------------------------------------------

	/** Pump strokes from 0 to 100. Design value TBD (grappling-hook.md); tweak in the debug menu. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hook|Pressure", meta = (ClampMin = "1", ClampMax = "100"))
	int32 PumpsToFullCharge = 5;

	/** Pressure at the start. The hook comes fully charged. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hook|Pressure", meta = (ClampMin = "0.0", ClampMax = "100.0"))
	float InitialCharge = 100.f;

	// --- Flight ---------------------------------------------------------------------------

	/** How far (cm) the head flies before it comes back empty. Design value TBD (grappling-hook.md). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hook|Flight", meta = (ClampMin = "100.0"))
	float MaxRange = 2500.f;

	/** Head speed going out (cm/s). TBD: needs tuning. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hook|Flight", meta = (ClampMin = "100.0"))
	float ThrowSpeed = 2500.f;

	/** Reeling-in speed (cm/s), with or without a pickup. Design value TBD (grappling-hook.md). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hook|Flight", meta = (ClampMin = "100.0"))
	float ReturnSpeed = 1500.f;

	/** Radius (cm) of the head's sweep: how close it must pass to catch something. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hook|Flight", meta = (ClampMin = "0.0"))
	float HookRadius = 12.f;

	/** The pickup is let go this far (cm) from the gun, so it drops in front of the player. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hook|Flight", meta = (ClampMin = "0.0"))
	float CatchDistance = 120.f;

	/** Draw the head's path and the aim trace. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hook|Flight")
	bool bDebugHook = false;

	// --- Look -----------------------------------------------------------------------------

	/** The rope drawn from the gun's muzzle to the head: 1 cm long along +X, stretched. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hook|Look")
	TObjectPtr<UStaticMesh> RopeMesh;

	/** Diffuse map of the rope (the M_Rope slot of the gun and RopeMesh). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hook|Look")
	TObjectPtr<UTexture> RopeTexture;

	/** Material slot on SK_Hook that takes RopeTexture instead of WeaponTexture. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hook|Look")
	FName RopeMaterialSlot = TEXT("M_Rope");

	/** Burst at the hook head when the claws close on a pickup (the pickup also plays its own GrabSound). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hook|Look")
	TObjectPtr<UNiagaraSystem> GrabEffect;

	/** Puff where a pickup with bConsumeOnDelivery disappears in front of the player. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hook|Look")
	TObjectPtr<UNiagaraSystem> ArrivalEffect;

private:
	void SetAction(EHookAction NewAction, UAnimSequenceBase* Anim, float Rate);
	void SetHeadClip(UAnimSequenceBase* Anim);
	void Launch();
	void StartReturning();
	void StowHead();
	/**
	 * Lets go of the carried pickup. bArrived: it reached the catch point in front of the player (OnPickupDelivered,
	 * consumed if it says so). Otherwise (the hook was put away mid-reel, or play ended) it just drops where it is.
	 */
	void DeliverPickup(bool bArrived);
	/** GrabEffect at the head and the pickup's own GrabSound. */
	void PlayGrabFeedback(AHookPickup* Pickup);
	void UpdateFlight(float DeltaTime);
	void UpdatePose();
	void UpdateHeadAndRope();
	void CreateWorldParts();
	void TextureRope(UMeshComponent* Target, int32 MaterialIndex);
	bool CanPump() const;

	/** Where the head sits on the gun, in the world (corrected for the first-person FOV). */
	FVector GetRestHeadLocation() const;
	FVector GetGunPoint(FName Bone) const;
	FCollisionQueryParams MakeQueryParams() const;

	/** The world-space copy of SK_Hook that shows the flying head. */
	UPROPERTY(Transient)
	TObjectPtr<USkeletalMeshComponent> HeadMesh;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Rope;

	UPROPERTY(Transient)
	TWeakObjectPtr<AHookPickup> CarriedPickup;

	EHookState State = EHookState::Stowed;
	EHookAction Action = EHookAction::Idle;

	TObjectPtr<UAnimSequenceBase> ActionAnim;
	float ActionRate = 1.f;
	/** Time in ActionAnim at play rate 1. */
	float ActionTime = 0.f;

	TObjectPtr<UAnimSequenceBase> HeadAnim;
	float HeadAnimTime = 0.f;

	float Charge = 100.f;
	float DisplayedCharge = 100.f;
	bool bPumpHeld = false;

	FVector HeadLocation = FVector::ZeroVector;
	FVector HeadVelocity = FVector::ZeroVector;
	FVector LaunchLocation = FVector::ZeroVector;
	FVector FlightDirection = FVector::ForwardVector;
	float GrabTimeLeft = 0.f;
};
