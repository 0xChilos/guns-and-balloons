#include "NemesisTowerGuard.h"

#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "NemesisBullet.h"
#include "NemesisPlaceholder.h"
#include "NemesisTargetSubsystem.h"
#include "UObject/ConstructorHelpers.h"

ANemesisTowerGuard::ANemesisTowerGuard()
{
	PrimaryActorTick.bCanEverTick = true;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Wood(TEXT("/Game/Assets/Nemesis/MI_Nemesis_Wood.MI_Nemesis_Wood"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Red(TEXT("/Game/Assets/Nemesis/MI_Nemesis_Red.MI_Nemesis_Red"));

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	// Placeholder: 3 x 3 m tower (pivot at its foot), 0.8 x 0.8 x 1.8 m guard on top.
	Tower = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Tower"));
	Tower->SetupAttachment(Root);
	Tower->SetStaticMesh(Cube.Object);
	Tower->SetMaterial(0, Wood.Object);
	NemesisCollision::Solid(Tower);

	Guard = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Guard"));
	Guard->SetupAttachment(Root);
	Guard->SetStaticMesh(Cube.Object);
	Guard->SetMaterial(0, Red.Object);
	Guard->SetRelativeScale3D(FVector(0.8f, 0.8f, 1.8f));
	NemesisCollision::None(Guard); // the HitBox takes the shots

	HitBox = CreateDefaultSubobject<UBoxComponent>(TEXT("HitBox"));
	HitBox->SetupAttachment(Root);
	NemesisCollision::Shootable(HitBox);

	// Scope glint: on while aiming, so the player can see who is about to shoot.
	Glint = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glint"));
	Glint->SetupAttachment(Guard);
	Glint->SetLightColor(FLinearColor(1.f, 0.95f, 0.7f));
	Glint->SetIntensity(300000.f);
	Glint->SetAttenuationRadius(2500.f);
	Glint->SetCastShadows(false);
	Glint->SetVisibility(false);

	BulletClass = ANemesisBullet::StaticClass();
}

void ANemesisTowerGuard::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Tower->SetRelativeLocation(FVector(0.f, 0.f, TowerHeight * 0.5f));
	Tower->SetRelativeScale3D(FVector(3.f, 3.f, TowerHeight / 100.f));
	Guard->SetRelativeLocation(FVector(0.f, 0.f, TowerHeight + 90.f));
	// Hangs down over the tower top too, so a shot that drops a little still counts.
	HitBox->SetBoxExtent(HitBoxExtent);
	HitBox->SetRelativeLocation(FVector(0.f, 0.f, TowerHeight + 90.f));
}

void ANemesisTowerGuard::BeginPlay()
{
	Super::BeginPlay();
	OnTakePointDamage.AddUniqueDynamic(this, &ANemesisTowerGuard::HandlePointDamage);
}

bool ANemesisTowerGuard::CanSeeBasket(FVector& OutBasket) const
{
	UNemesisTargetSubsystem* T = UNemesisTargetSubsystem::Get(this);
	AActor* Balloon = T ? T->GetBalloon() : nullptr;
	if (!Balloon || T->IsSandstormActive()) return false;

	OutBasket = T->GetBasketLocation();
	const FVector Eye = Guard->GetComponentLocation() + FVector(0.f, 0.f, 60.f);
	if (FVector::DistSquared(Eye, OutBasket) > FMath::Square(Range)) return false;

	// Line of sight: level geometry in between blocks it; the balloon and its riders don't.
	FCollisionQueryParams Params(SCENE_QUERY_STAT(TowerGuardSight), false, this);
	Params.AddIgnoredActor(Balloon);
	TArray<AActor*> Riders;
	Balloon->GetAttachedActors(Riders, true, true);
	Params.AddIgnoredActors(Riders);
	FHitResult Hit;
	return !GetWorld()->LineTraceSingleByObjectType(Hit, Eye, OutBasket, FCollisionObjectQueryParams(ECC_WorldStatic), Params);
}

void ANemesisTowerGuard::SetState(ETowerGuardState NewState)
{
	const bool bWasAiming = State == ETowerGuardState::Aiming;
	State = NewState;
	StateTime = 0.f;
	const bool bAiming = State == ETowerGuardState::Aiming;
	Glint->SetVisibility(bAiming);
	if (bAiming != bWasAiming) OnAimChanged(bAiming);
}

void ANemesisTowerGuard::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (State == ETowerGuardState::Dead) return;
	StateTime += DeltaSeconds;

	FVector Basket;
	const bool bSees = CanSeeBasket(Basket);
	if (bSees)
	{
		// The guard turns to face the balloon.
		const FVector To = Basket - Guard->GetComponentLocation();
		Guard->SetWorldRotation(FRotator(0.f, To.Rotation().Yaw, 0.f));
	}

	switch (State)
	{
	case ETowerGuardState::Idle:
		if (bSees) SetState(ETowerGuardState::Aiming);
		break;
	case ETowerGuardState::Aiming:
		if (!bSees)
		{
			SetState(ETowerGuardState::Idle); // lost sight: start over next time
		}
		else if (StateTime >= AimTime)
		{
			Fire(Basket);
			SetState(ETowerGuardState::Reloading);
		}
		break;
	case ETowerGuardState::Reloading:
		if (StateTime >= ReloadTime) SetState(bSees ? ETowerGuardState::Aiming : ETowerGuardState::Idle);
		break;
	default:
		break;
	}
}

void ANemesisTowerGuard::Fire(const FVector& Basket)
{
	if (!BulletClass) return;
	const FVector Muzzle = Guard->GetComponentLocation() + FVector(0.f, 0.f, 60.f);
	const FVector Dir = (Basket - Muzzle).GetSafeNormal(); // straight at the basket, no drop
	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (ANemesisBullet* Bullet = GetWorld()->SpawnActor<ANemesisBullet>(BulletClass, Muzzle, Dir.Rotation(), Params))
	{
		Bullet->Fire(Dir, BulletSpeed, Damage);
	}
	OnShot();
}

void ANemesisTowerGuard::HandlePointDamage(AActor* DamagedActor, float InDamage, AController* InstigatedBy, FVector HitLocation,
	UPrimitiveComponent* HitComponent, FName BoneName, FVector ShotFromDirection, const UDamageType* DamageType, AActor* DamageCauser)
{
	if (State == ETowerGuardState::Dead || HitComponent != HitBox) return; // shots into the tower do nothing
	SetState(ETowerGuardState::Dead);
	Guard->SetVisibility(false);
	NemesisCollision::None(HitBox);
	UE_LOG(LogTemp, Log, TEXT("%s: guard killed"), *GetName());
	OnGuardKilled();
}
