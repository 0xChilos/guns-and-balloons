#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Interactable.h"
#include "StoveComponent.generated.h"

class UHealthComponent;
class UBalloonFlightComponent;
class UStoveFuelType;
class UPrimitiveComponent;
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FStoveFuelAdded, const UStoveFuelType*, Fuel);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FStoveEvent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FStoveStateChanged, bool, bEnabled);

/** Stove simulation and door/firebox interaction. */
UCLASS(ClassGroup=(Balloon), meta=(BlueprintSpawnableComponent))
class GUNSANDBALLOONS_API UStoveComponent : public UActorComponent, public IInteractable
{
	GENERATED_BODY()
public:
	UStoveComponent();
	UFUNCTION(BlueprintPure, Category="Stove") static UStoveComponent* FindStove(AActor* Actor);
	virtual bool AcceptsHit_Implementation(const FHitResult& Hit) const override;
	virtual bool CanInteract_Implementation(APawn* InstigatorPawn) const override { return true; }
	virtual void Interact_Implementation(APawn* InstigatorPawn) override;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stove|Interaction") FName DoorBone = TEXT("furnace_door");
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stove|Interaction") TObjectPtr<UPrimitiveComponent> FireboxShape;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stove|Interaction") FName FireboxComponentName = TEXT("StoveFirebox");
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stove|Interaction") TEnumAsByte<ECollisionChannel> InteractTraceChannel = ECC_Visibility;
	/** Degrees from Balloon_build.py: G0, GSW = -120, 240. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stove|Animation") float GaugeSweepDeg = 240.f;
	UFUNCTION(BlueprintPure, Category="Stove") float GetGaugeAngle() const { return GetTemperature01() * GaugeSweepDeg; }
	/** Rotation in gauge parent space; derives the normal from the imported reference skeleton. */
	UFUNCTION(BlueprintPure, Category="Stove") FRotator GetGaugeRotation() const;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stove") TObjectPtr<UHealthComponent> Health;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stove") TObjectPtr<UBalloonFlightComponent> Flight;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stove", meta=(ClampMin="0")) float HeatRate = 5.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stove", meta=(ClampMin="0")) float DecayBurning = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stove", meta=(ClampMin="0")) float DecayEmpty = 5.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stove", meta=(ClampMin="0")) float DecayPause = 10.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stove", meta=(ClampMin="0")) float IgnitionDelay = 10.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stove", meta=(ClampMin="0")) float MaxBurnTime = 600.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stove") bool bClampBurnTimeOnOverfill = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stove", meta=(ClampMin="0")) float OverheatThreshold = 90.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stove", meta=(ClampMin="0")) float OverheatDamagePerSecond = 2.f; // TBD: prototype value
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stove", meta=(ClampMin="1")) float DoorOpenDecayMultiplier = 2.f; // TBD: prototype value
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stove", meta=(ClampMin="0")) float MaxTemperature = 120.f; // TBD: prototype value
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stove", meta=(ClampMin="0.001")) float GaugeMaxTemperature = 100.f; // TBD: prototype value
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stove") float SpeedMultAt90 = 1.6f; // TBD: prototype value
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stove") float SpeedMultOverheat = 2.f; // TBD: prototype value
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stove") float DoorOpenAngle = 100.f; // TBD: prototype value
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stove", meta=(ClampMin="0")) float DoorSpeed = 300.f; // TBD: prototype value
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stove", meta=(ClampMin="0")) float FlightModifierEpsilon = .001f;

	UFUNCTION(BlueprintCallable, Category="Stove") bool AddFuel(const UStoveFuelType* Fuel);
	/** Removes heat and the same amount of pending heating; fuel time is preserved. Returns heat removed. */
	UFUNCTION(BlueprintCallable, Category="Stove") float ApplyCooling(float Degrees);
	/** Spray can cool the stove body, door or configured firebox, not unrelated basket parts. */
	UFUNCTION(BlueprintPure, Category="Stove") bool AcceptsCoolingHit(const FHitResult& Hit) const;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stove|Cooling") TArray<FName> CoolingBones = {TEXT("stove"), TEXT("furnace_door")};
	UFUNCTION(BlueprintPure, Category="Stove") bool CanAddFuel(const UStoveFuelType* Fuel) const;
	UFUNCTION(BlueprintCallable, Category="Stove") void SetDoorOpen(bool bOpen);
	UFUNCTION(BlueprintCallable, Category="Stove") void ToggleDoor() { SetDoorOpen(!bDoorOpen); }
	UFUNCTION(BlueprintPure, Category="Stove") bool IsDoorOpen() const { return bDoorOpen; }
	UFUNCTION(BlueprintPure, Category="Stove") float GetTemperature() const { return Temperature; }
	UFUNCTION(BlueprintPure, Category="Stove") float GetTemperature01() const;
	UFUNCTION(BlueprintPure, Category="Stove") float GetBurnTimeLeft() const { return BurnTimeLeft; }
	UFUNCTION(BlueprintPure, Category="Stove") float GetBurnTime01() const;
	UFUNCTION(BlueprintPure, Category="Stove") float GetDoorAngle() const { return DoorAngle; }
	UFUNCTION(BlueprintPure, Category="Stove") bool IsOverheated() const { return bOverheated; }
	UFUNCTION(BlueprintPure, Category="Stove") bool IsLit() const { return Temperature > 0.f && !bIgniting; }
	UFUNCTION(BlueprintPure, Category="Stove") float GetSpeedMultiplier() const;

	UPROPERTY(BlueprintAssignable, Category="Stove") FStoveFuelAdded OnFuelAdded;
	UPROPERTY(BlueprintAssignable, Category="Stove") FStoveEvent OnIgnited;
	UPROPERTY(BlueprintAssignable, Category="Stove") FStoveEvent OnExtinguished;
	UPROPERTY(BlueprintAssignable, Category="Stove") FStoveStateChanged OnOverheatChanged;
	UPROPERTY(BlueprintAssignable, Category="Stove") FStoveStateChanged OnDoorChanged;
private:
	enum class EHitPart : uint8 { None, Door, Firebox };
	mutable EHitPart LastAcceptedPart = EHitPart::None;
	void UpdateOutputs(float DeltaTime);
	float Temperature = 0.f;
	float TargetTemperature = 0.f;
	float BurnTimeLeft = 0.f;
	float DecayPauseLeft = 0.f;
	float IgnitionDelayLeft = 0.f;
	float ActiveHeatRateOverride = 0.f;
	bool bHeatingUsesDefaultRate = true;
	float DoorAngle = 0.f;
	float LastPublishedMult = 1.f;
	bool bIgniting = false;
	bool bDoorOpen = false;
	bool bOverheated = false;
	bool bPublishedModifier = false;
};
