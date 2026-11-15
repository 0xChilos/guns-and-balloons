#include "NemesisBeacon.h"

#include "Components/BoxComponent.h"
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
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Dark(TEXT("/Game/Assets/Nemesis/MI_Nemesis_Dark.MI_Nemesis_Dark"));
	BrokenLampMaterial = Dark.Object;
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
	Lamp->SetRelativeScale3D(FVector(1.6f, 1.6f, 1.6f)); // a bigger target (was 1.2 m)
	NemesisCollision::Shootable(Lamp);

	LampHitBox = CreateDefaultSubobject<UBoxComponent>(TEXT("LampHitBox"));
	LampHitBox->SetupAttachment(LampPivot);
	NemesisCollision::Shootable(LampHitBox);

	Beam = CreateDefaultSubobject<USpotLightComponent>(TEXT("Beam"));
	Beam->SetupAttachment(LampPivot);
	Beam->SetRelativeLocation(FVector(90.f, 0.f, 0.f));
	Beam->SetLightColor(NemesisSearchlight::Searching);
	Beam->SetIntensity(5000000.f);
	Beam->SetCastShadows(false);

	BeamCone = NemesisSearchlight::CreateBeam(this, LampPivot, BeamMesh.Object, TEXT("BeamCone"));
	BeamCone->SetRelativeLocation(FVector(90.f, 0.f, 0.f));
}

void ANemesisBeacon::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyLayout();
	NemesisSearchlight::Configure(Beam, BeamCone, BeamAngle, Range);
	BeamPitch = SearchPitch;
	LampPivot->SetRelativeRotation(FRotator(BeamPitch, SweepYaw, 0.f));
}

void ANemesisBeacon::ApplyLayout()
{
	LampHitBox->SetBoxExtent(LampHitBoxExtent);
	Tower->SetRelativeLocation(FVector(0.f, 0.f, TowerHeight * 0.5f));
	Tower->SetRelativeScale3D(FVector(2.f, 2.f, TowerHeight / 100.f));
	LampPivot->SetRelativeLocation(FVector(0.f, 0.f, TowerHeight + 80.f));
}

void ANemesisBeacon::BeginPlay()
{
	Super::BeginPlay();
	if (bLampAtPlayerHeight)
	{
		UNemesisTargetSubsystem* T = UNemesisTargetSubsystem::Get(this);
		if (T && T->GetBalloon())
		{
			const float LampZ = T->GetBasketLocation().Z + LampHeightOffset;
			TowerHeight = FMath::Clamp(LampZ - GetActorLocation().Z - 80.f, 300.f, 30000.f);
			ApplyLayout();
		}
	}
	OnTakePointDamage.AddUniqueDynamic(this, &ANemesisBeacon::HandlePointDamage);
	BeamPitch = SearchPitch;
	NemesisSearchlight::SetLockedState(Beam, BeamCone, false);
}

