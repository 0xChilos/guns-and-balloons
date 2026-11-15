#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GunSmoke.h"
#include "StoveVfxComponent.generated.h"

class UStoveComponent;
class UStoveFuelType;

/** PS1-style fuel feedback and sustained overheat smoke, following the stove's events. */
UCLASS(ClassGroup=(Stove), meta=(BlueprintSpawnableComponent))
class GUNSANDBALLOONS_API UStoveVfxComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UStoveVfxComponent();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stove|VFX") TSubclassOf<AGunSmoke> EffectClass;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stove|VFX") FGunSmokePuffs FuelBurst;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stove|VFX") FGunSmokePuffs OverheatSmoke;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stove|VFX") FLinearColor FuelColor = FLinearColor(3.f, .65f, .06f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stove|VFX") FLinearColor OverheatColor = FLinearColor(.22f, .18f, .15f);
	/** Offset in mesh centimeters from the stove bone, unaffected by the imported bone scale. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stove|VFX") FVector OverheatOffset = FVector(0.f, 0.f, 100.f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stove|VFX") FName StoveBone = TEXT("stove");
	UFUNCTION(BlueprintPure, Category="Stove|VFX") AGunSmoke* GetFuelEffect() const { return FuelEffect; }
	UFUNCTION(BlueprintPure, Category="Stove|VFX") AGunSmoke* GetOverheatEffect() const { return HeatEffect; }
private:
	UPROPERTY(Transient) TObjectPtr<UStoveComponent> Stove;
	UPROPERTY(Transient) TObjectPtr<AGunSmoke> FuelEffect;
	UPROPERTY(Transient) TObjectPtr<AGunSmoke> HeatEffect;
	float EmissionRemainder = 0.f;
	UFUNCTION() void FuelAdded(const UStoveFuelType* Fuel);
	UFUNCTION() void OverheatChanged(bool bEnabled);
	AGunSmoke* EnsureEffect(TObjectPtr<AGunSmoke>& Effect, const FLinearColor& Color);
	FVector GetOverheatLocation() const;
	FVector GetSourceVelocity() const;
	void EmitOverheat(int32 Count);
};
