#include "NemesisTargetSubsystem.h"

#include "BalloonFlightComponent.h"
#include "EngineHealthComponent.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "NemesisAirship.h"
#include "NemesisCropDuster.h"

namespace
{
	template <typename T>
	void PruneDead(TArray<TWeakObjectPtr<T>>& List)
	{
		List.RemoveAll([](const TWeakObjectPtr<T>& P) { return !P.IsValid(); });
	}
}

UNemesisTargetSubsystem* UNemesisTargetSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UNemesisTargetSubsystem>() : nullptr;
}

void UNemesisTargetSubsystem::FindBalloon()
{
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (UBalloonFlightComponent* F = It->FindComponentByClass<UBalloonFlightComponent>())
		{
			Balloon = *It;
			Flight = F;
			break;
		}
	}
	if (!Balloon.IsValid()) return;

	UEngineHealthComponent* H = Balloon->FindComponentByClass<UEngineHealthComponent>();
	if (!H)
	{
		// Fallback until BP_Balloon has the component; damage must land somewhere.
		H = NewObject<UEngineHealthComponent>(Balloon.Get(), TEXT("EngineHealth"));
		H->RegisterComponent();
		Balloon->AddInstanceComponent(H);
		UE_LOG(LogTemp, Warning, TEXT("Nemesis: %s had no EngineHealthComponent, added one at runtime"), *Balloon->GetName());
	}
	Health = H;
}

AActor* UNemesisTargetSubsystem::GetBalloon()
{
	if (!Balloon.IsValid()) FindBalloon();
	return Balloon.Get();
}

UBalloonFlightComponent* UNemesisTargetSubsystem::GetFlight()
{
	if (!Balloon.IsValid()) FindBalloon();
	return Flight.Get();
}

UEngineHealthComponent* UNemesisTargetSubsystem::GetHealth()
{
	if (!Balloon.IsValid()) FindBalloon();
	return Health.Get();
}

FVector UNemesisTargetSubsystem::GetBasketLocation()
{
	const AActor* B = GetBalloon();
	return B ? B->GetActorTransform().TransformPosition(BasketOffset) : FVector::ZeroVector;
}

FVector UNemesisTargetSubsystem::GetBalloonForward()
{
	const AActor* B = GetBalloon();
	return B ? FRotator(0.f, B->GetActorRotation().Yaw, 0.f).Vector() : FVector::ForwardVector;
}

FVector UNemesisTargetSubsystem::GetBalloonVelocity()
{
	const UBalloonFlightComponent* F = GetFlight();
	return F ? GetBalloonForward() * F->GetCurrentSpeed() : FVector::ZeroVector;
}

void UNemesisTargetSubsystem::DamageBalloon(float Amount, AActor* Causer)
{
	if (AActor* B = GetBalloon())
	{
		UGameplayStatics::ApplyDamage(B, Amount, nullptr, Causer, nullptr);
	}
}

bool UNemesisTargetSubsystem::CanSpawnMine()
{
	PruneDead(Mines);
	return Mines.Num() < MaxMines;
}

void UNemesisTargetSubsystem::RegisterMine(AActor* Mine)
{
	Mines.Add(Mine);
}

bool UNemesisTargetSubsystem::TryClaimFlock(AActor* Flock)
{
	PruneDead(Flocks);
	if (Flocks.Contains(Flock)) return true;
	if (Flocks.Num() >= MaxActiveFlocks) return false;
	Flocks.Add(Flock);
	return true;
}

void UNemesisTargetSubsystem::ReleaseFlock(AActor* Flock)
{
	Flocks.Remove(Flock);
}

bool UNemesisTargetSubsystem::TryEngagePatrol(AActor* Patrol)
{
	PruneDead(Patrols);
	if (Patrols.Contains(Patrol)) return true;
	if (Patrols.Num() >= MaxEngagedPatrols) return false;
	Patrols.Add(Patrol);
	return true;
}

int32 UNemesisTargetSubsystem::CountAttackingCropDusters()
{
	int32 Count = 0;
	for (TActorIterator<ANemesisCropDuster> It(GetWorld()); It; ++It)
	{
		if (!It->IsDown() && !It->IsOnRoute()) ++Count;
	}
	return Count;
}

void UNemesisTargetSubsystem::ReleasePatrol(AActor* Patrol)
{
	Patrols.Remove(Patrol);
}

ANemesisAirship* UNemesisTargetSubsystem::GetAirship()
{
	if (!Airship.IsValid())
	{
		TActorIterator<ANemesisAirship> It(GetWorld());
		if (It) Airship = *It;
	}
	return Airship.Get();
}
