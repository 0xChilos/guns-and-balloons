#include "GunSmoke.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	/** Parameters of /Engine/EngineMaterials/Widget3DPassThrough_Translucent (as UWidgetComponent sets them). */
	const FName TextureParameter(TEXT("SlateUI"));
	const FName TintParameter(TEXT("TintColorAndOpacity"));
	const FName OpacityFromTextureParameter(TEXT("OpacityFromTexture"));

	/** Size of /Engine/BasicShapes/Plane (cm). */
	constexpr float PuffMeshSize = 100.f;

	/** A fresh puff fades in over this long (s), so it never pops in. */
	constexpr float FadeInTime = 0.05f;

	/** Longest physics step (s); a slower frame is split into several. */
	constexpr float MaxStep = 1.f / 60.f;
	constexpr int32 MaxSteps = 4;

	/** How fast (rad/s) a puff turns at most, so the same sprite never reads as the same shape. */
	constexpr float MaxRollSpeed = 0.8f;

	/** Offsets into the noise field for the three turbulence axes, so they are not the same. */
	const FVector TurbulenceOffsets[3] = { FVector(0.f, 0.f, 0.f), FVector(31.7f, 17.3f, 5.9f), FVector(-13.1f, 47.9f, 23.3f) };
}

AGunSmoke::AGunSmoke()
{
	PrimaryActorTick.bCanEverTick = true;
	// Sleeps while there is no smoke; Emit wakes it.
	PrimaryActorTick.bStartWithTickEnabled = false;
	// After the camera has moved this frame, so the sprites face where it is now.
	PrimaryActorTick.TickGroup = TG_PostPhysics;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Plane(TEXT("/Engine/BasicShapes/Plane.Plane"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Material(TEXT("/Engine/EngineMaterials/Widget3DPassThrough_Translucent.Widget3DPassThrough_Translucent"));
	PuffMesh = Plane.Object;
	PuffMaterial = Material.Object;
}

void AGunSmoke::BeginPlay()
{
	Super::BeginPlay();
	CreatePuffTexture();
}

void AGunSmoke::CreatePuffTexture()
{
	const int32 Size = FMath::Clamp(TextureSize, 8, 256);
	TArray<uint8> Pixels;
	Pixels.SetNumUninitialized(Size * Size * 4);

	for (int32 Y = 0; Y < Size; ++Y)
	{
		for (int32 X = 0; X < Size; ++X)
		{
			// Distance from the centre, 1 at the edge of the quad.
			const FVector2D P((X + 0.5f) / Size * 2.f - 1.f, (Y + 0.5f) / Size * 2.f - 1.f);
			const float Distance = P.Size();

			// Lumps: two octaves of noise around a soft round falloff, so the edge is a billow, not a disc.
			const FVector N(P.X * 2.2f, P.Y * 2.2f, 0.37f);
			const float Lumps = 0.65f * FMath::PerlinNoise3D(N) + 0.35f * FMath::PerlinNoise3D(N * 2.1f + FVector(5.3f, 1.7f, 0.f));
			const float Edge = Distance + 0.22f * Lumps;
			const float Falloff = FMath::Square(FMath::Clamp(1.f - Edge, 0.f, 1.f));
			const float Alpha = FMath::Clamp(Falloff * (1.25f + 0.5f * Lumps), 0.f, 1.f);

			// White; the density is the alpha (B, G, R, A).
			uint8* Pixel = &Pixels[(Y * Size + X) * 4];
			Pixel[0] = Pixel[1] = Pixel[2] = 255;
			Pixel[3] = static_cast<uint8>(FMath::RoundToInt(Alpha * 255.f));
		}
	}

	PuffTexture = UTexture2D::CreateTransient(Size, Size, PF_B8G8R8A8, NAME_None, MakeArrayView(Pixels));
	if (PuffTexture)
	{
		PuffTexture->Filter = bPixelated ? TF_Nearest : TF_Bilinear;
		PuffTexture->AddressX = TA_Clamp;
		PuffTexture->AddressY = TA_Clamp;
		PuffTexture->SRGB = false;
		PuffTexture->UpdateResource();
	}
}

void AGunSmoke::Emit(const FGunSmokePuffs& Kind, int32 Count, const FVector& Location, const FVector& Direction, const FVector& SourceVelocity, float Strength)
{
	if (Count <= 0 || Strength <= 0.f || Kind.Density <= 0.f)
	{
		return;
	}

	const FVector Heading = Direction.GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
	const float ConeHalfAngle = FMath::DegreesToRadians(Kind.ConeAngle);
	const int32 Limit = FMath::Max(MaxPuffs, 1);

	for (int32 i = 0; i < Count; ++i)
	{
		FPuff Puff;
		Puff.Velocity = FMath::VRandCone(Heading, ConeHalfAngle) * FMath::FRandRange(Kind.SpeedMin, FMath::Max(Kind.SpeedMin, Kind.SpeedMax))
			+ SourceVelocity * Kind.InheritVelocity;
		Puff.Location = Location + FMath::VRand() * (Kind.StartRadius * 0.5f * FMath::FRand());
		Puff.Age = 0.f;
		Puff.Life = FMath::FRandRange(Kind.LifeMin, FMath::Max(Kind.LifeMin, Kind.LifeMax));
		Puff.StartRadius = Kind.StartRadius * FMath::FRandRange(0.8f, 1.2f);
		Puff.EndRadius = FMath::Max(Kind.EndRadius * FMath::FRandRange(0.75f, 1.25f), Puff.StartRadius);
		Puff.GrowTime = Kind.GrowTime * FMath::FRandRange(0.8f, 1.2f);
		Puff.Density = Kind.Density * Strength * FMath::FRandRange(0.8f, 1.2f);
		Puff.Buoyancy = Kind.Buoyancy * FMath::FRandRange(0.8f, 1.2f);
		Puff.CoolTime = Kind.CoolTime;
		Puff.Roll = FMath::FRandRange(0.f, UE_TWO_PI);
		Puff.RollSpeed = FMath::FRandRange(-MaxRollSpeed, MaxRollSpeed);

		if (Puffs.Num() < Limit)
		{
			Puffs.Add(Puff);
			continue;
		}

		// Full: the puff nearest the end of its life gives way.
		int32 Oldest = 0;
		for (int32 j = 1; j < Puffs.Num(); ++j)
		{
			if (Puffs[j].Age / Puffs[j].Life > Puffs[Oldest].Age / Puffs[Oldest].Life)
			{
				Oldest = j;
			}
		}
		Puffs[Oldest] = Puff;
	}

	SetActorTickEnabled(true);
}

void AGunSmoke::Simulate(FPuff& Puff, float DeltaSeconds, float Time, const FCollisionQueryParams* Query) const
{
	// Hot smoke rises; the pull fades as it cools.
	FVector Accel(0.f, 0.f, Puff.Buoyancy * FMath::Exp(-Puff.Age / Puff.CoolTime));

	// One turbulence field for all puffs, so neighbours swirl together.
	if (Turbulence > 0.f)
	{
		const FVector Sample = Puff.Location / TurbulenceScale + FVector(Time * TurbulenceSpeed);
		Accel += Turbulence * FVector(
			FMath::PerlinNoise3D(Sample + TurbulenceOffsets[0]),
			FMath::PerlinNoise3D(Sample + TurbulenceOffsets[1]),
			FMath::PerlinNoise3D(Sample + TurbulenceOffsets[2]));
	}

	// Drag against the air (dv/dt = -(c + k|v|) v, relative to the wind), stepped implicitly so a fast
	// fresh puff can't overshoot and reverse.
	const FVector Relative = Puff.Velocity - Wind;
	const float DragRate = LinearDrag + QuadraticDrag * Relative.Size();
	Puff.Velocity = Wind + (Relative + Accel * DeltaSeconds) / (1.f + DragRate * DeltaSeconds);

	const FVector Start = Puff.Location;
	FVector End = Start + Puff.Velocity * DeltaSeconds;

	if (Query && !End.Equals(Start, 0.01f))
	{
		FHitResult Hit;
		if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, CollisionChannel, *Query) && !Hit.bStartPenetrating)
		{
			// Stop at the surface and slide along it: the part of the motion into it is gone.
			const FVector Normal = Hit.ImpactNormal;
			End = Hit.Location + Normal * 0.5f;
			const float Into = FVector::DotProduct(Puff.Velocity, Normal);
			if (Into < 0.f)
			{
				Puff.Velocity -= Normal * Into;
			}
		}
	}

	Puff.Location = End;
	Puff.Roll += Puff.RollSpeed * DeltaSeconds;
}

