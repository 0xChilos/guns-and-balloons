#include "GunProjectile.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/PlayerController.h"
#include "GunComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	/** /Engine/BasicShapes/Cylinder is 100 cm tall along Z and 100 cm across; the Sphere is 100 cm across. */
	constexpr float ShapeSize = 100.f;

	/** A purely visual shape: no collision, no shadow, hidden until first placed (so it never flashes at the spawn transform). */
	UStaticMeshComponent* MakeGlowShape(AActor* Owner, FName Name, USceneComponent* Parent, UStaticMesh* Shape)
	{
		UStaticMeshComponent* Comp = Owner->CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Comp->SetupAttachment(Parent);
		Comp->SetStaticMesh(Shape);
		Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Comp->SetGenerateOverlapEvents(false);
		Comp->CastShadow = false;
		Comp->bReceivesDecals = false;
		Comp->SetCanEverAffectNavigation(false);
		Comp->SetMobility(EComponentMobility::Movable);
		Comp->SetVisibility(false);
		return Comp;
	}

	UMaterialInstanceDynamic* MakeGlowMaterial(UMaterialInterface* Base, UTexture* Texture, const FLinearColor& Color, AActor* Owner, UStaticMeshComponent* Comp)
	{
		if (!Base)
		{
			return nullptr;
		}
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Base, Owner);
		MID->SetVectorParameterValue(TEXT("Color"), Color);
		if (Texture)
		{
			MID->SetTextureParameterValue(TEXT("Texture"), Texture);
		}
		Comp->SetMaterial(0, MID);
		return MID;
	}
}

AGunProjectile::AGunProjectile()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Emissive(TEXT("/Engine/EngineMaterials/EmissiveMeshMaterial.EmissiveMeshMaterial"));
	static ConstructorHelpers::FObjectFinder<UTexture> White(TEXT("/Engine/EngineResources/WhiteSquareTexture.WhiteSquareTexture"));

	// The root stays where the bullet spawned; the shapes are placed in world space every tick.
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	Tracer = MakeGlowShape(this, TEXT("Tracer"), Root, Cylinder.Object);
	Bullet = MakeGlowShape(this, TEXT("Bullet"), Root, Sphere.Object);

	TracerMaterial = Emissive.Object;
	TracerTexture = White.Object;
}

void AGunProjectile::BeginPlay()
{
	Super::BeginPlay();

	SetLifeSpan(MaxLifeSeconds);

	TracerMID = MakeGlowMaterial(TracerMaterial, TracerTexture, TracerColor, this, Tracer);
	BulletMID = MakeGlowMaterial(TracerMaterial, TracerTexture, BulletColor, this, Bullet);
}

void AGunProjectile::Launch(const FGunShot& InShot)
{
	Shot = InShot;
	Position = Shot.Start;
	Velocity = Shot.Velocity;
	Traveled = 0.f;
	bLaunched = true;
	UpdateTracer();
}

void AGunProjectile::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bLaunched)
	{
		return;
	}

	const FVector Gravity(0.f, 0.f, Shot.GravityZ);
	FVector NewPosition;
	FVector NewVelocity;
	if (Shot.Drag <= 0.f)
	{
		// Exact for constant gravity: the step lands where the real parabola is, at any frame rate.
		NewPosition = Position + Velocity * DeltaSeconds + 0.5f * Gravity * FMath::Square(DeltaSeconds);
		NewVelocity = Velocity + Gravity * DeltaSeconds;
	}
	else
	{
		// Gravity plus air drag (slows the bullet, so it also drops more the further it goes). Small
		// fixed substeps keep it the same at any frame rate.
		constexpr float MaxSubstep = 1.f / 120.f;
		const int32 Steps = FMath::Clamp(FMath::CeilToInt(DeltaSeconds / MaxSubstep), 1, 16);
		const float Step = DeltaSeconds / Steps;
		NewPosition = Position;
		NewVelocity = Velocity;
		for (int32 i = 0; i < Steps; ++i)
		{
			const FVector Accel = Gravity - Shot.Drag * NewVelocity.Size() * NewVelocity;
			const FVector NextVelocity = NewVelocity + Accel * Step;
			NewPosition += (NewVelocity + NextVelocity) * (0.5f * Step);
			NewVelocity = NextVelocity;
		}
	}

	// Never hit the shooter, nor what it rides on or stands in (the balloon and everything attached to it).
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GunProjectile), /*bTraceComplex=*/true, this);
	if (AActor* Shooter = Shot.Shooter.Get())
	{
		AActor* Ride = Shooter;
		while (AActor* Parent = Ride->GetAttachParentActor())
		{
			Ride = Parent;
		}
		TArray<AActor*> Attached;
		Ride->GetAttachedActors(Attached, /*bResetArray=*/true, /*bRecursivelyIncludeAttachedActors=*/true);
		Params.AddIgnoredActor(Ride);
		Params.AddIgnoredActors(Attached);
	}

	FHitResult Hit;
	const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, Position, NewPosition, Shot.TraceChannel, Params);

	if (Shot.bDebug)
	{
		DrawDebugLine(GetWorld(), Position, bHit ? Hit.ImpactPoint : NewPosition, bHit ? FColor::Green : FColor::Red, false, 2.f, 0, 0.5f);
	}

	if (bHit)
	{
		Traveled += FVector::Dist(Position, Hit.ImpactPoint);
		Position = Hit.ImpactPoint;
		HandleHit(Hit);
		Destroy();
		return;
	}

	Traveled += FVector::Dist(Position, NewPosition);
	Position = NewPosition;
	Velocity = NewVelocity;

	if (Traveled >= Shot.MaxDistance)
	{
		Destroy();
		return;
	}

	UpdateTracer();
}