void ANemesisBeacon::SetLocked(bool bLocked)
{
	if (bLocked == bLit) return;
	bLit = bLocked;
	LitTime = 0.f;
	bCalledThisLock = false;
	if (!bBroken) NemesisSearchlight::SetLockedState(Beam, BeamCone, bLit);
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
	if (To.SizeSquared() > FMath::Square(FMath::Min(Range, TrackRange))) return false; // never spot what it couldn't track

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
	if (bBroken)
	{
		// The shattered lamp smokes for a while, then stops; the tower stays.
		BrokenTime += DeltaSeconds;
		if (BrokenSmoke.IsActive())
		{
			if (BrokenTime < BrokenSmokeTime) BrokenSmoke.Tick(DeltaSeconds, Lamp->GetComponentLocation() + FVector(0.f, 0.f, 50.f), FVector::UpVector, FVector::ZeroVector);
			else BrokenSmoke.Stop();
		}
		if (!BrokenSmoke.IsActive() && BrokenTime >= BrokenSmokeTime) SetActorTickEnabled(false);
		return;
	}

	UNemesisTargetSubsystem* T = UNemesisTargetSubsystem::Get(this);
	CooldownLeft = FMath::Max(0.f, CooldownLeft - DeltaSeconds);
	if (bLit && T && T->GetBalloon())
	{
		// Too far from the beacon: let go and search again.
		if (FVector::DistSquared(T->GetBasketLocation(), Lamp->GetComponentLocation()) > FMath::Square(TrackRange))
		{
			SetLocked(false);
		}
		else
		{
			// Locked: the red beam follows the basket in yaw and pitch, no sweep limits.
			const FVector To = GetActorTransform().InverseTransformVector(T->GetBasketLocation() - LampPivot->GetComponentLocation());
			const FRotator Now = FMath::RInterpConstantTo(FRotator(BeamPitch, SweepYaw, 0.f), To.Rotation(), DeltaSeconds, TrackSpeed);
			BeamPitch = Now.Pitch;
			SweepYaw = FRotator::NormalizeAxis(Now.Yaw);
			LampPivot->SetRelativeRotation(FRotator(BeamPitch, SweepYaw, 0.f));

			LitTime += DeltaSeconds;
			if (!bCalledThisLock && LitTime >= SpotTime && CooldownLeft <= 0.f)
			{
				bCalledThisLock = true;
				CooldownLeft = Cooldown;
				ANemesisAirship* Airship = T->GetAirship();
				const int32 Launched = Airship ? Airship->RequestCropDusters(PlanesPerCall, 1) : 0;
				UE_LOG(LogTemp, Log, TEXT("%s: balloon spotted, %d crop duster(s) launched%s"), *GetName(), Launched,
					Airship ? TEXT("") : TEXT(" (no airship in the level)"));
				OnBalloonSpotted(Launched > 0);
			}
			return;
		}
	}

	// Searching: ping-pong sweep around the actor's forward, beam tilted up by SearchPitch.
	if (FMath::Abs(SweepYaw) > SweepAngle)
	{
		// Just lost the balloon outside the sweep: swing back into it smoothly.
		SweepYaw = FMath::FInterpConstantTo(SweepYaw, FMath::Clamp(SweepYaw, -SweepAngle, SweepAngle), DeltaSeconds, TrackSpeed);
		SweepDir = SweepYaw > 0.f ? -1.f : 1.f;
	}
	else
	{
		SweepYaw += SweepDir * SweepSpeed * DeltaSeconds;
		if (FMath::Abs(SweepYaw) >= SweepAngle)
		{
			SweepYaw = FMath::Clamp(SweepYaw, -SweepAngle, SweepAngle);
			SweepDir = -SweepDir;
		}
	}
	BeamPitch = FMath::FInterpConstantTo(BeamPitch, SearchPitch, DeltaSeconds, TrackSpeed);
	LampPivot->SetRelativeRotation(FRotator(BeamPitch, SweepYaw, 0.f));
	if (IsBasketInBeam()) SetLocked(true);
}

void ANemesisBeacon::HandlePointDamage(AActor* DamagedActor, float InDamage, AController* InstigatedBy, FVector HitLocation,
	UPrimitiveComponent* HitComponent, FName BoneName, FVector ShotFromDirection, const UDamageType* DamageType, AActor* DamageCauser)
{
	if (bBroken || (HitComponent != Lamp && HitComponent != LampHitBox)) return;
	bBroken = true;
	if (bLit)
	{
		bLit = false;
		OnLitChanged(false);
	}
	Beam->SetVisibility(false);
	BeamCone->SetVisibility(false);
	// The lamp shatters but stays on the tower: dark, knocked askew, smoking.
	const FVector LampAt = Lamp->GetComponentLocation();
	NemesisFx::FineExplosion(GetWorld(), LampAt, 0.45f);
	if (BrokenLampMaterial)
	{
		for (int32 i = 0; i < Lamp->GetNumMaterials(); ++i) Lamp->SetMaterial(i, BrokenLampMaterial);
	}
	Lamp->AddLocalRotation(FRotator(-25.f, 15.f, 10.f));
	Lamp->SetRelativeScale3D(Lamp->GetRelativeScale3D() * 0.85f);
	NemesisCollision::None(Lamp);
	NemesisCollision::None(LampHitBox);
	BrokenTime = 0.f;
	BrokenSmoke.Start(GetWorld(), NemesisFx::EKind::EngineSmoke, 10.f);
	UE_LOG(LogTemp, Log, TEXT("%s: lamp broken"), *GetName());
	OnLampBroken();
}
