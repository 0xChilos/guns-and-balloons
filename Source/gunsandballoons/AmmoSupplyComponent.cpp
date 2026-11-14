#include "AmmoSupplyComponent.h"

#include "GameFramework/Pawn.h"
#include "RevolverComponent.h"

bool UAmmoSupplyComponent::GiveRound(APawn* InstigatorPawn)
{
	if (!HasRounds() || !InstigatorPawn)
	{
		return false;
	}

	URevolverComponent* Revolver = InstigatorPawn->FindComponentByClass<URevolverComponent>();
	if (!Revolver || !Revolver->LoadRound())
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
