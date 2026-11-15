#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HealthComponent.generated.h"

class UHealthComponent;
class UDamageType;
class UPrimitiveComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnHealthChanged, UHealthComponent*, Health, float, NewHealth, float, Delta);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnHealthDepleted, UHealthComponent*, Health);

/** Reusable health pool; actor point damage can be restricted to selected bones. */
UCLASS(ClassGroup=(Gameplay), meta=(BlueprintSpawnableComponent))
class GUNSANDBALLOONS_API UHealthComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UHealthComponent();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Health", meta=(ClampMin="1"))
	float MaxHealth = 100.f; // TBD: prototype value
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Health")
	bool bListenToActorDamage = true;
	/** Empty accepts all point hits; otherwise only these bones accept point damage. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Health")
	TArray<FName> DamageBones;

	UFUNCTION(BlueprintCallable, Category="Health")
	float ApplyDamage(float Amount, AActor* DamageCauser = nullptr);
	UFUNCTION(BlueprintCallable, Category="Health")
	float Repair(float Amount, AActor* Repairer = nullptr);
	/** Non-finite values are ignored. Finite values are clamped. */
	UFUNCTION(BlueprintCallable, Category="Health")
	void SetHealth(float NewHealth);
	UFUNCTION(BlueprintPure, Category="Health")
	float GetHealth() const { return Health; }
	UFUNCTION(BlueprintPure, Category="Health")
	float GetHealth01() const;
	UFUNCTION(BlueprintPure, Category="Health")
	bool IsDepleted() const { return Health <= 0.f; }

	UPROPERTY(BlueprintAssignable, Category="Health")
	FOnHealthChanged OnHealthChanged;
	UPROPERTY(BlueprintAssignable, Category="Health")
	FOnHealthDepleted OnDepleted;
	UPROPERTY(BlueprintAssignable, Category="Health")
	FOnHealthDepleted OnRestored;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	friend class FHealthTest;
	UPROPERTY(Transient)
	float Health = 100.f;
	// AActor::TakeDamage broadcasts point damage before its matching any-damage event.
	int32 PendingPointDamageEvents = 0;

	UFUNCTION()
	void HandleAnyDamage(AActor* DamagedActor, float Damage, const UDamageType* DamageType,
		AController* InstigatedBy, AActor* DamageCauser);
	UFUNCTION()
	void HandlePointDamage(AActor* DamagedActor, float Damage, AController* InstigatedBy,
		FVector HitLocation, UPrimitiveComponent* HitComponent, FName BoneName,
		FVector ShotFromDirection, const UDamageType* DamageType, AActor* DamageCauser);
};
