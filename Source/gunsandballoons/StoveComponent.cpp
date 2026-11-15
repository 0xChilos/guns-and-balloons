#include "StoveComponent.h"
#include "StoveFuelType.h"
#include "HealthComponent.h"
#include "BalloonFlightComponent.h"
#include "GameFramework/Actor.h"
#include "Components/PrimitiveComponent.h"
#include "FirstPersonCameraPawn.h"
#include "FuelHandComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"

namespace { const FName StoveModifier(TEXT("Stove")); }

UStoveComponent* UStoveComponent::FindStove(AActor* Actor)
{
	return IsValid(Actor) ? Actor->FindComponentByClass<UStoveComponent>() : nullptr;
}

FRotator UStoveComponent::GetGaugeRotation() const
{
	if (const AActor* Owner = GetOwner())
	{
		TInlineComponentArray<USkeletalMeshComponent*> Meshes(Owner);
		for (const auto* Mesh : Meshes)
			if (const auto* Asset = Mesh->GetSkeletalMeshAsset())
			{
				const auto& Ref = Asset->GetRefSkeleton();
				const int32 Index = Ref.FindBoneIndex(TEXT("gauge_needle"));
				if (Index != INDEX_NONE)
				{
					// The needle origin is offset from its gauge parent along the dial normal.
					const FVector Axis = Ref.GetRefBonePose()[Index].GetTranslation().GetSafeNormal();
					return FQuat(Axis, FMath::DegreesToRadians(GetGaugeAngle())).Rotator();
				}
			}
	}
	return FRotator::ZeroRotator;
}

UStoveComponent::UStoveComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void UStoveComponent::BeginPlay()
{
	Super::BeginPlay();
	// Blueprint component references may still point at an SCS template after instancing.
	// Resolve the actual mouth on this actor before configuring collision or accepting hits.
	if ((!IsValid(FireboxShape) || FireboxShape->GetOwner() != GetOwner()) && GetOwner())
	{
		FireboxShape = nullptr;
		TInlineComponentArray<UPrimitiveComponent*> Shapes(GetOwner());
		for (auto* Shape : Shapes)
			if (Shape->GetFName() == FireboxComponentName) { FireboxShape = Shape; break; }
	}
	if (IsValid(FireboxShape))
	{
		FireboxShape->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		FireboxShape->SetCollisionResponseToAllChannels(ECR_Ignore);
		FireboxShape->SetCollisionResponseToChannel(InteractTraceChannel, ECR_Block);
		FireboxShape->SetGenerateOverlapEvents(false);
	}
	if (AActor* Owner = GetOwner())
	{
		if (!Health) Health = Owner->FindComponentByClass<UHealthComponent>();
		if (!Flight) Flight = Owner->FindComponentByClass<UBalloonFlightComponent>();
	}
	if (Flight) Flight->AddTickPrerequisiteComponent(this);
	UpdateOutputs(0.f);
}

bool UStoveComponent::AcceptsHit_Implementation(const FHitResult& Hit) const
{
	LastAcceptedPart = EHitPart::None;
	if (IsValid(FireboxShape) && Hit.GetComponent() == FireboxShape) LastAcceptedPart = EHitPart::Firebox;
	else if (!DoorBone.IsNone() && Hit.BoneName == DoorBone) LastAcceptedPart = EHitPart::Door;
	return LastAcceptedPart != EHitPart::None;
}

void UStoveComponent::Interact_Implementation(APawn* InstigatorPawn)
{
	// TryInteract filters immediately before dispatch. Consume the cache to avoid stale repeats.
	const EHitPart Part = LastAcceptedPart;
	LastAcceptedPart = EHitPart::None;
	if (Part == EHitPart::Door) { ToggleDoor(); return; }
	if (Part != EHitPart::Firebox) return;
	if (!IsDoorOpen()) return;
	auto* Player = Cast<AFirstPersonCameraPawn>(InstigatorPawn);
	auto* Hand = Player ? Cast<UFuelHandComponent>(Player->GetActiveGun()) : nullptr;
	if (Hand && !Player->IsHandEmpty() && Hand->Fuel && AddFuel(Hand->Fuel))
		Player->ConsumeHeldItem();
}

void UStoveComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	if (IsValid(Flight))
	{
		Flight->ClearFlightModifier(StoveModifier);
		Flight->RemoveTickPrerequisiteComponent(this);
	}
	bPublishedModifier = false;
	Super::EndPlay(Reason);
}

bool UStoveComponent::CanAddFuel(const UStoveFuelType* Fuel) const
{
	return IsValid(Fuel) && bDoorOpen && FMath::IsFinite(Fuel->BurnTime) && Fuel->BurnTime >= 0.f
		&& FMath::IsFinite(Fuel->HeatAdd) && Fuel->HeatAdd >= 0.f
		&& FMath::IsFinite(Fuel->HeatRateOverride) && Fuel->HeatRateOverride >= 0.f
		&& (bClampBurnTimeOnOverfill || BurnTimeLeft + Fuel->BurnTime <= MaxBurnTime);
}

bool UStoveComponent::AddFuel(const UStoveFuelType* Fuel)
{
	if (!CanAddFuel(Fuel)) return false;
	const bool bWasCold = Temperature <= 0.f;
	const bool bWasHeating = Temperature < TargetTemperature;
	// Keep overrides distinct so the stove's default rate remains adjustable at runtime.
	bHeatingUsesDefaultRate = (bWasHeating && bHeatingUsesDefaultRate) || Fuel->HeatRateOverride == 0.f;
	ActiveHeatRateOverride = bWasHeating ? FMath::Max(ActiveHeatRateOverride, Fuel->HeatRateOverride) : Fuel->HeatRateOverride;
	BurnTimeLeft = FMath::Min(BurnTimeLeft + Fuel->BurnTime, MaxBurnTime);
	TargetTemperature = FMath::Min(FMath::Max(Temperature, TargetTemperature) + Fuel->HeatAdd, MaxTemperature);
	DecayPauseLeft = DecayPause;
	if (Fuel->bInstantHeat)
	{
		Temperature = TargetTemperature;
		bIgniting = false;
		IgnitionDelayLeft = 0.f;
		if (bWasCold && Temperature > 0.f) OnIgnited.Broadcast();
	}
	else if (bWasCold && !bIgniting)
	{
		bIgniting = true;
		IgnitionDelayLeft = IgnitionDelay;
	}
	UpdateOutputs(0.f);
	OnFuelAdded.Broadcast(Fuel);
	return true;
}

void UStoveComponent::SetDoorOpen(bool bOpen)
{
	if (bDoorOpen == bOpen) return;
	bDoorOpen = bOpen;
	OnDoorChanged.Broadcast(bOpen);
}

bool UStoveComponent::AcceptsCoolingHit(const FHitResult& Hit) const
{
	return (IsValid(FireboxShape) && Hit.GetComponent() == FireboxShape)
		|| (!Hit.BoneName.IsNone() && CoolingBones.Contains(Hit.BoneName));
}

float UStoveComponent::ApplyRepair(float Amount, AActor* Repairer)
{
	return IsValid(Health) ? Health->Repair(Amount, Repairer) : 0.f;
}

float UStoveComponent::ApplyCooling(float Degrees)
{
	if (!FMath::IsFinite(Degrees) || Degrees <= 0.f) return 0.f;
	const float PreviousTemperature = Temperature;
	Temperature = FMath::Max(0.f, Temperature - Degrees);
	TargetTemperature = FMath::Max(0.f, TargetTemperature - Degrees);
	if (PreviousTemperature > 0.f && Temperature <= 0.f)
	{
		TargetTemperature = 0.f;
		bIgniting = false;
		IgnitionDelayLeft = 0.f;
		OnExtinguished.Broadcast();
	}
	// Refresh overheat, VFX listeners and flight immediately, without another damage tick.
	UpdateOutputs(0.f);
	return PreviousTemperature - Temperature;
}

float UStoveComponent::GetTemperature01() const
{
	return GaugeMaxTemperature > 0.f ? FMath::Clamp(Temperature / GaugeMaxTemperature, 0.f, 1.f) : 0.f;
}

float UStoveComponent::GetBurnTime01() const
{
	return MaxBurnTime > 0.f ? FMath::Clamp(BurnTimeLeft / MaxBurnTime, 0.f, 1.f) : 0.f;
}

