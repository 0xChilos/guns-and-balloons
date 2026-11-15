#include "NemesisTowerCannon.h"

#include "NemesisRadarSubsystem.h"
#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshSocket.h"
#include "Engine/World.h"
#include "FirstPersonCameraPawn.h"
#include "RevolverComponent.h"
#include "RifleComponent.h"
#include "ShotgunComponent.h"
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
	/** The team's turret meshes face −Y; the actor faces +X. */
	const FRotator TurretModelToActor(0.f, 90.f, 0.f);
	/** Our TowerGun model (Scripts/Blender/build_tower_gun.py, faces +X): the barrel's pivot (trunnion) in
	 *  the mount, and the muzzle's distance from it along the barrel, cm. */
	const FVector TowerGunTrunnion(15.f, 0.f, 135.f);
	constexpr float TowerGunMuzzle = 430.f;
	/** The team tower's deck is 70 cm thick (its deck collision, 19.75–20.45 m); our fuel tank hangs
	 *  from its underside below the tower's centre. Scaled with the tower's stretch. */
	constexpr float TowerDeckThickness = 70.f;

	FVector TurretSocketInMesh(const UStaticMeshComponent* C, FName Socket)
	{
		const UStaticMeshSocket* S = C && C->GetStaticMesh() ? C->GetStaticMesh()->FindSocket(Socket) : nullptr;
		return S ? TurretModelToActor.RotateVector(S->RelativeLocation) : FVector::ZeroVector;
	}
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

	// The team's model (Content/Assets/Locations/Desert): tower with a gun deck, a head that turns,
	// twin guns that elevate, and a boiler that is the weak spot. Keeps the cubes if any part is missing.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> TowerMesh(TEXT("/Game/Assets/Locations/Desert/SM_TurretTower.SM_TurretTower"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> HeadMesh(TEXT("/Game/Assets/Locations/Desert/SM_TurretTower_Head.SM_TurretTower_Head"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> GunsMesh(TEXT("/Game/Assets/Locations/Desert/SM_TurretTower_Guns.SM_TurretTower_Guns"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> EngineMesh(TEXT("/Game/Assets/Locations/Desert/SM_TurretTower_Engine.SM_TurretTower_Engine"));
	if (TowerMesh.Object && HeadMesh.Object && GunsMesh.Object && EngineMesh.Object)
	{
		bModel = true;
		NemesisModel::Use(Tower, TowerMesh.Object);
		NemesisModel::Use(Carriage, HeadMesh.Object);
		NemesisModel::Use(Barrel, GunsMesh.Object);
		NemesisModel::Use(FuelTank, EngineMesh.Object);
		for (UStaticMeshComponent* C : { Tower.Get(), Carriage.Get(), Barrel.Get(), FuelTank.Get() })
		{
			C->SetRelativeRotation(TurretModelToActor);
		}
		NemesisCollision::Solid(Carriage);
		NemesisCollision::Solid(Barrel);
		NemesisCollision::Solid(FuelTank); // blocks shots: a hit on the boiler counts like the TankHitBox
		Platform->SetVisibility(false);
		NemesisCollision::None(Platform);

		// Our steampunk cannon and fuel tank (team decision 2026-11-15) replace the team's head, guns and
		// boiler; the team's tower stays. Built facing +X, so no model rotation.
		static ConstructorHelpers::FObjectFinder<UStaticMesh> OwnMount(TEXT("/Game/Assets/Enemies/FBX/SM_TowerGun_Mount.SM_TowerGun_Mount"));
		static ConstructorHelpers::FObjectFinder<UStaticMesh> OwnBarrel(TEXT("/Game/Assets/Enemies/FBX/SM_TowerGun_Barrel.SM_TowerGun_Barrel"));
		static ConstructorHelpers::FObjectFinder<UStaticMesh> OwnTank(TEXT("/Game/Assets/Enemies/FBX/SM_TowerGun_FuelTank.SM_TowerGun_FuelTank"));
		if (OwnMount.Object && OwnBarrel.Object && OwnTank.Object)
		{
			bOwnGun = true;
			NemesisModel::Use(Carriage, OwnMount.Object);
			NemesisModel::Use(Barrel, OwnBarrel.Object);
			NemesisModel::Use(FuelTank, OwnTank.Object);
			// Shots go through the lattice tower to the tank under the deck (it still blocks the
			// balloon and the cannon's line of sight, which traces WorldStatic objects).
			Tower->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
		}
	}
}

void ANemesisTowerCannon::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyLayout();
}

