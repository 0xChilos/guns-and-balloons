#include "HandItemPickup.h"

#include "WeaponRackComponent.h"

AHandItemPickup::AHandItemPickup()
{
	PrimaryActorTick.bCanEverTick = false;

	Rack = CreateDefaultSubobject<UWeaponRackComponent>(TEXT("Rack"));
	SetRootComponent(Rack);
	// The item lies at the actor's own transform; the click box follows whichever item is picked.
	Rack->bFitInteractShape = true;
}
