#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "EngineHealthComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEngineHealthChanged, float, Health, float, Delta);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnEngineDestroyed);

/**
 * Balloon health. Enemy attacks hit the basket, but the damage is taken by the engine
 * (engine-stove.md: "the engine body is the balloon's health"). Lives on the balloon actor and
 * listens to the actor's OnTakeAnyDamage, so any ApplyDamage on the balloon lands here.
 */
UCLASS(ClassGroup = (Balloon), meta = (BlueprintSpawnableComponent))
class GUNSANDBALLOONS_API UEngineHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UEngineHealthComponent();

	UFUNCTION(BlueprintCallable, Category = "Balloon|Health")
	void ApplyEngineDamage(float Amount);

	/** Instant death (nemesis turrets). */
	UFUNCTION(BlueprintCallable, Category = "Balloon|Health")
	void Kill();

	UFUNCTION(BlueprintPure, Category = "Balloon|Health")
	float GetHealth() const { return Health; }

	UFUNCTION(BlueprintPure, Category = "Balloon|Health")
	float GetHealth01() const { return MaxHealth > 0.f ? Health / MaxHealth : 0.f; }

	UFUNCTION(BlueprintPure, Category = "Balloon|Health")
	bool IsDead() const { return Health <= 0.f; }

	/** Testing/demo: hits are logged but take no health, Kill() is ignored. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Balloon|Health|Debug")
	bool bInvulnerable = false;

	UPROPERTY(BlueprintAssignable, Category = "Balloon|Health")
	FOnEngineHealthChanged OnHealthChanged;

	UPROPERTY(BlueprintAssignable, Category = "Balloon|Health")
	FOnEngineDestroyed OnDestroyed;

protected:
	virtual void BeginPlay() override;

	/** TBD: prototype value, the design has no engine HP yet. */
	UPROPERTY(EditAnywhere, Category = "Balloon|Health", meta = (ClampMin = "1"))
	float MaxHealth = 100.f;

	/** Until there is a game-over screen: show a message and reload the level. */
	UPROPERTY(EditAnywhere, Category = "Balloon|Health")
	bool bRestartLevelOnDeath = true;

	UPROPERTY(EditAnywhere, Category = "Balloon|Health", meta = (EditCondition = "bRestartLevelOnDeath"))
	float RestartDelay = 3.f;

private:
	UFUNCTION()
	void HandleTakeAnyDamage(AActor* DamagedActor, float Damage, const UDamageType* DamageType, AController* InstigatedBy, AActor* DamageCauser);

	void Die();

	float Health = 0.f;
	FTimerHandle RestartTimer;
};
