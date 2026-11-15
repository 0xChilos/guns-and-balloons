#include "EngineHealthComponent.h"

#include "BalloonFlightComponent.h"
#include "BalloonPlayerController.h"
#include "BalloonRunSubsystem.h"
#include "NemesisAirship.h"
#include "NemesisBirdFlock.h"
#include "NemesisBomber.h"
#include "NemesisBullet.h"
#include "NemesisCropDuster.h"
#include "NemesisFlak.h"
#include "NemesisHomingMissile.h"
#include "NemesisMine.h"
#include "NemesisRocket.h"
#include "NemesisTowerCannon.h"
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
	if (Damage > 0.f && !IsDead() && DamageCauser) LastCauser = DamageCauser;
	ApplyEngineDamage(Damage);
}

#define LOCTEXT_NAMESPACE "EngineHealth"

FText UEngineHealthComponent::GetDeathCause() const
{
	// Walk up the owners: a bullet is owned by the tower or crop duster that fired it.
	for (const AActor* A = LastCauser.Get(); A; A = A->GetOwner())
	{
		if (A->IsA<ANemesisRocket>()) return LOCTEXT("Rocket", "HIT BY THE AIRSHIP'S ROCKET");
		if (A->IsA<ANemesisHomingMissile>()) return LOCTEXT("Missile", "HIT BY AN AA MISSILE");
		if (A->IsA<ANemesisMine>()) return LOCTEXT("Mine", "BLOWN UP BY A PARACHUTE MINE");
		if (A->IsA<ANemesisBirdFlock>()) return LOCTEXT("Birds", "BROUGHT DOWN BY A FLOCK OF BIRDS");
		if (A->IsA<ANemesisTowerCannon>()) return LOCTEXT("Tower", "SHOT DOWN BY A TOWER CANNON");
		if (A->IsA<ANemesisCropDuster>()) return LOCTEXT("CropDuster", "SHOT DOWN BY A CROP DUSTER");
		if (A->IsA<ANemesisBomber>()) return LOCTEXT("Bomber", "BOMBED BY THE AIRSHIP'S BOMBER");
		if (A->IsA<ANemesisFlak>()) return LOCTEXT("Flak", "SHOT DOWN BY AN AA GUN");
		if (A->IsA<ANemesisAirship>()) return LOCTEXT("Airship", "CAUGHT BY THE NEMESIS AIRSHIP");
		if (A->IsA<ANemesisBullet>()) continue; // its owner is checked next
	}
	return LOCTEXT("Unknown", "THE ENGINE GAVE OUT");
}

#undef LOCTEXT_NAMESPACE

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

void UEngineHealthComponent::Kill(AActor* Causer)
{
	if (IsDead()) return;
	if (bInvulnerable)
	{
		UE_LOG(LogTemp, Log, TEXT("Engine kill ignored (invulnerable)"));
		return;
	}
	if (Causer) LastCauser = Causer;
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
	// The player controller plays the death sequence and shows the death screen.
	if (ABalloonPlayerController* Player = Cast<ABalloonPlayerController>(GetWorld()->GetFirstPlayerController()))
	{
		Player->ShowDeath(GetDeathCause());
		return;
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
