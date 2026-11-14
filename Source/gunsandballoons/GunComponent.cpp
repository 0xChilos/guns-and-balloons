#include "GunComponent.h"

#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/Texture.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GunProjectile.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
	/** A casing never leaves faster than this (cm/s), whatever the frame timing. */
	constexpr float MaxCasingSpeed = 600.f;

	/** Nor spins faster than this (degrees/s) from the case's own motion. */
	constexpr float MaxCasingSpin = 1080.f;

	/** Motion (cm/s) a smoke source can hand on at most, whatever the frame timing (teleports, a mouse flick). */
	constexpr float MaxSmokeSourceSpeed = 3000.f;

	/** Smoke streams running at once per gun; the oldest stops for a new one. */
	constexpr int32 MaxSmokeStreams = 8;

	/** Below this much leftover smoke (share of a fresh shot's) opening the gun shows none. */
	constexpr float MinBreechResidue = 0.05f;
}

UGunComponent::UGunComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;

	ProjectileClass = AGunProjectile::StaticClass();
	SmokeClass = AGunSmoke::StaticClass();

	// Black powder: a thick cloud thrown a metre or so out of the muzzle, where drag stops it and it
	// hangs, spreads and rises. First-pass values (not in the design), tune in game.
	MuzzleBlastSmoke.Amount = 10.f;
	MuzzleBlastSmoke.SpeedMin = 150.f;
	MuzzleBlastSmoke.SpeedMax = 700.f;
	MuzzleBlastSmoke.ConeAngle = 14.f;
	MuzzleBlastSmoke.StartRadius = 3.f;
	MuzzleBlastSmoke.EndRadius = 30.f;
	MuzzleBlastSmoke.GrowTime = 0.8f;
	MuzzleBlastSmoke.Density = 3.f;
	MuzzleBlastSmoke.LifeMin = 2.f;
	MuzzleBlastSmoke.LifeMax = 3.5f;
	MuzzleBlastSmoke.Buoyancy = 40.f;
	MuzzleBlastSmoke.CoolTime = 1.5f;
	MuzzleBlastSmoke.InheritVelocity = 1.f;

	// Then the barrel keeps smoking: a thin stream that rises off the muzzle and trails it as the gun moves.
	MuzzleWispSmoke.Amount = 14.f;
	MuzzleWispSmoke.Duration = 1.6f;
	MuzzleWispSmoke.Delay = 0.08f;
	MuzzleWispSmoke.SpeedMin = 5.f;
	MuzzleWispSmoke.SpeedMax = 15.f;
	MuzzleWispSmoke.ConeAngle = 25.f;
	MuzzleWispSmoke.StartRadius = 1.2f;
	MuzzleWispSmoke.EndRadius = 10.f;
	MuzzleWispSmoke.GrowTime = 1.2f;
	MuzzleWispSmoke.Density = 1.5f;
	MuzzleWispSmoke.LifeMin = 1.5f;
	MuzzleWispSmoke.LifeMax = 2.5f;
	MuzzleWispSmoke.Buoyancy = 70.f;
	MuzzleWispSmoke.CoolTime = 2.f;
	MuzzleWispSmoke.InheritVelocity = 0.2f;

	// A little gas left in the chamber, let out when the action opens.
	BreechSmoke.Amount = 10.f;
	BreechSmoke.Duration = 1.f;
	BreechSmoke.SpeedMin = 5.f;
	BreechSmoke.SpeedMax = 20.f;
	BreechSmoke.ConeAngle = 35.f;
	BreechSmoke.StartRadius = 1.5f;
	BreechSmoke.EndRadius = 9.f;
	BreechSmoke.GrowTime = 1.f;
	BreechSmoke.Density = 1.2f;
	BreechSmoke.LifeMin = 1.2f;
	BreechSmoke.LifeMax = 2.2f;
	BreechSmoke.Buoyancy = 60.f;
	BreechSmoke.CoolTime = 1.5f;
	BreechSmoke.InheritVelocity = 0.2f;
}

void UGunComponent::SetupWeapon(USkeletalMeshComponent* InMesh, UCameraComponent* InCamera)
{
	Mesh = InMesh;
	Camera = InCamera;
}

