#include "FuelSupplyComponent.h"
#include "FuelHandComponent.h"
#include "FirstPersonCameraPawn.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Actor.h"
UFuelSupplyComponent::UFuelSupplyComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	BucketMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BucketMesh"));
	BucketMesh->SetupAttachment(this);
	BucketMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	InteractShape = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractShape"));
	InteractShape->SetupAttachment(this);
	InteractShape->SetBoxExtent(FVector(20.f));
	InteractShape->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractShape->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractShape->SetCollisionResponseToChannel(InteractTraceChannel, ECR_Block);
	InteractShape->SetGenerateOverlapEvents(false);
}
void UFuelSupplyComponent::OnRegister()
{
	Super::OnRegister();
	InteractShape->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractShape->SetCollisionResponseToChannel(InteractTraceChannel, ECR_Block);
	if (!BucketMesh->IsRegistered()) BucketMesh->RegisterComponent();
	if (!InteractShape->IsRegistered()) InteractShape->RegisterComponent();
}
void UFuelSupplyComponent::BeginPlay()
{
	Super::BeginPlay();
	SetCount(StartCount);
}
void UFuelSupplyComponent::SetCount(int32 Value)
{
	const int32 NewCount = FMath::Clamp(Value, 0, GetCapacity());
	if (NewCount == Count) return;
	Count = NewCount;
	OnContentsChanged.Broadcast(this, Count);
}
int32 UFuelSupplyComponent::AddFuel(int32 Amount)
{
	const int32 Added = FMath::Clamp(Amount, 0, FMath::Max(0, GetCapacity() - Count));
	SetCount(Count + Added);
	return Added;
}
UFuelSupplyComponent* UFuelSupplyComponent::FindSupplyFor(AActor* Owner, const UStoveFuelType* Type)
{
	if (!IsValid(Owner) || !Type) return nullptr;
	TInlineComponentArray<UFuelSupplyComponent*> Supplies(Owner);
	for (auto* Supply : Supplies) if (Supply->Fuel == Type) return Supply;
	return nullptr;
}
bool UFuelSupplyComponent::AcceptsHit_Implementation(const FHitResult& Hit) const
{
	return InteractShape && Hit.GetComponent() == InteractShape;
}
bool UFuelSupplyComponent::CanInteract_Implementation(APawn* InstigatorPawn) const
{
	const auto* Pawn = Cast<AFirstPersonCameraPawn>(InstigatorPawn);
	return Pawn && Fuel && ((Pawn->IsHandEmpty() && Count > 0)
		|| (!Pawn->IsHandEmpty() && Pawn->GetActiveGun() == Pawn->GetFuelHand() && Pawn->GetFuelHand()->Fuel));
}
void UFuelSupplyComponent::Interact_Implementation(APawn* InstigatorPawn)
{
	if (!CanInteract_Implementation(InstigatorPawn)) return;
	auto* Pawn = CastChecked<AFirstPersonCameraPawn>(InstigatorPawn);
	auto* Hand = Pawn->GetFuelHand();
	if (!Pawn->IsHandEmpty())
	{
		if (Hand->Fuel == Fuel)
		{
			if (Count >= GetCapacity()) return;
			Pawn->ConsumeHeldItem();
			AddFuel(1);
			return;
		}
		auto* Home = FindSupplyFor(GetOwner(), Hand->Fuel);
		if (Count <= 0 || !Home || Home->GetCount() >= Home->GetCapacity()) return;
		Pawn->ConsumeHeldItem();
		Home->AddFuel(1);
	}
	Hand->SetFuel(Fuel);
	if (Pawn->TakeHandItem(Hand)) SetCount(Count - 1);
	else Hand->SetFuel(nullptr);
}
