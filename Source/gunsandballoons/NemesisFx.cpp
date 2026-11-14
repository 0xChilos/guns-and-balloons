#include "NemesisFx.h"

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
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AGunSmoke* S = World->SpawnActor<AGunSmoke>(AGunSmoke::StaticClass(), FTransform::Identity, Params);
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
		case EKind::BlastFire:   S->Color = FLinearColor(1.f, 0.62f, 0.15f); S->MaxOpacity = 1.f; break;
		case EKind::BlastSmoke:  S->Color = FLinearColor(0.035f, 0.032f, 0.03f); S->MaxOpacity = 0.9f; break;
		}
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
