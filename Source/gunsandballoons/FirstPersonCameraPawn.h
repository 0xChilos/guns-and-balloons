#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Templates/SubclassOf.h"
#include "FirstPersonCameraPawn.generated.h"

class UCameraComponent;
class UPlayerInteractionComponent;
class FHookDebugMenu;
class UGunComponent;
class UExtinguisherComponent;
class UFuelHandComponent;
class UHookComponent;
class UWrenchComponent;
class URevolverComponent;
class URifleComponent;
class UShotgunComponent;
class USkeletalMeshComponent;
class UWeaponRackComponent;

/** What the player's hand holds: an item, or nothing (the items lie on their weapon racks), and the moves between. */
UENUM(BlueprintType)
enum class EPlayerHandState : uint8 { Armed, Holstering, Empty, Drawing };

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnHandStateChanged, EPlayerHandState, HandState);

/**
 * A bodiless, stationary first-person camera.
 * - No body, no collision, no movement component: WASD/any move input does nothing.
 * - Mouse X/Y rotates the view (yaw/pitch). The actor itself never moves or rotates.
 * - Needs no Input Action / Mapping Context assets: it reads the mouse delta directly.
 * - Holds a hand-less weapon viewmodel (Dusk style): the equipped gun floats bottom-right in front
 *   of the eyes, swings up into view on possession, lags behind fast look movement, breathes slightly
 *   while idle, kicks when fired and turns its loading port toward the eye while reloading.
 *   Where each gun sits and how hard it kicks is set on the gun component (UGunComponent).
 * - Three guns, three tools and a fuel item: Pax revolver (SK_Pax), Martini-Henry rifle (SK_MartiniHenry),
 *   Homestead 78 double-barrel shotgun (SK_Homestead78), grappling hook (SK_Hook, UHookComponent),
 *   fire extinguisher (SK_Extinguisher, UExtinguisherComponent; sprays while LMB is held), wrench
 *   (SK_Wrench, UWrenchComponent; turns while LMB is held) and fuel (UFuelHandComponent; carried from
 *   buckets to the stove).
 * - The hand starts empty and holds one item at a time. Each item lies on its own place in the world
 *   (UWeaponRackComponent: the basket's hooks, an AHandItemPickup): LMB on it takes the item, LMB on its
 *   empty place puts it back, LMB on another item swaps. There are no weapon keys. An item being put
 *   away first finishes and closes up, is lowered out of view, then the next one is raised.
 * - RMB held (not on an ammo supply) goes to the gun in hand as SetSecondaryHeld: the hook pumps with it.
 *   With a gun (CanAimDownSights) it also aims down the sights: the gun lifts to the eye line and the view
 *   zooms in (procedural, no clips), unless the gun is open for loading.
 *   H toggles the hook's debug menu (FHookDebugMenu).
 * - LMB interacts with the IInteractable under the crosshair, otherwise fires; held after that click it goes
 *   to the gun as SetTriggerHeld (the extinguisher sprays with it).
 *   There is no reload key: reloading is RMB on an ammo supply (UAmmoSupplyComponent), one round per click.
 * - With an empty hand LMB grabs and drags IGrabbable controls (the balloon's levers) instead of firing.
 * - Riding on the balloon, the view turns along with it.
 */
UCLASS(Blueprintable)
class GUNSANDBALLOONS_API AFirstPersonCameraPawn : public APawn
{
	GENERATED_BODY()

public:
	AFirstPersonCameraPawn();

	virtual void Tick(float DeltaSeconds) override;
	/** The camera component's view, with the gun's near clip plane while aiming down the sights. */
	virtual void CalcCamera(float DeltaTime, struct FMinimalViewInfo& OutResult) override;
	virtual void NotifyControllerChanged() override;
	virtual void UnPossessed() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Release held controls and tool input on menu entry, unpossession or shutdown. */
	UFUNCTION(BlueprintCallable, Category="Player|Input")
	void ReleaseGameplayInput();

