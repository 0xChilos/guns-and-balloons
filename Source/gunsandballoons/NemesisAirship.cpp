#include "NemesisAirship.h"

#include "BalloonFlightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "EngineHealthComponent.h"
#include "EngineUtils.h"
#include "NemesisBomber.h"
#include "NemesisCropDuster.h"
#include "NemesisMine.h"
#include "NemesisRocket.h"
#include "NemesisTargetSubsystem.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	const FName SlowdownSource(TEXT("NemesisRocketSlowdown"));

	// Mount points from the Blender model (Content/Assets/Nemesis/Nemesis.blend), in cm.
	// Blender +Y (left) becomes Unreal −Y.
	struct FMount { const TCHAR* Name; FVector Location; float Roll; };
	const FMount TurretMounts[] = {
		{ TEXT("Turret_Nose"), FVector(1360.f, 0.f, -715.f), 180.f },
		{ TEXT("Turret_L"), FVector(500.f, -524.4f, -120.f), -90.f },
		{ TEXT("Turret_R"), FVector(500.f, 524.4f, -120.f), 90.f },
		{ TEXT("Turret_Tail"), FVector(-1200.f, 0.f, -391.f), 180.f },
	};
	const FMount PropMounts[] = {
		{ TEXT("Prop_1"), FVector(140.f, -597.8f, -413.9f), 0.f },
		{ TEXT("Prop_2"), FVector(140.f, 597.8f, -413.9f), 0.f },
		{ TEXT("Prop_3"), FVector(-1060.f, -527.2f, -365.f), 0.f },
		{ TEXT("Prop_4"), FVector(-1060.f, 527.2f, -365.f), 0.f },
	};

	// The airship is a visual: bullets stop on it (Visibility), nothing else collides with it.
	void SetupVisualCollision(UStaticMeshComponent* Mesh)
	{
		Mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Mesh->SetCollisionObjectType(ECC_WorldDynamic);
		Mesh->SetCollisionResponseToAllChannels(ECR_Ignore);
		Mesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
		Mesh->SetGenerateOverlapEvents(false);
		Mesh->SetCanEverAffectNavigation(false);
	}
}

