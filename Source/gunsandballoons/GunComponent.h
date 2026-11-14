#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "GunSmoke.h"
#include "GunComponent.generated.h"

class AGunProjectile;
class UAnimSequenceBase;
class UCameraComponent;
class UMaterialInstanceDynamic;
class USkeletalMeshComponent;
class UStaticMesh;
class UTexture;

/** What a chamber holds. A fired round leaves a spent case behind until it is ejected on reload. */
UENUM(BlueprintType)
enum class EGunChamber : uint8
{
	Empty,
	Live,
	Spent,
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGunShotHit, const FHitResult&, Hit);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnGunEvent);

/**
 * Base for the hand-less viewmodel guns (revolver, rifle, shotgun). Owns what every gun shares:
 * - the weapon mesh it animates and the camera it shoots from (SetupWeapon);
 * - equipping (shows/hides the mesh, pauses the gun while holstered) and winding down before a swap;
 * - the shot: a ballistic bullet with a tracer (AGunProjectile) fired from the eye, not a hitscan;
 * - physics casings handed over from an animated case bone, and the diffuse texture forced onto the
 *   mesh at BeginPlay;
 * - black-powder smoke (AGunSmoke): a blast and a smoking barrel after every shot, and a wisp out of the
 *   breech when the gun is first opened after firing (EmitBreechSmoke);
 * - where the pawn holds it in front of the eyes (rest and reload pose) and how hard it kicks.
 *
 * Reloading is never a key press: every round comes from an ammo supply the player right-clicks
 * (UAmmoSupplyComponent -> LoadRound), and aiming at a supply (SetAimingAtAmmo) lets the gun open
 * up ahead of the click. Subclasses implement the mechanism.
 */
UCLASS(Abstract, ClassGroup = (Weapon))
class GUNSANDBALLOONS_API UGunComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGunComponent();

	/** Mesh to animate and the camera to shoot from. Call before BeginPlay. */
	void SetupWeapon(USkeletalMeshComponent* InMesh, UCameraComponent* InCamera);

	USkeletalMeshComponent* GetMesh() const { return Mesh; }

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// --- Equipping ---------------------------------------------------------------------

	/** Show the gun and let it run, or hide it and pause it (holstered). */
	UFUNCTION(BlueprintCallable, Category = "Gun")
	virtual void SetEquipped(bool bInEquipped);

	UFUNCTION(BlueprintPure, Category = "Gun")
	bool IsEquipped() const { return bEquipped; }

	/** Start winding down for a weapon swap: stop waiting for ammo, finish what is in hand, close up. */
	UFUNCTION(BlueprintCallable, Category = "Gun")
	virtual void PrepareToHolster() {}

	/** True once the gun is at rest (not firing or reloading) and can be put away. */
	UFUNCTION(BlueprintPure, Category = "Gun")
	virtual bool IsReadyToHolster() const { return true; }

	// --- Mechanism (per gun) ---------------------------------------------------------

	/** Fire (or click on an empty/spent chamber). Mid-reload, wraps the reload up instead. */
	UFUNCTION(BlueprintCallable, Category = "Gun")
	virtual void PullTrigger() {}

	/** Hand the gun one round from an ammo supply. Returns false, taking nothing, if it cannot take one. */
	UFUNCTION(BlueprintCallable, Category = "Gun")
	virtual bool LoadRound() { return false; }

	/**
	 * RMB on an ammo supply, before any round is taken: lets a gun whose fired cases come out by hand
	 * pull one out instead. Returns true if the click was used for that (the supply then gives nothing).
	 */
	UFUNCTION(BlueprintCallable, Category = "Gun")
	virtual bool EjectCase() { return false; }

	/** True if LoadRound would accept a round now. */
	UFUNCTION(BlueprintPure, Category = "Gun")
	virtual bool CanLoadRound() const { return false; }

	/** Call every frame with whether the player is aiming at an ammo supply, so the gun can open ahead of the click. */
	UFUNCTION(BlueprintCallable, Category = "Gun")
	virtual void SetAimingAtAmmo(bool bAiming) {}

	/** True while the gun is open for loading (the pawn holds it in the reload pose). */
	UFUNCTION(BlueprintPure, Category = "Gun")
	virtual bool IsReloading() const { return false; }

	UFUNCTION(BlueprintPure, Category = "Gun")
	virtual int32 GetLiveRounds() const { return 0; }

	// --- Viewmodel pose (read by the pawn) -------------------------------------------------

	const FVector& GetViewOffset() const { return ViewOffset; }
	const FRotator& GetViewRotation() const { return ViewRotation; }
	const FVector& GetReloadViewOffset() const { return ReloadViewOffset; }
	const FRotator& GetReloadViewRotation() const { return ReloadViewRotation; }
	float GetRecoilKickBack() const { return RecoilKickBack; }
	float GetRecoilKickPitch() const { return RecoilKickPitch; }

	// --- Handling (read by the pawn) -------------------------------------------------------

	/** A random aim kick for one shot (pitch up, yaw either way), from AimRecoilPitch/Variance/Yaw. */
	FRotator RollAimRecoil() const;
	float GetAimRecoilRecovery() const { return AimRecoilRecovery; }
	float GetAimRecoilRecoveryTime() const { return AimRecoilRecoveryTime; }

	/** Hand sway of this gun (degrees, pitch/yaw) and its speed; the pawn adds the balloon's on top. */
	FVector2D GetHandSway() const { return FVector2D(HandSwayPitch, HandSwayYaw); }
	float GetHandSwaySpeed() const { return HandSwaySpeed; }

	// --- Events ------------------------------------------------------------------------

	/** A shot left the barrel (the bullet is in flight). Hook muzzle flash, sound, camera shake here. */
	UPROPERTY(BlueprintAssignable, Category = "Gun")
	FOnGunEvent OnFired;

	/** A bullet from this gun landed (after its flight time). Hook impact effects and hit markers here. */
	UPROPERTY(BlueprintAssignable, Category = "Gun")
	FOnGunShotHit OnShotHit;

	/** The hammer fell on an empty or spent chamber. */
	UPROPERTY(BlueprintAssignable, Category = "Gun")
	FOnGunEvent OnDryFire;

	/** The gun started opening for a round. */
	UPROPERTY(BlueprintAssignable, Category = "Gun")
	FOnGunEvent OnReloadStarted;

	/** A spent case left the gun. */
	UPROPERTY(BlueprintAssignable, Category = "Gun")
	FOnGunEvent OnCaseEjected;

	UPROPERTY(BlueprintAssignable, Category = "Gun")
	FOnGunEvent OnRoundInserted;

	/** The gun is closed again after loading. */
	UPROPERTY(BlueprintAssignable, Category = "Gun")
	FOnGunEvent OnReloadFinished;

	/** The actor simulating this gun's smoke (spawned at BeginPlay; null if SmokeClass is unset). */
	UFUNCTION(BlueprintPure, Category = "Gun|Smoke")
	AGunSmoke* GetSmoke() const { return Smoke; }

