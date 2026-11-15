#include "StoveVfxComponent.h"
#include "StoveComponent.h"
#include "HealthComponent.h"
#include "BalloonFlightComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"

UStoveVfxComponent::UStoveVfxComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	EffectClass = AGunSmoke::StaticClass();
	// TBD: prototype visual tuning, editable on BP_Balloon's StoveVFX component.
	FuelBurst.Amount = 12.f;
	FuelBurst.SpeedMin = 25.f; FuelBurst.SpeedMax = 65.f;
	FuelBurst.ConeAngle = 65.f;
	FuelBurst.StartRadius = .7f; FuelBurst.EndRadius = 1.5f;
	FuelBurst.LifeMin = .3f; FuelBurst.LifeMax = .7f;
	FuelBurst.Density = 6.f; FuelBurst.Buoyancy = 35.f;
	FuelBurst.Gravity = 60.f; FuelBurst.InheritVelocity = 1.f;
	OverheatSmoke.Amount = 8.f;
	OverheatSmoke.SpeedMin = 18.f; OverheatSmoke.SpeedMax = 35.f;
	OverheatSmoke.ConeAngle = 30.f;
	OverheatSmoke.StartRadius = 3.f; OverheatSmoke.EndRadius = 12.f;
	OverheatSmoke.LifeMin = .8f; OverheatSmoke.LifeMax = 1.6f;
	OverheatSmoke.Density = 4.f; OverheatSmoke.Buoyancy = 65.f;
	OverheatSmoke.InheritVelocity = 1.f;
	DamageSmoke.Amount = 3.f;
	DamageSmoke.SpeedMin = 8.f; DamageSmoke.SpeedMax = 18.f;
	DamageSmoke.ConeAngle = 20.f;
	DamageSmoke.StartRadius = 1.f; DamageSmoke.EndRadius = 6.f;
	DamageSmoke.LifeMin = .6f; DamageSmoke.LifeMax = 1.2f;
	DamageSmoke.Density = 3.f; DamageSmoke.Buoyancy = 55.f;
	DamageSmoke.InheritVelocity = 1.f;
}

void UStoveVfxComponent::BeginPlay()
{
	Super::BeginPlay();
	if (GetNetMode() == NM_DedicatedServer) return;
	Stove = UStoveComponent::FindStove(GetOwner());
	if (!Stove) return;
	Stove->OnFuelAdded.AddUniqueDynamic(this, &UStoveVfxComponent::FuelAdded);
	Stove->OnOverheatChanged.AddUniqueDynamic(this, &UStoveVfxComponent::OverheatChanged);
	Health = GetOwner()->FindComponentByClass<UHealthComponent>();
	if (Health) Health->OnHealthChanged.AddUniqueDynamic(this, &UStoveVfxComponent::HealthChanged);
	OverheatChanged(Stove->IsOverheated());
}

AGunSmoke* UStoveVfxComponent::EnsureEffect(TObjectPtr<AGunSmoke>& Effect, const FLinearColor& Color)
{
	if (!IsValid(Effect) && EffectClass && GetWorld())
	{
		FActorSpawnParameters Params;
		Params.Owner = GetOwner();
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Effect = GetWorld()->SpawnActor<AGunSmoke>(EffectClass, GetOwner()->GetActorTransform(), Params);
	}
	if (IsValid(Effect))
	{
		Effect->Color = Color;
		Effect->Wind = GetSourceVelocity();
	}
	return Effect;
}

FVector UStoveVfxComponent::GetSourceVelocity() const
{
	// Flight moves the actor directly, so AActor::GetVelocity can remain zero.
	if (auto* Flight = GetOwner()->FindComponentByClass<UBalloonFlightComponent>())
		return GetOwner()->GetActorForwardVector() * Flight->GetCurrentSpeed();
	return GetOwner()->GetVelocity();
}

void UStoveVfxComponent::FuelAdded(const UStoveFuelType* Fuel)
{
	if (!Stove || !IsValid(Stove->FireboxShape) || FuelBurst.Amount <= 0.f) return;
	if (auto* Effect = EnsureEffect(FuelEffect, FuelColor))
		Effect->Emit(FuelBurst, FMath::Clamp(FMath::RoundToInt(FuelBurst.Amount), 0, 64),
			Stove->FireboxShape->GetComponentLocation(), FVector::UpVector, GetSourceVelocity());
}

USkeletalMeshComponent* UStoveVfxComponent::GetMesh() const
{
	return GetOwner()->FindComponentByClass<USkeletalMeshComponent>();
}

