#include "NemesisBeacon.h"

#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "NemesisAirship.h"
#include "NemesisPlaceholder.h"
#include "NemesisSearchlight.h"
#include "NemesisTargetSubsystem.h"
#include "UObject/ConstructorHelpers.h"

ANemesisBeacon::ANemesisBeacon()
{
	PrimaryActorTick.bCanEverTick = true;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Iron(TEXT("/Game/Assets/Nemesis/MI_Nemesis_Iron.MI_Nemesis_Iron"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Glow(TEXT("/Game/Assets/Nemesis/MI_Nemesis_Glow.MI_Nemesis_Glow"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> BeamMesh(TEXT("/Game/Assets/Enemies/FBX/SM_LightBeam.SM_LightBeam"));

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
	Beam->SetLightColor(NemesisSearchlight::Searching);
	Beam->SetIntensity(5000000.f);
	Beam->SetCastShadows(false);

	BeamCone = NemesisSearchlight::CreateBeam(this, LampPivot, BeamMesh.Object, TEXT("BeamCone"));
	BeamCone->SetRelativeLocation(FVector(70.f, 0.f, 0.f));
}

void ANemesisBeacon::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Tower->SetRelativeLocation(FVector(0.f, 0.f, TowerHeight * 0.5f));
	Tower->SetRelativeScale3D(FVector(2.f, 2.f, TowerHeight / 100.f));
	LampPivot->SetRelativeLocation(FVector(0.f, 0.f, TowerHeight + 60.f));
	NemesisSearchlight::Configure(Beam, BeamCone, BeamAngle, Range);
	BeamPitch = SearchPitch;
	LampPivot->SetRelativeRotation(FRotator(BeamPitch, SweepYaw, 0.f));
}

void ANemesisBeacon::BeginPlay()
{
	Super::BeginPlay();
	OnTakePointDamage.AddUniqueDynamic(this, &ANemesisBeacon::HandlePointDamage);
	BeamPitch = SearchPitch;
	NemesisSearchlight::SetColor(Beam, BeamCone, NemesisSearchlight::Searching);
}

void ANemesisBeacon::SetLocked(bool bLocked)
{
	if (bLocked == bLit) return;
	bLit = bLocked;
	LitTime = 0.f;
	if (!bBroken) NemesisSearchlight::SetColor(Beam, BeamCone, bLit ? NemesisSearchlight::Locked : NemesisSearchlight::Searching);
	UE_LOG(LogTemp, Log, TEXT("%s: %s"), *GetName(), bLit ? TEXT("locked on the balloon") : TEXT("lock lost"));
	OnLitChanged(bLit);
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

	UNemesisTargetSubsystem* T = UNemesisTargetSubsystem::Get(this);
	CooldownLeft = FMath::Max(0.f, CooldownLeft - DeltaSeconds);
	if (bLit && T && T->GetBalloon())
	{
		// Locked for good: the red beam follows the basket in yaw and pitch, no sweep limits.
		const FVector To = GetActorTransform().InverseTransformVector(T->GetBasketLocation() - LampPivot->GetComponentLocation());
		const FRotator Now = FMath::RInterpConstantTo(FRotator(BeamPitch, SweepYaw, 0.f), To.Rotation(), DeltaSeconds, TrackSpeed);
		BeamPitch = Now.Pitch;
		SweepYaw = FRotator::NormalizeAxis(Now.Yaw);
		LampPivot->SetRelativeRotation(FRotator(BeamPitch, SweepYaw, 0.f));

		LitTime += DeltaSeconds;
		const bool bInRange = FVector::DistSquared(T->GetBasketLocation(), Lamp->GetComponentLocation()) <= FMath::Square(Range);
		if (bInRange && LitTime >= SpotTime && CooldownLeft <= 0.f)
		{
			CooldownLeft = Cooldown;
			ANemesisAirship* Airship = T->GetAirship();
			const bool bLaunched = Airship && Airship->RequestCropDuster();
			UE_LOG(LogTemp, Log, TEXT("%s: balloon spotted, crop duster %s"), *GetName(),
				bLaunched ? TEXT("launched") : (Airship ? TEXT("not launched (limit)") : TEXT("not launched (no airship)")));
			OnBalloonSpotted(bLaunched);
		}
		return;
	}

	// Searching: ping-pong sweep around the actor's forward, beam tilted up by SearchPitch.
	SweepYaw += SweepDir * SweepSpeed * DeltaSeconds;
	if (FMath::Abs(SweepYaw) >= SweepAngle)
	{
		SweepYaw = FMath::Clamp(SweepYaw, -SweepAngle, SweepAngle);
		SweepDir = -SweepDir;
	}
	BeamPitch = FMath::FInterpConstantTo(BeamPitch, SearchPitch, DeltaSeconds, TrackSpeed);
	LampPivot->SetRelativeRotation(FRotator(BeamPitch, SweepYaw, 0.f));
	if (IsBasketInBeam()) SetLocked(true);
}

void ANemesisBeacon::HandlePointDamage(AActor* DamagedActor, float InDamage, AController* InstigatedBy, FVector HitLocation,
	UPrimitiveComponent* HitComponent, FName BoneName, FVector ShotFromDirection, const UDamageType* DamageType, AActor* DamageCauser)
{
	if (bBroken || HitComponent != Lamp) return;
	bBroken = true;
	if (bLit)
	{
		bLit = false;
		OnLitChanged(false);
	}
	Beam->SetVisibility(false);
	BeamCone->SetVisibility(false);
	Lamp->SetVisibility(false);
	NemesisCollision::None(Lamp);
	UE_LOG(LogTemp, Log, TEXT("%s: lamp broken"), *GetName());
	OnLampBroken();
}