void ANemesisTowerCannon::ApplyLayout()
{
	if (bModel)
	{
		// Everything hangs off the model's sockets: head on the deck, guns on the head, boiler on the deck.
		// The tower may be stretched (TowerZScale) so the deck reaches the player's height.
		const FVector Stretch(1.f, 1.f, TowerZScale);
		Tower->SetRelativeScale3D(Stretch);
		const FVector HeadAt = TurretSocketInMesh(Tower, TEXT("head")) * Stretch;
		DeckHeight = HeadAt.Z;
		CannonYaw->SetRelativeLocation(HeadAt);
		if (bOwnGun)
		{
			CannonPitch->SetRelativeLocation(TowerGunTrunnion);
			MuzzleGlow->SetRelativeLocation(FVector(TowerGunMuzzle + 20.f, 0.f, 0.f));
		}
		else
		{
			CannonPitch->SetRelativeLocation(TurretSocketInMesh(Carriage, TEXT("guns")));
			const FVector Muzzle = TurretSocketInMesh(Barrel, TEXT("muzzle_1"));
			MuzzleGlow->SetRelativeLocation(FVector(Muzzle.X + 20.f, 0.f, Muzzle.Z));
		}
		// Our tank hangs under the deck below the tower's centre; the team's boiler stands on its socket.
		const FVector EngineAt = bOwnGun ? FVector(0.f, 0.f, DeckHeight - TowerDeckThickness * TowerZScale)
			: TurretSocketInMesh(Tower, TEXT("engine")) * Stretch;
		FuelTank->SetRelativeLocation(EngineAt);
		// Hit box around the fuel tank, a bit bigger than it.
		const FRotator TankRotation = bOwnGun ? FRotator::ZeroRotator : TurretModelToActor;
		const FBox Box = FuelTank->GetStaticMesh()->GetBoundingBox();
		TankHitBox->SetBoxExtent(Box.GetExtent().ComponentMax(FVector(1.f)) * FVector(1.1f, 1.1f, 1.05f));
		TankHitBox->SetRelativeLocation(EngineAt + TankRotation.RotateVector(Box.GetCenter()));
		TankHitBox->SetRelativeRotation(TankRotation);
		return;
	}
	Tower->SetRelativeLocation(FVector(0.f, 0.f, TowerHeight * 0.5f));
	Tower->SetRelativeScale3D(FVector(3.f, 3.f, TowerHeight / 100.f));
	Platform->SetRelativeLocation(FVector(0.f, 0.f, TowerHeight + CannonPlatformHeight * 0.5f));
	const float Top = TowerHeight + CannonPlatformHeight;
	DeckHeight = Top;
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
	if (bDeckAtPlayerHeight)
	{
		UNemesisTargetSubsystem* T = UNemesisTargetSubsystem::Get(this);
		if (T && T->GetBalloon())
		{
			const float WantDeck = T->GetBasketLocation().Z + DeckHeightOffset - GetActorLocation().Z;
			if (bModel)
			{
				const float ModelDeck = TurretSocketInMesh(Tower, TEXT("head")).Z;
				TowerZScale = ModelDeck > 1.f ? FMath::Clamp(WantDeck / ModelDeck, 0.3f, 20.f) : 1.f;
			}
			else
			{
				TowerHeight = FMath::Max(300.f, WantDeck - CannonPlatformHeight);
			}
		}
	}
	// Also recomputes DeckHeight: plain members set in OnConstruction don't reach the PIE copy of a placed actor.
	ApplyLayout();
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
	if (State == ETowerCannonState::Destroyed)
	{
		TickWreck(DeltaSeconds);
		return;
	}
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
	FVector Muzzle = CannonPitch->GetComponentLocation() + CannonPitch->GetForwardVector() * (CannonBarrelLength * 0.9f);
	if (bOwnGun)
	{
		Muzzle = CannonPitch->GetComponentTransform().TransformPosition(FVector(TowerGunMuzzle, 0.f, 0.f));
	}
	else if (bModel)
	{
		Muzzle = Barrel->GetSocketLocation(bSecondMuzzle ? TEXT("muzzle_2") : TEXT("muzzle_1"));
		bSecondMuzzle = !bSecondMuzzle;
	}
	const FVector Dir = (Basket - Muzzle).GetSafeNormal(); // straight at the basket, no drop
	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (ANemesisBullet* Shell = GetWorld()->SpawnActor<ANemesisBullet>(ShellClass, Muzzle, Dir.Rotation(), Params))
	{
		Shell->Fire(Dir, ShellSpeed, Damage);
		Shell->SetActorScale3D(FVector(2.5f)); // a cannon shell, not a bullet
	}
	NemesisFx::FineExplosion(GetWorld(), Muzzle, 0.5f); // muzzle blast, fine-grained like the AA missile's
	OnShot();
}