void UGunComponent::BeginPlay()
{
	Super::BeginPlay();

	if (Mesh)
	{
		// Pose values are written here, so the mesh evaluates after this component each frame.
		Mesh->PrimaryComponentTick.AddPrerequisite(this, PrimaryComponentTick);
	}

	ApplyWeaponTexture();

	if (SmokeClass)
	{
		// The smoke lives in the world, not on the gun: it stays behind as the player moves on.
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = GetOwner();
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		SpawnParams.ObjectFlags |= RF_Transient;
		Smoke = GetWorld()->SpawnActor<AGunSmoke>(SmokeClass, FTransform::Identity, SpawnParams);
	}
}

void UGunComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Smoke)
	{
		Smoke->Destroy();
		Smoke = nullptr;
	}
	SmokeStreams.Reset();

	Super::EndPlay(EndPlayReason);
}

void UGunComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	PrevTickDeltaTime = CurrentTickDeltaTime;
	CurrentTickDeltaTime = DeltaTime;

	if (Camera && DeltaTime > KINDA_SMALL_NUMBER)
	{
		const FVector Location = Camera->GetComponentLocation();
		CameraVelocity = bHasCameraLocation ? ((Location - LastCameraLocation) / DeltaTime).GetClampedToMaxSize(MaxSmokeSourceSpeed) : FVector::ZeroVector;
		LastCameraLocation = Location;
		bHasCameraLocation = true;
	}

	UpdateSmoke(DeltaTime);
}

void UGunComponent::SetEquipped(bool bInEquipped)
{
	bEquipped = bInEquipped;

	// A stream follows the gun mesh; a holstered gun stops smoking (what is already out stays in the air).
	SmokeStreams.Reset();
	bHasCameraLocation = false;

	// A holstered gun keeps its state (rounds, spent cases) but neither runs nor draws.
	SetComponentTickEnabled(bInEquipped);
	if (Mesh)
	{
		Mesh->SetVisibility(bInEquipped, /*bPropagateToChildren=*/true);
		Mesh->SetComponentTickEnabled(bInEquipped);
	}
}

void UGunComponent::ApplyWeaponTexture()
{
	if (!Mesh || !WeaponTexture)
	{
		return;
	}

	for (int32 i = 0; i < Mesh->GetNumMaterials(); ++i)
	{
		UMaterialInstanceDynamic* Material = Mesh->CreateAndSetMaterialInstanceDynamic(i);
		if (!Material)
		{
			continue;
		}

		Material->SetTextureParameterValue(WeaponTextureParameter, WeaponTexture);
		// Interchange blends the map over the flat diffuse colour by "<Map>Weight"; make it all map.
		Material->SetScalarParameterValue(FName(*(WeaponTextureParameter.ToString() + TEXT("Weight"))), 1.f);

		if (!CasingMaterial)
		{
			CasingMaterial = Material;
		}
	}
}

