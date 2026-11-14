#include "InteractSwap.h"

#include "Components/BillboardComponent.h"
#include "Components/SceneComponent.h"

AInteractSwap::AInteractSwap()
{
	PrimaryActorTick.bCanEverTick = false;

	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));

#if WITH_EDITORONLY_DATA
	if (UBillboardComponent* Icon = CreateEditorOnlyDefaultSubobject<UBillboardComponent>(TEXT("Icon")))
	{
		Icon->SetupAttachment(GetRootComponent());
	}
#endif
}

void AInteractSwap::BeginPlay()
{
	Super::BeginPlay();

	// Clicks land on the swapped actors themselves; the pawn walks the Owner chain to find us.
	// AfterActors too, so an ammo supply on this actor keeps working once the crate is open.
	for (AActor* Actor : BeforeActors)
	{
		if (Actor)
		{
			Actor->SetOwner(this);
		}
	}

	for (AActor* Actor : AfterActors)
	{
		if (Actor)
		{
			Actor->SetOwner(this);
		}
		SetActorActive(Actor, false);
	}
}

bool AInteractSwap::CanInteract_Implementation(APawn* InstigatorPawn) const
{
	return !bSwapped;
}

void AInteractSwap::Interact_Implementation(APawn* InstigatorPawn)
{
	Swap(InstigatorPawn);
}

void AInteractSwap::Swap(APawn* InstigatorPawn)
{
	if (bSwapped)
	{
		return;
	}
	bSwapped = true;

	AActor* BeforeAnchor = BeforeActors.Num() > 0 ? BeforeActors[0].Get() : nullptr;
	AActor* AfterAnchor = AfterActors.Num() > 0 ? AfterActors[0].Get() : nullptr;
	const bool bMove = bMoveAfterActorsIntoPlace && BeforeAnchor && AfterAnchor;
	const FTransform BeforeXf = BeforeAnchor ? BeforeAnchor->GetActorTransform() : FTransform::Identity;
	const FTransform AfterXf = AfterAnchor ? AfterAnchor->GetActorTransform() : FTransform::Identity;

	for (AActor* Actor : BeforeActors)
	{
		SetActorActive(Actor, false);
	}

	for (AActor* Actor : AfterActors)
	{
		if (!Actor)
		{
			continue;
		}

		if (bMove)
		{
			if (USceneComponent* Root = Actor->GetRootComponent(); Root && Root->Mobility != EComponentMobility::Movable)
			{
				Root->SetMobility(EComponentMobility::Movable);
			}
			// Keep this actor's placement relative to the After anchor, re-based onto the Before anchor.
			Actor->SetActorTransform(Actor->GetActorTransform().GetRelativeTransform(AfterXf) * BeforeXf);
		}

		SetActorActive(Actor, true);
	}

	OnSwapped.Broadcast(this, InstigatorPawn);
}

void AInteractSwap::SetActorActive(AActor* Actor, bool bActive)
{
	if (Actor)
	{
		Actor->SetActorHiddenInGame(!bActive);
		Actor->SetActorEnableCollision(bActive);
	}
}
