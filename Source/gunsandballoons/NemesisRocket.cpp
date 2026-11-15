#include "NemesisRocket.h"

#include "Components/PointLightComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "NemesisAirship.h"
#include "NemesisPlaceholder.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	constexpr int32 CurveSamples = 32;
}

ANemesisRocket::ANemesisRocket()
{
	PrimaryActorTick.bCanEverTick = true;

	// Only bullets (Visibility traces) see the rocket; it moves by itself and ignores physics.
	HitSphere = CreateDefaultSubobject<USphereComponent>(TEXT("HitSphere"));
	HitSphere->InitSphereRadius(60.f);
	HitSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	HitSphere->SetCollisionObjectType(ECC_WorldDynamic);
	HitSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	HitSphere->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	HitSphere->SetGenerateOverlapEvents(false);
	SetRootComponent(HitSphere);

	// Placeholder body until the rocket has its own model: a red 2.5 m tube with a glowing exhaust.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BodyMat(TEXT("/Game/Assets/Nemesis/MI_Nemesis_Red.MI_Nemesis_Red"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> GlowMat(TEXT("/Game/Assets/Nemesis/MI_Nemesis_Glow.MI_Nemesis_Glow"));

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(HitSphere);
	Mesh->SetStaticMesh(Cylinder.Object);
	Mesh->SetMaterial(0, BodyMat.Object);
	Mesh->SetRelativeRotation(FRotator(-90.f, 0.f, 0.f));
	Mesh->SetRelativeScale3D(FVector(0.5f, 0.5f, 2.5f));
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Exhaust = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Exhaust"));
	Exhaust->SetupAttachment(HitSphere);
	Exhaust->SetStaticMesh(Sphere.Object);
	Exhaust->SetMaterial(0, GlowMat.Object);
	Exhaust->SetRelativeLocation(FVector(-140.f, 0.f, 0.f));
	Exhaust->SetRelativeScale3D(FVector(0.9f, 0.6f, 0.6f));
	Exhaust->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Exhaust->SetCastShadow(false);

	// Model (Content/Assets/Enemies/Rocket.blend): body along +X, flame behind the nozzle.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> BodyMesh(TEXT("/Game/Assets/Enemies/FBX/SM_Rocket_Body.SM_Rocket_Body"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> FlameMesh(TEXT("/Game/Assets/Enemies/FBX/SM_Rocket_Flame.SM_Rocket_Flame"));
	if (NemesisModel::Use(Mesh, BodyMesh.Object))
	{
		NemesisModel::Use(Exhaust, FlameMesh.Object);
		NemesisCollision::Shootable(Mesh); // the whole 2.6 m body can be shot, not just the centre sphere
	}

	// Red light: readable from far away and in the radar-less biome.
	Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("Light"));
	Light->SetupAttachment(HitSphere);
	Light->SetRelativeLocation(FVector(-160.f, 0.f, 0.f));
	Light->SetLightColor(FLinearColor(1.f, 0.25f, 0.05f));
	Light->SetIntensity(200000.f);
	Light->SetAttenuationRadius(4000.f);
	Light->SetCastShadows(false);
}

void ANemesisRocket::Launch(ANemesisAirship* InAirship)
{
	Airship = InAirship;
	OnTakeAnyDamage.AddUniqueDynamic(this, &ANemesisRocket::HandleTakeAnyDamage);

	// Intercept point: where the balloon will be after the flight time of a straight shot.
	// The curve is longer than the straight line, so lead it by the curve's length instead.
	P0 = GetActorLocation();
	const FVector Basket = InAirship->GetBasketLocation();
	const FVector Velocity = InAirship->GetBalloonVelocity();
	FVector Target = Basket;
	for (int32 Iter = 0; Iter < 3; ++Iter)
	{
		P2 = Target;
		P1 = (P0 + P2) * 0.5f + FVector(0.f, 0.f, ArcHeight);
		ArcLengths.SetNum(CurveSamples + 1);
		ArcLengths[0] = 0.f;
		FVector Prev = P0;
		for (int32 I = 1; I <= CurveSamples; ++I)
		{
			const FVector P = EvalCurve(float(I) / CurveSamples);
			ArcLengths[I] = ArcLengths[I - 1] + FVector::Dist(Prev, P);
			Prev = P;
		}
		// Flight time with the motor's run-up from LaunchSpeed to Speed.
		const float Time = ArcLengths.Last() / Speed + BoostTime * 0.5f * (1.f - LaunchSpeed / Speed);
		Target = Basket + Velocity * Time;
	}

	bLaunched = true;
	FVector Dir;
	PositionAtDistance(0.f, Dir);
	SetActorRotation(Dir.Rotation());

	// Ignition: a puff of smoke at the launcher, then a trail behind the nozzle for the whole flight.
	if (AGunSmoke* Puff = NemesisFx::Spawn(GetWorld(), NemesisFx::EKind::BlastSmoke))
	{
		FGunSmokePuffs P = NemesisFx::Puffs(NemesisFx::EKind::BlastSmoke);
		P.StartRadius *= 0.5f; P.EndRadius *= 0.5f; P.SpeedMin = 50.f; P.SpeedMax = 200.f;
		Puff->Emit(P, 6, P0 - Dir * 150.f, -Dir, FVector::ZeroVector);
		Puff->SetLifeSpan(P.LifeMax + 0.5f);
	}
	Trail.Start(GetWorld(), NemesisFx::EKind::MissileTrail, TrailRate); // same plume as the AA missile
}

