#include "NemesisPlane.h"

#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "FirstPersonCameraPawn.h"
#include "NemesisAirship.h"
#include "NemesisPlaceholder.h"
#include "NemesisTargetSubsystem.h"
#include "RevolverComponent.h"
#include "RifleComponent.h"
#include "ShotgunComponent.h"
#include "UObject/ConstructorHelpers.h"

ANemesisPlane::ANemesisPlane()
{
	PrimaryActorTick.bCanEverTick = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Iron(TEXT("/Game/Assets/Nemesis/MI_Nemesis_Iron.MI_Nemesis_Iron"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Canvas(TEXT("/Game/Assets/Nemesis/MI_Nemesis_Canvas.MI_Nemesis_Canvas"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Glow(TEXT("/Game/Assets/Nemesis/MI_Nemesis_Glow.MI_Nemesis_Glow"));

	// Placeholder: 4 m fuselage, 8 m wings, glowing engine cube on the nose (separate hit zone).
	Fuselage = MakeCube(TEXT("Fuselage"), FVector::ZeroVector, FVector(4.f, 0.9f, 0.9f), Iron.Object);
	Wings = MakeCube(TEXT("Wings"), FVector(30.f, 0.f, 20.f), FVector(1.2f, 8.f, 0.15f), Canvas.Object);
	// Wider and taller than the fuselage, so it shows around it even from behind.
	Engine = MakeCube(TEXT("Engine"), FVector(240.f, 0.f, 0.f), FVector(0.8f, 1.5f, 1.3f), Glow.Object);
	EngineZones.Add(Engine);

	FireLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("FireLight"));
	FireLight->SetupAttachment(Root);
	FireLight->SetRelativeLocation(FVector(0.f, 0.f, 80.f));
	FireLight->SetLightColor(FLinearColor(1.f, 0.45f, 0.1f));
	FireLight->SetAttenuationRadius(1500.f);
	FireLight->SetCastShadows(false);
	FireLight->SetVisibility(false);
}

UStaticMeshComponent* ANemesisPlane::MakePart(const TCHAR* Name, UStaticMesh* Mesh, const FVector& Location)
{
	if (!Mesh) return nullptr;
	UStaticMeshComponent* C = CreateDefaultSubobject<UStaticMeshComponent>(Name);
	C->SetupAttachment(Root);
	C->SetStaticMesh(Mesh);
	C->SetRelativeLocation(Location);
	NemesisCollision::Shootable(C);
	return C;
}

UStaticMeshComponent* ANemesisPlane::MakeCube(const TCHAR* Name, const FVector& Location, const FVector& SizeM, UMaterialInterface* Material)
{
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	UStaticMeshComponent* C = CreateDefaultSubobject<UStaticMeshComponent>(Name);
	C->SetupAttachment(Root);
	C->SetStaticMesh(Cube.Object);
	if (Material) C->SetMaterial(0, Material);
	C->SetRelativeLocation(Location);
	C->SetRelativeScale3D(SizeM); // the engine cube is 1 m
	// Bullets (Visibility traces) hit it; nothing else collides.
	C->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	C->SetCollisionObjectType(ECC_WorldDynamic);
	C->SetCollisionResponseToAllChannels(ECR_Ignore);
	C->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	C->SetGenerateOverlapEvents(false);
	C->SetCanEverAffectNavigation(false);
	return C;
}

void ANemesisPlane::Launch(ANemesisAirship* InAirship)
{
	Airship = InAirship;
	FixedZ = GetActorLocation().Z;
	EngineHealth = 1.f;
	FuselageHealth = 1.f;
	Velocity = GetBalloonForward() * MaxSpeed * 0.5f; // leaves the hangar already moving
	OnTakePointDamage.AddUniqueDynamic(this, &ANemesisPlane::HandlePointDamage);
}

FVector ANemesisPlane::GetBalloonForward() const
{
	UNemesisTargetSubsystem* T = UNemesisTargetSubsystem::Get(this);
	return T && T->GetBalloon() ? T->GetBalloonForward() : GetActorForwardVector();
}

void ANemesisPlane::SetRouteTarget(const FVector& Point)
{
	bOnRoute = true;
	RouteTarget = Point;
}

void ANemesisPlane::ClearRouteTarget()
{
	bOnRoute = false;
}

bool ANemesisPlane::IsInSlot(float Tolerance) const
{
	return FVector::DistXY(GetActorLocation(), GetSlotLocation()) <= Tolerance;
}

void ANemesisPlane::Leave()
{
	if (bLeaving || bDown) return;
	bLeaving = true;
	StateTime = 0.f;
}

void ANemesisPlane::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	StateTime += DeltaSeconds;

	UNemesisTargetSubsystem* T = UNemesisTargetSubsystem::Get(this);
	if (!T || !T->GetBalloon())
	{
		Destroy();
		return;
	}
	if (bDown)
	{
		Fall(DeltaSeconds);
		return;
	}
	Fly(DeltaSeconds);
	if (bLeaving)
	{
		if (StateTime > LeaveTime) Destroy();
		return;
	}
	if (!bOnRoute) TickAttack(DeltaSeconds);
}

void ANemesisPlane::Fly(float DeltaSeconds)
{
	for (UStaticMeshComponent* P : Props)
	{
		if (P) P->AddLocalRotation(FRotator(0.f, 0.f, PropSpinSpeed * DeltaSeconds));
	}
	UNemesisTargetSubsystem* A = UNemesisTargetSubsystem::Get(this);
	const FVector Pos = GetActorLocation();
	FVector DesiredVel;
	if (bLeaving)
	{
		DesiredVel = GetActorForwardVector() * MaxSpeed;
	}
	else if (bOnRoute)
	{
		const FVector ToPoint = FVector(RouteTarget.X - Pos.X, RouteTarget.Y - Pos.Y, 0.f);
		DesiredVel = ToPoint.GetSafeNormal() * FMath::Min(PatrolSpeed, ToPoint.Size() * 2.f);
	}
	else
	{
		// Match the balloon's velocity and close the slot error; only XY (fixed altitude).
		const FVector Error = GetSlotLocation() - Pos;
		DesiredVel = A->GetBalloonVelocity() + FVector(Error.X, Error.Y, 0.f) * SlotGain;
		DesiredVel.Z = 0.f;
		DesiredVel = DesiredVel.GetClampedToMaxSize(MaxSpeed);
	}
	Velocity = FMath::VInterpConstantTo(Velocity, DesiredVel, DeltaSeconds, Acceleration);
	Velocity.Z = 0.f;

	FVector NewPos = Pos + Velocity * DeltaSeconds;
	NewPos.Z = FixedZ;

	// Nose follows the flight direction; when hovering in the slot, the balloon's heading.
	// Planes never turn around (design): drifting back or sideways keeps the nose forward.
	const FVector Fwd = GetBalloonForward();
	FVector Face = Velocity.SizeSquared() > FMath::Square(150.f) ? Velocity : Fwd;
	if (bOnRoute) Face = Velocity.SizeSquared() > 1.f ? Velocity : GetActorForwardVector(); // patrolling: nose along the route
	else if (!bLeaving && FVector::DotProduct(Face.GetSafeNormal(), Fwd) < 0.5f) Face = Fwd;
	const FRotator Rot = FMath::RInterpTo(GetActorRotation(), FRotator(0.f, Face.Rotation().Yaw, 0.f), DeltaSeconds, 2.f);
	SetActorLocationAndRotation(NewPos, Rot);
}

void ANemesisPlane::Fall(float DeltaSeconds)
{
	Velocity.Z -= 980.f * DeltaSeconds;
	SetActorLocationAndRotation(GetActorLocation() + Velocity * DeltaSeconds,
		GetActorRotation() + FRotator(-25.f * DeltaSeconds, 0.f, 90.f * DeltaSeconds));

	// Black smoke pours from the dead engine; a burning plane trails flames from the fuselage too.
	const FVector Up = FVector::UpVector;
	SmokeStream.Tick(DeltaSeconds, (FireStream.IsActive() ? Fuselage : Engine)->Bounds.Origin, Up, Velocity);
	if (FireStream.IsActive())
	{
		FireStream.Tick(DeltaSeconds, Fuselage->Bounds.Origin + FVector(0.f, 0.f, 40.f), Up, Velocity);
		FireLight->SetIntensity(FMath::FRandRange(25000.f, 70000.f));
	}

	if (StateTime > FallTime)
	{
		NemesisFx::Explosion(GetWorld(), GetActorLocation(), 1.6f);
		Destroy();
	}
}

void ANemesisPlane::GoDown(bool bEngine)
{
	bDown = true;
	StateTime = 0.f;
	SmokeStream.Start(GetWorld(), NemesisFx::EKind::EngineSmoke, bEngine ? 32.f : 20.f); // dense: the plane falls fast
	if (!bEngine)
	{
		FireStream.Start(GetWorld(), NemesisFx::EKind::Fire, 40.f);
		FireLight->SetVisibility(true);
		NemesisFx::Explosion(GetWorld(), Fuselage->Bounds.Origin, 0.8f); // the fuel catching
	}
	OnPlaneDowned(bEngine);
}

void ANemesisPlane::EndPlay(const EEndPlayReason::Type Reason)
{
	SmokeStream.Stop();
	FireStream.Stop();
	Super::EndPlay(Reason);
}

int32 ANemesisPlane::FuselageHitsFor(AActor* DamageCauser)
{
	// Gun bullets name the shooter as the causer; the gun is the one in the shooter's hand.
	const AFirstPersonCameraPawn* Pawn = Cast<AFirstPersonCameraPawn>(DamageCauser);
	const UGunComponent* Gun = Pawn ? Pawn->GetActiveGun() : nullptr;
	if (Cast<URevolverComponent>(Gun)) return RevolverFuselageHits;
	if (Cast<URifleComponent>(Gun)) return RifleFuselageHits;
	if (Cast<UShotgunComponent>(Gun))
	{
		const double Now = GetWorld()->GetTimeSeconds();
		const bool bSameShot = LastShotgunHitTime >= 0.0 && Now - LastShotgunHitTime <= ShotgunPelletWindow;
		LastShotgunHitTime = Now;
		return bSameShot ? 0 : ShotgunFuselageHits;
	}
	return OtherFuselageHits;
}

void ANemesisPlane::HandlePointDamage(AActor* DamagedActor, float Damage, AController* InstigatedBy, FVector HitLocation,
	UPrimitiveComponent* HitComponent, FName BoneName, FVector ShotFromDirection, const UDamageType* DamageType, AActor* DamageCauser)
{
	if (bDown || Damage <= 0.f) return;
	if (EngineZones.Contains(HitComponent))
	{
		// One hit on any engine, from any weapon.
		EngineHealth = 0.f;
		UE_LOG(LogTemp, Log, TEXT("%s engine hit: down"), *GetName());
		GoDown(true);
		return;
	}

	const int32 Hits = FuselageHitsFor(DamageCauser);
	if (Hits <= 0) return; // another pellet of the same shotgun shot
	FuselageHealth = FMath::Max(0.f, FuselageHealth - 1.f / Hits);
	UE_LOG(LogTemp, Log, TEXT("%s fuselage hit (1/%d): %.2f left"), *GetName(), Hits, FuselageHealth);
	if (FuselageHealth <= KINDA_SMALL_NUMBER)
	{
		UE_LOG(LogTemp, Log, TEXT("%s fuselage destroyed: on fire"), *GetName());
		GoDown(false);
	}
}
