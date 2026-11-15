#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GunSmoke.h"
#include "StoveVfxComponent.generated.h"

class UStoveComponent;
class UStoveFuelType;
class UHealthComponent;
class USkeletalMeshComponent;

/** PS1-style fuel feedback, sustained overheat smoke and smoke from visible damage holes, following the stove's events. */
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
	/** Thin smoke stream from each visible damage hole; Amount = puffs per second per hole. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stove|VFX") FGunSmokePuffs DamageSmoke;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stove|VFX") FLinearColor DamageColor = FLinearColor(.16f, .14f, .13f);
	/** Damage-hole bones are DamageBonePrefix + 1..MaxDamageHoles; a hole smokes while its bone is not hidden. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stove|VFX") FString DamageBonePrefix = TEXT("stove_damage_");
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stove|VFX", meta=(ClampMin="0")) int32 MaxDamageHoles = 10;
	UFUNCTION(BlueprintPure, Category="Stove|VFX") AGunSmoke* GetFuelEffect() const { return FuelEffect; }
	UFUNCTION(BlueprintPure, Category="Stove|VFX") AGunSmoke* GetOverheatEffect() const { return HeatEffect; }
	UFUNCTION(BlueprintPure, Category="Stove|VFX") AGunSmoke* GetDamageEffect() const { return DamageEffect; }
private:
	UPROPERTY(Transient) TObjectPtr<UStoveComponent> Stove;
	UPROPERTY(Transient) TObjectPtr<UHealthComponent> Health;
	UPROPERTY(Transient) TObjectPtr<AGunSmoke> FuelEffect;
	UPROPERTY(Transient) TObjectPtr<AGunSmoke> HeatEffect;
	UPROPERTY(Transient) TObjectPtr<AGunSmoke> DamageEffect;
	float EmissionRemainder = 0.f;
	float DamageRemainder = 0.f;
	int32 DamageCursor = 0;
	UFUNCTION() void FuelAdded(const UStoveFuelType* Fuel);
	UFUNCTION() void OverheatChanged(bool bEnabled);
	UFUNCTION() void HealthChanged(UHealthComponent* InHealth, float NewHealth, float Delta);
	void UpdateTickEnabled();
	bool IsDamaged() const;
	USkeletalMeshComponent* GetMesh() const;
	void EmitDamage(float DeltaTime);
	AGunSmoke* EnsureEffect(TObjectPtr<AGunSmoke>& Effect, const FLinearColor& Color);
	FVector GetOverheatLocation() const;
	FVector GetSourceVelocity() const;
	void EmitOverheat(int32 Count);
};
