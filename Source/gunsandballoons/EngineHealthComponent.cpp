#include "EngineHealthComponent.h"

#include "BalloonFlightComponent.h"
#include "BalloonRunSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

UEngineHealthComponent::UEngineHealthComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UEngineHealthComponent::BeginPlay()
{
	Super::BeginPlay();
	Health = MaxHealth;
	if (AActor* Owner = GetOwner())
	{
		Owner->OnTakeAnyDamage.AddUniqueDynamic(this, &UEngineHealthComponent::HandleTakeAnyDamage);
	}
}

void UEngineHealthComponent::HandleTakeAnyDamage(AActor* DamagedActor, float Damage, const UDamageType* DamageType, AController* InstigatedBy, AActor* DamageCauser)
{
	ApplyEngineDamage(Damage);
}

void UEngineHealthComponent::ApplyEngineDamage(float Amount)
{
	if (Amount <= 0.f || IsDead()) return;
	if (bInvulnerable)
	{
		UE_LOG(LogTemp, Log, TEXT("Engine hit for %.0f (invulnerable)"), Amount);
		return;
	}
	const float Old = Health;
	Health = FMath::Max(0.f, Health - Amount);
	OnHealthChanged.Broadcast(Health, Health - Old);
	UE_LOG(LogTemp, Log, TEXT("Engine health %.0f / %.0f (-%.0f)"), Health, MaxHealth, Old - Health);
	if (IsDead()) Die();
}

void UEngineHealthComponent::Kill()
{
	if (IsDead()) return;
	if (bInvulnerable)
	{
		UE_LOG(LogTemp, Log, TEXT("Engine kill ignored (invulnerable)"));
		return;
	}
	const float Old = Health;
	Health = 0.f;
	OnHealthChanged.Broadcast(Health, -Old);
	Die();
}

void UEngineHealthComponent::Die()
{
	UE_LOG(LogTemp, Warning, TEXT("Engine destroyed: run over"));
	OnDestroyed.Broadcast();

	if (UBalloonFlightComponent* Flight = GetOwner()->FindComponentByClass<UBalloonFlightComponent>())
	{
		Flight->SetComponentTickEnabled(false);
	}
	if (bRestartLevelOnDeath)
	{
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, RestartDelay, FColor::Red, TEXT("ENGINE DESTROYED"), true, FVector2D(3.f, 3.f));
		GetWorld()->GetTimerManager().SetTimer(RestartTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			// During a run: back to the shop at the last checkpoint. Otherwise (a level played on its own) restart it.
			UBalloonRunSubsystem* Run = UBalloonRunSubsystem::Get(this);
			if (!Run || !Run->FailLevel(this))
			{
				UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this)));
			}
		}), RestartDelay, false);
	}
}
