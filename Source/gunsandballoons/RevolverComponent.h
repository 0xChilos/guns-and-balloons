#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "RevolverComponent.generated.h"

class UAnimSequenceBase;
class UCameraComponent;
class URevolverAnimInstance;
class USkeletalMeshComponent;
class UStaticMesh;

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
 * Reloading: A_Reload_Start opens the gate, then for every chamber that is not live the cylinder
 * turns it to the gate, A_Reload_EjectCase pushes out a spent case (empty chambers skip this) and
 * A_Reload_InsertRound loads a round. A_Reload_End closes the gate. Live rounds are kept.
 * Pulling the trigger mid-reload finishes the current chamber and closes the gate.
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

	/** Start a reload if any chamber is not loaded. Returns true if it started. */
	UFUNCTION(BlueprintCallable, Category = "Revolver")
	bool StartReload();

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

	/** Pulling the trigger with no live rounds starts a reload instead of clicking. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Revolver|Shooting")
	bool bAutoReloadWhenEmpty = true;

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

private:
	void SetState(ERevolverState NewState, UAnimSequenceBase* Anim, float Rate);
	/** Turn to the next chamber that needs work, or close the gate when there is none. */
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

	TArray<ERevolverChamber> Chambers;

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
	bool bCancelReload = false;

	/** World location of the ejected case last frame, to give the casing the case's own velocity. */
	FVector LastCaseLocation = FVector::ZeroVector;
	bool bHasLastCaseLocation = false;
};