void ANemesisTowerCannon::HandlePointDamage(AActor* DamagedActor, float InDamage, AController* InstigatedBy, FVector HitLocation,
	UPrimitiveComponent* HitComponent, FName BoneName, FVector ShotFromDirection, const UDamageType* DamageType, AActor* DamageCauser)
{
	if (State == ETowerCannonState::Destroyed) return;
	if (HitComponent == TankHitBox || HitComponent == FuelTank)
	{
		BlowUp(); // one hit on the fuel tank
		return;
	}
	if (HitComponent != Carriage && HitComponent != Barrel) return; // the tower ignores shots
	const int32 Hits = CannonHitsFor(DamageCauser);
	if (Hits <= 0) return; // another pellet of the same shotgun shot
	CannonHealth = FMath::Max(0.f, CannonHealth - 1.f / Hits);
	UE_LOG(LogTemp, Log, TEXT("%s: cannon hit (1/%d), %.2f left"), *GetName(), Hits, CannonHealth);
	if (CannonHealth <= KINDA_SMALL_NUMBER) BlowUp();
}

int32 ANemesisTowerCannon::CannonHitsFor(AActor* DamageCauser)
{
	// Gun bullets name the shooter as the causer; the gun is the one in the shooter's hand.
	const AFirstPersonCameraPawn* Pawn = Cast<AFirstPersonCameraPawn>(DamageCauser);
	const UGunComponent* Gun = Pawn ? Pawn->GetActiveGun() : nullptr;
	if (Cast<URevolverComponent>(Gun)) return RevolverCannonHits;
	if (Cast<URifleComponent>(Gun)) return RifleCannonHits;
	if (Cast<UShotgunComponent>(Gun))
	{
		const double Now = GetWorld()->GetTimeSeconds();
		const bool bSameShot = LastShotgunHitTime >= 0.0 && Now - LastShotgunHitTime <= ShotgunPelletWindow;
		LastShotgunHitTime = Now;
		return bSameShot ? 0 : ShotgunCannonHits;
	}
	return OtherCannonHits;
}

FVector ANemesisTowerCannon::DeckLocation() const
{
	return GetActorTransform().TransformPosition(FVector(0.f, 0.f, DeckHeight));
}

void ANemesisTowerCannon::BlowUp()
{
	SetState(ETowerCannonState::Destroyed);
	UNemesisRadarSubsystem::RemoveFromRadar(this); // the tower keeps standing
	UWorld* World = GetWorld();
	// The tank goes up and takes the cannon with it; the tower stays with an empty deck.
	// The AA missile's fine-grained blast (team decision 2026-11-15), not the shared cartoony one.
	NemesisFx::FineExplosion(World, FuelTank->Bounds.Origin, 3.f);
	NemesisFx::FineExplosion(World, CannonYaw->GetComponentLocation() + FVector(0.f, 0.f, 80.f), 2.f);
	for (UStaticMeshComponent* Part : { FuelTank.Get(), Carriage.Get(), Barrel.Get() })
	{
		Part->SetVisibility(false);
		NemesisCollision::None(Part);
	}
	NemesisCollision::None(TankHitBox);
	MuzzleGlow->SetVisibility(false);
	WreckTime = 0.f;
	WreckFire.Start(World, NemesisFx::EKind::Fire, 20.f);
	WreckSmoke.Start(World, NemesisFx::EKind::EngineSmoke, 10.f);
	UE_LOG(LogTemp, Log, TEXT("%s: the fuel tank and the cannon blew up"), *GetName());
	OnTowerDestroyed();
}

void ANemesisTowerCannon::TickWreck(float DeltaSeconds)
{
	WreckTime += DeltaSeconds;
	const FVector Deck = DeckLocation() + FVector(0.f, 0.f, 60.f);
	// The fire burns out quickly, the smoke lasts a bit longer.
	if (WreckFire.IsActive())
	{
		if (WreckTime < WreckSmokeTime * 0.35f) WreckFire.Tick(DeltaSeconds, Deck, FVector::UpVector, FVector::ZeroVector);
		else WreckFire.Stop();
	}
	if (WreckSmoke.IsActive())
	{
		if (WreckTime < WreckSmokeTime) WreckSmoke.Tick(DeltaSeconds, Deck, FVector::UpVector, FVector::ZeroVector);
		else WreckSmoke.Stop();
	}
	if (!WreckFire.IsActive() && !WreckSmoke.IsActive()) SetActorTickEnabled(false);
}
