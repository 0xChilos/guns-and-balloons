#include "NemesisBullet.h"

#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "NemesisTargetSubsystem.h"
#include "UObject/ConstructorHelpers.h"

ANemesisBullet::ANemesisBullet()
{
	PrimaryActorTick.bCanEverTick = true;
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Glow(TEXT("/Game/Assets/Nemesis/MI_Nemesis_Glow.MI_Nemesis_Glow"));
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	Mesh->SetStaticMesh(Cube.Object);
	Mesh->SetMaterial(0, Glow.Object);
	Mesh->SetRelativeScale3D(FVector(0.6f, 0.08f, 0.08f)); // a tracer streak
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCastShadow(false);
}

void ANemesisBullet::Fire(const FVector& Direction, float InSpeed, float InDamage)
{
	if (InSpeed > 0.f) Speed = InSpeed;
	if (InDamage > 0.f) Damage = InDamage;
	Velocity = Direction.GetSafeNormal() * Speed;
}

void ANemesisBullet::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UNemesisTargetSubsystem* T = UNemesisTargetSubsystem::Get(this);
	Age += DeltaSeconds;
	if (!T || !T->GetBalloon() || Age > LifeTime)
	{
		Destroy();
		return;
	}

	const FVector From = GetActorLocation();
	const FVector To = From + Velocity * DeltaSeconds;
	if (FMath::PointDistToSegment(T->GetBasketLocation(), From, To) <= BasketHitRadius)
	{
		T->DamageBalloon(Damage, this);
		Destroy();
		return;
	}
	SetActorLocation(To);
}
