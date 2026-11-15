#include "PlayerInteractionComponent.h"

#include "AmmoSupplyComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Interactable.h"
#include "WeaponRackComponent.h"

UPlayerInteractionComponent::UPlayerInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UPlayerInteractionComponent::Trace(FHitResult& Hit, const FVector& Start, const FRotator& View,
	float Range, ECollisionChannel Channel, bool bDebugDraw) const
{
	if (!GetWorld()) return false;
	const FVector End = Start + View.Vector() * Range;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(FirstPersonInteract), /*bTraceComplex=*/false, GetOwner());
	const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, Start, End, Channel, Params);

	if (bDebugDraw)
	{
		DrawDebugLine(GetWorld(), Start, bHit ? Hit.ImpactPoint : End, bHit ? FColor::Green : FColor::Red, false, 1.f, 0, 1.f);
	}

	return bHit;
}

AActor* UPlayerInteractionComponent::FindAimedActor(const FHitResult& Hit) const
{
	// An interactable component on the hit actor (a weapon rack), if it takes this part of the actor.
	if (AActor* HitActor = Hit.GetActor())
	{
		TInlineComponentArray<UActorComponent*> Components(HitActor);
		for (UActorComponent* Component : Components)
		{
			if (Component->Implements<UInteractable>() && IInteractable::Execute_AcceptsHit(Component, Hit))
			{
				return HitActor;
			}
		}
	}

	// The hit actor itself, or whoever owns it (e.g. an AInteractSwap owns the meshes it swaps).
	for (AActor* Actor = Hit.GetActor(); Actor; Actor = Actor->GetOwner())
	{
		if (Actor->Implements<UInteractable>())
		{
			return Actor;
		}
	}
	if (UAmmoSupplyComponent* Supply = UAmmoSupplyComponent::FindForHit(Hit))
	{
		return Supply->GetOwner();
	}
	return nullptr;
}

UAmmoSupplyComponent* UPlayerInteractionComponent::TraceSupply(const FVector& Start, const FRotator& View,
	float Range, ECollisionChannel Channel, bool bDebugDraw) const
{
	if (!GetWorld()) return nullptr;
	const FVector End = Start + View.Vector() * Range;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(FirstPersonAmmo), /*bTraceComplex=*/false, GetOwner());
	FHitResult Hit;
	for (int32 Attempt = 0; Attempt < 4; ++Attempt)
	{
		if (!GetWorld()->LineTraceSingleByChannel(Hit, Start, End, Channel, Params)) break;
		// A weapon rack's click box: look past it.
		UPrimitiveComponent* HitComponent = Hit.GetComponent();
		if (HitComponent && Cast<UWeaponRackComponent>(HitComponent->GetAttachParent()))
		{
			Params.AddIgnoredComponent(HitComponent);
			continue;
		}
		if (bDebugDraw)
		{
			DrawDebugLine(GetWorld(), Start, Hit.ImpactPoint, FColor::Cyan, false, 1.f, 0, 1.f);
		}
		return UAmmoSupplyComponent::FindForHit(Hit);
	}
	return nullptr;
}

bool UPlayerInteractionComponent::TryInteract(const FHitResult& Hit, APawn* InstigatorPawn) const
{
	if (!IsValid(InstigatorPawn)) return false;
	// The hit actor itself, or whoever owns it (e.g. an AInteractSwap owns the meshes it swaps).
	AActor* Target = Hit.GetActor();
	while (Target && !Target->Implements<UInteractable>() && !Target->FindComponentByClass<UAmmoSupplyComponent>())
	{
		Target = Target->GetOwner();
	}

	// Ammo supplies (and whatever they sit on, e.g. the crate's lid) belong to RMB: see TryTakeRound.
	if (UAmmoSupplyComponent::FindForHit(Hit))
	{
		return false;
	}

	// Interactable components first (e.g. a weapon rack on the balloon), each checking the part that was hit.
	if (AActor* HitActor = Hit.GetActor())
	{
		TInlineComponentArray<UActorComponent*> Components(HitActor);
		for (UActorComponent* Component : Components)
		{
			if (Component->Implements<UInteractable>()
				&& IInteractable::Execute_AcceptsHit(Component, Hit)
				&& IInteractable::Execute_CanInteract(Component, InstigatorPawn))
			{
				IInteractable::Execute_Interact(Component, InstigatorPawn);
				return true;
			}
		}
	}

	if (!Target || !Target->Implements<UInteractable>()
		|| !IInteractable::Execute_AcceptsHit(Target, Hit)
		|| !IInteractable::Execute_CanInteract(Target, InstigatorPawn))
	{
		return false;
	}

	IInteractable::Execute_Interact(Target, InstigatorPawn);
	return true;
}

