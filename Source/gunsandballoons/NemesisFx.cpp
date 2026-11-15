#include "NemesisFx.h"

#include "Engine/PointLight.h"
#include "Components/PointLightComponent.h"
#include "Engine/World.h"

namespace NemesisFx
{
	FGunSmokePuffs Puffs(EKind Kind)
	{
		FGunSmokePuffs P;
		switch (Kind)
		{
		case EKind::EngineSmoke: // thick black smoke pouring out of a dead engine
			P.SpeedMin = 40.f; P.SpeedMax = 140.f; P.ConeAngle = 30.f;
			P.StartRadius = 65.f; P.EndRadius = 260.f; P.GrowTime = 1.2f;
			P.Density = 5.f; P.LifeMin = 2.5f; P.LifeMax = 4.f;
			P.Buoyancy = 60.f; P.CoolTime = 2.f; P.InheritVelocity = 0.15f;
			break;
		case EKind::Fire: // short-lived bright licks of flame
			P.SpeedMin = 120.f; P.SpeedMax = 300.f; P.ConeAngle = 35.f;
			P.StartRadius = 55.f; P.EndRadius = 85.f; P.GrowTime = 0.25f;
			P.Density = 9.f; P.LifeMin = 0.25f; P.LifeMax = 0.5f;
			P.Buoyancy = 400.f; P.CoolTime = 0.5f; P.InheritVelocity = 0.6f;
			break;
		case EKind::RocketTrail: // pale propellant smoke left hanging behind the rocket
			P.SpeedMin = 20.f; P.SpeedMax = 80.f; P.ConeAngle = 12.f;
			P.StartRadius = 30.f; P.EndRadius = 150.f; P.GrowTime = 1.6f;
			P.Density = 3.f; P.LifeMin = 3.f; P.LifeMax = 5.f;
			P.Buoyancy = 12.f; P.CoolTime = 2.f; P.InheritVelocity = 0.f;
			break;
		case EKind::MissileTrail:
			// AA missile: many small puffs that overlap into one plume and widen as it ages,
			// instead of a row of separate white balls.
			P.SpeedMin = 30.f; P.SpeedMax = 140.f; P.ConeAngle = 25.f;
			P.StartRadius = 40.f; P.EndRadius = 190.f; P.GrowTime = 0.9f;
			P.Density = 4.f; P.LifeMin = 2.f; P.LifeMax = 3.5f;
			P.Buoyancy = 8.f; P.CoolTime = 1.5f; P.InheritVelocity = 0.f;
			break;
		case EKind::FineBlastFire: // fireball: many small, short-lived licks thrown out fast
			P.SpeedMin = 200.f; P.SpeedMax = 750.f; P.ConeAngle = 90.f;
			P.StartRadius = 30.f; P.EndRadius = 130.f; P.GrowTime = 0.2f;
			P.Density = 12.f; P.LifeMin = 0.25f; P.LifeMax = 0.7f;
			P.Buoyancy = 250.f; P.CoolTime = 0.3f; P.InheritVelocity = 0.f;
			break;
		case EKind::FineBlastSmoke: // the dark cloud the fireball leaves, spreading and rising slowly
			P.SpeedMin = 60.f; P.SpeedMax = 380.f; P.ConeAngle = 90.f;
			P.StartRadius = 35.f; P.EndRadius = 230.f; P.GrowTime = 1.2f;
			P.Density = 6.f; P.LifeMin = 2.f; P.LifeMax = 4.f;
			P.Buoyancy = 45.f; P.CoolTime = 2.f; P.InheritVelocity = 0.f;
			break;
		case EKind::BlastFire:
			P.SpeedMin = 300.f; P.SpeedMax = 900.f; P.ConeAngle = 90.f;
			P.StartRadius = 80.f; P.EndRadius = 220.f; P.GrowTime = 0.2f;
			P.Density = 10.f; P.LifeMin = 0.3f; P.LifeMax = 0.6f;
			P.Buoyancy = 200.f; P.CoolTime = 0.4f; P.InheritVelocity = 0.f;
			break;
		case EKind::BlastSmoke:
			P.SpeedMin = 100.f; P.SpeedMax = 400.f; P.ConeAngle = 90.f;
			P.StartRadius = 90.f; P.EndRadius = 320.f; P.GrowTime = 1.f;
			P.Density = 4.f; P.LifeMin = 2.f; P.LifeMax = 3.5f;
			P.Buoyancy = 50.f; P.CoolTime = 1.5f; P.InheritVelocity = 0.f;
			break;
		}
		return P;
	}

