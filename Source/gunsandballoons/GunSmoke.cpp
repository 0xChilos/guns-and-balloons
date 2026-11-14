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
	/** Parameters of the engine's Widget3DPassThrough materials (as UWidgetComponent sets them). */
	const FName TextureParameter(TEXT("SlateUI"));
	const FName TintParameter(TEXT("TintColorAndOpacity"));
	const FName OpacityFromTextureParameter(TEXT("OpacityFromTexture"));

	/** Size of /Engine/BasicShapes/Plane (cm). */
	constexpr float PuffMeshSize = 100.f;

	/** A fresh puff fades in over this long (s) when it moves every frame (AnimationRate 0). */
	constexpr float FadeInTime = 0.05f;

	/** Longest physics step (s); a longer animation step is split into several. */
	constexpr float MaxStep = 1.f / 60.f;
	constexpr int32 MaxSteps = 8;

	/** Most time (s) one tick steps, so a hitch doesn't fast-forward the smoke. */
	constexpr float MaxTickTime = 0.25f;

	/** 4x4 Bayer matrix: the order in which a puff's pixels drop out as it thins. */
	constexpr int32 Bayer[4][4] = { { 0, 8, 2, 10 }, { 12, 4, 14, 6 }, { 3, 11, 1, 9 }, { 15, 7, 13, 5 } };

	/** Opacity steps the dither can show (one per Bayer threshold). */
	constexpr int32 DitherLevels = 16;

	/** Edge measure (0 centre, 1 edge) where the light core ends and the darker rim begins. */
	constexpr float CoreEdge = 0.5f;

	/** Edge measure from which the puff starts to dither out toward its outline. */
	constexpr float SolidEdge = 0.6f;

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
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Material(TEXT("/Engine/EngineMaterials/Widget3DPassThrough_Masked.Widget3DPassThrough_Masked"));
	PuffMesh = Plane.Object;
	PuffMaterial = Material.Object;
}

void AGunSmoke::BeginPlay()
{
	Super::BeginPlay();
	CreatePuffTextures();
}

void AGunSmoke::CreatePuffTextures()
{
	const int32 Size = FMath::Clamp(TextureSize, 8, 64);
	NumShapes = FMath::Clamp(ShapeVariants, 1, 8);
	PuffTextures.Reset(NumShapes * DitherLevels);

	TArray<float> Density;
	TArray<uint8> Shade;
	Density.SetNumUninitialized(Size * Size);
	Shade.SetNumUninitialized(Size * Size);
	TArray<uint8> Pixels;
	Pixels.SetNumUninitialized(Size * Size * 4);

	const uint8 RimValue = static_cast<uint8>(FMath::RoundToInt(FMath::Clamp(RimShade, 0.f, 1.f) * 255.f));

	for (int32 ShapeIndex = 0; ShapeIndex < NumShapes; ++ShapeIndex)
	{
		// A lumpy blob: two octaves of noise push the outline in and out; each shape samples another slice.
		const float Slice = 0.37f + 1.91f * ShapeIndex;
		for (int32 Y = 0; Y < Size; ++Y)
		{
			for (int32 X = 0; X < Size; ++X)
			{
				// Distance from the centre, 1 at the edge of the quad (a little margin so lumps fit).
				const FVector2D P((X + 0.5f) / Size * 2.f - 1.f, (Y + 0.5f) / Size * 2.f - 1.f);
				const FVector N(P.X * 2.2f, P.Y * 2.2f, Slice);
				const float Lumps = 0.65f * FMath::PerlinNoise3D(N) + 0.35f * FMath::PerlinNoise3D(N * 2.1f + FVector(5.3f, 1.7f, 0.f));
				const float Edge = P.Size() * 1.1f + 0.25f * Lumps;

				// Solid inside, thinning to nothing at the outline; the thin part dithers away first.
				Density[Y * Size + X] = FMath::Clamp((1.f - Edge) / (1.f - SolidEdge), 0.f, 1.f);
				// Flat two-tone: light core, darker rim.
				Shade[Y * Size + X] = Edge < CoreEdge ? 255 : RimValue;
			}
		}

		for (int32 Level = 1; Level <= DitherLevels; ++Level)
		{
			for (int32 Y = 0; Y < Size; ++Y)
			{
				for (int32 X = 0; X < Size; ++X)
				{
					const int32 i = Y * Size + X;
					// Drawn where density x level clears this pixel's Bayer threshold (B, G, R, A).
					const bool bDrawn = Density[i] * Level > Bayer[Y & 3][X & 3] + 0.5f;
					uint8* Pixel = &Pixels[i * 4];
					Pixel[0] = Pixel[1] = Pixel[2] = Shade[i];
					Pixel[3] = bDrawn ? 255 : 0;
				}
			}

			UTexture2D* Texture = UTexture2D::CreateTransient(Size, Size, PF_B8G8R8A8, NAME_None, MakeArrayView(Pixels));
			if (Texture)
			{
				Texture->Filter = TF_Nearest;
				Texture->AddressX = TA_Clamp;
				Texture->AddressY = TA_Clamp;
				Texture->SRGB = false;
				Texture->NeverStream = true;
				Texture->UpdateResource();
			}
			PuffTextures.Add(Texture);
		}
	}
}