FVector UStoveVfxComponent::GetOverheatLocation() const
{
	if (auto* Mesh = GetMesh())
		if (Mesh->DoesSocketExist(StoveBone))
		{
			const FVector Origin = Mesh->GetSocketTransform(StoveBone, RTS_Component).GetLocation();
			return Mesh->GetComponentTransform().TransformPosition(Origin + OverheatOffset);
		}
	return GetOwner()->GetActorTransform().TransformPosition(OverheatOffset);
}

void UStoveVfxComponent::EmitOverheat(int32 Count)
{
	if (Count <= 0) return;
	if (auto* Effect = EnsureEffect(HeatEffect, OverheatColor))
		Effect->Emit(OverheatSmoke, Count, GetOverheatLocation(), FVector::UpVector, GetSourceVelocity());
}

void UStoveVfxComponent::OverheatChanged(bool bEnabled)
{
	EmissionRemainder = 0.f;
	UpdateTickEnabled();
	if (bEnabled && OverheatSmoke.Amount > 0.f) EmitOverheat(3);
}

bool UStoveVfxComponent::IsDamaged() const
{
	return IsValid(Health) && Health->GetHealth() < Health->MaxHealth;
}

void UStoveVfxComponent::HealthChanged(UHealthComponent* InHealth, float NewHealth, float Delta)
{
	UpdateTickEnabled();
}

void UStoveVfxComponent::UpdateTickEnabled()
{
	// Damage holes are shown/hidden by BP_Balloon from the health; tick only while one could be visible.
	SetComponentTickEnabled((Stove && Stove->IsOverheated()) || IsDamaged());
}

void UStoveVfxComponent::EmitDamage(float DeltaTime)
{
	auto* Mesh = GetMesh();
	if (!Mesh || DamageSmoke.Amount <= 0.f) return;
	TArray<FName, TInlineAllocator<10>> Holes;
	for (int32 i = 1; i <= MaxDamageHoles; ++i)
	{
		const FName Bone(*FString::Printf(TEXT("%s%d"), *DamageBonePrefix, i));
		if (Mesh->GetBoneIndex(Bone) != INDEX_NONE && !Mesh->IsBoneHiddenByName(Bone)) Holes.Add(Bone);
	}
	if (Holes.IsEmpty()) { DamageRemainder = 0.f; return; }
	DamageRemainder += DamageSmoke.Amount * Holes.Num() * DeltaTime;
	const int32 Count = FMath::FloorToInt(FMath::Min(DamageRemainder, 64.f));
	DamageRemainder = FMath::Fmod(DamageRemainder, 1.f);
	if (Count <= 0) return;
	auto* Effect = EnsureEffect(DamageEffect, DamageColor);
	if (!Effect) return;
	const FVector Center = Mesh->DoesSocketExist(StoveBone) ? Mesh->GetSocketLocation(StoveBone) : GetOwner()->GetActorLocation();
	const FVector Velocity = GetSourceVelocity();
	for (int32 i = 0; i < Count; ++i)
	{
		const FVector Hole = Mesh->GetSocketLocation(Holes[DamageCursor++ % Holes.Num()]);
		// Out of the stove's side (horizontal, away from its axis) and a little up; buoyancy lifts it from there.
		const FVector Out = FVector(Hole.X - Center.X, Hole.Y - Center.Y, 0.f).GetSafeNormal();
		Effect->Emit(DamageSmoke, 1, Hole + Out, (Out + FVector(0.f, 0.f, .35f)).GetSafeNormal(), Velocity);
	}
}

void UStoveVfxComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction)
{
	Super::TickComponent(DeltaTime, TickType, TickFunction);
	if (!FMath::IsFinite(DeltaTime) || DeltaTime <= 0.f) return;
	if (IsDamaged()) EmitDamage(DeltaTime);
	if (!Stove || !Stove->IsOverheated()) return;
	EmissionRemainder += FMath::Max(0.f, OverheatSmoke.Amount) * DeltaTime;
	const int32 Count = FMath::FloorToInt(FMath::Min(EmissionRemainder, 64.f));
	EmissionRemainder = FMath::Fmod(EmissionRemainder, 1.f);
	EmitOverheat(Count);
}

void UStoveVfxComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	if (IsValid(Stove))
	{
		Stove->OnFuelAdded.RemoveDynamic(this, &UStoveVfxComponent::FuelAdded);
		Stove->OnOverheatChanged.RemoveDynamic(this, &UStoveVfxComponent::OverheatChanged);
	}
	if (IsValid(Health)) Health->OnHealthChanged.RemoveDynamic(this, &UStoveVfxComponent::HealthChanged);
	if (IsValid(FuelEffect)) FuelEffect->Destroy();
	if (IsValid(HeatEffect)) HeatEffect->Destroy();
	if (IsValid(DamageEffect)) DamageEffect->Destroy();
	Super::EndPlay(Reason);
}