	UCameraComponent* GetCamera() const { return Camera; }
	USkeletalMeshComponent* GetWeaponMesh() const { return WeaponMesh; }
	USkeletalMeshComponent* GetRifleMesh() const { return RifleMesh; }
	USkeletalMeshComponent* GetShotgunMesh() const { return ShotgunMesh; }
	URevolverComponent* GetRevolver() const { return Revolver; }
	URifleComponent* GetRifle() const { return Rifle; }
	UShotgunComponent* GetShotgun() const { return Shotgun; }
	USkeletalMeshComponent* GetHookMesh() const { return HookMesh; }
	UHookComponent* GetHook() const { return Hook; }
	USkeletalMeshComponent* GetExtinguisherMesh() const { return ExtinguisherMesh; }
	UExtinguisherComponent* GetExtinguisher() const { return Extinguisher; }
	USkeletalMeshComponent* GetWrenchMesh() const { return WrenchMesh; }
	UWrenchComponent* GetWrench() const { return Wrench; }
	UFuelHandComponent* GetFuelHand() const { return FuelHand; }

	/** The item in hand (the one still being put away during a swap); with an empty hand, the one held last. */
	UFUNCTION(BlueprintPure, Category = "Weapon")
	UGunComponent* GetActiveGun() const { return ActiveGun; }

	/** This pawn's item of the given kind (it carries one of each), or null. */
	UFUNCTION(BlueprintPure, Category = "Weapon")
	UGunComponent* FindGun(TSubclassOf<UGunComponent> GunClass) const;

	/** The viewmodel mesh of one of this pawn's items. Works on the class defaults too, unlike UGunComponent::GetMesh. */
	USkeletalMeshComponent* GetGunMesh(const UGunComponent* Gun) const;

	/** The IInteractable actor or ammo supply under the crosshair within InteractRange, or null. */
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	AActor* GetAimedInteractable() const;

	/**
	 * Interact with the IInteractable under the crosshair, unless it is an ammo supply (those are RMB).
	 * Called on LMB. Returns true if the click was used, so the gun does not fire.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	bool TryInteract();

	/**
	 * Take one round for the gun in hand from the ammo supply under the crosshair (interacting with it
	 * too, e.g. opening a crate's lid). Called on RMB. If the gun has a fired case to pull out by hand
	 * (UGunComponent::EjectCase), the click does that instead and takes nothing. Returns true if the gun took the round.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	bool TryTakeRound();

	/**
	 * Put the item in hand on the rack: it finishes what it is doing, is lowered over HolsterTime, then
	 * hidden and shown on the rack. The rack must be empty and made for this kind of item (ItemClass);
	 * not while the item is open for loading or a swap is running.
	 */
	UFUNCTION(BlueprintCallable, Category = "Weapon|Hand")
	bool HolsterWeapon(UWeaponRackComponent* Rack);

	/** Take the rack's item (its rounds and contents kept) and raise it. Needs an empty hand. */
	UFUNCTION(BlueprintCallable, Category = "Weapon|Hand")
	bool DrawWeapon(UWeaponRackComponent* Rack);

	/**
	 * Swap the item in hand for the rack's: the one in hand finishes and closes, is lowered out of view
	 * and goes back to the rack it was taken from, then the rack's item is raised. Needs that rack to be
	 * free still; not while the item in hand is open for loading.
	 */
	UFUNCTION(BlueprintCallable, Category = "Weapon|Hand")
	bool SwapWeapon(UWeaponRackComponent* Rack);

	UFUNCTION(BlueprintPure, Category = "Weapon|Hand")
	EPlayerHandState GetHandState() const { return HandState; }

	UFUNCTION(BlueprintPure, Category = "Weapon|Hand")
	bool IsHandEmpty() const { return HandState == EPlayerHandState::Empty; }

	/** True while LMB is dragging an IGrabbable control (a balloon lever). Read by the crosshair. */
	bool IsGrabbing() const { return GrabTarget.IsValid(); }