UStaticMeshComponent* AGunSmoke::GetPuffComponent(int32 Index)
{
	while (PuffComponents.Num() <= Index)
	{
		UStaticMeshComponent* Component = NewObject<UStaticMeshComponent>(this, NAME_None, RF_Transient);
		Component->SetMobility(EComponentMobility::Movable);
		Component->SetUsingAbsoluteLocation(true);
		Component->SetUsingAbsoluteRotation(true);
		Component->SetUsingAbsoluteScale(true);
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Component->SetGenerateOverlapEvents(false);
		Component->SetCanEverAffectNavigation(false);
		Component->SetCastShadow(false);
		Component->SetReceivesDecals(false);
		Component->SetStaticMesh(PuffMesh);
		Component->SetVisibility(false);
		Component->SetupAttachment(Root);
		Component->RegisterComponent();

		UMaterialInstanceDynamic* Material = PuffMaterial ? UMaterialInstanceDynamic::Create(PuffMaterial, this) : nullptr;
		if (Material)
		{
			Material->SetTextureParameterValue(TextureParameter, PuffTexture);
			Material->SetScalarParameterValue(OpacityFromTextureParameter, 1.f);
			Component->SetMaterial(0, Material);
		}

		PuffComponents.Add(Component);
		PuffMaterials.Add(Material);
	}
	return PuffComponents[Index];
}

