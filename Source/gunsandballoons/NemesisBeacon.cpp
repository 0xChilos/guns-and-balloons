#include "NemesisBeacon.h"

#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "NemesisAirship.h"
#include "NemesisPlaceholder.h"
#include "NemesisTargetSubsystem.h"
#include "UObject/ConstructorHelpers.h"

ANemesisBeacon::ANemesisBeacon()
{
	PrimaryActorTick.bCanEverTick = true;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Iron(TEXT("/Game/Assets/Nemesis/MI_Nemesis_Iron.MI_Nemesis_Iron"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Glow(TEXT("/Game/Assets/Nemesis/MI_Nemesis_Glow.MI_Nemesis_Glow"));

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Tower = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Tower"));
	Tower->SetupAttachment(Root);
	Tower->SetStaticMesh(Cube.Object);
	Tower->SetMaterial(0, Iron.Object);
	NemesisCollision::Solid(Tower);

	LampPivot = CreateDefaultSubobject<USceneComponent>(TEXT("LampPivot"));
	LampPivot->SetupAttachment(Root);

	Lamp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Lamp"));
	Lamp->SetupAttachment(LampPivot);
	Lamp->SetStaticMesh(Cube.Object);
	Lamp->SetMaterial(0, Glow.Object);
	Lamp->SetRelativeScale3D(FVector(1.2f, 1.2f, 1.2f));
	NemesisCollision::Shootable(Lamp);

	Beam = CreateDefaultSubobject<USpotLightComponent>(TEXT("Beam"));
	Beam->SetupAttachment(LampPivot);
	Beam->SetRelativeLocation(FVector(70.f, 0.f, 0.f));
	Beam->SetLightColor(FLinearColor(1.f, 0.9f, 0.6f));
	Beam->SetIntensity(5000000.f);
	Beam->SetCastShadows(false);
}

void ANemesisBeacon::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Tower->SetRelativeLocation(FVector(0.f, 0.f, TowerHeight * 0.5f));
	Tower->SetRelativeScale3D(FVector(2.f, 2.f, TowerHeight / 100.f));
	LampPivot->SetRelativeLocation(FVector(0.f, 0.f, TowerHeight + 60.f));
	Beam->SetAttenuationRadius(Range);
	Beam->SetOuterConeAngle(BeamAngle * 0.5f);
	Beam->SetInnerConeAngle(BeamAngle * 0.35f);
}

void ANemesisBeacon::BeginPlay()
{
	Super::BeginPlay();
	OnTakePointDamage.AddUniqueDynamic(this, &ANemesisBeacon::HandlePointDamage);
}

bool ANemesisBeacon::IsBasketInBeam() const
{
	UNemesisTargetSubsystem* T = UNemesisTargetSubsystem::Get(this);
	AActor* Balloon = T ? T->GetBalloon() : nullptr;
	if (!Balloon) return false;

	const FVector From = Lamp->GetComponentLocation();
	const FVector Basket = T->GetBasketLocation();
	const FVector To = Basket - From;
	if (To.SizeSquared() > FMath::Square(Range)) return false;

	// Vertical fan: compare only the horizontal angle.
	const FVector BeamDir = FVector(LampPivot->GetForwardVector().X, LampPivot->GetForwardVector().Y, 0.f).GetSafeNormal();
	const FVector ToFlat = FVector(To.X, To.Y, 0.f).GetSafeNormal();
	if (FVector::DotProduct(BeamDir, ToFlat) < FMath::Cos(FMath::DegreesToRadians(BeamAngle * 0.5f))) return false;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(BeaconSight), false, this);
	Params.AddIgnoredActor(Balloon);
	TArray<AActor*> Riders;
	Balloon->GetAttachedActors(Riders, true, true);
	Params.AddIgnoredActors(Riders);
	FHitResult Hit;
	return !GetWorld()->LineTraceSingleByObjectType(Hit, From, Basket, FCollisionObjectQueryParams(ECC_WorldStatic), Params);
}

void ANemesisBeacon::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bBroken) return;

	CooldownLeft = FMath::Max(0.f, CooldownLeft - DeltaSeconds);
	if (bLit && CooldownLeft <= 0.f)
	{
		// Locked on: the beam follows the basket (within the sweep limits).
		UNemesisTargetSubsystem* T = UNemesisTargetSubsystem::Get(this);
		const FVector To = GetActorTransform().InverseTransformVector(T->GetBasketLocation() - Lamp->GetComponentLocation());
		const float WantYaw = FMath::Clamp(To.Rotation().Yaw, -SweepAngle, SweepAngle);
		SweepYaw = FMath::FInterpConstantTo(SweepYaw, WantYaw, DeltaSeconds, TrackSpeed);
	}
	else
	{
		// Ping-pong sweep around the actor's forward.
		SweepYaw += SweepDir * SweepSpeed * DeltaSeconds;
		if (FMath::Abs(SweepYaw) >= SweepAngle)
		{
			SweepYaw = FMath::Clamp(SweepYaw, -SweepAngle, SweepAngle);
			SweepDir = -SweepDir;
		}
	}
	LampPivot->SetRelativeRotation(FRotator(0.f, SweepYaw, 0.f));

	const bool bNowLit = IsBasketInBeam();
	if (bNowLit != bLit)
	{
		bLit = bNowLit;
		OnLitChanged(bLit);
	}
	LitTime = bLit ? LitTime + DeltaSeconds : 0.f;

	if (bLit && LitTime >= SpotTime && CooldownLeft <= 0.f)
	{
		CooldownLeft = Cooldown;
		LitTime = 0.f;
		UNemesisTargetSubsystem* T = UNemesisTargetSubsystem::Get(this);
		ANemesisAirship* Airship = T ? T->GetAirship() : nullptr;
		const bool bLaunched = Airship && Airship->RequestCropDuster();
		UE_LOG(LogTemp, Log, TEXT("%s: balloon spotted, crop duster %s"), *GetName(),
			bLaunched ? TEXT("launched") : (Airship ? TEXT("not launched (limit)") : TEXT("not launched (no airship)")));
		OnBalloonSpotted(bLaunched);
	}
}

void ANemesisBeacon::HandlePointDamage(AActor* DamagedActor, float InDamage, AController* InstigatedBy, FVector HitLocation,
	UPrimitiveComponent* HitComponent, FName BoneName, FVector ShotFromDirection, const UDamageType* DamageType, AActor* DamageCauser)
{
	if (bBroken || HitComponent != Lamp) return;
	bBroken = true;
	bLit = false;
	Beam->SetVisibility(false);
	Lamp->SetVisibility(false);
	NemesisCollision::None(Lamp);
	UE_LOG(LogTemp, Log, TEXT("%s: lamp broken"), *GetName());
	OnLampBroken();
}