	/** Hide and consume the active item immediately, leaving the hand empty without touching racks. */
	UFUNCTION(BlueprintCallable, Category="Weapon|Hand")
	bool ConsumeHeldItem();

	/** Raise a pawn inventory item into an empty hand without taking it from a rack. */
	UFUNCTION(BlueprintCallable, Category="Weapon|Hand")
	bool TakeHandItem(UGunComponent* Item);

	/** How far the gun is lowered out of view: 0 = up, 1 = gone (swap, possession, rack). Read by the crosshair. */
	float GetGunLowered() const { return FMath::Max(FMath::Square(RaiseAlpha), FMath::Square(HandLowerAlpha)); }

	/** Shot kick of the gun in hand: 1 on the shot, easing to 0. Read by the crosshair. */
	float GetRecoilAlpha() const { return RecoilAlpha; }

	/** True while RMB holds the gun in hand up to the eye (it may still be on its way). */
	UFUNCTION(BlueprintPure, Category = "Weapon|Aim Down Sights")
	bool IsAimingDownSights() const { return bWantsAimDownSights; }

	/** How far the gun is raised to the eye: 0 = held at the hip, 1 = aimed down the sights (eased). */
	UFUNCTION(BlueprintPure, Category = "Weapon|Aim Down Sights")
	float GetAimDownSightsAlpha() const { return FMath::SmoothStep(0.f, 1.f, AimAlpha); }

	UPROPERTY(BlueprintAssignable, Category = "Weapon|Hand")
	FOnHandStateChanged OnHandStateChanged;

protected:
	/**
	 * Only Armed and Empty are valid initial states. Armed starts with the StartingGunIndex item in hand;
	 * its rack in the level, if it has one, should then have bStartsWithWeapon off.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Hand")
	EPlayerHandState InitialHandState = EPlayerHandState::Empty;

	/** TBD: prototype duration, needs tuning. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Hand", meta = (ClampMin = "0.0"))
	float HolsterTime = 0.35f;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Weapon|Hand")
	EPlayerHandState HandState = EPlayerHandState::Armed;

	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;

	/** GetAimedInteractable, with the debug line optional (it runs every frame for the loading gate). */
	AActor* TraceAimedTarget(bool bDebugDraw) const;

	/** The interaction trace from the eye along the view. */
	bool TraceAimed(FHitResult& OutHit, bool bDebugDraw) const;

	/** Applies mouse look to the control rotation. */
	void UpdateLook(APlayerController* PC);

	/**
	 * Moves the aim itself (not just the viewmodel): the gun's recoil kick and its partial settle, the
	 * hand sway of the gun in hand and the balloon's sway. The player's mouse fights all of it.
	 */
	void UpdateAimMotion(APlayerController* PC, float DeltaSeconds);

	/** Adds to the control rotation within the look limits (pitch clamp, optional yaw limit). */
	void AddAimRotation(APlayerController* PC, const FRotator& Delta);

	/** Places the weapon in front of the eyes (rest or reload pose), plus raise, look lag, idle breathing and recoil. */
	void UpdateWeaponSway(float DeltaSeconds);

	/** Lowers the item being swapped out, and once it is out of view puts it on its rack and brings out the next one. */
	void UpdateWeaponSwap(float DeltaSeconds);

	/** Inventory order: revolver, rifle, shotgun, hook, extinguisher, wrench, fuel (fuel is never a starting item). */
	TArray<UGunComponent*, TInlineAllocator<7>> GetGuns() const;

	/** Each gun's mesh, in the same order as GetGuns. */
	TArray<USkeletalMeshComponent*, TInlineAllocator<7>> GetGunMeshes() const;

	/** H: open/close the hook debug menu; while open, its arrow keys. */
	void UpdateHookDebugMenu(APlayerController* PC);

	UFUNCTION()
	void HandleWeaponFired();

