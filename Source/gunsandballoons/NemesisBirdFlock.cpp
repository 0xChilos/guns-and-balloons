#include "NemesisBirdFlock.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "FirstPersonCameraPawn.h"
#include "GunComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "NemesisPlaceholder.h"
#include "NemesisTargetSubsystem.h"
#include "UObject/ConstructorHelpers.h"

ANemesisBirdFlock::ANemesisBirdFlock()
{
	PrimaryActorTick.bCanEverTick = true;
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Canvas(TEXT("/Game/Assets/Nemesis/MI_Nemesis_Dark.MI_Nemesis_Dark"));
	BirdMesh = Cube.Object;
	BirdMaterial = Canvas.Object;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
}

void ANemesisBirdFlock::BeginPlay()
{
	Super::BeginPlay();
	OnTakePointDamage.AddUniqueDynamic(this, &ANemesisBirdFlock::HandlePointDamage);
}

void ANemesisBirdFlock::EndPlay(const EEndPlayReason::Type Reason)
{
	if (bActive)
	{
		BindToPlayerGuns(false);
		if (UNemesisTargetSubsystem* T = UNemesisTargetSubsystem::Get(this)) T->ReleaseFlock(this);
	}
	Super::EndPlay(Reason);
}

void ANemesisBirdFlock::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UNemesisTargetSubsystem* T = UNemesisTargetSubsystem::Get(this);
	if (!T || !T->GetBalloon()) return;

	if (!bActive)
	{
		if (FVector::DistSquared(GetActorLocation(), T->GetBasketLocation()) <= FMath::Square(ActivationDistance) && T->TryClaimFlock(this))
		{
			StartCrossing();
		}
		return;
	}
	TickBirds(DeltaSeconds);
}

void ANemesisBirdFlock::StartCrossing()
{
	UNemesisTargetSubsystem* T = UNemesisTargetSubsystem::Get(this);
	bActive = true;

	// Meet the basket where it will be when the birds get there, coming from one side.
	const FVector Fwd = T->GetBalloonForward();
	const FVector Right = FVector::CrossProduct(FVector::UpVector, Fwd);
	const float Side = FMath::RandBool() ? 1.f : -1.f;
	const float TravelTime = StartSideDistance / BirdSpeed;
	const FVector Meet = T->GetBasketLocation() + FVector(0.f, 0.f, FlightHeight) + T->GetBalloonVelocity() * TravelTime;
	const FVector Start = Meet + Right * (Side * StartSideDistance);
	Goal = Meet - Right * (Side * StartSideDistance * 2.f);
	SetActorLocation(Meet); // the flock actor now marks the crossing point

	const int32 Num = FMath::RandRange(MinBirds, FMath::Max(MinBirds, MaxBirds));
	for (int32 i = 0; i < Num; ++i)
	{
		UStaticMeshComponent* M = NewObject<UStaticMeshComponent>(this, *FString::Printf(TEXT("Bird_%d"), i));
		M->SetStaticMesh(BirdMesh);
		M->SetMaterial(0, BirdMaterial);
		M->SetCastShadow(false);
		NemesisCollision::Shootable(M);
		M->SetupAttachment(Root);
		M->RegisterComponent();
		AddInstanceComponent(M);
		M->SetWorldScale3D(FVector(0.6f, 0.3f, 0.15f)); // placeholder bird ~60 cm
		M->SetWorldLocation(Start + FMath::VRand() * FMath::FRandRange(0.f, 300.f));

		FBird Bird;
		Bird.Mesh = M;
		Bird.Velocity = (Meet - Start).GetSafeNormal() * BirdSpeed;
		Birds.Add(Bird);
	}
	BindToPlayerGuns(true);
	UE_LOG(LogTemp, Log, TEXT("%s: %d birds crossing"), *GetName(), Num);
	OnFlockStarted(Num);
}

