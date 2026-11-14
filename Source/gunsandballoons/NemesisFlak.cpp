#include "NemesisFlak.h"

#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "NemesisBullet.h"
#include "NemesisPlaceholder.h"
#include "NemesisSearchlight.h"
#include "NemesisTargetSubsystem.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	constexpr float BaseHeight = 150.f;
	constexpr float BarrelLength = 240.f;
}

ANemesisFlak::ANemesisFlak()
{
	PrimaryActorTick.bCanEverTick = true;
	BulletClass = ANemesisBullet::StaticClass();

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Iron(TEXT("/Game/Assets/Nemesis/MI_Nemesis_Iron.MI_Nemesis_Iron"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Glow(TEXT("/Game/Assets/Nemesis/MI_Nemesis_Glow.MI_Nemesis_Glow"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> BeamMesh(TEXT("/Game/Assets/Enemies/FBX/SM_LightBeam.SM_LightBeam"));

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Base = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Base"));
	Base->SetupAttachment(Root);
	Base->SetStaticMesh(Cube.Object);
	Base->SetMaterial(0, Iron.Object);
	Base->SetRelativeLocation(FVector(0.f, 0.f, BaseHeight * 0.5f));
	Base->SetRelativeScale3D(FVector(3.f, 3.f, BaseHeight / 100.f));
	NemesisCollision::Solid(Base);

	TurretYaw = CreateDefaultSubobject<USceneComponent>(TEXT("TurretYaw"));
	TurretYaw->SetupAttachment(Root);
	TurretYaw->SetRelativeLocation(FVector(0.f, 0.f, BaseHeight));

	TurretBody = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TurretBody"));
	TurretBody->SetupAttachment(TurretYaw);
	TurretBody->SetStaticMesh(Cube.Object);
	TurretBody->SetMaterial(0, Iron.Object);
	TurretBody->SetRelativeLocation(FVector(0.f, 0.f, 40.f));
	TurretBody->SetRelativeScale3D(FVector(1.6f, 1.6f, 0.8f));
	NemesisCollision::Solid(TurretBody);

	TurretPitch = CreateDefaultSubobject<USceneComponent>(TEXT("TurretPitch"));
	TurretPitch->SetupAttachment(TurretYaw);
	TurretPitch->SetRelativeLocation(FVector(0.f, 0.f, 90.f));

	Barrel = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Barrel"));
	Barrel->SetupAttachment(TurretPitch);
	Barrel->SetStaticMesh(Cube.Object);
	Barrel->SetMaterial(0, Iron.Object);
	Barrel->SetRelativeLocation(FVector(BarrelLength * 0.5f, 0.f, 0.f));
	Barrel->SetRelativeScale3D(FVector(BarrelLength / 100.f, 0.25f, 0.25f));
	NemesisCollision::None(Barrel);

	Lamp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Lamp"));
	Lamp->SetupAttachment(TurretPitch);
	Lamp->SetStaticMesh(Cube.Object);
	Lamp->SetMaterial(0, Glow.Object);
	Lamp->SetRelativeLocation(FVector(20.f, 0.f, 55.f));
	Lamp->SetRelativeScale3D(FVector(0.5f, 0.6f, 0.6f));
	NemesisCollision::None(Lamp);

	Light = CreateDefaultSubobject<USpotLightComponent>(TEXT("Light"));
	Light->SetupAttachment(TurretPitch);
	Light->SetRelativeLocation(FVector(50.f, 0.f, 55.f));
	Light->SetLightColor(NemesisSearchlight::Searching);
	Light->SetIntensity(5000000.f);
	Light->SetCastShadows(false);

	BeamCone = NemesisSearchlight::CreateBeam(this, TurretPitch, BeamMesh.Object, TEXT("BeamCone"));
	BeamCone->SetRelativeLocation(FVector(50.f, 0.f, 55.f));
}

void ANemesisFlak::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	NemesisSearchlight::Configure(Light, BeamCone, BeamAngle, BeamLength);
	Pitch = SearchPitch;
	TurretPitch->SetRelativeRotation(FRotator(Pitch, 0.f, 0.f));
}

void ANemesisFlak::BeginPlay()
{
	Super::BeginPlay();
	Pitch = SearchPitch;
	NemesisSearchlight::SetColor(Light, BeamCone, NemesisSearchlight::Searching);
}

bool ANemesisFlak::CanSeeBasket() const
{
	UNemesisTargetSubsystem* T = UNemesisTargetSubsystem::Get(this);
	AActor* Balloon = T ? T->GetBalloon() : nullptr;
	if (!Balloon) return false;

	const FVector From = TurretPitch->GetComponentLocation();
	const FVector Basket = T->GetBasketLocation();
	const float Radius = EngageRadius + (bEngaged ? ReleaseMargin : 0.f);
	if (FVector::DistSquared(From, Basket) > FMath::Square(Radius)) return false;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(FlakSight), false, this);
	Params.AddIgnoredActor(Balloon);
	TArray<AActor*> Riders;
	Balloon->GetAttachedActors(Riders, true, true);
	Params.AddIgnoredActors(Riders);
	FHitResult Hit;
	return !GetWorld()->LineTraceSingleByObjectType(Hit, From, Basket, FCollisionObjectQueryParams(ECC_WorldStatic), Params);
}

void ANemesisFlak::SetEngaged(bool bNow)
{
	if (bNow == bEngaged) return;
	bEngaged = bNow;
	EngagedTime = 0.f;
	ShotsLeftInBurst = 0;
	NextShotIn = WarmUp;
	NemesisSearchlight::SetColor(Light, BeamCone, bEngaged ? NemesisSearchlight::Locked : NemesisSearchlight::Searching);
	UE_LOG(LogTemp, Log, TEXT("%s: %s"), *GetName(), bEngaged ? TEXT("engaging the balloon") : TEXT("searching"));
	OnEngagedChanged(bEngaged);
}

void ANemesisFlak::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	SetEngaged(CanSeeBasket());

	if (!bEngaged)
	{
		// Searching: the light circles the sky.
		Yaw = FRotator::NormalizeAxis(Yaw + SearchSpeed * DeltaSeconds);
		Pitch = FMath::FInterpConstantTo(Pitch, SearchPitch, DeltaSeconds, TrackSpeed);
		TurretYaw->SetRelativeRotation(FRotator(0.f, Yaw, 0.f));
		TurretPitch->SetRelativeRotation(FRotator(Pitch, 0.f, 0.f));
		return;
	}

	// Engaged: aim at where the basket will be when the bullet arrives (straight bullets, no drop).
	UNemesisTargetSubsystem* T = UNemesisTargetSubsystem::Get(this);
	const FVector Muzzle = TurretPitch->GetComponentLocation();
	const FVector Basket = T->GetBasketLocation();
	const FVector Vel = T->GetBalloon()->GetVelocity();
	FVector Aim = Basket;
	for (int32 i = 0; i < 2; ++i)
	{
		Aim = Basket + Vel * (FVector::Dist(Muzzle, Aim) / BulletSpeed);
	}

	const FRotator Want = GetActorTransform().InverseTransformVector(Aim - Muzzle).Rotation();
	const FRotator Now = FMath::RInterpConstantTo(FRotator(Pitch, Yaw, 0.f), Want, DeltaSeconds, TrackSpeed);
	Pitch = FMath::Clamp(Now.Pitch, -10.f, 89.f);
	Yaw = FRotator::NormalizeAxis(Now.Yaw);
	TurretYaw->SetRelativeRotation(FRotator(0.f, Yaw, 0.f));
	TurretPitch->SetRelativeRotation(FRotator(Pitch, 0.f, 0.f));

	EngagedTime += DeltaSeconds;
	NextShotIn -= DeltaSeconds;
	if (NextShotIn > 0.f) return;

	const FVector AimDir = (Aim - Muzzle).GetSafeNormal();
	const float ErrorDeg = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(TurretPitch->GetForwardVector(), AimDir), -1.f, 1.f)));
	if (ShotsLeftInBurst <= 0)
	{
		if (ErrorDeg > FireTolerance) return; // still turning: start the burst once on target
		ShotsLeftInBurst = BurstShots;
	}
	FireShot(Aim);
	--ShotsLeftInBurst;
	NextShotIn = ShotsLeftInBurst > 0 ? ShotInterval : BurstCooldown;
}

void ANemesisFlak::FireShot(const FVector& AimPoint)
{
	if (!BulletClass) return;
	const FVector Muzzle = TurretPitch->GetComponentLocation() + TurretPitch->GetForwardVector() * BarrelLength;
	const FVector Dir = FMath::VRandCone((AimPoint - Muzzle).GetSafeNormal(), FMath::DegreesToRadians(Spread));
	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (ANemesisBullet* Bullet = GetWorld()->SpawnActor<ANemesisBullet>(BulletClass, Muzzle, Dir.Rotation(), Params))
	{
		Bullet->Fire(Dir, BulletSpeed, Damage);
	}
	OnShot();
}