	/** How far from the camera (cm) the player can interact. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction", meta = (ClampMin = "0.0"))
	float InteractRange = 400.f;

	/** TBD: prototype centimeters per mouse delta unit, needs tuning. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Grab", meta = (ClampMin = "0.0"))
	float GrabDragScale = 0.5f;

	/** TBD: break distance in cm; 0 uses InteractRange * 1.25 (needs tuning). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Grab", meta = (ClampMin = "0.0"))
	float GrabBreakDistance = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	TEnumAsByte<ECollisionChannel> InteractTraceChannel = ECC_Visibility;

	/** Draw the interaction trace for 1s on each click. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	bool bDebugInteractTrace = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interaction")
	TObjectPtr<UPlayerInteractionComponent> Interaction;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UCameraComponent> Camera;

	/** Degrees of rotation per unit of processed mouse delta. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Look", meta = (ClampMin = "0.0"))
	float LookSensitivity = 2.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Look")
	bool bInvertY = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Look", meta = (ClampMin = "-89.9", ClampMax = "0.0"))
	float MinPitch = -89.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Look", meta = (ClampMin = "0.0", ClampMax = "89.9"))
	float MaxPitch = 89.f;

	/** If true, yaw is limited to +/- MaxYawFromStart around the actor's placed facing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Look")
	bool bLimitYaw = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Look", meta = (EditCondition = "bLimitYaw", ClampMin = "0.0", ClampMax = "180.0"))
	float MaxYawFromStart = 90.f;

	// --- Weapon viewmodel ---------------------------------------------------------------

	/** The revolver, attached to the camera. Rendered as a first-person primitive so it never clips into walls. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<USkeletalMeshComponent> WeaponMesh;

	/** Revolver chambers, firing, reloading; drives WeaponMesh's animation. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<URevolverComponent> Revolver;

	/** The rifle, attached to the camera like WeaponMesh. Hidden until taken in hand. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<USkeletalMeshComponent> RifleMesh;

	/** Rifle chamber, firing, reloading; drives RifleMesh's animation. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<URifleComponent> Rifle;

	/** The double-barrel shotgun, attached to the camera like WeaponMesh. Hidden until taken in hand. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<USkeletalMeshComponent> ShotgunMesh;

	/** Shotgun barrels, firing, reloading; drives ShotgunMesh's animation. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<UShotgunComponent> Shotgun;

	/** The grappling hook, attached to the camera like WeaponMesh. Hidden until taken in hand. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<USkeletalMeshComponent> HookMesh;

	/** Grappling hook: pressure, throw, grab, reel in, pump; drives HookMesh's animation. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<UHookComponent> Hook;

	/** The fire extinguisher, attached to the camera like WeaponMesh. Hidden until taken in hand. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<USkeletalMeshComponent> ExtinguisherMesh;

	/** Fire extinguisher: contents counter, foam spray; drives ExtinguisherMesh's animation. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<UExtinguisherComponent> Extinguisher;

	/** The wrench, attached to the camera like WeaponMesh. Hidden until taken in hand. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<USkeletalMeshComponent> WrenchMesh;

	/** Wrench: equip, ready, turning while LMB is held; drives WrenchMesh's animation. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<UWrenchComponent> Wrench;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Weapon")
	TObjectPtr<USkeletalMeshComponent> FuelHandMesh;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Weapon")
	TObjectPtr<UFuelHandComponent> FuelHand;


	/** Item in hand at BeginPlay when InitialHandState is Armed: 0 = revolver, 1 = rifle, 2 = shotgun, 3 = hook, 4 = extinguisher, 5 = wrench. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon", meta = (ClampMin = "0", ClampMax = "5"))
	int32 StartingGunIndex = 0;

	/** Seconds to lower an item out of view when swapping. Raising the next one takes RaiseTime. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Raise", meta = (ClampMin = "0.0"))
	float LowerTime = 0.25f;

	/** Seconds to move into or out of the reload pose. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Reload Pose", meta = (ClampMin = "0.01"))
	float ReloadPoseBlendTime = 0.25f;

	/** Seconds for the weapon to swing up into view when the pawn is possessed. 0 disables it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Raise", meta = (ClampMin = "0.0"))
	float RaiseTime = 0.4f;

	/** How far below the rest position (cm) the weapon starts its raise. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Raise", meta = (ClampMin = "0.0"))
	float RaiseDrop = 20.f;

	/** How far (degrees) the muzzle is tipped down at the start of the raise. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Raise", meta = (ClampMin = "0.0"))
	float RaisePitch = 35.f;

	/** Degrees the weapon lags per degree/second of look speed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Sway", meta = (ClampMin = "0.0"))
	float SwayAmount = 0.012f;

	/** Largest lag angle (degrees). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Sway", meta = (ClampMin = "0.0"))
	float MaxSwayAngle = 5.f;

	/** How fast the weapon catches up with the view. Higher = snappier. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Sway", meta = (ClampMin = "0.0"))
	float SwayReturnSpeed = 9.f;

	/** How far (cm) the weapon slides sideways per degree of lag. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Sway", meta = (ClampMin = "0.0"))
	float SwayShift = 0.15f;

	/** Idle breathing height (cm). 0 disables it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Sway", meta = (ClampMin = "0.0"))
	float IdleBobAmount = 0.25f;

	/** Idle breathing speed (radians/second). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Sway", meta = (ClampMin = "0.0"))
	float IdleBobSpeed = 1.6f;

	/** How fast the gun settles after a shot (the kick itself is set per gun). Higher = snappier. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Recoil", meta = (ClampMin = "0.0"))
	float RecoilRecoverySpeed = 12.f;

	/** How fast a gun's aim kick (AimRecoilPitch/Yaw) is applied. Higher = sharper snap. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Recoil", meta = (ClampMin = "1.0"))
	float AimKickSpeed = 30.f;

	// --- Aim down sights ------------------------------------------------------------------
	// RMB held with a gun in hand (not on an ammo supply): the gun moves to its AimViewOffset and the view
	// zooms to its AimFieldOfView, over its AimTime. Visual only: spread and sway are unchanged.
	// All values TBD, tune in game.

	/** Share of the look lag (weapon trailing the view) left while aimed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Aim Down Sights", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float AimSwayScale = 0.3f;

	/** Share of the idle breathing left while aimed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Aim Down Sights", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float AimBobScale = 0.25f;

	/**
	 * Share of the viewmodel's shot kick (RecoilKickBack and RecoilKickPitch) left while aimed, so the sights
	 * come back sooner and a long gun's stock is not thrown back into the near plane.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Aim Down Sights", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float AimKickScale = 0.6f;

	/** How far (cm) the gun dips halfway between hip and eye, so the move reads as a lift rather than a slide. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Aim Down Sights", meta = (ClampMin = "0.0"))
	float AimTransitionDip = 1.f;

	/** How far (degrees) the gun rolls inward halfway between hip and eye. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Aim Down Sights")
	float AimTransitionRoll = -6.f;

	/** Slow the mouse look by the zoom while aimed, so the screen moves the same under the cursor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Aim Down Sights")
	bool bScaleLookWithAimZoom = true;

	// --- Balloon sway ---------------------------------------------------------------------
	// The basket rocking under the player, added to the aim on top of the gun's hand sway. Placeholder
	// motion for now: once the balloon flies, drive it from the basket's real movement.

	/** How far (degrees) the basket rocks the aim up/down. 0 turns that axis off. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aim|Balloon Sway", meta = (ClampMin = "0.0", Units = "Degrees"))
	float BalloonSwayPitch = 1.f;

	/** How far (degrees) the basket turns the aim left/right. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aim|Balloon Sway", meta = (ClampMin = "0.0", Units = "Degrees"))
	float BalloonSwayYaw = 0.8f;

	/** How far (degrees) the basket tilts the horizon. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aim|Balloon Sway", meta = (ClampMin = "0.0", Units = "Degrees"))
	float BalloonSwayRoll = 1.5f;

	/** Balloon sway speed: roughly how many direction changes per second (slow and heavy). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aim|Balloon Sway", meta = (ClampMin = "0.0"))
	float BalloonSwaySpeed = 0.12f;

	/** How fast the hand sway blends to a new gun's amount after a swap. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aim", meta = (ClampMin = "0.0"))
	float HandSwayBlendSpeed = 2.f;

private:
	void RaiseHandItem(UGunComponent* Item, UWeaponRackComponent* Rack);
	friend class FPlayerGrabTest;
	friend class FPlayerArchitectureTest;
	friend class FHandItemTest;
	friend class FStoveInteractionTest;

	/** Starts dragging the IGrabbable under the crosshair (empty hand only). True if the click was used. */
	bool TryBeginGrab();
	/** Moves the held control with the mouse; lets go when LMB is released or the player is too far. */
	void UpdateHeldGrab(APlayerController* PC);
	void EndGrab();
	TWeakObjectPtr<UObject> GrabTarget;

