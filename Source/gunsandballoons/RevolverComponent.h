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
	/** Swinging the cylinder out and ejecting spent casings. */
	Opening,
	/** Turning the cylinder to the next empty chamber while the next round appears. */
	Advancing,
	Inserting,
	Closing,
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnRevolverFired, bool, bHit, const FHitResult&, Hit);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnRevolverEvent);

/**
 * Six-shot revolver logic for SK_Revolver: chambers, double-action firing, hitscan shots, and the
 * open / insert-per-round / close reload, driving URevolverAnimInstance on the weapon mesh.
 *
 * Firing: the cylinder turns to the next chamber while the hammer cocks, and the shot leaves when
 * the hammer drops. An empty or spent chamber just clicks.
 * Reloading: spent casings are ejected (live rounds stay), then a round is inserted into each empty
 * chamber. Pulling the trigger mid-reload closes the cylinder after the current round.
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

	/** Fire (or click on an empty chamber). Mid-reload, finishes the current round and closes instead. */
	UFUNCTION(BlueprintCallable, Category = "Revolver")
	void PullTrigger();

	/** Start a reload if any chamber is not loaded. Returns true if it started. */
	UFUNCTION(BlueprintCallable, Category = "Revolver")
	bool StartReload();

	UFUNCTION(BlueprintPure, Category = "Revolver")
	ERevolverState GetState() const { return State; }

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

	UPROPERTY(BlueprintAssignable, Category = "Revolver")
	FOnRevolverEvent OnRoundInserted;

	UPROPERTY(BlueprintAssignable, Category = "Revolver")
	FOnRevolverEvent OnReloadFinished;

protected:
	virtual void BeginPlay() override;

	// --- Animations ---------------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Revolver|Animation")
	TObjectPtr<UAnimSequenceBase> IdleAnim;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Revolver|Animation")
	TObjectPtr<UAnimSequenceBase> FireAnim;

	/** Opens and ejects every chamber. Used when no live rounds are left. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Revolver|Animation")
	TObjectPtr<UAnimSequenceBase> OpenAnim;

	/** Opens without ejecting; spent chambers are ejected by code. Used when live rounds remain. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Revolver|Animation")
	TObjectPtr<UAnimSequenceBase> OpenKeepLiveAnim;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Revolver|Animation")
	TObjectPtr<UAnimSequenceBase> InsertRoundAnim;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Revolver|Animation")
	TObjectPtr<UAnimSequenceBase> CloseAnim;

	/** Speed of the fire animation. Also sets the trigger-to-shot delay and the fire rate. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Revolver|Animation", meta = (ClampMin = "0.1"))
	float FirePlayRate = 2.f;

	/** Speed of every reload clip. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Revolver|Animation", meta = (ClampMin = "0.1"))
	float ReloadPlayRate = 1.5f;

	/** Time in FireAnim (at play rate 1) when the hammer drops and the shot leaves. */
	UPROPERTY(EditAnywhere, Category = "Revolver|Animation|Timing", meta = (ClampMin = "0.0"))
	float HammerDropTime = 0.19f;

	/** Time in FireAnim by which the cylinder has turned to the next chamber. */
	UPROPERTY(EditAnywhere, Category = "Revolver|Animation|Timing", meta = (ClampMin = "0.01"))
	float CylinderTurnTime = 0.167f;

	/** Window in the open clips while the ejector pushes the casings out. */
	UPROPERTY(EditAnywhere, Category = "Revolver|Animation|Timing", meta = (ClampMin = "0.0"))
	float EjectStartTime = 0.70f;

	UPROPERTY(EditAnywhere, Category = "Revolver|Animation|Timing", meta = (ClampMin = "0.0"))
	float EjectEndTime = 0.87f;

	/** How fast the open cylinder turns to the next empty chamber (degrees/second, at play rate 1). */
	UPROPERTY(EditAnywhere, Category = "Revolver|Animation|Timing", meta = (ClampMin = "1.0"))
	float AdvanceDegreesPerSecond = 450.f;

	/** Shortest time for the next round to appear, even when no turn is needed. */
	UPROPERTY(EditAnywhere, Category = "Revolver|Animation|Timing", meta = (ClampMin = "0.01"))
	float MinAdvanceTime = 0.13f;

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

	/** Spawned as a physics prop for each spent casing ejected. Null disables casings. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Revolver|Casings")
	TObjectPtr<UStaticMesh> CasingMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Revolver|Casings", meta = (ClampMin = "0.0"))
	float CasingLifeSpan = 5.f;

	/** Ejection speed (cm/s), out of the back of the cylinder. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Revolver|Casings", meta = (ClampMin = "0.0"))
	float CasingEjectSpeed = 60.f;

private:
	void SetState(ERevolverState NewState, UAnimSequenceBase* Anim, float Rate);
	void BeginAdvanceOrClose();
	void Shoot();
	void EjectSpentCasings();
	void UpdatePose();
	int32 FindNextEmptyOffset() const;
	int32 TopChamber() const { return ((CylinderStep % NumChambers) + NumChambers) % NumChambers; }

	UPROPERTY(Transient)
	TObjectPtr<USkeletalMeshComponent> Mesh;

	UPROPERTY(Transient)
	TObjectPtr<UCameraComponent> Camera;

	TArray<ERevolverChamber> Chambers;
	/** Chambers whose casing is sliding out during the current open. */
	TArray<bool> Ejecting;

	ERevolverState State = ERevolverState::Idle;
	TObjectPtr<UAnimSequenceBase> StateAnim;
	float StateRate = 1.f;
	/** Time in StateAnim (or, while advancing, seconds into the advance). */
	float StateTime = 0.f;
	float AdvanceDuration = 0.f;

	/** Chamber index under the hammer / at the loading gate is CylinderStep mod 6. */
	int32 CylinderStep = 0;
	float CylinderFromAngle = 0.f;
	float CylinderToAngle = 0.f;

	bool bShotDone = false;
	bool bCasingsEjected = false;
	bool bCancelReload = false;
};