	AGunSmoke* Spawn(UWorld* World, EKind Kind)
	{
		if (!World) return nullptr;
		// Deferred: the puff textures (TextureSize, ShapeVariants, RimShade) are built in BeginPlay,
		// so the settings below must be in place before it runs.
		AGunSmoke* S = World->SpawnActorDeferred<AGunSmoke>(AGunSmoke::StaticClass(), FTransform::Identity, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!S) return nullptr;
		// Big puffs high in the air: larger eddies, no collision traces (cheap).
		S->bCollide = false;
		S->TurbulenceScale = 300.f;
		S->Turbulence = 80.f;
		S->LinearDrag = 0.8f;
		S->MaxPuffs = 128;
		S->NearFadeDistance = 60.f;
		switch (Kind)
		{
		// The puff material shows dark tints much lighter than their value, so "black" has to be near zero.
		case EKind::EngineSmoke: S->Color = FLinearColor(0.012f, 0.011f, 0.01f); S->MaxOpacity = 0.95f; break;
		case EKind::Fire:        S->Color = FLinearColor(1.f, 0.52f, 0.1f); S->MaxOpacity = 1.f; break;
		case EKind::RocketTrail: S->Color = FLinearColor(0.82f, 0.8f, 0.76f); S->MaxOpacity = 0.75f; break;
		case EKind::MissileTrail:
			// Grey and dense, almost flat shading (no cartoon rim), fine-grained blobs, many of them,
			// stronger and larger swirls that break the line up.
			S->Color = FLinearColor(0.42f, 0.41f, 0.39f);
			S->MaxOpacity = 0.7f;
			S->RimShade = 0.9f;
			S->TextureSize = 64; // fine grain: thin smoke dithers into dense noise, not a sparse dot grid
			S->ShapeVariants = 8;
			S->MaxPuffs = 400;
			S->Turbulence = 140.f;
			S->TurbulenceScale = 220.f;
			S->TurbulenceSpeed = 0.6f;
			break;
		case EKind::BlastFire:   S->Color = FLinearColor(1.f, 0.62f, 0.15f); S->MaxOpacity = 1.f; break;
		case EKind::FineBlastFire:
			S->Color = FLinearColor(1.f, 0.5f, 0.12f); S->MaxOpacity = 1.f;
			S->RimShade = 0.75f; S->TextureSize = 64; S->ShapeVariants = 8; S->MaxPuffs = 200;
			break;
		case EKind::FineBlastSmoke:
			S->Color = FLinearColor(0.016f, 0.015f, 0.014f); S->MaxOpacity = 0.9f; // reads as dark grey
			S->RimShade = 0.9f; S->TextureSize = 64; S->ShapeVariants = 8; S->MaxPuffs = 200;
			S->Turbulence = 160.f; S->TurbulenceScale = 180.f; S->TurbulenceSpeed = 0.7f;
			break;
		case EKind::BlastSmoke:  S->Color = FLinearColor(0.035f, 0.032f, 0.03f); S->MaxOpacity = 0.9f; break;
		}
		S->FinishSpawning(FTransform::Identity);
		return S;
	}

	void Explosion(UWorld* World, const FVector& Location, float Scale)
	{
		for (const EKind Kind : { EKind::BlastSmoke, EKind::BlastFire })
		{
			if (AGunSmoke* S = Spawn(World, Kind))
			{
				FGunSmokePuffs P = Puffs(Kind);
				P.StartRadius *= Scale; P.EndRadius *= Scale; P.SpeedMin *= Scale; P.SpeedMax *= Scale;
				S->Emit(P, FMath::RoundToInt((Kind == EKind::BlastFire ? 14 : 10) * Scale), Location, FVector::UpVector, FVector::ZeroVector);
				S->SetLifeSpan(P.LifeMax + 0.5f);
			}
		}
	}

	void FineExplosion(UWorld* World, const FVector& Location, float Scale)
	{
		if (!World) return;
		// A short flash lights the surroundings for a moment.
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (APointLight* Flash = World->SpawnActor<APointLight>(Location, FRotator::ZeroRotator, Params))
		{
			Flash->PointLightComponent->SetMobility(EComponentMobility::Movable);
			Flash->PointLightComponent->SetLightColor(FLinearColor(1.f, 0.55f, 0.2f));
			Flash->PointLightComponent->SetIntensity(3000000.f * Scale);
			Flash->PointLightComponent->SetAttenuationRadius(3000.f * Scale);
			Flash->PointLightComponent->SetCastShadows(false);
			Flash->SetLifeSpan(0.12f);
		}
		// Smoke first, so the fireball draws over it while it lasts.
		for (const EKind Kind : { EKind::FineBlastSmoke, EKind::FineBlastFire })
		{
			if (AGunSmoke* S = Spawn(World, Kind))
			{
				FGunSmokePuffs P = Puffs(Kind);
				P.StartRadius *= Scale; P.EndRadius *= Scale; P.SpeedMin *= Scale; P.SpeedMax *= Scale;
				S->Emit(P, FMath::RoundToInt((Kind == EKind::FineBlastFire ? 60 : 45) * Scale), Location, FVector::UpVector, FVector::ZeroVector);
				S->SetLifeSpan(P.LifeMax + 0.5f);
			}
		}
	}

	void FStream::Start(UWorld* World, EKind Kind, float InRate)
	{
		if (Smoke.IsValid()) return;
		Smoke = Spawn(World, Kind);
		Settings = Puffs(Kind);
		Rate = InRate;
		Carry = 0.f;
		bHasLast = false;
	}

	void FStream::Tick(float DeltaSeconds, const FVector& Location, const FVector& Direction, const FVector& SourceVelocity)
	{
		AGunSmoke* S = Smoke.Get();
		if (!S) return;
		const FVector From = bHasLast ? LastLocation : Location;
		LastLocation = Location;
		bHasLast = true;
		Carry += Rate * DeltaSeconds;
		const int32 Count = FMath::FloorToInt(Carry);
		if (Count <= 0) return;
		Carry -= Count;
		for (int32 i = 1; i <= Count; ++i)
		{
			S->Emit(Settings, 1, FMath::Lerp(From, Location, float(i) / Count), Direction, SourceVelocity);
		}
	}

	void FStream::Stop()
	{
		if (AGunSmoke* S = Smoke.Get()) S->SetLifeSpan(Settings.LifeMax + 0.5f);
		Smoke.Reset();
	}
}