	void SetHandState(EPlayerHandState NewState);
	/** Lowers the item being put away once it is at rest, and finishes holstering/drawing. */
	void UpdateHandState(float DeltaSeconds);
	/** The rack of the move under way: where a holstered item is going, or where a swap takes the next one from. */
	TWeakObjectPtr<UWeaponRackComponent> TransitionRack;
	/** The rack the item in hand was taken from: a swap puts it back there. */
	TWeakObjectPtr<UWeaponRackComponent> HeldItemRack;
	/** 0 = in hand, 1 = lowered out of view for the rack (linear; eased when applied). */
	float HandLowerAlpha = 0.f;

	/** Ensure the balloon moves before the rider reads its transform. */
	void SetupBalloonTickPrerequisite();
	/** Parent actor's yaw last frame; the view carries along with the balloon's turning. */
	float LastParentYaw = 0.f;
	bool bHasParentYaw = false;

	/** The item in hand; with an empty hand, the hidden one held last (never null after BeginPlay). */
	UPROPERTY(Transient)
	TObjectPtr<UGunComponent> ActiveGun;

	/** The item a swap is bringing out; null when not swapping. */
	UPROPERTY(Transient)
	TObjectPtr<UGunComponent> PendingGun;

	/** LMB still held since the click that pulled the trigger (the extinguisher sprays while it is). */
	bool bTriggerHeld = false;

