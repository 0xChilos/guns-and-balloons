#include "StoveVfxComponent.h"
#include "StoveComponent.h"
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
}

void UStoveVfxComponent::BeginPlay()
{
	Super::BeginPlay();
	if (GetNetMode() == NM_DedicatedServer) return;
	Stove = UStoveComponent::FindStove(GetOwner());
	if (!Stove) return;
	Stove->OnFuelAdded.AddUniqueDynamic(this, &UStoveVfxComponent::FuelAdded);
	Stove->OnOverheatChanged.AddUniqueDynamic(this, &UStoveVfxComponent::OverheatChanged);
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

FVector UStoveVfxComponent::GetOverheatLocation() const
{
	if (auto* Mesh = GetOwner()->FindComponentByClass<USkeletalMeshComponent>())
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
	SetComponentTickEnabled(bEnabled);
	if (bEnabled && OverheatSmoke.Amount > 0.f) EmitOverheat(3);
}

void UStoveVfxComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction)
{
	Super::TickComponent(DeltaTime, TickType, TickFunction);
	if (!Stove || !Stove->IsOverheated() || !FMath::IsFinite(DeltaTime) || DeltaTime <= 0.f) return;
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
	if (IsValid(FuelEffect)) FuelEffect->Destroy();
	if (IsValid(HeatEffect)) HeatEffect->Destroy();
	Super::EndPlay(Reason);
}
