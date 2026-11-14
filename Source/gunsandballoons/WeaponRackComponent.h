#pragma once
#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Interactable.h"
#include "WeaponRackComponent.generated.h"
class UBoxComponent;
class USkeletalMeshComponent;
/** Visual storage; ammunition remains in the player's revolver. */
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
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon|Rack") bool bStartsWithWeapon = false;
	/** TBD: fit barrel and grip to WeaponHookSmallL/R in the editor. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon|Rack") FVector DisplayOffset = FVector::ZeroVector;
	/** TBD: fit to the hooks in the editor. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon|Rack") FRotator DisplayRotation = FRotator::ZeroRotator;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon|Rack") TEnumAsByte<ECollisionChannel> InteractTraceChannel = ECC_Visibility;
private:
	UPROPERTY(Transient) bool bHasWeapon = false;
};