	float RecoilAlpha = 0.f;
	/** 0 = rest pose, 1 = reload pose (linear; eased when applied). */
	float ReloadPoseAlpha = 0.f;
	/** 1 = fully lowered, 0 = raised (linear; eased when applied). */
	float RaiseAlpha = 0.f;
	/** RMB is holding the gun in hand up to the eye. */
	bool bWantsAimDownSights = false;
	/** 0 = at the hip, 1 = aimed down the sights (linear; eased when applied). */
	float AimAlpha = 0.f;
	/** The camera's field of view when not aimed (taken at BeginPlay). */
	float BaseFieldOfView = 90.f;
	/** The camera's first-person (viewmodel) field of view when not aimed (taken at BeginPlay). */
	float BaseFirstPersonFieldOfView = 80.f;
	FRotator CurrentSway = FRotator::ZeroRotator;
	FRotator LastViewRotation = FRotator::ZeroRotator;
	bool bHasLastViewRotation = false;

	/** Aim kick still to apply, and the share of past kicks still to settle back. */
	FRotator PendingAimKick = FRotator::ZeroRotator;
	FRotator PendingAimSettle = FRotator::ZeroRotator;
	float AimSettleTime = 0.5f;

	/**
	 * Aim sway: position along each noise (advanced by its speed, so a speed change never jumps), the
	 * offset applied so far (only its change is applied each tick), the hand sway amount in use.
	 */
	float HandSwayPhase = 0.f;
	float BalloonSwayPhase = 0.f;
	FRotator AppliedAimSway = FRotator::ZeroRotator;
	bool bHasAppliedAimSway = false;
	FVector2D CurrentHandSway = FVector2D::ZeroVector;
	float CurrentHandSwaySpeed = 0.7f;

	TSharedPtr<FHookDebugMenu> HookDebugMenu;
};