void ANemesisBirdFlock::TickBirds(float DeltaSeconds)
{
	UNemesisTargetSubsystem* T = UNemesisTargetSubsystem::Get(this);
	// Strike point: just above the player's head (ropes, lower envelope).
	const FVector Basket = T->GetBasketLocation() + FVector(0.f, 0.f, FlightHeight);

	FVector Center = FVector::ZeroVector, AvgVel = FVector::ZeroVector;
	int32 Alive = 0;
	for (const FBird& B : Birds)
	{
		if (!B.bAlive) continue;
		Center += B.Mesh->GetComponentLocation();
		AvgVel += B.Velocity;
		++Alive;
	}

	// Remove fallen birds after a while.
	for (FBird& B : Birds)
	{
		if (B.bAlive || !B.Mesh || !B.Mesh->IsVisible()) continue;
		B.FallTime += DeltaSeconds;
		B.Velocity.Z -= 980.f * DeltaSeconds;
		B.Mesh->AddWorldOffset(B.Velocity * DeltaSeconds);
		if (B.FallTime > 3.f) B.Mesh->SetVisibility(false);
	}

	if (Alive == 0)
	{
		Destroy();
		return;
	}
	Center /= Alive;
	AvgVel /= Alive;

	// Phase: head for the crossing point, then on to the goal on the other side.
	if (!bCrossed && FVector::DotProduct(Center - GetActorLocation(), Goal - GetActorLocation()) > 0.f) bCrossed = true;
	const FVector Target = bCrossed ? Goal : GetActorLocation();
	if ((bCrossed || bScared) && FVector::Dist(Center, Basket) > DespawnDistance)
	{
		Destroy();
		return;
	}

	const float SepRadius = bScared ? SeparationRadius * 4.f : SeparationRadius;
	// Scared: away from the shot and from the balloon, sideways and up, for good.
	FVector FleeDir = FVector::ZeroVector;
	if (bScared)
	{
		FleeDir = (Center - ScareFrom).GetSafeNormal2D() + (Center - Basket).GetSafeNormal2D();
		FleeDir = FleeDir.GetSafeNormal2D() + FVector(0.f, 0.f, 0.6f);
	}
	for (FBird& B : Birds)
	{
		if (!B.bAlive) continue;
		const FVector P = B.Mesh->GetComponentLocation();

		FVector Separation = FVector::ZeroVector;
		for (const FBird& O : Birds)
		{
			if (&O == &B || !O.bAlive) continue;
			const FVector D = P - O.Mesh->GetComponentLocation();
			const float Dist = D.Size();
			if (Dist > KINDA_SMALL_NUMBER && Dist < SepRadius) Separation += D / Dist * (1.f - Dist / SepRadius);
		}
		FVector Steer = Separation * SeparationWeight
			+ (Center - P).GetSafeNormal() * CohesionWeight
			+ AvgVel.GetSafeNormal() * AlignmentWeight;
		Steer += bScared ? FleeDir * 3.f : (Target - P).GetSafeNormal() * GoalWeight;

		const FVector Desired = Steer.GetSafeNormal() * BirdSpeed;
		B.Velocity = FMath::VInterpTo(B.Velocity, Desired, DeltaSeconds, TurnRate);
		const FVector NewP = P + B.Velocity * DeltaSeconds;
		B.Mesh->SetWorldLocationAndRotation(NewP, B.Velocity.Rotation());

		// Bird strike on the basket (a scared flock never strikes).
		if (!bScared && FVector::DistSquared(NewP, Basket) <= FMath::Square(BirdHitRadius))
		{
			if (DamageDealt + DamagePerBird <= MaxDamage + KINDA_SMALL_NUMBER)
			{
				DamageDealt += DamagePerBird;
				T->DamageBalloon(DamagePerBird, this);
				UE_LOG(LogTemp, Log, TEXT("%s: bird hit the basket (%.0f / %.0f)"), *GetName(), DamageDealt, MaxDamage);
				OnBirdHitBalloon(DamagePerBird);
			}
			B.bAlive = false;
			B.Mesh->SetVisibility(false);
			NemesisCollision::None(B.Mesh);
		}
	}
}

void ANemesisBirdFlock::Scatter(const FVector& From)
{
	if (bScared) return;
	bScared = true;
	ScareFrom = From;
	UE_LOG(LogTemp, Log, TEXT("%s: scared off, fleeing for good"), *GetName());
	OnScattered();
}

void ANemesisBirdFlock::HandlePointDamage(AActor* DamagedActor, float InDamage, AController* InstigatedBy, FVector HitLocation,
	UPrimitiveComponent* HitComponent, FName BoneName, FVector ShotFromDirection, const UDamageType* DamageType, AActor* DamageCauser)
{
	for (FBird& B : Birds)
	{
		if (B.bAlive && B.Mesh == HitComponent)
		{
			B.bAlive = false; // falls (TickBirds), one hit from any weapon
			UE_LOG(LogTemp, Log, TEXT("%s: bird shot"), *GetName());
			B.Velocity *= 0.3f;
			NemesisCollision::None(B.Mesh);
			Scatter(HitLocation);
			return;
		}
	}
}

void ANemesisBirdFlock::HandlePlayerFired()
{
	// A shot that passes near the flock scares it, hit or not.
	const APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!Pawn || Birds.IsEmpty()) return;
	FVector Center = FVector::ZeroVector;
	int32 Alive = 0;
	for (const FBird& B : Birds)
	{
		if (B.bAlive) { Center += B.Mesh->GetComponentLocation(); ++Alive; }
	}
	if (Alive == 0) return;
	Center /= Alive;
	const FVector Eye = Pawn->GetPawnViewLocation();
	const FVector Dir = Pawn->GetViewRotation().Vector();
	const float Along = FVector::DotProduct(Center - Eye, Dir);
	if (Along > 0.f && FVector::Dist(Eye + Dir * Along, Center) <= ScareRadius)
	{
		Scatter(Eye + Dir * Along);
	}
}

void ANemesisBirdFlock::BindToPlayerGuns(bool bBind)
{
	const AFirstPersonCameraPawn* Pawn = Cast<AFirstPersonCameraPawn>(UGameplayStatics::GetPlayerPawn(this, 0));
	if (!Pawn) return;
	TInlineComponentArray<UGunComponent*> Guns(Pawn);
	for (UGunComponent* Gun : Guns)
	{
		if (!Gun) continue;
		if (bBind) Gun->OnFired.AddUniqueDynamic(this, &ANemesisBirdFlock::HandlePlayerFired);
		else Gun->OnFired.RemoveDynamic(this, &ANemesisBirdFlock::HandlePlayerFired);
	}
}