void UGunComponent::FireShot()
{
	AActor* Owner = GetOwner();
	APawn* Pawn = Cast<APawn>(Owner);

	// The shot flies from the eye along the view, so it lands on the crosshair (before it drops) give or
	// take the spread. Center-weighted: most shots stay close, only a few reach the edge.
	FGunShot Shot;
	Shot.Start = Camera ? Camera->GetComponentLocation() : Owner->GetActorLocation();
	const FVector ShotLine = OffsetDirection((Pawn ? Pawn->GetViewRotation() : Owner->GetActorRotation()).Vector(), SpreadAngle, /*bEven=*/false);
	Shot.Muzzle = Mesh ? FirstPersonToWorld(Mesh->GetComponentTransform().TransformPosition(MuzzleOffset)) : Shot.Start;
	Shot.GravityZ = GetWorld()->GetGravityZ() * GravityScale;
	// Quadratic drag (dv/dt = -k v^2, so dv/dx = -k v and v = v0 * e^(-k x)) halves the speed over
	// SpeedHalvingDistance when k = ln2 / d.
	Shot.Drag = SpeedHalvingDistance > 0.f ? UE_LN2 / (SpeedHalvingDistance * 100.f) : 0.f;
	Shot.VisualScale = ProjectileVisualScale;
	Shot.MaxDistance = Range;
	Shot.Damage = Damage;
	Shot.HitImpulse = HitImpulse;
	Shot.TraceChannel = TraceChannel;
	Shot.bDebug = bDebugShots;
	Shot.Shooter = Owner;
	Shot.InstigatorController = Pawn ? Pawn->GetController() : nullptr;
	Shot.Gun = this;

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = Owner;
	SpawnParams.Instigator = Pawn;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	// One bullet, or a pattern of pellets spread evenly around the shot's line.
	UClass* Class = ProjectileClass ? ProjectileClass.Get() : AGunProjectile::StaticClass();
	for (int32 i = 0; i < ProjectilesPerShot; ++i)
	{
		const FVector Direction = ProjectilesPerShot > 1 ? OffsetDirection(ShotLine, PelletSpreadAngle, /*bEven=*/true) : ShotLine;
		Shot.Velocity = Direction * MuzzleVelocity;
		if (AGunProjectile* Projectile = GetWorld()->SpawnActor<AGunProjectile>(Class, Shot.Muzzle, Shot.Velocity.Rotation(), SpawnParams))
		{
			Projectile->Launch(Shot);
		}
	}

	// The muzzle blast and the barrel smoking after it; and the chamber is full of smoke until opened.
	BreechResidue = 1.f;
	ResidueTime = GetWorld()->GetTimeSeconds();
	StartSmokeStream(MuzzleBlastSmoke, NAME_None, MuzzleOffset, FVector::ForwardVector, 1.f);
	StartSmokeStream(MuzzleWispSmoke, NAME_None, MuzzleOffset, FVector::ForwardVector, 1.f);

	OnFired.Broadcast();
}

float UGunComponent::GetBreechResidue() const
{
	const double Elapsed = GetWorld()->GetTimeSeconds() - ResidueTime;
	return BreechResidue * FMath::Exp(-float(Elapsed) / FMath::Max(BreechSmokeClearTime, 0.1f));
}

void UGunComponent::EmitBreechSmoke(TConstArrayView<FName> Bones)
{
	const float Residue = GetBreechResidue();
	if (Residue < MinBreechResidue)
	{
		return;
	}
	// Out it goes: there's none left for the next time the gun opens.
	BreechResidue = 0.f;

	// The gas leaves the chamber backwards, out of the open breech, then rises.
	for (const FName Bone : Bones)
	{
		StartSmokeStream(BreechSmoke, Bone, FVector::ZeroVector, -FVector::ForwardVector, Residue);
	}
}

void UGunComponent::StartSmokeStream(const FGunSmokePuffs& Puffs, FName Bone, const FVector& Offset, const FVector& Direction, float Strength)
{
	if (!Smoke || !Mesh || Puffs.Amount <= 0.f)
	{
		return;
	}

	FSmokeStream Stream;
	Stream.Puffs = Puffs;
	Stream.Bone = Bone;
	Stream.Offset = Offset;
	Stream.Direction = Direction;
	Stream.Strength = Strength;

	if (Puffs.Duration <= 0.f)
	{
		// A burst: all at once, from where the source is now, carrying the eye's motion.
		const FTransform MeshTransform = Mesh->GetComponentTransform();
		const FVector Location = FirstPersonToWorld(Bone.IsNone() ? MeshTransform.TransformPosition(Offset) : Mesh->GetSocketLocation(Bone));
		const FVector WorldDirection = FirstPersonToWorldVector(MeshTransform.TransformVectorNoScale(Direction));
		Smoke->Emit(Puffs, FMath::RoundToInt(Puffs.Amount), Location, WorldDirection, CameraVelocity, Strength);
		return;
	}

	if (SmokeStreams.Num() >= MaxSmokeStreams)
	{
		SmokeStreams.RemoveAt(0);
	}
	SmokeStreams.Add(Stream);
}