UTexture2D* AGunSmoke::GetPuffTexture(int32 Shape, float Opacity) const
{
	const int32 Level = FMath::Min(FMath::RoundToInt(Opacity * DitherLevels), DitherLevels);
	if (Level <= 0 || NumShapes <= 0)
	{
		return nullptr;
	}
	const int32 Index = FMath::Clamp(Shape, 0, NumShapes - 1) * DitherLevels + Level - 1;
	return PuffTextures.IsValidIndex(Index) ? PuffTextures[Index].Get() : nullptr;
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
		Puff.Shape = FMath::RandHelper(FMath::Max(NumShapes, 1));
		Puff.Quarter = FMath::RandHelper(4);
		// Shows at once, at its fresh look (no wait for the next animation step).
		UpdateLook(Puff);

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

	if (!IsActorTickEnabled())
	{
		PendingTime = 0.f;
		SetActorTickEnabled(true);
	}
}

void AGunSmoke::UpdateLook(FPuff& Puff) const
{
	// Spreads out from StartRadius toward EndRadius; the same smoke over a bigger puff is thinner
	// with the square of the radius (Beer-Lambert: opacity = 1 - e^-depth).
	Puff.Radius = Puff.EndRadius + (Puff.StartRadius - Puff.EndRadius) * FMath::Exp(-Puff.Age / Puff.GrowTime);
	const float Depth = Puff.Density * FMath::Square(Puff.StartRadius / Puff.Radius);
	float Opacity = (1.f - FMath::Exp(-Depth)) * MaxOpacity;

	// A stepped puff pops in, like an old sprite; a smooth one fades in so it doesn't.
	if (AnimationRate <= 0.f)
	{
		Opacity *= FMath::Clamp(Puff.Age / FadeInTime, 0.f, 1.f);
	}
	Opacity *= FMath::Clamp((1.f - Puff.Age / Puff.Life) / FadeOutShare, 0.f, 1.f);
	Puff.Opacity = Opacity;
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
			Material->SetScalarParameterValue(OpacityFromTextureParameter, 1.f);
			Component->SetMaterial(0, Material);
		}

		PuffComponents.Add(Component);
		PuffMaterials.Add(Material);
		SlotTextures.Add(nullptr);
	}
	return PuffComponents[Index];
}

void AGunSmoke::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Step the motion and look, every frame or at AnimationRate.
	PendingTime = FMath::Min(PendingTime + DeltaSeconds, MaxTickTime);
	const float Interval = AnimationRate > 0.f ? 1.f / AnimationRate : 0.f;
	const float StepTime = Interval > 0.f ? Interval * FMath::FloorToFloat(PendingTime / Interval) : PendingTime;

	if (StepTime > 0.f)
	{
		PendingTime -= StepTime;
		const float Time = GetWorld()->GetTimeSeconds();

		// Puffs don't catch on the shooter (nor the gun, which has no collision).
		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(GunSmoke), /*bTraceComplex=*/false);
		QueryParams.AddIgnoredActor(this);
		if (GetOwner())
		{
			QueryParams.AddIgnoredActor(GetOwner());
		}

		const int32 Steps = FMath::Clamp(FMath::CeilToInt(StepTime / MaxStep), 1, MaxSteps);
		const float Step = StepTime / Steps;
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
				continue;
			}
			UpdateLook(Puff);
		}
	}

	// Draw every frame: slot i shows Puffs[i], upright on screen, facing the camera.
	const APlayerCameraManager* CameraManager = UGameplayStatics::GetPlayerCameraManager(this, 0);
	const FVector CameraLocation = CameraManager ? CameraManager->GetCameraLocation() : FVector::ZeroVector;
	const FVector CameraUp = CameraManager ? CameraManager->GetCameraRotation().Quaternion().GetUpVector() : FVector::UpVector;

	for (int32 i = 0; i < Puffs.Num(); ++i)
	{
		const FPuff& Puff = Puffs[i];
		const FVector ToCamera = CameraLocation - Puff.Location;
		const float Distance = ToCamera.Size();

		// Dissolves (dither) as the camera comes close, so it never cuts at the near plane.
		const float Opacity = Puff.Opacity * FMath::SmoothStep(NearFadeDistance, NearFadeDistance + Puff.Radius, Distance);
		UTexture2D* Texture = GetPuffTexture(Puff.Shape, Opacity);

		UStaticMeshComponent* Component = GetPuffComponent(i);
		if (!Texture)
		{
			// Too thin for a single dither pixel.
			Component->SetVisibility(false);
			continue;
		}

		const FVector Normal = Distance > UE_KINDA_SMALL_NUMBER ? ToCamera / Distance : FVector::UpVector;
		const FQuat Facing = FQuat(Normal, Puff.Quarter * UE_HALF_PI) * FRotationMatrix::MakeFromZX(Normal, CameraUp).ToQuat();
		Component->SetWorldTransform(FTransform(Facing, Puff.Location, FVector(Puff.Radius * 2.f / PuffMeshSize)));

		if (UMaterialInstanceDynamic* Material = PuffMaterials[i])
		{
			if (SlotTextures[i] != Texture)
			{
				Material->SetTextureParameterValue(TextureParameter, Texture);
				SlotTextures[i] = Texture;
			}
			Material->SetVectorParameterValue(TintParameter, FLinearColor(Color.R, Color.G, Color.B, 1.f));
		}
		Component->SetVisibility(true);
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
