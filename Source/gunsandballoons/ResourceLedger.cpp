#include "ResourceLedger.h"

#include "HookPickup.h"

void UResourceLedger::Add(EResourceType Type, int32 Amount)
{
	const int32 Index = ResourceTypes::ToIndex(Type);
	if (Index == INDEX_NONE || Amount <= 0)
	{
		return;
	}
	Counts[Index] += Amount;
	OnCountChanged.Broadcast(Type, Counts[Index], Amount);
}

int32 UResourceLedger::GetCount(EResourceType Type) const
{
	const int32 Index = ResourceTypes::ToIndex(Type);
	return Index != INDEX_NONE ? Counts[Index] : 0;
}

bool UResourceLedger::CollectPickup(AHookPickup* Pickup)
{
	if (!Pickup || !Pickup->GetContents().ContainsByPredicate([](const FResourceYield& Yield) { return Yield.ResourceType != EResourceType::None; })
		|| !Pickup->MarkCollected())
	{
		return false;
	}
	for (const FResourceYield& Yield : Pickup->GetContents())
	{
		Add(Yield.ResourceType, Yield.RollAmount());
	}
	return true;
}