void AGunSmoke::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const float Time = GetWorld()->GetTimeSeconds();

	// Puffs don't catch on the shooter (nor the gun, which has no collision).
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(GunSmoke), /*bTraceComplex=*/false);
	QueryParams.AddIgnoredActor(this);
	if (GetOwner())
	{
		QueryParams.AddIgnoredActor(GetOwner());
	}

	const int32 Steps = FMath::Clamp(FMath::CeilToInt(DeltaSeconds / MaxStep), 1, MaxSteps);
	const float Step = DeltaSeconds / Steps;
	for (int32 i = Puffs.Num() - 1; i >= 0; --i)
	{
		FPuff& Puff = Puffs[i];
		for (int32 s = 0; s < Steps; ++s)
		{
			Simulate(Puff, Step, Time, bCollide ? &QueryParams : nullptr);
			Puff.Age += Step;
		}
		if (Puff.Age >= Puff.Life)
		{
			Puffs.RemoveAtSwap(i, EAllowShrinking::No);
		}
	}

	// Draw: slot i shows Puffs[i], facing the camera.
	const APlayerCameraManager* CameraManager = UGameplayStatics::GetPlayerCameraManager(this, 0);
	const FVector CameraLocation = CameraManager ? CameraManager->GetCameraLocation() : FVector::ZeroVector;

	for (int32 i = 0; i < Puffs.Num(); ++i)
	{
		const FPuff& Puff = Puffs[i];

		// Spreads out from StartRadius toward EndRadius; the same smoke over a bigger puff is thinner
		// with the square of the radius (Beer-Lambert: opacity = 1 - e^-depth).
		const float Radius = Puff.EndRadius + (Puff.StartRadius - Puff.EndRadius) * FMath::Exp(-Puff.Age / Puff.GrowTime);
		const float Depth = Puff.Density * FMath::Square(Puff.StartRadius / Radius);
		float Opacity = (1.f - FMath::Exp(-Depth)) * MaxOpacity;

		const FVector ToCamera = CameraLocation - Puff.Location;
		const float Distance = ToCamera.Size();
		Opacity *= FMath::Clamp(Puff.Age / FadeInTime, 0.f, 1.f);
		Opacity *= FMath::Clamp((1.f - Puff.Age / Puff.Life) / FadeOutShare, 0.f, 1.f);
		Opacity *= FMath::SmoothStep(NearFadeDistance, NearFadeDistance + Radius, Distance);

		const FVector Normal = Distance > UE_KINDA_SMALL_NUMBER ? ToCamera / Distance : FVector::UpVector;
		const FQuat Facing = FQuat(Normal, Puff.Roll) * FRotationMatrix::MakeFromZ(Normal).ToQuat();

		UStaticMeshComponent* Component = GetPuffComponent(i);
		Component->SetWorldTransform(FTransform(Facing, Puff.Location, FVector(Radius * 2.f / PuffMeshSize)));
		if (UMaterialInstanceDynamic* Material = PuffMaterials[i])
		{
			Material->SetVectorParameterValue(TintParameter, FLinearColor(Color.R, Color.G, Color.B, Opacity));
		}
		if (i >= NumVisible)
		{
			Component->SetVisibility(true);
		}
	}

	for (int32 i = Puffs.Num(); i < NumVisible; ++i)
	{
		PuffComponents[i]->SetVisibility(false);
	}
	NumVisible = Puffs.Num();

	if (Puffs.IsEmpty())
	{
		SetActorTickEnabled(false);
	}
}