float UStoveComponent::GetSpeedMultiplier() const
{
	if (Temperature > OverheatThreshold) return SpeedMultOverheat;
	return FMath::Lerp(1.f, SpeedMultAt90, OverheatThreshold > 0.f ? FMath::Clamp(Temperature / OverheatThreshold, 0.f, 1.f) : 0.f);
}

void UStoveComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!FMath::IsFinite(DeltaTime) || DeltaTime <= 0.f) return;
	DoorAngle = FMath::FInterpConstantTo(DoorAngle, bDoorOpen ? DoorOpenAngle : 0.f, DeltaTime, DoorSpeed);
	float SimulationTime = DeltaTime;
	bool bJustIgnited = false;
	if (bIgniting)
	{
		// Only the part of the frame after ignition can burn fuel or heat the stove.
		SimulationTime = FMath::Max(0.f, DeltaTime - IgnitionDelayLeft);
		IgnitionDelayLeft = FMath::Max(0.f, IgnitionDelayLeft - DeltaTime);
		if (IgnitionDelayLeft > 0.f) { UpdateOutputs(DeltaTime); return; }
		bIgniting = false;
		bJustIgnited = true;
		OnIgnited.Broadcast();
	}
	const float PreviousTemperature = Temperature;
	while (SimulationTime > 0.f)
	{
		// Each interval uses the state at its start, ending at the next rate change.
		float Step = SimulationTime;
		const bool bBurning = Temperature > 0.f || bJustIgnited;
		if (bBurning && BurnTimeLeft > 0.f) Step = FMath::Min(Step, BurnTimeLeft);
		if (DecayPauseLeft > 0.f) Step = FMath::Min(Step, DecayPauseLeft);
		if (Temperature < TargetTemperature)
		{
			const float Rate = FMath::Max(0.f, bHeatingUsesDefaultRate ? FMath::Max(HeatRate, ActiveHeatRateOverride) : ActiveHeatRateOverride);
			if (Rate > 0.f) Step = FMath::Min(Step, (TargetTemperature - Temperature) / Rate);
			Temperature = FMath::FInterpConstantTo(Temperature, TargetTemperature, Step, Rate);
		}
		else
		{
			float Rate = BurnTimeLeft > 0.f ? DecayBurning : DecayEmpty;
			if (DecayPauseLeft > 0.f)
				Rate = bDoorOpen ? DecayBurning * FMath::Max(0.f, DoorOpenDecayMultiplier - 1.f) : 0.f;
			else if (bDoorOpen) Rate *= DoorOpenDecayMultiplier;
			if (Rate > 0.f && Temperature > 0.f) Step = FMath::Min(Step, Temperature / Rate);
			Temperature = FMath::Max(0.f, Temperature - Rate * Step);
			TargetTemperature = FMath::Min(TargetTemperature, Temperature);
		}
		if (bBurning) BurnTimeLeft = FMath::Max(0.f, BurnTimeLeft - Step);
		DecayPauseLeft = FMath::Max(0.f, DecayPauseLeft - Step);
		SimulationTime = FMath::Max(0.f, SimulationTime - Step);
		bJustIgnited = false;
	}
	Temperature = FMath::Clamp(Temperature, 0.f, FMath::Max(0.f, MaxTemperature));
	if (PreviousTemperature > 0.f && Temperature <= 0.f)
	{
		TargetTemperature = 0.f;
		OnExtinguished.Broadcast();
	}
	UpdateOutputs(DeltaTime);
}

void UStoveComponent::UpdateOutputs(float DeltaTime)
{
	const bool bNow = Temperature > OverheatThreshold;
	if (bNow != bOverheated) { bOverheated = bNow; OnOverheatChanged.Broadcast(bNow); }
	if (bOverheated && IsValid(Health)) Health->ApplyDamage(OverheatDamagePerSecond * DeltaTime, GetOwner());
	const float Mult = GetSpeedMultiplier();
	if (IsValid(Flight) && (!bPublishedModifier || FMath::Abs(Mult - LastPublishedMult) > FlightModifierEpsilon))
	{
		Flight->SetFlightModifier(StoveModifier, 0.f, Mult);
		LastPublishedMult = Mult;
		bPublishedModifier = true;
	}
}