protected:
	/** Play length (s) of a clip, 0 if unset. */
	static float AnimLength(const UAnimSequenceBase* Anim);

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/**
	 * The action has just opened with fired cases in: leftover powder smoke drifts out of each of Bones
	 * (BreechSmoke). How much depends on how long ago the last shot was (BreechSmokeClearTime). The gas
	 * is then gone, so opening again shows none until the next shot.
	 */
	void EmitBreechSmoke(TConstArrayView<FName> Bones);
	void EmitBreechSmoke(FName Bone) { EmitBreechSmoke(MakeArrayView(&Bone, 1)); }

	/**
	 * Fire one bullet (ProjectileClass) from the eye along the view at MuzzleVelocity, so it lands on
	 * the crosshair up close and drops with distance; its tracer leaves from MuzzleOffset. Damage and
	 * impulse are applied where it lands (OnShotHit). Broadcasts OnFired. The caller has already turned
	 * the live round into a spent case.
	 */
	void FireShot();

	/**
	 * Where a point on the first-person gun mesh appears in the world. The gun is drawn with its own
	 * (narrower) FOV, world objects with the camera's: pulling the point toward the eye along the view
	 * axis by the ratio of the two lands a world object on the same pixels. Velocity is converted alike.
	 */
	FVector FirstPersonToWorld(const FVector& Location) const;

	/** Direction tilted a random angle (up to MaxAngle degrees) off Direction: evenly over the circle, or center-weighted. */
	static FVector OffsetDirection(const FVector& Direction, float MaxAngle, bool bEven);
	FVector FirstPersonToWorldVector(const FVector& Vector) const;

	/**
	 * Call when the trigger starts a firing cycle. False (start nothing) while RateOfFire says it is too
	 * soon after the last one; otherwise records this pull and returns true.
	 */
	bool ConsumeRateOfFire();

	/**
	 * Play rate for the rest of a fire clip after the shot, so the whole cycle takes CycleTime.
	 * ClipTimeLeft: clip seconds still to play; Elapsed: real seconds since the pull. Returns CurrentRate
	 * when CycleTime is 0 or already used up.
	 */
	float RateForRestOfCycle(float ClipTimeLeft, float Elapsed, float CurrentRate) const;

	/** Forget the recorded case motion. Call when a case starts moving out. */
	void ResetCaseSamples() { NumCaseSamples = 0; }

	/** Records where the animated case (CaseEjectBone) is, to hand its motion over to the physics casing. */
	void SampleCase() { SampleCase(CaseEjectBone); }

	/** SampleCase for a gun with more than one case bone (one per barrel). */
	void SampleCase(FName Bone);

	/** Spawns a physics CasingMesh exactly where the animated case is, carrying on with its motion. */
	void ReleaseCasing();

	UPROPERTY(Transient)
	TObjectPtr<USkeletalMeshComponent> Mesh;

	UPROPERTY(Transient)
	TObjectPtr<UCameraComponent> Camera;

	// --- Shooting -----------------------------------------------------------------------

	/** Needs tuning: the design value is TBD (weapons.md). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gun|Shooting", meta = (ClampMin = "0.0"))
	float Damage = 25.f;

	/** Flight distance (cm) after which a bullet is gone without a hit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gun|Shooting", meta = (ClampMin = "0.0"))
	float Range = 10000.f;

	/** Impulse pushed into physics objects that are hit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gun|Shooting", meta = (ClampMin = "0.0"))
	float HitImpulse = 3000.f;

	/** Channel the bullet traces its flight on (complex collision). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gun|Shooting")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

	/** Draw each bullet's flight path for 2s (red in flight, green to the hit). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gun|Shooting")
	bool bDebugShots = false;

	// --- Ballistics ---------------------------------------------------------------------

	/** Bullet speed (cm/s) as it leaves the gun. Needs tuning: the design value is TBD (weapons.md). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gun|Ballistics", meta = (ClampMin = "1.0"))
	float MuzzleVelocity = 20000.f;

	/**
	 * Bullet drop: multiplier on the world's gravity for this gun's bullets. 1 is real gravity, which at
	 * these speeds barely shows over a fight's distances; higher makes the drop readable. Per gun.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gun|Ballistics", meta = (ClampMin = "0.0"))
	float GravityScale = 1.f;

	/**
	 * Air drag as the distance (m) over which a bullet loses half its speed. A slowing bullet also drops
	 * more and more steeply. 0 = no drag (pistol and rifle bullets keep their speed well; buckshot does not).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gun|Ballistics", meta = (ClampMin = "0.0", Units = "Meters"))
	float SpeedHalvingDistance = 0.f;

	/** Bullets (pellets) per shot: 1 for a bullet, more for shot. Each is its own projectile with Damage and HitImpulse. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gun|Ballistics", meta = (ClampMin = "1", ClampMax = "32"))
	int32 ProjectilesPerShot = 1;

	/**
	 * Pattern of a multi-pellet shot: the largest angle (degrees) a pellet leaves off the shot's line,
	 * spread evenly over the circle. Separate from SpreadAngle, which moves the whole shot.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gun|Ballistics", meta = (ClampMin = "0.0", ClampMax = "30.0", Units = "Degrees", EditCondition = "ProjectilesPerShot > 1"))
	float PelletSpreadAngle = 0.f;

	/** Size of the drawn bullet and its tracer (1 = a full bullet). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gun|Ballistics", meta = (ClampMin = "0.05"))
	float ProjectileVisualScale = 1.f;

	/** Bullet actor spawned per shot. A Blueprint child of AGunProjectile can restyle the tracer. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gun|Ballistics")
	TSubclassOf<AGunProjectile> ProjectileClass;

	/** Muzzle on the weapon mesh (cm, mesh space: X along the barrel). The tracer is drawn from here. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gun|Ballistics")
	FVector MuzzleOffset = FVector(30.f, 0.f, 0.f);

	// --- Handling -------------------------------------------------------------------------
	// All off (0) by default; a gun turns on what it needs.

	/**
	 * Seconds from the trigger pull until the gun takes the next pull (the firing cycle). The shot still
	 * leaves at the clip's own shot time; the rest of the fire clip is stretched or squeezed over what is
	 * left. 0 = the clip's own length. Used by the revolver and the shotgun (time between the barrels).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gun|Handling", meta = (ClampMin = "0.0", Units = "Seconds"))
	float CycleTime = 0.f;

	/** Shots per minute at most: a trigger pull sooner after the last one does nothing. 0 = no limit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gun|Handling", meta = (ClampMin = "0.0"))
	float RateOfFire = 0.f;

	/**
	 * Largest angle (degrees) a bullet leaves off the crosshair. The offset is center-weighted: most
	 * shots land well inside, few at the edge.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gun|Handling", meta = (ClampMin = "0.0", ClampMax = "30.0", Units = "Degrees"))
	float SpreadAngle = 0.f;

	/** How far (degrees) each shot throws the aim up. The player has to pull it back down. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gun|Handling|Recoil", meta = (ClampMin = "0.0", Units = "Degrees"))
	float AimRecoilPitch = 0.f;

	/** Random share of AimRecoilPitch, either way (0.25 = 75%..125%), so the kick can't be learned exactly. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gun|Handling|Recoil", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float AimRecoilPitchVariance = 0.f;

	/** Largest sideways throw (degrees), random left or right. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gun|Handling|Recoil", meta = (ClampMin = "0.0", Units = "Degrees"))
	float AimRecoilYaw = 0.f;

	/** Share of the kick that settles back by itself (0 = the player does it all, 1 = the aim returns fully). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gun|Handling|Recoil", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float AimRecoilRecovery = 0.f;

	/** Roughly how long (seconds) that settling takes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gun|Handling|Recoil", meta = (ClampMin = "0.05", Units = "Seconds"))
	float AimRecoilRecoveryTime = 0.5f;

	/** Hand sway: how far (degrees) the aim wanders up/down while holding this gun. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gun|Handling|Sway", meta = (ClampMin = "0.0", Units = "Degrees"))
	float HandSwayPitch = 0.f;

	/** Hand sway: how far (degrees) the aim wanders left/right. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gun|Handling|Sway", meta = (ClampMin = "0.0", Units = "Degrees"))
	float HandSwayYaw = 0.f;

	/** Hand sway speed: roughly how many direction changes per second. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gun|Handling|Sway", meta = (ClampMin = "0.0"))
	float HandSwaySpeed = 0.7f;

	// --- Casings ------------------------------------------------------------------------

	/** Spawned as a physics prop where each ejected case leaves the gun. Null disables casings. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gun|Casings")
	TObjectPtr<UStaticMesh> CasingMesh;

	/** Bone that carries the case being ejected in the reload clips. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gun|Casings")
	FName CaseEjectBone = TEXT("case_eject");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gun|Casings", meta = (ClampMin = "0.0"))
	float CasingLifeSpan = 5.f;

	/** Extra random speed (cm/s) on top of the case's own motion out of the gun. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gun|Casings", meta = (ClampMin = "0.0"))
	float CasingScatter = 30.f;

	/** Extra random spin (degrees/s) on top of the case's own rotation out of the gun. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gun|Casings", meta = (ClampMin = "0.0"))
	float CasingSpin = 120.f;

	/**
	 * Rotation of CasingMesh relative to CaseEjectBone. The casing meshes lie along their +X while the
	 * rigs' cases lie along the bone's -Y (same geometry and pivot otherwise), hence yaw -90.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gun|Casings")
	FRotator CasingRotationOffset = FRotator(0.f, -90.f, 0.f);

	// --- Smoke --------------------------------------------------------------------------
	// Simulated in the world by SmokeClass. Set a kind's Amount to 0 to turn it off.

	/** Actor that simulates and draws this gun's smoke. A Blueprint child of AGunSmoke can retune the air and look. Null = no smoke. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gun|Smoke")
	TSubclassOf<AGunSmoke> SmokeClass;

	/** The cloud blown out of the muzzle with each shot (a burst along the barrel). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gun|Smoke")
	FGunSmokePuffs MuzzleBlastSmoke;

	/** The barrel smoking after each shot: a thin stream out of the muzzle that follows it as the gun moves. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gun|Smoke")
	FGunSmokePuffs MuzzleWispSmoke;

	/** The little that drifts out of the breech / gate when the gun is first opened after a shot. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gun|Smoke")
	FGunSmokePuffs BreechSmoke;

	/** How quickly (s, time constant) the smoke left inside the gun clears by itself if it is not opened. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gun|Smoke", meta = (ClampMin = "0.1", Units = "Seconds"))
	float BreechSmokeClearTime = 8.f;

	// --- Look ---------------------------------------------------------------------------

	/**
	 * Diffuse map forced onto the weapon mesh (and casings) at BeginPlay through a dynamic material
	 * instance. The materials are instances the FBX importer owns: every reimport resets their
	 * parameters and drops the texture, so the gun sets it itself. Null leaves the materials alone.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gun|Look")
	TObjectPtr<UTexture> WeaponTexture;

	/** Texture parameter of the material that takes WeaponTexture (Interchange's FBXLegacyPhongSurfaceMaterial). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gun|Look")
	FName WeaponTextureParameter = TEXT("DiffuseColorMap");

	// --- Viewmodel ----------------------------------------------------------------------

	/** Rest position of the weapon's root bone relative to the eyes (cm): X forward, Y right, Z up. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gun|Viewmodel")
	FVector ViewOffset = FVector(32.f, 12.f, -7.5f);

	/** Rest rotation of the weapon relative to the view. A little inward yaw points the muzzle at the crosshair. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gun|Viewmodel")
	FRotator ViewRotation = FRotator(0.f, -3.f, 0.f);

	/** Position while reloading: closer and more central, so the loading port is in view. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gun|Viewmodel")
	FVector ReloadViewOffset = FVector(26.f, 5.f, -3.f);

	/** Rotation while reloading, turning the loading port toward the eye. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gun|Viewmodel")
	FRotator ReloadViewRotation = FRotator(15.f, 35.f, -45.f);

	/** How far (cm) the gun jumps back toward the eye on each shot. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gun|Viewmodel", meta = (ClampMin = "0.0"))
	float RecoilKickBack = 4.f;

	/** How far (degrees) the muzzle flips up on each shot. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gun|Viewmodel", meta = (ClampMin = "0.0"))
	float RecoilKickPitch = 12.f;

private:
	/** Apply WeaponTexture to the mesh's materials. */
	void ApplyWeaponTexture();

	/** A stream of smoke leaving a point on the gun mesh (a bone, or a mesh-space offset) for a while. */
	struct FSmokeStream
	{
		FGunSmokePuffs Puffs;
		/** None = Offset is in mesh space; otherwise the stream follows this bone. */
		FName Bone;
		FVector Offset = FVector::ZeroVector;
		/** Mesh-space direction the smoke leaves in. */
		FVector Direction = FVector::ForwardVector;
		float Strength = 1.f;
		float Elapsed = 0.f;
		/** Puffs owed but not emitted yet (fractions carry over). */
		float Owed = 0.f;
		FVector LastLocation = FVector::ZeroVector;
		bool bHasLastLocation = false;
	};

	void StartSmokeStream(const FGunSmokePuffs& Puffs, FName Bone, const FVector& Offset, const FVector& Direction, float Strength);
	void UpdateSmoke(float DeltaTime);

	/** Smoke still inside the gun from the last shots (1 = right after a shot), cleared over BreechSmokeClearTime. */
	float GetBreechResidue() const;

	UPROPERTY(Transient)
	TObjectPtr<AGunSmoke> Smoke;

	TArray<FSmokeStream> SmokeStreams;

	/** Residue at ResidueTime (world seconds); it clears from there. */
	float BreechResidue = 0.f;
	double ResidueTime = 0.0;

	/** The eye's motion (cm/s), carried along by fresh smoke (the balloon flying, the player moving). */
	FVector CameraVelocity = FVector::ZeroVector;
	FVector LastCameraLocation = FVector::ZeroVector;
	bool bHasCameraLocation = false;

	/** Material given to spawned casings, so they match the textured gun. */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> CasingMaterial;

	bool bEquipped = true;

	/** World time of the last trigger pull that started a firing cycle (RateOfFire). */
	double LastFireCycleTime = -1.0e9;

	/**
	 * World transform of the ejected case at the last two samples. The mesh evaluates after this
	 * component, so a sample taken in a tick shows the pose from the tick before; the two samples
	 * are therefore one previous tick (CaseSampleDeltaTime) apart.
	 */
	FTransform LastCaseTransform;
	FTransform PrevCaseTransform;
	int32 NumCaseSamples = 0;
	float CaseSampleDeltaTime = 0.f;

	/** Delta time of the previous tick (the pose a sample reads was built then). */
	float PrevTickDeltaTime = 0.f;
	float CurrentTickDeltaTime = 0.f;
};
