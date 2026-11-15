#include "NemesisPatrol.h"

#include "Components/SplineComponent.h"
#include "Engine/World.h"
#include "NemesisBomber.h"
#include "NemesisCropDuster.h"
#include "NemesisPlane.h"
#include "NemesisTargetSubsystem.h"

ANemesisPatrol::ANemesisPatrol()
{
	PrimaryActorTick.bCanEverTick = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	// Default route: a 300 × 300 m loop at the actor's height; reshape it in the level.
	Route = CreateDefaultSubobject<USplineComponent>(TEXT("Route"));
	Route->SetupAttachment(Root);
	Route->ClearSplinePoints(false);
	const float H = 15000.f;
	Route->AddSplinePoint(FVector(H, 0.f, 0.f), ESplineCoordinateSpace::Local, false);
	Route->AddSplinePoint(FVector(0.f, H, 0.f), ESplineCoordinateSpace::Local, false);
	Route->AddSplinePoint(FVector(-H, 0.f, 0.f), ESplineCoordinateSpace::Local, false);
	Route->AddSplinePoint(FVector(0.f, -H, 0.f), ESplineCoordinateSpace::Local, false);
	Route->SetClosedLoop(true, false);
	Route->UpdateSpline();

	PlaneClass = ANemesisCropDuster::StaticClass();
}

void ANemesisPatrol::BeginPlay()
{
	Super::BeginPlay();
	SpawnGroup();
}

void ANemesisPatrol::EndPlay(const EEndPlayReason::Type Reason)
{
	if (bEngaged)
	{
		if (UNemesisTargetSubsystem* T = UNemesisTargetSubsystem::Get(this)) T->ReleasePatrol(this);
	}
	Super::EndPlay(Reason);
}

void ANemesisPatrol::SpawnGroup()
{
	if (!PlaneClass) return;
	const float Length = Route->GetSplineLength();
	for (int32 i = 0; i < GroupSize; ++i)
	{
		const float D = FMath::Fmod(Length - i * Spacing + Length, Length);
		const FTransform At(Route->GetRotationAtDistanceAlongSpline(D, ESplineCoordinateSpace::World),
			Route->GetLocationAtDistanceAlongSpline(D, ESplineCoordinateSpace::World));
		ANemesisPlane* Plane = GetWorld()->SpawnActorDeferred<ANemesisPlane>(PlaneClass, At, this, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!Plane) continue;
		// Formation once they attack: crop dusters one per side, bombers side by side.
		if (ANemesisCropDuster* Duster = Cast<ANemesisCropDuster>(Plane)) Duster->SetSide(i % 2 == 0 ? 1 : -1);
		if (ANemesisBomber* Bomber = Cast<ANemesisBomber>(Plane)) Bomber->SetSlotSideOffset(GroupSize > 1 ? (i % 2 == 0 ? 1.f : -1.f) * BomberSideSpacing : 0.f);
		Plane->FinishSpawning(At);
		Plane->Launch(nullptr); // no airship: the plane's altitude is the route's
		Plane->SetRouteTarget(Route->GetLocationAtDistanceAlongSpline(FMath::Fmod(D + 1500.f, Length), ESplineCoordinateSpace::World));
		Planes.Add(Plane);
	}
}

void ANemesisPatrol::SetEngaged(bool bNowEngaged)
{
	if (bEngaged == bNowEngaged) return;
	bEngaged = bNowEngaged;
	for (const TWeakObjectPtr<ANemesisPlane>& P : Planes)
	{
		if (P.IsValid() && !bEngaged) P->SetRouteTarget(P->GetActorLocation()); // re-targeted below
		else if (P.IsValid()) P->ClearRouteTarget();
	}
	UE_LOG(LogTemp, Log, TEXT("%s: %s"), *GetName(), bEngaged ? TEXT("attacking") : TEXT("back to patrol"));
	OnEngagedChanged(bEngaged);
}

void ANemesisPatrol::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Planes.RemoveAll([](const TWeakObjectPtr<ANemesisPlane>& P) { return !P.IsValid() || P->IsDown(); });
	UNemesisTargetSubsystem* T = UNemesisTargetSubsystem::Get(this);
	if (Planes.IsEmpty())
	{
		if (bEngaged && T) T->ReleasePatrol(this);
		bEngaged = false;
		return;
	}
	if (!T || !T->GetBalloon()) return;
	const FVector Basket = T->GetBasketLocation();

	const FVector OnRoute = Route->FindLocationClosestToWorldLocation(Basket, ESplineCoordinateSpace::World);
	const bool bBalloonInZone = FVector::Dist2D(OnRoute, Basket) <= DisengageDistance;
	if (!bEngaged && bBalloonInZone) // planes still near a balloon that left the zone must not re-engage
	{
		for (const TWeakObjectPtr<ANemesisPlane>& P : Planes)
		{
			// Crop dusters join only while there's room under the map-wide limit of attacking crop dusters.
			const bool bRoom = !PlaneClass || !PlaneClass->IsChildOf(ANemesisCropDuster::StaticClass()) || T->CanAddCropDusters(Planes.Num());
			if (bRoom && FVector::DistSquared(P->GetActorLocation(), Basket) <= FMath::Square(AggroDistance) && T->TryEngagePatrol(this))
			{
				SetEngaged(true);
				break;
			}
		}
	}
	else if (bEngaged)
	{
		if (!bBalloonInZone)
		{
			T->ReleasePatrol(this);
			SetEngaged(false);
		}
	}

	if (!bEngaged)
	{
		// Patrol: a point moves along the route; each plane chases its own spot behind it.
		const float Length = Route->GetSplineLength();
		RouteDistance = FMath::Fmod(RouteDistance + RouteSpeed * DeltaSeconds, Length);
		for (int32 i = 0; i < Planes.Num(); ++i)
		{
			const float D = FMath::Fmod(RouteDistance - i * Spacing + Length * 2.f, Length);
			Planes[i]->SetRouteTarget(Route->GetLocationAtDistanceAlongSpline(D + 1500.f > Length ? D + 1500.f - Length : D + 1500.f, ESplineCoordinateSpace::World));
		}
	}
}