FVector ANemesisRocket::EvalCurve(float T) const
{
	const float U = 1.f - T;
	return U * U * P0 + 2.f * U * T * P1 + T * T * P2;
}

FVector ANemesisRocket::PositionAtDistance(float S, FVector& OutDir) const
{
	const float Total = ArcLengths.Last();
	if (S >= Total)
	{
		// Past the intercept point: keep flying straight along the end tangent.
		OutDir = (P2 - P1).GetSafeNormal();
		return P2 + OutDir * (S - Total);
	}
	int32 I = 1;
	while (I < CurveSamples && ArcLengths[I] < S) ++I;
	const float Seg = ArcLengths[I] - ArcLengths[I - 1];
	const float Alpha = Seg > KINDA_SMALL_NUMBER ? (S - ArcLengths[I - 1]) / Seg : 0.f;
	const float T = (I - 1 + Alpha) / CurveSamples;
	const float U = 1.f - T;
	OutDir = (2.f * U * (P1 - P0) + 2.f * T * (P2 - P1)).GetSafeNormal();
	return EvalCurve(T);
}

void ANemesisRocket::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	ANemesisAirship* A = Airship.Get();
	if (!bLaunched || bExploded || !A)
	{
		if (!A && bLaunched) Destroy();
		return;
	}

	const FVector From = GetActorLocation();
	Age += DeltaSeconds;
	const float CurSpeed = BoostTime > 0.f ? FMath::Lerp(LaunchSpeed, Speed, FMath::Clamp(Age / BoostTime, 0.f, 1.f)) : Speed;
	Travelled += CurSpeed * DeltaSeconds;
	FVector Dir;
	const FVector To = PositionAtDistance(Travelled, Dir);

	// Direct hit only: the segment flown this frame passes through the basket.
	const FVector Basket = A->GetBasketLocation();
	if (FMath::PointDistToSegment(Basket, From, To) <= BasketHitRadius)
	{
		SetActorLocation(FMath::ClosestPointOnSegment(Basket, From, To));
		if (AActor* Balloon = A->GetBalloon())
		{
			UGameplayStatics::ApplyDamage(Balloon, Damage, nullptr, this, nullptr);
		}
		A->ApplyRocketSlowdown();
		Explode(true);
		return;
	}

	if (bStopOnWorldGeometry)
	{
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(NemesisRocket), false, this);
		Params.AddIgnoredActor(A);
		if (AActor* Balloon = A->GetBalloon()) Params.AddIgnoredActor(Balloon);
		if (GetWorld()->LineTraceSingleByObjectType(Hit, From, To, FCollisionObjectQueryParams(ECC_WorldStatic), Params))
		{
			SetActorLocation(Hit.ImpactPoint);
			Explode(false);
			return;
		}
	}

	SetActorLocationAndRotation(To, Dir.Rotation());

	// Looks: spin around the axis, a flickering flame and light, smoke left hanging behind the nozzle.
	Roll = FMath::Fmod(Roll + SpinSpeed * DeltaSeconds, 360.f);
	Mesh->SetRelativeRotation(FRotator(0.f, 0.f, Roll));
	Exhaust->SetRelativeScale3D(FVector(FMath::FRandRange(0.75f, 1.35f), FMath::FRandRange(0.9f, 1.1f), FMath::FRandRange(0.9f, 1.1f)));
	Light->SetIntensity(FMath::FRandRange(120000.f, 260000.f));
	Trail.Tick(DeltaSeconds, To - Dir * 160.f, -Dir, FVector::ZeroVector);

	if (AActor* Balloon = A->GetBalloon())
	{
		if (Travelled > ArcLengths.Last() && FVector::Dist(To, Balloon->GetActorLocation()) > DespawnDistance)
		{
			Destroy();
		}
	}
}

void ANemesisRocket::HandleTakeAnyDamage(AActor* DamagedActor, float InDamage, const UDamageType* DamageType, AController* InstigatedBy, AActor* DamageCauser)
{
	Explode(false); // one shot from any weapon
}

void ANemesisRocket::Explode(bool bHitBalloon)
{
	if (bExploded) return;
	bExploded = true;
	UE_LOG(LogTemp, Log, TEXT("Nemesis rocket exploded (%s)"), bHitBalloon ? TEXT("direct hit") : TEXT("shot down / crashed"));
	NemesisFx::Explosion(GetWorld(), GetActorLocation(), 1.f);
	OnExploded(bHitBalloon);
	Destroy();
}

void ANemesisRocket::EndPlay(const EEndPlayReason::Type Reason)
{
	Trail.Stop(); // the trail hangs in the air and fades by itself
	if (ANemesisAirship* A = Airship.Get())
	{
		A->OnRocketGone(this);
	}
	Super::EndPlay(Reason);
}