void UGunComponent::UpdateSmoke(float DeltaTime)
{
	if (!Smoke || !Mesh || DeltaTime <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	const FTransform MeshTransform = Mesh->GetComponentTransform();
	for (int32 i = SmokeStreams.Num() - 1; i >= 0; --i)
	{
		FSmokeStream& Stream = SmokeStreams[i];
		Stream.Elapsed += DeltaTime;
		const float StreamTime = Stream.Elapsed - Stream.Puffs.Delay;
		if (StreamTime >= Stream.Puffs.Duration)
		{
			SmokeStreams.RemoveAt(i);
			continue;
		}

		// Follow the source on the (first-person) mesh, and how fast it moves: a swinging barrel trails its smoke.
		const FVector Location = FirstPersonToWorld(Stream.Bone.IsNone() ? MeshTransform.TransformPosition(Stream.Offset) : Mesh->GetSocketLocation(Stream.Bone));
		const FVector From = Stream.bHasLastLocation ? Stream.LastLocation : Location;
		const FVector Velocity = Stream.bHasLastLocation ? ((Location - From) / DeltaTime).GetClampedToMaxSize(MaxSmokeSourceSpeed) : CameraVelocity;
		Stream.LastLocation = Location;
		Stream.bHasLastLocation = true;

		if (StreamTime < 0.f)
		{
			continue;
		}

		// Thins out to nothing over the stream's Duration.
		Stream.Owed += Stream.Puffs.Amount * (1.f - StreamTime / Stream.Puffs.Duration) * DeltaTime;
		const int32 Count = FMath::FloorToInt(Stream.Owed);
		if (Count <= 0)
		{
			continue;
		}
		Stream.Owed -= Count;

		// Spread this frame's puffs along the path the source took, so a moving source leaves a trail, not clumps.
		const FVector Direction = FirstPersonToWorldVector(MeshTransform.TransformVectorNoScale(Stream.Direction));
		for (int32 k = 0; k < Count; ++k)
		{
			Smoke->Emit(Stream.Puffs, 1, FMath::Lerp(From, Location, float(k + 1) / Count), Direction, Velocity, Stream.Strength);
		}
	}
}

FVector UGunComponent::OffsetDirection(const FVector& Direction, float MaxAngle, bool bEven)
{
	if (MaxAngle <= 0.f)
	{
		return Direction;
	}
	// Even: uniform over the cone's cross-section (sqrt). Center-weighted: uniform off-angle.
	const float Angle = MaxAngle * (bEven ? FMath::Sqrt(FMath::FRand()) : FMath::FRand());
	const FVector Side = FVector::CrossProduct(Direction, FVector::UpVector).GetSafeNormal(UE_KINDA_SMALL_NUMBER, FVector::RightVector);
	const FVector Tilt = Side.RotateAngleAxis(FMath::FRandRange(0.f, 360.f), Direction);
	return Direction.RotateAngleAxis(Angle, Tilt);
}

bool UGunComponent::ConsumeRateOfFire()
{
	const double Now = GetWorld()->GetTimeSeconds();
	if (RateOfFire > 0.f && Now - LastFireCycleTime < 60.0 / RateOfFire)
	{
		return false;
	}
	LastFireCycleTime = Now;
	return true;
}

float UGunComponent::RateForRestOfCycle(float ClipTimeLeft, float Elapsed, float CurrentRate) const
{
	const float TimeLeft = CycleTime - Elapsed;
	if (CycleTime <= 0.f || ClipTimeLeft <= 0.f || TimeLeft <= KINDA_SMALL_NUMBER)
	{
		return CurrentRate;
	}
	return ClipTimeLeft / TimeLeft;
}

FRotator UGunComponent::RollAimRecoil() const
{
	const float Pitch = AimRecoilPitch * (1.f + FMath::FRandRange(-AimRecoilPitchVariance, AimRecoilPitchVariance));
	return FRotator(Pitch, FMath::FRandRange(-AimRecoilYaw, AimRecoilYaw), 0.f);
}

namespace
{
	/** How much nearer the eye a world object must be to cover the same pixels as the first-person mesh; 1 if no correction applies. */
	float FirstPersonDepthScale(const UCameraComponent* Camera, const USkeletalMeshComponent* Mesh)
	{
		if (!Camera || !Mesh || !Camera->bEnableFirstPersonFieldOfView || Mesh->FirstPersonPrimitiveType != EFirstPersonPrimitiveType::FirstPerson)
		{
			return 1.f;
		}
		const float WorldTan = FMath::Tan(FMath::DegreesToRadians(Camera->FieldOfView * 0.5f));
		const float FirstPersonTan = FMath::Tan(FMath::DegreesToRadians(Camera->FirstPersonFieldOfView * 0.5f));
		return WorldTan > KINDA_SMALL_NUMBER ? FirstPersonTan / WorldTan : 1.f;
	}
}

FVector UGunComponent::FirstPersonToWorld(const FVector& Location) const
{
	const float DepthScale = FirstPersonDepthScale(Camera, Mesh);
	if (DepthScale == 1.f)
	{
		return Location;
	}
	const FTransform View = Camera->GetComponentTransform();
	FVector ViewLocation = View.InverseTransformPositionNoScale(Location);
	ViewLocation.X *= DepthScale;
	return View.TransformPositionNoScale(ViewLocation);
}

FVector UGunComponent::FirstPersonToWorldVector(const FVector& Vector) const
{
	const float DepthScale = FirstPersonDepthScale(Camera, Mesh);
	if (DepthScale == 1.f)
	{
		return Vector;
	}
	const FTransform View = Camera->GetComponentTransform();
	FVector ViewVector = View.InverseTransformVectorNoScale(Vector);
	ViewVector.X *= DepthScale;
	return View.TransformVectorNoScale(ViewVector);
}

void UGunComponent::SampleCase(FName Bone)
{
	if (!Mesh)
	{
		return;
	}

	// This tick reads the pose evaluated last tick, so it is one previous tick after the last sample.
	PrevCaseTransform = LastCaseTransform;
	LastCaseTransform = Mesh->GetSocketTransform(Bone);
	CaseSampleDeltaTime = PrevTickDeltaTime;
	NumCaseSamples = FMath::Min(NumCaseSamples + 1, 2);
}

void UGunComponent::ReleaseCasing()
{
	if (!CasingMesh || !Mesh || NumCaseSamples == 0)
	{
		return;
	}

	// Take over from the animated case exactly where it is, carrying on with its motion. The sample is
	// the pose shown last frame; the physics step this frame moves the casing on by one frame of it.
	FVector Location = LastCaseTransform.GetLocation();
	const FQuat Rotation = LastCaseTransform.GetRotation() * CasingRotationOffset.Quaternion();
	FVector Velocity = -Mesh->GetForwardVector() * 60.f;
	FVector SpinDegrees = FVector::ZeroVector;
	if (NumCaseSamples >= 2 && CaseSampleDeltaTime > KINDA_SMALL_NUMBER)
	{
		Velocity = (LastCaseTransform.GetLocation() - PrevCaseTransform.GetLocation()) / CaseSampleDeltaTime;

		FQuat Turn = LastCaseTransform.GetRotation() * PrevCaseTransform.GetRotation().Inverse();
		Turn.EnforceShortestArcWith(FQuat::Identity);
		FVector Axis;
		double Angle = 0.0;
		Turn.ToAxisAndAngle(Axis, Angle);
		SpinDegrees = (Axis * FMath::RadiansToDegrees(Angle) / CaseSampleDeltaTime).GetClampedToMaxSize(MaxCasingSpin);
	}

	// Lands the casing on the same pixels at the same size as the animated case it replaces.
	Location = FirstPersonToWorld(Location);
	Velocity = FirstPersonToWorldVector(Velocity);

	Velocity = Velocity.GetClampedToMaxSize(MaxCasingSpeed) + FMath::VRand() * CasingScatter;
	SpinDegrees += FMath::VRand() * CasingSpin;

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = GetOwner();
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AStaticMeshActor* Casing = GetWorld()->SpawnActor<AStaticMeshActor>(Location, Rotation.Rotator(), SpawnParams);
	if (!Casing)
	{
		return;
	}

	Casing->SetMobility(EComponentMobility::Movable);
	Casing->SetLifeSpan(CasingLifeSpan);

	UStaticMeshComponent* CasingComp = Casing->GetStaticMeshComponent();
	CasingComp->SetStaticMesh(CasingMesh);
	if (CasingMaterial)
	{
		CasingComp->SetMaterial(0, CasingMaterial);
	}
	CasingComp->SetCollisionProfileName(UCollisionProfile::PhysicsActor_ProfileName);
	CasingComp->SetSimulatePhysics(true);
	CasingComp->SetPhysicsLinearVelocity(Velocity);
	CasingComp->SetPhysicsAngularVelocityInDegrees(SpinDegrees);
}
