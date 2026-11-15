#include "NemesisHomingMissile.h"

#include "Components/PointLightComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "NemesisFlak.h"
#include "NemesisPlaceholder.h"
#include "NemesisTargetSubsystem.h"
#include "UObject/ConstructorHelpers.h"

ANemesisHomingMissile::ANemesisHomingMissile()
{
	PrimaryActorTick.bCanEverTick = true;

	// Only bullets (Visibility traces) see the missile; it moves by itself and ignores physics.
	HitSphere = CreateDefaultSubobject<USphereComponent>(TEXT("HitSphere"));
	HitSphere->InitSphereRadius(60.f);
	NemesisCollision::Shootable(HitSphere);
	SetRootComponent(HitSphere);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BodyMat(TEXT("/Game/Assets/Nemesis/MI_Nemesis_Red.MI_Nemesis_Red"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> GlowMat(TEXT("/Game/Assets/Nemesis/MI_Nemesis_Glow.MI_Nemesis_Glow"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> BodyMesh(TEXT("/Game/Assets/Enemies/FBX/SM_Rocket_Body.SM_Rocket_Body"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> FlameMesh(TEXT("/Game/Assets/Enemies/FBX/SM_Rocket_Flame.SM_Rocket_Flame"));

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(HitSphere);
	Mesh->SetStaticMesh(Cylinder.Object);
	Mesh->SetMaterial(0, BodyMat.Object);
	Mesh->SetRelativeRotation(FRotator(-90.f, 0.f, 0.f));
	Mesh->SetRelativeScale3D(FVector(0.35f, 0.35f, 1.6f));
	NemesisCollision::None(Mesh);

	Exhaust = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Exhaust"));
	Exhaust->SetupAttachment(HitSphere);
	Exhaust->SetStaticMesh(Sphere.Object);
	Exhaust->SetMaterial(0, GlowMat.Object);
	Exhaust->SetRelativeLocation(FVector(-90.f, 0.f, 0.f));
	Exhaust->SetRelativeScale3D(FVector(0.6f, 0.4f, 0.4f));
	NemesisCollision::None(Exhaust);
	Exhaust->SetCastShadow(false);

	// The team's map-edge rocket (Content/Assets/Map_edge): 1.6 m, faces −Y with the exhaust socket at
	// its nozzle, plus their flame mesh. Falls back to the airship rocket's model at 0.6 scale.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> MapEdgeRocket(TEXT("/Game/Assets/Map_edge/SM_Rocket.SM_Rocket"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> MapEdgeFlame(TEXT("/Game/Assets/Map_edge/SM_FX_RocketFlame.SM_FX_RocketFlame"));
	if (NemesisModel::Use(Mesh, MapEdgeRocket.Object))
	{
		bTeamModel = true;
		Mesh->SetRelativeRotation(FRotator(0.f, 90.f, 0.f));
		NemesisCollision::Shootable(Mesh);
		if (NemesisModel::Use(Exhaust, MapEdgeFlame.Object))
		{
			// The flame's pivot is at the nozzle and it points +Y (backwards once turned).
			Exhaust->SetRelativeLocationAndRotation(FVector(-80.f, 0.f, 0.f), FRotator(0.f, 90.f, 0.f));
		}
	}
	else if (NemesisModel::Use(Mesh, BodyMesh.Object))
	{
		NemesisModel::Use(Exhaust, FlameMesh.Object);
		Mesh->SetRelativeScale3D(FVector(0.6f));
		Exhaust->SetRelativeScale3D(FVector(0.6f));
		NemesisCollision::Shootable(Mesh);
	}

	Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("Light"));
	Light->SetupAttachment(HitSphere);
	Light->SetRelativeLocation(FVector(-100.f, 0.f, 0.f));
	Light->SetLightColor(FLinearColor(1.f, 0.2f, 0.05f));
	Light->SetIntensity(150000.f);
	Light->SetAttenuationRadius(3000.f);
	Light->SetCastShadows(false);
}

void ANemesisHomingMissile::Launch(ANemesisFlak* InFlak)
{
	Flak = InFlak;
	Direction = GetActorForwardVector();
	ExhaustBase = Exhaust->GetRelativeScale3D();
	bLaunched = true;
	OnTakeAnyDamage.AddUniqueDynamic(this, &ANemesisHomingMissile::HandleTakeAnyDamage);
	Trail.Start(GetWorld(), NemesisFx::EKind::MissileTrail, TrailRate);
}

void ANemesisHomingMissile::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bLaunched || bExploded) return;

	ANemesisFlak* F = Flak.Get();
	UNemesisTargetSubsystem* T = UNemesisTargetSubsystem::Get(this);
	Age += DeltaSeconds;
	// The basket left the AA gun's radius (or the gun is gone): blow up where it is, no damage.
	if (!F || !F->IsEngaged() || !T || !T->GetBalloon() || Age >= MaxLifeTime)
	{
		Explode(false);
		return;
	}

	// Steer at the basket with a limited turn rate.
	const FVector From = GetActorLocation();
	const FVector Basket = T->GetBasketLocation();
	const FVector Want = (Basket - From).GetSafeNormal();
	const float MaxTurn = FMath::DegreesToRadians(TurnRate * DeltaSeconds);
	const float Angle = FMath::Acos(FMath::Clamp(FVector::DotProduct(Direction, Want), -1.f, 1.f));
	if (Angle > KINDA_SMALL_NUMBER)
	{
		Direction = FQuat::Slerp(FQuat::Identity, FQuat::FindBetweenNormals(Direction, Want), FMath::Min(1.f, MaxTurn / Angle)).RotateVector(Direction).GetSafeNormal();
	}

	const float CurSpeed = BoostTime > 0.f ? FMath::Lerp(LaunchSpeed, Speed, FMath::Clamp(Age / BoostTime, 0.f, 1.f)) : Speed;
	const FVector To = From + Direction * CurSpeed * DeltaSeconds;

	// Direct hit: the segment flown this frame passes through the basket.
	if (FMath::PointDistToSegment(Basket, From, To) <= BasketHitRadius)
	{
		SetActorLocation(FMath::ClosestPointOnSegment(Basket, From, To));
		T->DamageBalloon(Damage, this);
		Explode(true);
		return;
	}

	// Level geometry stops it.
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(NemesisHomingMissile), false, this);
	Params.AddIgnoredActor(F);
	Params.AddIgnoredActor(T->GetBalloon());
	if (GetWorld()->LineTraceSingleByObjectType(Hit, From, To, FCollisionObjectQueryParams(ECC_WorldStatic), Params))
	{
		SetActorLocation(Hit.ImpactPoint);
		Explode(false);
		return;
	}

	SetActorLocationAndRotation(To, Direction.Rotation());
	Roll = FMath::Fmod(Roll + SpinSpeed * DeltaSeconds, 360.f);
	// Spin around the flight axis (the team model is first turned from −Y to +X).
	Mesh->SetRelativeRotation(FQuat(FVector::ForwardVector, FMath::DegreesToRadians(Roll)) * (bTeamModel ? FRotator(0.f, 90.f, 0.f).Quaternion() : FQuat::Identity));
	// Flicker along the flame's length (X for the old flame, Y for the team's).
	const float Flick = FMath::FRandRange(0.75f, 1.35f);
	Exhaust->SetRelativeScale3D(ExhaustBase * (bTeamModel ? FVector(1.f, Flick, 1.f) : FVector(Flick, 1.f, 1.f)));
	Light->SetIntensity(FMath::FRandRange(90000.f, 200000.f));
	Trail.Tick(DeltaSeconds, To - Direction * 100.f, -Direction, FVector::ZeroVector);
}

void ANemesisHomingMissile::HandleTakeAnyDamage(AActor* DamagedActor, float InDamage, const UDamageType* DamageType, AController* InstigatedBy, AActor* DamageCauser)
{
	Explode(false); // one shot from any weapon
}

void ANemesisHomingMissile::Explode(bool bHitBalloon)
{
	if (bExploded) return;
	bExploded = true;
	UE_LOG(LogTemp, Log, TEXT("AA missile exploded (%s)"), bHitBalloon ? TEXT("direct hit") : TEXT("shot down / target left the radius / crashed"));
	NemesisFx::FineExplosion(GetWorld(), GetActorLocation(), 1.5f);
	OnExploded(bHitBalloon);
	Destroy();
}

void ANemesisHomingMissile::EndPlay(const EEndPlayReason::Type Reason)
{
	Trail.Stop();
	Super::EndPlay(Reason);
}
