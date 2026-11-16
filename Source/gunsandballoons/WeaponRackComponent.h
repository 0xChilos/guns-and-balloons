#pragma once
#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Interactable.h"
#include "Templates/SubclassOf.h"
#include "WeaponRackComponent.generated.h"
class UBoxComponent;
class UGunComponent;
class USkeletalMeshComponent;
/**
 * A place where one of the player's hand items lies or hangs: the basket's hooks for the revolver, or a
 * spot in the world (AHandItemPickup). LMB on it with an empty hand takes the item; with that item in
 * hand puts it back; with another item in hand does nothing. Put the held item back before taking another.
 * Visual storage only: rounds, pressure and contents stay in the player's own component of ItemClass.
 */
UCLASS(ClassGroup=(Weapon), meta=(BlueprintSpawnableComponent))
class GUNSANDBALLOONS_API UWeaponRackComponent : public USceneComponent, public IInteractable
{
	GENERATED_BODY()
public:
	UWeaponRackComponent();
	virtual void OnRegister() override;
	virtual void BeginPlay() override;
	virtual bool AcceptsHit_Implementation(const FHitResult& Hit) const override;
	virtual bool CanInteract_Implementation(APawn* InstigatorPawn) const override;
	virtual void Interact_Implementation(APawn* InstigatorPawn) override;
	UFUNCTION(BlueprintPure, Category="Weapon|Rack") bool HasWeapon() const { return bHasWeapon; }
	UFUNCTION(BlueprintCallable, Category="Weapon|Rack") void SetHasWeapon(bool bNewHasWeapon);
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Weapon|Rack") TObjectPtr<USkeletalMeshComponent> DisplayMesh;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Weapon|Rack") TObjectPtr<UBoxComponent> InteractShape;
	/** Which of the player's items belongs here: the class of its component on AFirstPersonCameraPawn (the revolver by default). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon|Rack") TSubclassOf<UGunComponent> ItemClass;
	/**
	 * Socket on the owner's skeletal mesh named "Mesh" (the balloon's SK_Balloon) this place snaps to when it registers,
	 * so it moves with the basket: socket_revolver, socket_rifle, socket_shotgun, socket_tool_vertical_1 (extinguisher),
	 * socket_tool_horizontal (hook), socket_tool_vertical_2 (wrench). None or missing: stays where it is placed.
	 * The place takes only the socket's position and keeps the mesh's axes, so DisplayOffset / DisplayRotation
	 * are in the mesh's space. A place already attached to a socket in its Blueprint keeps that one.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon|Rack") FName AttachSocket = TEXT("socket_small_weapon");
	/** The item is here when play starts. Off: the place is empty until the player puts the item on it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon|Rack") bool bStartsWithWeapon = true;
	/** TBD: fit barrel and grip to WeaponHookSmallL/R in the editor. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon|Rack") FVector DisplayOffset = FVector::ZeroVector;
	/** TBD: fit to the hooks in the editor. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon|Rack") FRotator DisplayRotation = FRotator::ZeroRotator;
	/** Size InteractShape to the item's mesh (plus InteractPadding) instead of keeping the box set by hand. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon|Rack") bool bFitInteractShape = false;
	/** TBD: cm added around the mesh bounds by bFitInteractShape, so a thin item is easy to aim at (needs tuning). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon|Rack", meta=(ClampMin="0.0", EditCondition="bFitInteractShape")) float InteractPadding = 3.f;
	/** TBD: tighter padding for the shotgun pickup area, in cm (needs tuning). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon|Rack", meta=(ClampMin="0.0", EditCondition="bFitInteractShape")) float ShotgunInteractPadding = 0.5f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon|Rack") TEnumAsByte<ECollisionChannel> InteractTraceChannel = ECC_Visibility;
private:
	/** The player's item of ItemClass on the pawn's class defaults: the source of the mesh and its texture. */
	const UGunComponent* GetItemDefaults() const;
	/** Shows ItemClass's mesh and, with bFitInteractShape, sizes the click box to it. */
	void RefreshDisplay();
	UPROPERTY(Transient) bool bHasWeapon = false;
};
