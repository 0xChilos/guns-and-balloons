#include "NemesisCropDuster.h"

#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "NemesisTargetSubsystem.h"
#include "NemesisBullet.h"
#include "NemesisPlaceholder.h"
#include "UObject/ConstructorHelpers.h"

ANemesisCropDuster::ANemesisCropDuster()
{
	BulletClass = ANemesisBullet::StaticClass();
	MaxSpeed = 2500.f;

	// Model (Content/Assets/Enemies/CropDuster.blend). Pivots from the Blender file (Blender +Y = Unreal -Y).
	static ConstructorHelpers::FObjectFinder<UStaticMesh> BodyMesh(TEXT("/Game/Assets/Enemies/FBX/SM_CropDuster_Body.SM_CropDuster_Body"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> EngineMesh(TEXT("/Game/Assets/Enemies/FBX/SM_CropDuster_Engine.SM_CropDuster_Engine"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> PropMesh(TEXT("/Game/Assets/Enemies/FBX/SM_CropDuster_Prop.SM_CropDuster_Prop"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> GunMesh(TEXT("/Game/Assets/Enemies/FBX/SM_CropDuster_Gun.SM_CropDuster_Gun"));
	if (NemesisModel::Use(Fuselage, BodyMesh.Object))
	{
		NemesisModel::Use(Engine, EngineMesh.Object);
		NemesisCollision::None(Wings); // the body mesh has the wings
		Wings->SetVisibility(false);
		if (UStaticMeshComponent* P = MakePart(TEXT("Prop"), PropMesh.Object, FVector(340.f, 0.f, 0.f)))
		{
			Props.Add(P);
			EngineZones.Add(P);
		}
		Turret = MakePart(TEXT("Turret"), GunMesh.Object, FVector(-90.f, 0.f, 80.f));
	}
	else
	{
		static ConstructorHelpers::FObjectFinder<UMaterialInterface> Red(TEXT("/Game/Assets/Nemesis/MI_Nemesis_Red.MI_Nemesis_Red"));
		Fuselage->SetMaterial(0, Red.Object); // cube fallback: red body tells it apart from the bomber
	}
}

void ANemesisCropDuster::Launch(ANemesisAirship* InAirship)
{
	Super::Launch(InAirship);
	BurstTimer = BurstCooldown * 0.5f;
}

FVector ANemesisCropDuster::GetSlotLocation() const
{
	UNemesisTargetSubsystem* T = UNemesisTargetSubsystem::Get(this);
	const AActor* B = T ? T->GetBalloon() : nullptr;
	if (!B) return GetActorLocation();
	const FVector Fwd = GetBalloonForward();
	const FVector Right = FVector::CrossProduct(FVector::UpVector, Fwd);
	return B->GetActorLocation() + Fwd * AheadDistance + Right * (SideDistance * Side);
}

void ANemesisCropDuster::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	// The automatic turret keeps its barrels on the basket.
	UNemesisTargetSubsystem* T = UNemesisTargetSubsystem::Get(this);
	if (Turret && !IsDown() && T && T->GetBalloon())
	{
		const FVector To = T->GetBasketLocation() - Turret->GetComponentLocation();
		const FRotator Want(FMath::Clamp(To.Rotation().Pitch, -30.f, 45.f), To.Rotation().Yaw, 0.f);
		Turret->SetWorldRotation(FMath::RInterpTo(Turret->GetComponentRotation(), Want, DeltaSeconds, 6.f));
	}
}

void ANemesisCropDuster::TickAttack(float DeltaSeconds)
{
	if (ShotsLeft > 0)
	{
		ShotTimer -= DeltaSeconds;
		if (ShotTimer <= 0.f)
		{
			FireBullet();
			--ShotsLeft;
			ShotTimer = BulletInterval;
		}
		return;
	}

	// Holds its side for good (design). Overshot (the balloon braked or turned): no attacks until
	// back in the slot.
	if (!IsInSlot(SlotTolerance)) return;

	BurstTimer -= DeltaSeconds;
	if (!bWarned && BurstTimer <= BurstWarningTime)
	{
		bWarned = true;
		OnBurstWarning();
	}
	if (BurstTimer <= 0.f)
	{
		ShotsLeft = BulletsPerBurst;
		ShotTimer = 0.f;
		BurstTimer = BurstCooldown;
		bWarned = false;
	}
}

void ANemesisCropDuster::FireBullet()
{
	UNemesisTargetSubsystem* A = UNemesisTargetSubsystem::Get(this);
	if (!A || !BulletClass) return;
	// From the turret's barrels when the model is in, else from the nose.
	const FVector Muzzle = Turret ? Turret->GetComponentLocation() + Turret->GetForwardVector() * 120.f + FVector(0.f, 0.f, 30.f)
		: Engine->GetComponentLocation();
	// Straight at the basket, no lead and no drop compensation (team decision), plus spread.
	const FVector Aim = (A->GetBasketLocation() - Muzzle).GetSafeNormal();
	const FVector Dir = FMath::VRandCone(Aim, FMath::DegreesToRadians(SpreadDegrees));

	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (ANemesisBullet* Bullet = GetWorld()->SpawnActor<ANemesisBullet>(BulletClass, Muzzle, Dir.Rotation(), Params))
	{
		Bullet->Fire(Dir);
	}
}
