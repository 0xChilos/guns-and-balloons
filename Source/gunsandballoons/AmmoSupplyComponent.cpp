#include "AmmoSupplyComponent.h"

#include "GameFramework/Pawn.h"
#include "GunComponent.h"

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
