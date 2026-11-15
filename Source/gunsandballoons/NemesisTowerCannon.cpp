#include "NemesisTowerCannon.h"

#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "NemesisBullet.h"
#include "NemesisFx.h"
#include "NemesisPlaceholder.h"
#include "NemesisTargetSubsystem.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	constexpr float CannonBarrelLength = 300.f;
	constexpr float CannonPlatformHeight = 40.f;
}

ANemesisTowerCannon::ANemesisTowerCannon()
{
	PrimaryActorTick.bCanEverTick = true;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Wood(TEXT("/Game/Assets/Nemesis/MI_Nemesis_Wood.MI_Nemesis_Wood"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Iron(TEXT("/Game/Assets/Nemesis/MI_Nemesis_Iron.MI_Nemesis_Iron"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Dark(TEXT("/Game/Assets/Nemesis/MI_Nemesis_Dark.MI_Nemesis_Dark"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Red(TEXT("/Game/Assets/Nemesis/MI_Nemesis_Red.MI_Nemesis_Red"));

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	// Placeholder: 3 x 3 m tower (pivot at its foot), a 4 x 4 m platform, the cannon in the middle,
	// the fuel tank on a corner of the platform.
	Tower = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Tower"));
	Tower->SetupAttachment(Root);
	Tower->SetStaticMesh(Cube.Object);
	Tower->SetMaterial(0, Wood.Object);
	NemesisCollision::Solid(Tower);

	Platform = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Platform"));
	Platform->SetupAttachment(Root);
	Platform->SetStaticMesh(Cube.Object);
	Platform->SetMaterial(0, Wood.Object);
	Platform->SetRelativeScale3D(FVector(4.f, 4.f, CannonPlatformHeight / 100.f));
	NemesisCollision::Solid(Platform);

	CannonYaw = CreateDefaultSubobject<USceneComponent>(TEXT("CannonYaw"));
	CannonYaw->SetupAttachment(Root);

	Carriage = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Carriage"));
	Carriage->SetupAttachment(CannonYaw);
	Carriage->SetStaticMesh(Cube.Object);
	Carriage->SetMaterial(0, Iron.Object);
	Carriage->SetRelativeLocation(FVector(0.f, 0.f, 40.f));
	Carriage->SetRelativeScale3D(FVector(1.4f, 1.2f, 0.8f));
	NemesisCollision::None(Carriage);

	CannonPitch = CreateDefaultSubobject<USceneComponent>(TEXT("CannonPitch"));
	CannonPitch->SetupAttachment(CannonYaw);
	CannonPitch->SetRelativeLocation(FVector(0.f, 0.f, 100.f));

	Barrel = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Barrel"));
	Barrel->SetupAttachment(CannonPitch);
	Barrel->SetStaticMesh(Cube.Object);
	Barrel->SetMaterial(0, Dark.Object);
	Barrel->SetRelativeLocation(FVector(CannonBarrelLength * 0.4f, 0.f, 0.f));
	Barrel->SetRelativeScale3D(FVector(CannonBarrelLength / 100.f, 0.45f, 0.45f));
	NemesisCollision::None(Barrel);

	MuzzleGlow = CreateDefaultSubobject<UPointLightComponent>(TEXT("MuzzleGlow"));
	MuzzleGlow->SetupAttachment(CannonPitch);
	MuzzleGlow->SetRelativeLocation(FVector(CannonBarrelLength * 0.9f + 20.f, 0.f, 0.f));
	MuzzleGlow->SetLightColor(FLinearColor(1.f, 0.45f, 0.1f));
	MuzzleGlow->SetIntensity(400000.f);
	MuzzleGlow->SetAttenuationRadius(2500.f);
	MuzzleGlow->SetCastShadows(false);
	MuzzleGlow->SetVisibility(false);

	FuelTank = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FuelTank"));
	FuelTank->SetupAttachment(Root);
	FuelTank->SetStaticMesh(Cylinder.Object);
	FuelTank->SetMaterial(0, Red.Object);
	FuelTank->SetRelativeScale3D(FVector(1.4f, 1.4f, 1.8f));
	NemesisCollision::None(FuelTank); // the TankHitBox takes the shots

	TankHitBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TankHitBox"));
	TankHitBox->SetupAttachment(Root);
	NemesisCollision::Shootable(TankHitBox);

	ShellClass = ANemesisBullet::StaticClass();
}

void ANemesisTowerCannon::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Tower->SetRelativeLocation(FVector(0.f, 0.f, TowerHeight * 0.5f));
	Tower->SetRelativeScale3D(FVector(3.f, 3.f, TowerHeight / 100.f));
	Platform->SetRelativeLocation(FVector(0.f, 0.f, TowerHeight + CannonPlatformHeight * 0.5f));
	const float Top = TowerHeight + CannonPlatformHeight;
	CannonYaw->SetRelativeLocation(FVector(0.f, 0.f, Top));
	const FVector TankAt(-130.f, 130.f, Top + 90.f); // back-left corner; the cylinder pivot is its centre
	FuelTank->SetRelativeLocation(TankAt);
	TankHitBox->SetBoxExtent(TankHitBoxExtent);
	TankHitBox->SetRelativeLocation(TankAt);
}

void ANemesisTowerCannon::BeginPlay()
{
	Super::BeginPlay();
	OnTakePointDamage.AddUniqueDynamic(this, &ANemesisTowerCannon::HandlePointDamage);
}

bool ANemesisTowerCannon::CanSeeBasket(FVector& OutBasket) const
{
	UNemesisTargetSubsystem* T = UNemesisTargetSubsystem::Get(this);
	AActor* Balloon = T ? T->GetBalloon() : nullptr;
	if (!Balloon || T->IsSandstormActive()) return false;

	OutBasket = T->GetBasketLocation();
	const FVector Eye = CannonPitch->GetComponentLocation();
	if (FVector::DistSquared(Eye, OutBasket) > FMath::Square(Range)) return false;

	// Line of sight: level geometry in between blocks it; the balloon and its riders don't.
	FCollisionQueryParams Params(SCENE_QUERY_STAT(TowerCannonSight), false, this);
	Params.AddIgnoredActor(Balloon);
	TArray<AActor*> Riders;
	Balloon->GetAttachedActors(Riders, true, true);
	Params.AddIgnoredActors(Riders);
	FHitResult Hit;
	return !GetWorld()->LineTraceSingleByObjectType(Hit, Eye, OutBasket, FCollisionObjectQueryParams(ECC_WorldStatic), Params);
}

void ANemesisTowerCannon::SetState(ETowerCannonState NewState)
{
	const bool bWasAiming = State == ETowerCannonState::Aiming;
	State = NewState;
	StateTime = 0.f;
	const bool bAiming = State == ETowerCannonState::Aiming;
	MuzzleGlow->SetVisibility(bAiming);
	if (bAiming != bWasAiming) OnAimChanged(bAiming);
}

void ANemesisTowerCannon::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (State == ETowerCannonState::Destroyed) return;
	StateTime += DeltaSeconds;

	FVector Basket;
	const bool bSees = CanSeeBasket(Basket);
	if (bSees)
	{
		// The cannon turns to the basket.
		const FRotator Want = GetActorTransform().InverseTransformVector(Basket - CannonPitch->GetComponentLocation()).Rotation();
		const FRotator Now = FMath::RInterpConstantTo(FRotator(Pitch, Yaw, 0.f), Want, DeltaSeconds, TurnSpeed);
		Pitch = FMath::Clamp(Now.Pitch, -30.f, 60.f);
		Yaw = FRotator::NormalizeAxis(Now.Yaw);
		CannonYaw->SetRelativeRotation(FRotator(0.f, Yaw, 0.f));
		CannonPitch->SetRelativeRotation(FRotator(Pitch, 0.f, 0.f));
	}

	switch (State)
	{
	case ETowerCannonState::Idle:
		if (bSees) SetState(ETowerCannonState::Aiming);
		break;
	case ETowerCannonState::Aiming:
		if (!bSees)
		{
			SetState(ETowerCannonState::Idle); // lost sight: start over next time
		}
		else if (StateTime >= AimTime)
		{
			Fire(Basket);
			SetState(ETowerCannonState::Reloading);
		}
		break;
	case ETowerCannonState::Reloading:
		if (StateTime >= ReloadTime) SetState(bSees ? ETowerCannonState::Aiming : ETowerCannonState::Idle);
		break;
	default:
		break;
	}
}

void ANemesisTowerCannon::Fire(const FVector& Basket)
{
	if (!ShellClass) return;
	const FVector Muzzle = CannonPitch->GetComponentLocation() + CannonPitch->GetForwardVector() * (CannonBarrelLength * 0.9f);
	const FVector Dir = (Basket - Muzzle).GetSafeNormal(); // straight at the basket, no drop
	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (ANemesisBullet* Shell = GetWorld()->SpawnActor<ANemesisBullet>(ShellClass, Muzzle, Dir.Rotation(), Params))
	{
		Shell->Fire(Dir, ShellSpeed, Damage);
		Shell->SetActorScale3D(FVector(2.5f)); // a cannon shell, not a bullet
	}
	NemesisFx::Explosion(GetWorld(), Muzzle, 0.35f); // muzzle blast
	OnShot();
}

void ANemesisTowerCannon::HandlePointDamage(AActor* DamagedActor, float InDamage, AController* InstigatedBy, FVector HitLocation,
	UPrimitiveComponent* HitComponent, FName BoneName, FVector ShotFromDirection, const UDamageType* DamageType, AActor* DamageCauser)
{
	if (State == ETowerCannonState::Destroyed || HitComponent != TankHitBox) return; // the cannon and tower ignore shots
	BlowUp();
}

void ANemesisTowerCannon::BlowUp()
{
	SetState(ETowerCannonState::Destroyed);
	UWorld* World = GetWorld();
	// The tank goes up first, then the fire runs down the tower.
	NemesisFx::Explosion(World, FuelTank->GetComponentLocation(), 3.f);
	NemesisFx::Explosion(World, CannonYaw->GetComponentLocation() + FVector(0.f, 0.f, 80.f), 2.f);
	for (float Z = TowerHeight * 0.75f; Z > 0.f; Z -= TowerHeight * 0.25f)
	{
		NemesisFx::Explosion(World, GetActorTransform().TransformPosition(FVector(0.f, 0.f, Z)), 1.8f);
	}
	// Cannon and tower are gone.
	TArray<UPrimitiveComponent*> Parts;
	GetComponents<UPrimitiveComponent>(Parts);
	for (UPrimitiveComponent* P : Parts)
	{
		P->SetVisibility(false);
		NemesisCollision::None(P);
	}
	UE_LOG(LogTemp, Log, TEXT("%s: fuel tank hit, tower destroyed"), *GetName());
	OnTowerDestroyed();
	SetLifeSpan(10.f);
}
