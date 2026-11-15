#include "HealthComponent.h"
#include "GameFramework/Actor.h"

UHealthComponent::UHealthComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UHealthComponent::BeginPlay()
{
	Super::BeginPlay();
	MaxHealth = FMath::IsFinite(MaxHealth) ? FMath::Max(1.f, MaxHealth) : 100.f;
	Health = MaxHealth;
	PendingPointDamageEvents = 0;
	if (bListenToActorDamage && GetOwner())
	{
		GetOwner()->OnTakePointDamage.AddUniqueDynamic(this, &UHealthComponent::HandlePointDamage);
		GetOwner()->OnTakeAnyDamage.AddUniqueDynamic(this, &UHealthComponent::HandleAnyDamage);
	}
}

void UHealthComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (AActor* Owner = GetOwner())
	{
		Owner->OnTakePointDamage.RemoveDynamic(this, &UHealthComponent::HandlePointDamage);
		Owner->OnTakeAnyDamage.RemoveDynamic(this, &UHealthComponent::HandleAnyDamage);
	}
	PendingPointDamageEvents = 0;
	Super::EndPlay(EndPlayReason);
}

float UHealthComponent::ApplyDamage(float Amount, AActor* DamageCauser)
{
	if (!FMath::IsFinite(Amount) || Amount <= 0.f || IsDepleted()) return 0.f;
	const float Applied = FMath::Min(Amount, Health);
	SetHealth(Health - Applied);
	return Applied;
}

float UHealthComponent::Repair(float Amount, AActor* Repairer)
{
	if (!FMath::IsFinite(Amount) || Amount <= 0.f) return 0.f;
	const float Restored = FMath::Max(0.f, FMath::Min(Amount, MaxHealth - Health));
	SetHealth(Health + Restored);
	return Restored;
}

void UHealthComponent::SetHealth(float NewHealth)
{
	if (!FMath::IsFinite(NewHealth)) return;
	NewHealth = FMath::Clamp(NewHealth, 0.f, MaxHealth);
	const float OldHealth = Health;
	if (NewHealth == OldHealth) return;
	Health = NewHealth;
	OnHealthChanged.Broadcast(this, Health, Health - OldHealth);
	if (OldHealth > 0.f && NewHealth == 0.f) OnDepleted.Broadcast(this);
	else if (OldHealth == 0.f && NewHealth > 0.f) OnRestored.Broadcast(this);
}

float UHealthComponent::GetHealth01() const
{
	return MaxHealth > 0.f ? FMath::Clamp(Health / MaxHealth, 0.f, 1.f) : 0.f;
}

void UHealthComponent::HandlePointDamage(AActor* DamagedActor, float Damage, AController* InstigatedBy,
	FVector HitLocation, UPrimitiveComponent* HitComponent, FName BoneName,
	FVector ShotFromDirection, const UDamageType* DamageType, AActor* DamageCauser)
{
	++PendingPointDamageEvents;
	if (DamageBones.IsEmpty() || DamageBones.Contains(BoneName)) ApplyDamage(Damage, DamageCauser);
}

void UHealthComponent::HandleAnyDamage(AActor* DamagedActor, float Damage, const UDamageType* DamageType,
	AController* InstigatedBy, AActor* DamageCauser)
{
	if (PendingPointDamageEvents > 0)
	{
		--PendingPointDamageEvents;
		return;
	}
	ApplyDamage(Damage, DamageCauser);
}
