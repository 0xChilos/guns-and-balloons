#include "NemesisWaveDirector.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "NemesisBeacon.h"
#include "NemesisBirdFlock.h"
#include "NemesisFlak.h"
#include "NemesisPatrol.h"
#include "NemesisTargetSubsystem.h"
#include "NemesisTowerCannon.h"

namespace
{
	FNemesisWaveEvent Ev(float Time, ENemesisWaveKind Kind, float AheadM, float SideM, const TCHAR* Label,
		ENemesisBand Band = ENemesisBand::CropDusters)
	{
		FNemesisWaveEvent E;
		E.Time = Time;
		E.Kind = Kind;
		E.Ahead = AheadM * 100.f;
		E.Side = SideM * 100.f;
		E.Band = Band;
		E.Label = Label;
		return E;
	}
}

ANemesisWaveDirector::ANemesisWaveDirector()
{
	PrimaryActorTick.bCanEverTick = true;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
	AirshipClass = ANemesisAirship::StaticClass();

	// Every enemy once, about a minute and a half (6–14 s apart). Distances in metres. TBD
	using K = ENemesisWaveKind;
	Events = {
		Ev(2.f, K::Message, 0, 0, TEXT("Take a weapon: LMB on an item in front of you")),
		Ev(6.f, K::BirdFlock, 100, 0, TEXT("Field: a bird flock (shoot towards it to scare it off)")),
		Ev(14.f, K::AirshipBand, 0, 0, TEXT("Airship: crop dusters (hit the engine or the fuselage)"), ENemesisBand::CropDusters),
		Ev(24.f, K::Beacon, 60, -35, TEXT("Desert: a beacon on the left (avoid the light or shoot the lamp)")),
		Ev(36.f, K::AirshipBand, 0, 0, TEXT("Airship: the bomber and its mines (shoot the mine or its parachute)"), ENemesisBand::Bombers),
		Ev(42.f, K::TowerCannon, 70, 45, TEXT("Desert: a cannon tower on the right (shoot the boiler)")),
		Ev(56.f, K::AirshipBand, 0, 0, TEXT("Airship: rockets (shoot them down)"), ENemesisBand::Rockets),
		Ev(62.f, K::Flak, 35, -18, TEXT("AA balloon on the left: leave its radius or shoot its missiles")),
		Ev(76.f, K::Patrol, 110, 0, TEXT("Lava: a crop duster patrol ahead")),
		Ev(90.f, K::AirshipByDistance, 0, 0, TEXT("Airship: back to attacks by distance")),
	};

}

void ANemesisWaveDirector::BeginPlay()
{
	Super::BeginPlay();
	Events.StableSort([](const FNemesisWaveEvent& A, const FNemesisWaveEvent& B) { return A.Time < B.Time; });

	UNemesisTargetSubsystem* T = UNemesisTargetSubsystem::Get(this);
	if (T && !T->GetAirship() && AirshipClass && T->GetBalloon())
	{
		// Spawned behind the balloon; it places itself at StartDistance on BeginPlay.
		const FVector Behind = T->GetBasketLocation() - T->GetBalloonForward() * AirshipStartDistance;
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (ANemesisAirship* Airship = GetWorld()->SpawnActorDeferred<ANemesisAirship>(AirshipClass, FTransform(T->GetBalloonForward().Rotation(), Behind),
			nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
		{
			Airship->SetInvulnerableBalloon(bInvulnerableBalloon);
			Airship->SetStartDistance(AirshipStartDistance);
			Airship->FinishSpawning(FTransform(T->GetBalloonForward().Rotation(), Behind));
		}
	}
	else if (T && T->GetAirship())
	{
		T->GetAirship()->SetInvulnerableBalloon(bInvulnerableBalloon);
	}
	UE_LOG(LogTemp, Log, TEXT("WaveDirector: %d events, balloon %s"), Events.Num(), T && T->GetBalloon() ? TEXT("found") : TEXT("MISSING"));
}

void ANemesisWaveDirector::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Elapsed += DeltaSeconds;
	while (NextEvent < Events.Num() && Elapsed >= Events[NextEvent].Time)
	{
		Fire(Events[NextEvent]);
		++NextEvent;
	}
	if (NextEvent >= Events.Num() && bLoop && Events.Num() > 0)
	{
		Elapsed = 0.f;
		NextEvent = 0;
	}
}

void ANemesisWaveDirector::Fire(const FNemesisWaveEvent& Event)
{
	UNemesisTargetSubsystem* T = UNemesisTargetSubsystem::Get(this);
	ANemesisAirship* Airship = T ? T->GetAirship() : nullptr;
	switch (Event.Kind)
	{
	case ENemesisWaveKind::BirdFlock:   SpawnEnemy(ANemesisBirdFlock::StaticClass(), Event, false); break;
	case ENemesisWaveKind::Beacon:      SpawnEnemy(ANemesisBeacon::StaticClass(), Event, true); break;
	case ENemesisWaveKind::TowerCannon: SpawnEnemy(ANemesisTowerCannon::StaticClass(), Event, true); break;
	case ENemesisWaveKind::Flak:        SpawnEnemy(ANemesisFlak::StaticClass(), Event, false); break;
	case ENemesisWaveKind::Patrol:      SpawnEnemy(ANemesisPatrol::StaticClass(), Event, false); break;
	case ENemesisWaveKind::AirshipBand:
		if (Airship) Airship->SetForcedBand(true, Event.Band);
		break;
	case ENemesisWaveKind::AirshipByDistance:
		if (Airship) Airship->SetForcedBand(false, Event.Band);
		break;
	default:
		break;
	}
	UE_LOG(LogTemp, Log, TEXT("WaveDirector t=%.0f: %s"), Elapsed, *Event.Label);
	if (!Event.Label.IsEmpty()) Say(Event.Label);
}

AActor* ANemesisWaveDirector::SpawnEnemy(UClass* Class, const FNemesisWaveEvent& Event, bool bOnGround)
{
	UNemesisTargetSubsystem* T = UNemesisTargetSubsystem::Get(this);
	AActor* Balloon = T ? T->GetBalloon() : nullptr;
	if (!Balloon || !Class) return nullptr;

	FVector Forward = T->GetBalloonForward();
	Forward.Z = 0.f;
	Forward = Forward.GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector);
	const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward);
	const FVector Basket = T->GetBasketLocation();
	FVector At = Basket + Forward * Event.Ahead + Right * Event.Side;

	if (bOnGround)
	{
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(WaveGround), false, Balloon);
		TArray<AActor*> Riders;
		Balloon->GetAttachedActors(Riders, true, true);
		Params.AddIgnoredActors(Riders);
		const bool bGround = GetWorld()->LineTraceSingleByObjectType(Hit, At, At - FVector(0.f, 0.f, 1000000.f), FCollisionObjectQueryParams(ECC_WorldStatic), Params);
		At.Z = bGround ? Hit.ImpactPoint.Z : Basket.Z - FallbackGroundDepth;
	}

	// Face the balloon (the beacon sweeps around its forward).
	FVector ToBalloon = Basket - At;
	ToBalloon.Z = 0.f;
	const FRotator Facing(0.f, ToBalloon.Rotation().Yaw, 0.f);
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AActor* Spawned = GetWorld()->SpawnActor<AActor>(Class, At, Facing, Params);
	UE_LOG(LogTemp, Log, TEXT("WaveDirector: spawned %s at %s"), *GetNameSafe(Spawned), *At.ToCompactString());
	return Spawned;
}

void ANemesisWaveDirector::Say(const FString& Text) const
{
	if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 6.f, FColor(255, 200, 80), Text);
}
