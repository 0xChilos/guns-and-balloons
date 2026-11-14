#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "RevolverComponent.generated.h"

class UAnimSequenceBase;
class UCameraComponent;
class UMaterialInstanceDynamic;
class URevolverAnimInstance;
class USkeletalMeshComponent;
class UStaticMesh;
class UTexture;

UENUM(BlueprintType)
enum class ERevolverChamber : uint8
{
	Empty,
	Live,
	Spent,
};

UENUM(BlueprintType)
enum class ERevolverState : uint8
{
	Idle,
	Firing,
	/** Half-cocking the hammer and swinging the loading gate open. */
	ReloadStart,
	/** Gate open, waiting for a round (the player is aiming at an ammo supply). */
	GateOpen,
	/** Turning the cylinder to bring the next spent/empty chamber to the gate. */
	Turning,
	/** The ejector rod pushes one spent case out of the gate. */
	Ejecting,
	/** One round slides in through the gate. */
	Inserting,
	/** Closing the gate, then lowering the hammer. */
	ReloadEnd,
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnRevolverFired, bool, bHit, const FHitResult&, Hit);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnRevolverEvent);

/**
 * Six-shot loading-gate revolver (SK_Pax): chambers, double-action firing, hitscan shots, and the
 * one-chamber-at-a-time reload, driving URevolverAnimInstance on the weapon mesh.
 *
 * Firing (A_Fire): the cylinder turns to the next chamber while the hammer cocks; the shot leaves
 * when the hammer drops. An empty or spent chamber just clicks.
 * Reloading is never a key press: every round comes from an ammo supply the player clicks on
 * (UAmmoSupplyComponent -> LoadRound), one round per click. Each handed-over round is queued;
 * A_Reload_Start opens the gate, then per queued round the cylinder turns the next chamber that is
 * not live to the gate, A_Reload_EjectCase pushes out a spent case (empty chambers skip this) and
 * A_Reload_InsertRound loads the round. A_Reload_End closes the gate once the queue is empty, so
 * clicking again while the gate is still open keeps it open for the next round.
 * Aiming at an ammo supply (SetHoldGateOpen) opens the gate ahead of the click and keeps it open
 * between rounds; looking away (after GateHoldGraceTime) closes it.
 * Pulling the trigger mid-reload finishes the current chamber, drops the rest of the queue and closes.
 *
 * The cylinder turn is not in any clip, so it is driven here, matching the author's
 * Preview_Reload_1..6 clips (60 degrees in ~0.17s between chambers).
 */
UCLASS(ClassGroup = (Weapon), meta = (BlueprintSpawnableComponent))
class GUNSANDBALLOONS_API URevolverComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URevolverComponent();

	static constexpr int32 NumChambers = 6;

	/** Mesh to animate and the camera to shoot from. Call before BeginPlay. */
	void SetupWeapon(USkeletalMeshComponent* InMesh, UCameraComponent* InCamera);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Fire (or click on an empty chamber). Mid-reload, finishes the current chamber and closes instead. */
	UFUNCTION(BlueprintCallable, Category = "Revolver")
	void PullTrigger();

	/**
	 * Hand the gun one round to load (e.g. taken from an ammo crate). Opens the gate if needed and
	 * loads it into the next empty or spent chamber. Returns false, taking nothing, if every chamber
	 * is already live or has a round queued for it.
	 */
	UFUNCTION(BlueprintCallable, Category = "Revolver")
	bool LoadRound();

	/**
	 * Call every frame with whether the player is aiming at an ammo supply. While true the gate
	 * opens (if a chamber is free) and waits for rounds instead of closing.
	 */
	UFUNCTION(BlueprintCallable, Category = "Revolver")
	void SetHoldGateOpen(bool bHold);

	/** True if LoadRound would accept a round now. */
	UFUNCTION(BlueprintPure, Category = "Revolver")
	bool CanLoadRound() const;

	/** Rounds handed over that are not in a chamber yet. */
	UFUNCTION(BlueprintPure, Category = "Revolver")
	int32 GetQueuedRounds() const { return QueuedRounds; }

	UFUNCTION(BlueprintPure, Category = "Revolver")
	ERevolverState GetState() const { return State; }

	/** True from the gate opening until it has closed again. */
	UFUNCTION(BlueprintPure, Category = "Revolver")
	bool IsReloading() const;

	UFUNCTION(BlueprintPure, Category = "Revolver")
	int32 GetLiveRounds() const;

	UFUNCTION(BlueprintPure, Category = "Revolver")
	ERevolverChamber GetChamber(int32 Index) const { return Chambers.IsValidIndex(Index) ? Chambers[Index] : ERevolverChamber::Empty; }

	/** A shot left the barrel (the moment the hammer drops). Hook muzzle flash, sound, camera shake here. */
	UPROPERTY(BlueprintAssignable, Category = "Revolver")
	FOnRevolverFired OnFired;

	/** The hammer fell on an empty or spent chamber. */
	UPROPERTY(BlueprintAssignable, Category = "Revolver")
	FOnRevolverEvent OnDryFire;

	UPROPERTY(BlueprintAssignable, Category = "Revolver")
	FOnRevolverEvent OnReloadStarted;

	/** A spent case left the gate. */
	UPROPERTY(BlueprintAssignable, Category = "Revolver")
	FOnRevolverEvent OnCaseEjected;

	UPROPERTY(BlueprintAssignable, Category = "Revolver")
	FOnRevolverEvent OnRoundInserted;

	UPROPERTY(BlueprintAssignable, Category = "Revolver")
	FOnRevolverEvent OnReloadFinished;

