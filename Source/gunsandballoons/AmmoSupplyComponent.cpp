#include "AmmoSupplyComponent.h"

#include "GameFramework/Pawn.h"
#include "GunComponent.h"

bool UAmmoSupplyComponent::AcceptsHit(const FHitResult& Hit) const
{
	return HitBones.IsEmpty() || (Hit.GetActor() == GetOwner() && HitBones.Contains(Hit.BoneName));
}

UAmmoSupplyComponent* UAmmoSupplyComponent::FindForHit(const FHitResult& Hit)
{
	// The hit actor itself, or whoever owns it (e.g. an AInteractSwap owns the meshes it swaps).
	for (AActor* Actor = Hit.GetActor(); Actor; Actor = Actor->GetOwner())
	{
		UAmmoSupplyComponent* Supply = Actor->FindComponentByClass<UAmmoSupplyComponent>();
		if (Supply && Supply->AcceptsHit(Hit)) return Supply;
	}
	return nullptr;
}

bool UAmmoSupplyComponent::GiveRound(APawn* InstigatorPawn)
{
	if (!HasRounds() || !InstigatorPawn)
	{
		return false;
	}

	// The gun in the player's hand takes it (a holstered gun keeps what it has).
	UGunComponent* Gun = nullptr;
	TInlineComponentArray<UGunComponent*> Guns(InstigatorPawn);
	for (UGunComponent* Candidate : Guns)
	{
		if (Candidate->IsEquipped())
		{
			Gun = Candidate;
			break;
		}
	}

	if (!Gun || !Gun->LoadRound())
	{
		OnRoundRefused.Broadcast(this, InstigatorPawn);
		return false;
	}

	if (Rounds > 0)
	{
		--Rounds;
	}
	OnRoundTaken.Broadcast(this, InstigatorPawn);
	return true;
}
