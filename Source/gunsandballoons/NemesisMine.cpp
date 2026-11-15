#include "NemesisMine.h"

#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "NemesisFx.h"
#include "NemesisPlaceholder.h"
#include "NemesisTargetSubsystem.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	void SetupShootable(UStaticMeshComponent* C)
	{
		C->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		C->SetCollisionObjectType(ECC_WorldDynamic);
		C->SetCollisionResponseToAllChannels(ECR_Ignore);
		C->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
		C->SetGenerateOverlapEvents(false);
		C->SetCanEverAffectNavigation(false);
	}
}

ANemesisMine::ANemesisMine()
{
	PrimaryActorTick.bCanEverTick = true;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Iron(TEXT("/Game/Assets/Nemesis/MI_Nemesis_Iron.MI_Nemesis_Iron"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Red(TEXT("/Game/Assets/Nemesis/MI_Nemesis_Red.MI_Nemesis_Red"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Canvas(TEXT("/Game/Assets/Nemesis/MI_Nemesis_Canvas.MI_Nemesis_Canvas"));
	IdleMaterial = Iron.Object;
	WarnMaterial = Red.Object;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	MineMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mine"));
	MineMesh->SetupAttachment(Root);
	MineMesh->SetStaticMesh(Cube.Object);
	MineMesh->SetMaterial(0, Iron.Object);
	MineMesh->SetRelativeScale3D(FVector(0.8f));
	SetupShootable(MineMesh);

	// Flat 3 x 3 m canopy 4 m above the mine.
	Parachute = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Parachute"));
	Parachute->SetupAttachment(Root);
	Parachute->SetStaticMesh(Cube.Object);
	Parachute->SetMaterial(0, Canvas.Object);
	Parachute->SetRelativeLocation(FVector(0.f, 0.f, 400.f));
	Parachute->SetRelativeScale3D(FVector(3.f, 3.f, 0.3f));
	SetupShootable(Parachute);

	// Model (Content/Assets/Enemies/Mine.blend): body + parachute with its lines (canopy at +4 m).
	static ConstructorHelpers::FObjectFinder<UStaticMesh> BodyMesh(TEXT("/Game/Assets/Enemies/FBX/SM_Mine_Body.SM_Mine_Body"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ChuteMesh(TEXT("/Game/Assets/Enemies/FBX/SM_Mine_Parachute.SM_Mine_Parachute"));
	if (NemesisModel::Use(MineMesh, BodyMesh.Object))
	{
		NemesisModel::Use(Parachute, ChuteMesh.Object);
	}

	WarningLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("WarningLight"));
	WarningLight->SetupAttachment(Root);
	WarningLight->SetLightColor(FLinearColor(1.f, 0.05f, 0.02f));
	WarningLight->SetIntensity(100000.f);
	WarningLight->SetAttenuationRadius(1500.f);
	WarningLight->SetCastShadows(false);
	WarningLight->SetVisibility(false);
}

void ANemesisMine::Arm(float DesiredFallSpeed)
{
	FallSpeed = DesiredFallSpeed > 0.f ? FMath::Clamp(DesiredFallSpeed, MinFallSpeed, MaxFallSpeed) : MaxFallSpeed;
	OnTakePointDamage.AddUniqueDynamic(this, &ANemesisMine::HandlePointDamage);
}

void ANemesisMine::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UNemesisTargetSubsystem* A = UNemesisTargetSubsystem::Get(this);
	if (!A || !A->GetBalloon() || bExploded)
	{
		if (!bExploded) Destroy();
		return;
	}
	Age += DeltaSeconds;

	if (CutTime >= 0.f)
	{
		AddActorWorldOffset(FVector(0.f, 0.f, -CutFallSpeed * DeltaSeconds));
		if (Age - CutTime >= CutExplodeDelay) Explode(false);
		return;
	}

	AddActorWorldOffset(FVector(0.f, 0.f, -FallSpeed * DeltaSeconds));

	const float Dist = FVector::Dist(GetActorLocation(), A->GetBasketLocation());
	const bool bInRadius = Dist <= Radius;
	if (bInRadius != bWarning)
	{
		bWarning = bInRadius;
		MineMesh->SetMaterial(0, bWarning ? WarnMaterial.Get() : IdleMaterial.Get());
		WarningLight->SetVisibility(bWarning);
	}

	if (Dist <= TriggerDistance)
	{
		Explode(true);
	}
	else if (Age >= LifeTime || IsPassed())
	{
		Destroy();
	}
}

bool ANemesisMine::IsPassed() const
{
	UNemesisTargetSubsystem* A = UNemesisTargetSubsystem::Get(this);
	if (!A || !A->GetBalloon()) return false;
	return FVector::DotProduct(GetActorLocation() - A->GetBasketLocation(), A->GetBalloonForward()) < -PassedDistance;
}

void ANemesisMine::HandlePointDamage(AActor* DamagedActor, float InDamage, AController* InstigatedBy, FVector HitLocation,
	UPrimitiveComponent* HitComponent, FName BoneName, FVector ShotFromDirection, const UDamageType* DamageType, AActor* DamageCauser)
{
	if (bExploded || CutTime >= 0.f) return;
	if (HitComponent == Parachute)
	{
		CutTime = Age;
		Parachute->SetVisibility(false);
		Parachute->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		WarningLight->SetVisibility(false);
		UE_LOG(LogTemp, Log, TEXT("Mine parachute shot: falling"));
		OnParachuteCut();
	}
	else
	{
		Explode(true);
	}
}

void ANemesisMine::Explode(bool bCanDamage)
{
	if (bExploded) return;
	bExploded = true;
	bool bDamaged = false;
	UNemesisTargetSubsystem* A = UNemesisTargetSubsystem::Get(this);
	if (bCanDamage && A && A->GetBalloon() && FVector::Dist(GetActorLocation(), A->GetBasketLocation()) <= Radius)
	{
		A->DamageBalloon(Damage, this);
		bDamaged = true;
	}
	UE_LOG(LogTemp, Log, TEXT("Mine exploded (%s)"), bDamaged ? TEXT("balloon in radius") : TEXT("harmless"));
	NemesisFx::Explosion(GetWorld(), GetActorLocation(), 1.2f);
	OnExploded(bDamaged);
	Destroy();
}