ANemesisAirship::ANemesisAirship()
{
	PrimaryActorTick.bCanEverTick = true;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> BodyMesh(TEXT("/Game/Assets/Nemesis/SM_Nemesis_Airship.SM_Nemesis_Airship"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> TurretMesh(TEXT("/Game/Assets/Nemesis/SM_Nemesis_Turret.SM_Nemesis_Turret"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> PropMesh(TEXT("/Game/Assets/Nemesis/SM_Nemesis_Prop.SM_Nemesis_Prop"));

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(Root);
	Body->SetStaticMesh(BodyMesh.Object);
	SetupVisualCollision(Body);

	for (const FMount& M : TurretMounts)
	{
		UStaticMeshComponent* T = CreateDefaultSubobject<UStaticMeshComponent>(M.Name);
		T->SetupAttachment(Root);
		T->SetStaticMesh(TurretMesh.Object);
		T->SetRelativeLocationAndRotation(M.Location, FRotator(0.f, 0.f, M.Roll));
		SetupVisualCollision(T);
		Turrets.Add(T);
	}
	for (const FMount& M : PropMounts)
	{
		UStaticMeshComponent* P = CreateDefaultSubobject<UStaticMeshComponent>(M.Name);
		P->SetupAttachment(Root);
		P->SetStaticMesh(PropMesh.Object);
		P->SetRelativeLocation(M.Location);
		SetupVisualCollision(P);
		Props.Add(P);
	}

	// Front of the rocket launcher under the control car.
	RocketMuzzle = CreateDefaultSubobject<USceneComponent>(TEXT("RocketMuzzle"));
	RocketMuzzle->SetupAttachment(Root);
	RocketMuzzle->SetRelativeLocation(FVector(1300.f, 0.f, -890.f));

	HangarMouth = CreateDefaultSubobject<USceneComponent>(TEXT("HangarMouth"));
	HangarMouth->SetupAttachment(Root);
	HangarMouth->SetRelativeLocation(FVector(560.f, 0.f, -520.f));

	RocketClass = ANemesisRocket::StaticClass();
	BomberClass = ANemesisBomber::StaticClass();
	CropDusterClass = ANemesisCropDuster::StaticClass();
}

void ANemesisAirship::BeginPlay()
{
	Super::BeginPlay();

	for (UStaticMeshComponent* T : Turrets)
	{
		TurretRest.Add(T->GetRelativeTransform());
	}
	FindBalloon();

	if (AActor* B = Balloon.Get())
	{
		if (StartDistance > 0.f)
		{
			const FVector Fwd = FRotator(0.f, B->GetActorRotation().Yaw, 0.f).Vector();
			SetActorLocationAndRotation(B->GetActorLocation() - Fwd * StartDistance + FVector(0.f, 0.f, HeightOffset), Fwd.Rotation());
		}
		RecordTrail();
	}
	RocketTimer = 0.f; // first rocket as soon as the balloon is in D1
}

void ANemesisAirship::FindBalloon()
{
	UNemesisTargetSubsystem* T = UNemesisTargetSubsystem::Get(this);
	if (!T || !T->GetBalloon())
	{
		UE_LOG(LogTemp, Warning, TEXT("NemesisAirship: no actor with BalloonFlightComponent in the level"));
		return;
	}
	// The airship's tuning drives the shared values every enemy uses.
	T->BasketOffset = BasketOffset;
	T->MaxMines = MaxMines;
	Balloon = T->GetBalloon();
	Flight = T->GetFlight();
	Health = T->GetHealth();
	if (bDebugInvulnerableBalloon && Health.IsValid()) Health->bInvulnerable = true;
}

FVector ANemesisAirship::GetBasketLocation() const
{
	const AActor* B = Balloon.Get();
	return B ? B->GetActorTransform().TransformPosition(BasketOffset) : FVector::ZeroVector;
}

FVector ANemesisAirship::GetBalloonVelocity() const
{
	const AActor* B = Balloon.Get();
	const UBalloonFlightComponent* F = Flight.Get();
	if (!B || !F) return FVector::ZeroVector;
	return FRotator(0.f, B->GetActorRotation().Yaw, 0.f).Vector() * F->GetCurrentSpeed();
}

float ANemesisAirship::GetSpeed() const
{
	if (SpeedOverride > 0.f) return SpeedOverride;
	const UBalloonFlightComponent* F = Flight.Get();
	return F ? F->GetMinSpeed() : 0.f;
}

void ANemesisAirship::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!Balloon.IsValid()) return;

	TrailTimer += DeltaSeconds;
	if (TrailTimer >= TrailSampleInterval)
	{
		TrailTimer = 0.f;
		RecordTrail();
	}

	Move(DeltaSeconds);
	UpdateBand();
	UpdateTurrets(DeltaSeconds);
	UpdateAttacks(DeltaSeconds);

	for (UStaticMeshComponent* P : Props)
	{
		P->AddLocalRotation(FRotator(0.f, 0.f, PropSpinSpeed * DeltaSeconds));
	}
}

void ANemesisAirship::RecordTrail()
{
	Trail.Add(Balloon->GetActorLocation() + FVector(0.f, 0.f, HeightOffset));
}

void ANemesisAirship::Move(float DeltaSeconds)
{
	// Walk along the recorded trail at constant speed, carrying the leftover step to the next point.
	float Step = GetSpeed() * DeltaSeconds;
	FVector Pos = GetActorLocation();
	FVector Dir = GetActorForwardVector();
	while (Step > 0.f && Trail.Num() > 0)
	{
		const FVector ToNext = Trail[0] - Pos;
		const float Len = ToNext.Size();
		if (Len > KINDA_SMALL_NUMBER) Dir = ToNext / Len;
		if (Len <= Step)
		{
			Pos = Trail[0];
			Step -= Len;
			Trail.RemoveAt(0, 1, EAllowShrinking::No);
		}
		else
		{
			Pos += Dir * Step;
			Step = 0.f;
		}
	}
	SetActorLocation(Pos);

	const FVector FlatDir = FVector(Dir.X, Dir.Y, 0.f).GetSafeNormal();
	if (!FlatDir.IsNearlyZero())
	{
		SetActorRotation(FMath::RInterpTo(GetActorRotation(), FlatDir.Rotation(), DeltaSeconds, TurnInterpSpeed));
	}

	if (bDebugDraw)
	{
		for (int32 I = 1; I < Trail.Num(); ++I)
		{
			DrawDebugLine(GetWorld(), Trail[I - 1], Trail[I], FColor::Purple, false, 0.f, 0, 20.f);
		}
	}
}

void ANemesisAirship::UpdateBand()
{
	const FVector Nose = GetActorTransform().TransformPosition(DistanceReference);
	DistanceToBalloon = FVector::Dist(Nose, GetBasketLocation());

	if (DistanceToBalloon < KillDistance) Band = ENemesisBand::Turrets;
	else if (DistanceToBalloon < RocketBandMax) Band = ENemesisBand::Rockets;
	else if (DistanceToBalloon < BomberBandMax) Band = ENemesisBand::Bombers;
	else Band = ENemesisBand::CropDusters;

	if (bDebugDraw)
	{
		DrawDebugString(GetWorld(), Nose, FString::Printf(TEXT("%.0f m  %s"), DistanceToBalloon / 100.f,
			*StaticEnum<ENemesisBand>()->GetNameStringByValue((int64)Band)), nullptr, FColor::Red, 0.f, true, 2.f);
	}
}

void ANemesisAirship::UpdateTurrets(float DeltaSeconds)
{
	const bool bWarn = DistanceToBalloon < TurretWarnDistance;
	if (bWarn != bTurretWarning)
	{
		bTurretWarning = bWarn;
		OnTurretWarning(bWarn);
	}

	// Each turret keeps its mount's up axis and swings its barrels (+X) toward the basket.
	const FVector Target = GetBasketLocation();
	for (int32 I = 0; I < Turrets.Num(); ++I)
	{
		UStaticMeshComponent* T = Turrets[I];
		const FTransform RestWorld = TurretRest[I] * GetActorTransform();
		FQuat Goal = RestWorld.GetRotation();
		if (bWarn)
		{
			const FVector ToTarget = (Target - T->GetComponentLocation()).GetSafeNormal();
			const FVector Up = RestWorld.GetRotation().GetUpVector();
			// Only aim if the target is on the turret's side of its mount.
			if (FVector::DotProduct(ToTarget, Up) > -0.2f)
			{
				Goal = FRotationMatrix::MakeFromXZ(ToTarget, Up).ToQuat();
			}
		}
		T->SetWorldRotation(FMath::QInterpTo(T->GetComponentQuat(), Goal, DeltaSeconds, TurretTurnSpeed));
	}

	if (Band == ENemesisBand::Turrets && !bKilled && !bDebugInvulnerableBalloon)
	{
		bKilled = true;
		OnTurretsFired();
		if (UEngineHealthComponent* H = Health.Get())
		{
			H->Kill();
		}
	}
}

void ANemesisAirship::UpdateAttacks(float DeltaSeconds)
{
	auto Prune = [](auto& List) { List.RemoveAll([](const auto& P) { return !P.IsValid(); }); };
	Prune(ActiveRockets);
	Prune(ActiveBombers);
	Prune(ActiveCropDusters);
	if (bKilled) return;

	RocketTimer = FMath::Max(0.f, RocketTimer - DeltaSeconds);
	BomberTimer = FMath::Max(0.f, BomberTimer - DeltaSeconds);
	CropDusterTimer = FMath::Max(0.f, CropDusterTimer - DeltaSeconds);

	// Demo/testing: every attack type runs at once, each on its own cooldown and limit.
	if (bDebugAllAttacks)
	{
		if (RocketTimer <= 0.f && ActiveRockets.Num() < MaxRockets) { LaunchRocket(); RocketTimer = RocketCooldown; }
		if (BomberTimer <= 0.f && ActiveBombers.Num() < MaxBombers) { LaunchBomber(); BomberTimer = BomberCooldown; }
		if (CropDusterTimer <= 0.f && CanLaunchOwnCropDuster()) { LaunchCropDuster(); CropDusterTimer = CropDusterCooldown; }
		return;
	}

	// Planes already in the air keep attacking whatever the band is now.
	switch (bForceBand ? ForcedBand : Band)
	{
	case ENemesisBand::Rockets:
		if (RocketTimer <= 0.f && ActiveRockets.Num() < MaxRockets)
		{
			LaunchRocket();
			RocketTimer = RocketCooldown;
		}
		break;
	case ENemesisBand::Bombers:
		if (BomberTimer <= 0.f && ActiveBombers.Num() < MaxBombers)
		{
			LaunchBomber();
			BomberTimer = BomberCooldown;
		}
		break;
	case ENemesisBand::CropDusters:
		if (CropDusterTimer <= 0.f && CanLaunchOwnCropDuster())
		{
			LaunchCropDuster();
			CropDusterTimer = CropDusterCooldown;
		}
		break;
	default:
		break;
	}
}

void ANemesisAirship::LaunchBomber()
{
	if (!BomberClass || !Balloon.IsValid()) return;
	FVector At = HangarMouth->GetComponentLocation();
	At.Z = Balloon->GetActorLocation().Z + GetDefault<ANemesisBomber>(BomberClass)->HeightAboveBalloon;
	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (ANemesisBomber* Bomber = GetWorld()->SpawnActor<ANemesisBomber>(BomberClass, At, FRotator(0.f, GetActorRotation().Yaw, 0.f), Params))
	{
		Bomber->Launch(this);
		ActiveBombers.Add(Bomber);
		UE_LOG(LogTemp, Log, TEXT("NemesisAirship: bomber launched"));
	}
}

void ANemesisAirship::LaunchCropDuster(bool bOwn)
{
	if (!CropDusterClass || !Balloon.IsValid()) return;
	// Take the free side: right if nobody holds it, else left.
	const int32 Side = IsCropDusterSideTaken(1, nullptr) ? -1 : 1;
	FVector At = HangarMouth->GetComponentLocation();
	At.Z = GetBasketLocation().Z + GetDefault<ANemesisCropDuster>(CropDusterClass)->HeightAboveBasket;
	if (ANemesisCropDuster* Duster = GetWorld()->SpawnActorDeferred<ANemesisCropDuster>(CropDusterClass, FTransform(FRotator(0.f, GetActorRotation().Yaw, 0.f), At), this, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
	{
		Duster->SetSide(Side);
		Duster->FinishSpawning(FTransform(FRotator(0.f, GetActorRotation().Yaw, 0.f), At));
		Duster->Launch(this);
		if (bOwn) ActiveCropDusters.Add(Duster);
		UE_LOG(LogTemp, Log, TEXT("NemesisAirship: crop duster launched (%s, %s)"), Side > 0 ? TEXT("right") : TEXT("left"), bOwn ? TEXT("own") : TEXT("for a beacon"));
	}
}

bool ANemesisAirship::IsCropDusterSideTaken(int32 Side, const ANemesisCropDuster* Except) const
{
	// Every attacking crop duster counts (the airship's, beacons', patrols'): the free side is the emptier one.
	for (TActorIterator<ANemesisCropDuster> It(GetWorld()); It; ++It)
	{
		if (*It != Except && !It->IsDown() && !It->IsOnRoute() && It->GetSide() == Side) return true;
	}
	return false;
}

bool ANemesisAirship::CanLaunchOwnCropDuster()
{
	ActiveCropDusters.RemoveAll([](const TWeakObjectPtr<ANemesisCropDuster>& D) { return !D.IsValid() || D->IsDown(); });
	UNemesisTargetSubsystem* T = UNemesisTargetSubsystem::Get(this);
	return ActiveCropDusters.Num() < MaxCropDusters && (!T || T->CanAddCropDusters());
}

bool ANemesisAirship::RequestCropDuster()
{
	if (bKilled || !CropDusterClass) return false;
	UNemesisTargetSubsystem* T = UNemesisTargetSubsystem::Get(this);
	if (T && !T->CanAddCropDusters()) return false;
	LaunchCropDuster(false);
	return true;
}

void ANemesisAirship::LaunchRocket()
{
	if (!RocketClass) return;
	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ANemesisRocket* Rocket = GetWorld()->SpawnActor<ANemesisRocket>(RocketClass, RocketMuzzle->GetComponentTransform(), Params);
	if (!Rocket) return;
	Rocket->Launch(this);
	ActiveRockets.Add(Rocket);
	OnRocketLaunched(Rocket);
}

void ANemesisAirship::OnRocketGone(ANemesisRocket* Rocket)
{
	ActiveRockets.Remove(Rocket);
}

void ANemesisAirship::ApplyRocketSlowdown()
{
	UBalloonFlightComponent* F = Flight.Get();
	if (!F) return;
	F->SetFlightModifier(SlowdownSource, 0.f, 1.f - SlowdownPercent);
	GetWorldTimerManager().SetTimer(SlowdownTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		if (UBalloonFlightComponent* F2 = Flight.Get()) F2->ClearFlightModifier(SlowdownSource);
	}), SlowdownDuration, false);
}
