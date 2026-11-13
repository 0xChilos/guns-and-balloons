#include "AmmoCrate.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	UStaticMeshComponent* MakeMesh(AAmmoCrate* Owner, FName Name, USceneComponent* Parent, UStaticMesh* Mesh)
	{
		UStaticMeshComponent* Comp = Owner->CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Comp->SetupAttachment(Parent);
		Comp->SetStaticMesh(Mesh);
		Comp->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
		return Comp;
	}

	void SetStateEnabled(USceneComponent* StateRoot, bool bEnabled)
	{
		StateRoot->SetVisibility(bEnabled, /*bPropagateToChildren=*/true);

		TArray<USceneComponent*> Children;
		StateRoot->GetChildrenComponents(/*bIncludeAllDescendants=*/true, Children);
		for (USceneComponent* Child : Children)
		{
			if (UPrimitiveComponent* Prim = Cast<UPrimitiveComponent>(Child))
			{
				Prim->SetCollisionEnabled(bEnabled ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
			}
		}
	}
}

AAmmoCrate::AAmmoCrate()
{
	PrimaryActorTick.bCanEverTick = false;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> BodyMesh(TEXT("/Game/Materials/AmmoCrate/Model/AmmoCrate.AmmoCrate"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> LidMesh(TEXT("/Game/Materials/AmmoCrate/Model/Upper.Upper"));

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	ClosedState = CreateDefaultSubobject<USceneComponent>(TEXT("ClosedState"));
	ClosedState->SetupAttachment(Root);
	ClosedBody = MakeMesh(this, TEXT("ClosedBody"), ClosedState, BodyMesh.Object);
	ClosedLid = MakeMesh(this, TEXT("ClosedLid"), ClosedState, LidMesh.Object);

	OpenState = CreateDefaultSubobject<USceneComponent>(TEXT("OpenState"));
	OpenState->SetupAttachment(Root);
	OpenBody = MakeMesh(this, TEXT("OpenBody"), OpenState, BodyMesh.Object);
	OpenLid = MakeMesh(this, TEXT("OpenLid"), OpenState, LidMesh.Object);
	OpenLid->SetRelativeLocation(FVector(0.f, 0.f, 25.f)); // lid lifted straight up (mesh units, before actor scale)
}

void AAmmoCrate::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyState(bStartOpen);
}

void AAmmoCrate::BeginPlay()
{
	Super::BeginPlay();
	ApplyState(bStartOpen);
}

bool AAmmoCrate::CanInteract_Implementation(APawn* InstigatorPawn) const
{
	return !bIsOpen;
}

void AAmmoCrate::Interact_Implementation(APawn* InstigatorPawn)
{
	Open(InstigatorPawn);
}

void AAmmoCrate::Open(APawn* InstigatorPawn)
{
	if (bIsOpen)
	{
		return;
	}

	ApplyState(true);
	OnOpened.Broadcast(this, InstigatorPawn);
}

void AAmmoCrate::ApplyState(bool bOpen)
{
	bIsOpen = bOpen;
	SetStateEnabled(ClosedState, !bOpen);
	SetStateEnabled(OpenState, bOpen);
}