void AGunProjectile::HandleHit(const FHitResult& Hit)
{
	const FVector Direction = Velocity.GetSafeNormal();

	if (Shot.bDebug)
	{
		DrawDebugPoint(GetWorld(), Hit.ImpactPoint, 8.f, FColor::Yellow, false, 2.f);
	}

	if (AActor* HitActor = Hit.GetActor())
	{
		UGameplayStatics::ApplyPointDamage(HitActor, Shot.Damage, Direction, Hit, Shot.InstigatorController.Get(), Shot.Shooter.Get(), UDamageType::StaticClass());
	}

	UPrimitiveComponent* HitComponent = Hit.GetComponent();
	if (HitComponent && HitComponent->IsSimulatingPhysics())
	{
		HitComponent->AddImpulseAtLocation(Direction * Shot.HitImpulse, Hit.ImpactPoint, Hit.BoneName);
	}

	if (UGunComponent* Gun = Shot.Gun.Get())
	{
		Gun->OnShotHit.Broadcast(Hit);
	}
}

void AGunProjectile::UpdateTracer()
{
	// Start on the muzzle and slide onto the real path from the eye as the bullet gets away.
	const float Offset = VisualConvergeDistance > 0.f ? FMath::Clamp(1.f - Traveled / VisualConvergeDistance, 0.f, 1.f) : 0.f;
	const FVector Head = Position + (Shot.Muzzle - Shot.Start) * Offset;
	const FVector Direction = Velocity.GetSafeNormal();

	// Far shapes are kept a minimum size on screen, measured from the camera.
	float CameraDistance = 0.f;
	if (const APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		if (PC->PlayerCameraManager)
		{
			CameraDistance = FVector::Dist(PC->PlayerCameraManager->GetCameraLocation(), Head);
		}
	}

	// The bullet: visible from the moment it leaves the muzzle.
	const float Size = FMath::Max(BulletSize, CameraDistance * BulletMinSizePerDistance) * Shot.VisualScale;
	Bullet->SetWorldLocation(Head);
	Bullet->SetWorldScale3D(FVector(Size / ShapeSize));
	Bullet->SetVisibility(true);

	// The streak behind it. Its tail never reaches back past the muzzle, so it grows out of the barrel.
	const float Length = FMath::Min(Velocity.Size() * TracerSeconds * Shot.VisualScale, FVector::Dist(Head, Shot.Muzzle));
	if (Length < 1.f || Direction.IsNearlyZero())
	{
		Tracer->SetVisibility(false);
		return;
	}

	const FVector Center = Head - Direction * (Length * 0.5f);
	const float Width = FMath::Max(TracerWidth, CameraDistance * TracerMinWidthPerDistance) * Shot.VisualScale;

	// The cylinder's axis is Z: point it along the flight and stretch it to the streak.
	Tracer->SetWorldLocationAndRotation(Center, FRotationMatrix::MakeFromZ(Direction).Rotator());
	Tracer->SetWorldScale3D(FVector(Width / ShapeSize, Width / ShapeSize, Length / ShapeSize));
	Tracer->SetVisibility(true);
}