protected:
	virtual void BeginPlay() override;

	// --- Animations (SK_Pax_Skeleton) -----------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Revolver|Animation")
	TObjectPtr<UAnimSequenceBase> IdleAnim;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Revolver|Animation")
	TObjectPtr<UAnimSequenceBase> FireAnim;

	/** Half-cocks the hammer and opens the loading gate. Its last frame is held while the cylinder turns. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Revolver|Animation")
	TObjectPtr<UAnimSequenceBase> ReloadStartAnim;

	/** Pushes one case out of the chamber at the gate. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Revolver|Animation")
	TObjectPtr<UAnimSequenceBase> EjectCaseAnim;

	/** Slides one round into the chamber at the gate. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Revolver|Animation")
	TObjectPtr<UAnimSequenceBase> InsertRoundAnim;

	/** Closes the gate, brings the hammer to full cock and lowers it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Revolver|Animation")
	TObjectPtr<UAnimSequenceBase> ReloadEndAnim;

	/** Speed of the fire animation. Also sets the trigger-to-shot delay and the fire rate. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Revolver|Animation", meta = (ClampMin = "0.1"))
	float FirePlayRate = 2.f;

	/** Speed of every reload clip and of the cylinder turns between them. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Revolver|Animation", meta = (ClampMin = "0.1"))
	float ReloadPlayRate = 1.5f;

	/** Time in FireAnim (at play rate 1) when the hammer drops and the shot leaves. */
	UPROPERTY(EditAnywhere, Category = "Revolver|Animation|Timing", meta = (ClampMin = "0.0"))
	float HammerDropTime = 0.15f;

	/** Time in FireAnim by which the hammer is fully cocked and the cylinder has turned to the next chamber. */
	UPROPERTY(EditAnywhere, Category = "Revolver|Animation|Timing", meta = (ClampMin = "0.01"))
	float CylinderTurnTime = 0.133f;

	/** Time in EjectCaseAnim when the case clears the gate and becomes a physics casing. */
	UPROPERTY(EditAnywhere, Category = "Revolver|Animation|Timing", meta = (ClampMin = "0.0"))
	float CaseReleaseTime = 0.5f;

	/** How fast the cylinder turns between chambers while the gate is open (degrees/second, at play rate 1). */
	UPROPERTY(EditAnywhere, Category = "Revolver|Animation|Timing", meta = (ClampMin = "1.0"))
	float TurnDegreesPerSecond = 360.f;

	/** Shortest cylinder turn between chambers (seconds, at play rate 1). */
	UPROPERTY(EditAnywhere, Category = "Revolver|Animation|Timing", meta = (ClampMin = "0.01"))
	float MinTurnTime = 0.167f;

	/** Seconds the gate stays open after the aim leaves the ammo supply, so grazing its edge doesn't flap it. */
	UPROPERTY(EditAnywhere, Category = "Revolver|Animation|Timing", meta = (ClampMin = "0.0"))
	float GateHoldGraceTime = 0.3f;

	// --- Shooting -----------------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Revolver|Shooting", meta = (ClampMin = "0.0"))
	float Damage = 25.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Revolver|Shooting", meta = (ClampMin = "0.0"))
	float Range = 10000.f;

	/** Impulse pushed into physics objects that are hit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Revolver|Shooting", meta = (ClampMin = "0.0"))
	float HitImpulse = 3000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Revolver|Shooting")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

	/** Live rounds in the cylinder at BeginPlay (the rest start empty). Needs tuning: design value is TBD. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Revolver|Shooting", meta = (ClampMin = "0", ClampMax = "6"))
	int32 InitialRounds = NumChambers;

	/** Draw each shot's trace for 2s. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Revolver|Shooting")
	bool bDebugShots = false;

	// --- Casings ------------------------------------------------------------------------

	/** Spawned as a physics prop where each ejected case leaves the gate. Null disables casings. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Revolver|Casings")
	TObjectPtr<UStaticMesh> CasingMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Revolver|Casings", meta = (ClampMin = "0.0"))
	float CasingLifeSpan = 5.f;

	/** Extra random speed (cm/s) on top of the case's own motion out of the gate. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Revolver|Casings", meta = (ClampMin = "0.0"))
	float CasingScatter = 30.f;

	// --- Look ---------------------------------------------------------------------------

	/**
	 * Diffuse map forced onto the weapon mesh (and casings) at BeginPlay through a dynamic material
	 * instance. M_Pax is an instance the FBX importer owns: every reimport of SK_Pax.fbx resets its
	 * parameters and drops the texture, so the gun sets it itself. Null leaves the materials alone.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Revolver|Look")
	TObjectPtr<UTexture> WeaponTexture;

	/** Texture parameter of the material that takes WeaponTexture (Interchange's FBXLegacyPhongSurfaceMaterial). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Revolver|Look")
	FName WeaponTextureParameter = TEXT("DiffuseColorMap");

private:
	void SetState(ERevolverState NewState, UAnimSequenceBase* Anim, float Rate);
	/** Go idle, or reopen the gate straight away if rounds were handed over meanwhile. */
	void EnterIdle();
	void OpenGate();
	/** Apply WeaponTexture to the mesh's materials. */
	void ApplyWeaponTexture();
	/** Turn to the next chamber that needs a queued round, or close the gate when the queue is empty. */
	void ContinueReload();
	/** Eject or load the chamber now at the gate. */
	void ServiceGateChamber();
	void Shoot();
	void ReleaseCasing();
	void UpdatePose();
	int32 TopChamber() const { return ((CylinderStep % NumChambers) + NumChambers) % NumChambers; }
	/** The chamber at the loading gate: the one that was under the hammer one step earlier. */
	int32 GateChamber() const { return (TopChamber() + NumChambers - 1) % NumChambers; }

	UPROPERTY(Transient)
	TObjectPtr<USkeletalMeshComponent> Mesh;

	UPROPERTY(Transient)
	TObjectPtr<UCameraComponent> Camera;

	/** Material given to spawned casings, so they match the textured gun. */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> CasingMaterial;

	TArray<ERevolverChamber> Chambers;

	/** Rounds handed over by LoadRound that are not in a chamber yet (includes the one sliding in). */
	int32 QueuedRounds = 0;

	/** Last value passed to SetHoldGateOpen, and seconds since it went false. */
	bool bHoldGateRequested = false;
	float HoldReleasedTime = TNumericLimits<float>::Max();

	/** Holding the gate open: aiming at a supply now or within the grace time, and a chamber is free. */
	bool ShouldHoldGateOpen() const;

	ERevolverState State = ERevolverState::Idle;
	TObjectPtr<UAnimSequenceBase> StateAnim;
	float StateRate = 1.f;
	/** Time in StateAnim (or, while turning, seconds into the turn), at play rate 1. */
	float StateTime = 0.f;

	/** Chamber index under the hammer is CylinderStep mod 6. Only ever counts up. */
	int32 CylinderStep = 0;
	int32 TurnFromStep = 0;
	float TurnDuration = 0.f;

	bool bShotDone = false;
	bool bCasingReleased = false;

	/** World location of the ejected case last frame, to give the casing the case's own velocity. */
	FVector LastCaseLocation = FVector::ZeroVector;
	bool bHasLastCaseLocation = false;
};
