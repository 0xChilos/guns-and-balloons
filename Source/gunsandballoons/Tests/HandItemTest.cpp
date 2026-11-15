#include "../FirstPersonCameraPawn.h"
#include "../HandItemPickup.h"
#include "../HookComponent.h"
#include "../FuelHandComponent.h"
#include "../FuelSupplyComponent.h"
#include "../StoveComponent.h"
#include "../StoveFuelType.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "../RevolverComponent.h"
#include "../RifleComponent.h"
#include "../WeaponRackComponent.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Components/SkeletalMeshComponent.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHandItemTest, "GunsAndBalloons.Player.HandItems",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHandItemTest::RunTest(const FString& Parameters)
{
	UWorld* World = GWorld;
	if (!TestNotNull(TEXT("Editor world available"), World)) return false;
	AFirstPersonCameraPawn* Pawn = World->SpawnActor<AFirstPersonCameraPawn>();
	AHandItemPickup* RifleSpot = World->SpawnActor<AHandItemPickup>();
	AHandItemPickup* HookSpot = World->SpawnActor<AHandItemPickup>();
	if (!Pawn || !RifleSpot || !HookSpot) return false;
	ON_SCOPE_EXIT { World->DestroyActor(HookSpot); World->DestroyActor(RifleSpot); World->DestroyActor(Pawn); };

	// BeginPlay does not run in the editor world: set up what it would (an empty hand, the items on their spots).
	Pawn->ActiveGun = Pawn->GetRevolver();
	Pawn->HandState = EPlayerHandState::Empty;
	Pawn->HandLowerAlpha = 1.f;
	UWeaponRackComponent* RifleRack = RifleSpot->GetRack();
	UWeaponRackComponent* HookRack = HookSpot->GetRack();
	RifleRack->ItemClass = URifleComponent::StaticClass();
	HookRack->ItemClass = UHookComponent::StaticClass();
	RifleRack->ReregisterComponent();
	HookRack->ReregisterComponent();
	RifleRack->SetHasWeapon(true);
	HookRack->SetHasWeapon(true);
	for (UGunComponent* Gun : Pawn->GetGuns())
	{
		Gun->SetEquipped(false);
	}

	auto Advance = [Pawn](float Seconds)
	{
		for (float T = 0.f; T < Seconds; T += 1.f / 60.f)
		{
			Pawn->UpdateHandState(1.f / 60.f);
			Pawn->UpdateWeaponSwap(1.f / 60.f);
		}
	};

	TestEqual(TEXT("A spot shows its item's own mesh"), RifleRack->DisplayMesh->GetSkeletalMeshAsset(), Pawn->GetRifleMesh()->GetSkeletalMeshAsset());
	TestEqual(TEXT("The other spot shows the other item"), HookRack->DisplayMesh->GetSkeletalMeshAsset(), Pawn->GetHookMesh()->GetSkeletalMeshAsset());

	// Nothing to put down or swap with an empty hand.
	TestFalse(TEXT("Empty hand holsters nothing"), Pawn->HolsterWeapon(RifleRack));
	TestFalse(TEXT("Empty hand swaps nothing"), Pawn->SwapWeapon(RifleRack));

	// Take the rifle.
	TestTrue(TEXT("Empty hand takes the rifle"), Pawn->DrawWeapon(RifleRack));
	TestEqual(TEXT("The rifle is the item in hand"), Pawn->GetActiveGun(), static_cast<UGunComponent*>(Pawn->GetRifle()));
	TestTrue(TEXT("The rifle is shown in hand"), Pawn->GetRifle()->IsEquipped());
	TestFalse(TEXT("The rifle left its spot"), RifleRack->HasWeapon());
	TestEqual(TEXT("Being raised"), Pawn->GetHandState(), EPlayerHandState::Drawing);
	TestFalse(TEXT("No second item while raising"), Pawn->DrawWeapon(HookRack));
	Advance(1.f);
	TestEqual(TEXT("Armed once raised"), Pawn->GetHandState(), EPlayerHandState::Armed);

	// A spot takes its own kind of item only.
	TestFalse(TEXT("The hook's spot does not take the rifle"), Pawn->HolsterWeapon(HookRack));

	// Swap for the hook: the rifle goes back where it came from.
	TestTrue(TEXT("Swap for the hook"), Pawn->SwapWeapon(HookRack));
	TestFalse(TEXT("No second swap meanwhile"), Pawn->SwapWeapon(HookRack));
	Advance(1.f);
	TestEqual(TEXT("The hook is the item in hand"), Pawn->GetActiveGun(), static_cast<UGunComponent*>(Pawn->GetHook()));
	TestTrue(TEXT("The hook is shown in hand"), Pawn->GetHook()->IsEquipped());
	TestFalse(TEXT("The rifle is hidden"), Pawn->GetRifle()->IsEquipped());
	TestTrue(TEXT("The rifle is back on its spot"), RifleRack->HasWeapon());
	TestFalse(TEXT("The hook left its spot"), HookRack->HasWeapon());
	TestEqual(TEXT("Still armed after the swap"), Pawn->GetHandState(), EPlayerHandState::Armed);

	// Put the hook back: the hand is empty again.
	TestFalse(TEXT("The rifle's spot is taken and of another kind"), Pawn->HolsterWeapon(RifleRack));
	TestTrue(TEXT("Put the hook back"), Pawn->HolsterWeapon(HookRack));
	TestEqual(TEXT("Being lowered"), Pawn->GetHandState(), EPlayerHandState::Holstering);
	Advance(1.f);
	TestTrue(TEXT("Hand is empty"), Pawn->IsHandEmpty());
	TestTrue(TEXT("The hook is back on its spot"), HookRack->HasWeapon());
	TestFalse(TEXT("The hook is hidden"), Pawn->GetHook()->IsEquipped());

	// A swap is called off if the spot of the item in hand got taken meanwhile.
	TestTrue(TEXT("Take the rifle again"), Pawn->DrawWeapon(RifleRack));
	Advance(1.f);
	TestTrue(TEXT("Start a swap"), Pawn->SwapWeapon(HookRack));
	RifleRack->SetHasWeapon(true);
	Advance(1.f);
	TestEqual(TEXT("The rifle stays in hand"), Pawn->GetActiveGun(), static_cast<UGunComponent*>(Pawn->GetRifle()));
	TestTrue(TEXT("The hook stays on its spot"), HookRack->HasWeapon());

	// Fuel cannot replace a held gun or come from another pawn.
	auto* Hand = Pawn->GetFuelHand();
	TestFalse(TEXT("Gun in hand rejects fuel"), Pawn->TakeHandItem(Hand));
	TestTrue(TEXT("Clear held gun immediately"), Pawn->ConsumeHeldItem());
	auto* ForeignItem = NewObject<UFuelHandComponent>(RifleSpot);
	TestFalse(TEXT("Only pawn inventory items can be taken"), Pawn->TakeHandItem(ForeignItem));
	auto* Wood = NewObject<UStoveFuelType>(); Wood->BurnTime = 180.f; Wood->HeatAdd = 20.f;
	auto* Coal = NewObject<UStoveFuelType>(); Coal->BurnTime = 120.f; Coal->HeatAdd = 30.f;
	auto* Crystal = NewObject<UStoveFuelType>(); Crystal->BurnTime = 10.f; Crystal->HeatAdd = 50.f;
	Wood->HandMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	Wood->HandTransform.SetScale3D(FVector(0.1f));
	TArray<UFuelSupplyComponent*> Supplies;
	for (auto* Type : {Wood, Coal, Crystal})
	{
		auto* Supply = NewObject<UFuelSupplyComponent>(RifleSpot);
		Supply->Fuel = Type;
		Supply->RegisterComponentWithWorld(World);
		Supply->AddFuel(2);
		Supplies.Add(Supply);
	}
	ON_SCOPE_EXIT { for (auto* Supply : Supplies) Supply->DestroyComponent(); };
	auto Click = [Pawn](UObject* Item) { IInteractable::Execute_Interact(Item, Pawn); };
	auto CheckCounts = [&](int32 W, int32 C, int32 X)
	{
		TestEqual(TEXT("Wood stock"), Supplies[0]->GetCount(), W);
		TestEqual(TEXT("Coal stock"), Supplies[1]->GetCount(), C);
		TestEqual(TEXT("Crystal stock"), Supplies[2]->GetCount(), X);
	};
	Click(Supplies[0]); Advance(1.f);
	TestEqual(TEXT("Bucket equips pawn fuel item"), Pawn->GetActiveGun(), static_cast<UGunComponent*>(Hand));
	TestTrue(TEXT("Fuel hand armed"), Pawn->GetHandState() == EPlayerHandState::Armed);
	CheckCounts(1, 2, 2);
	TestTrue(TEXT("Hand mesh loaded synchronously"), Hand->FuelProp && Hand->FuelProp->GetStaticMesh() == Wood->HandMesh.Get());
	TestTrue(TEXT("Prop uses fuel transform"), Hand->FuelProp->GetRelativeTransform().Equals(Wood->HandTransform));
	TestFalse(TEXT("Fuel cannot holster on rack"), Pawn->HolsterWeapon(RifleRack));
	TestFalse(TEXT("Fuel cannot swap with rack"), Pawn->SwapWeapon(HookRack));
	TestFalse(TEXT("Rack declines fuel click"), IInteractable::Execute_CanInteract(RifleRack, Pawn));
	Click(RifleRack); Hand->PullTrigger(); Hand->TickComponent(0.1f, LEVELTICK_All, nullptr);
	TestTrue(TEXT("Rack click and trigger retain fuel"), Hand->Fuel == Wood);
	TestNull(TEXT("Fuel has no smoke actor"), Hand->GetSmoke());
	Click(Supplies[1]); Advance(1.f); CheckCounts(2, 1, 2);
	TestTrue(TEXT("Swap takes coal"), Hand->Fuel == Coal);
	Click(Supplies[2]); Advance(1.f); CheckCounts(2, 2, 1);
	Click(Supplies[2]); Advance(1.f); CheckCounts(2, 2, 2);
	TestTrue(TEXT("Same bucket return empties hand"), Pawn->IsHandEmpty());
	TestFalse(TEXT("Returned prop hidden immediately"), Hand->FuelProp->IsVisible());
	TestEqual(TEXT("Capacity limits refill"), Supplies[0]->AddFuel(100), 8);
	TestEqual(TEXT("Negative refill adds nothing"), Supplies[0]->AddFuel(-1), 0);
	Click(Supplies[1]); Advance(1.f);
	Supplies[1]->AddFuel(100);
	Click(Supplies[0]); CheckCounts(10, 10, 2);
	TestTrue(TEXT("Full home bucket prevents swap"), Hand->Fuel == Coal);
	Click(Supplies[1]);
	TestFalse(TEXT("Full same bucket retains fuel"), Pawn->IsHandEmpty());
	Pawn->ConsumeHeldItem();
	Supplies[2]->Capacity = 2;
	Click(Supplies[2]); Pawn->ConsumeHeldItem();
	Click(Supplies[2]); Pawn->ConsumeHeldItem();
	Click(Supplies[2]);
	TestTrue(TEXT("Empty bucket gives nothing"), Pawn->IsHandEmpty());
	Click(Supplies[0]); Advance(1.f);
	Click(Supplies[2]);
	TestTrue(TEXT("Empty target bucket prevents swap"), Hand->Fuel == Wood);
	TestEqual(TEXT("Failed swap retains wood count"), Supplies[0]->GetCount(), 9);
	// Feeding uses the real hit dispatch; only the door hit may open it.
	auto* Stove = NewObject<UStoveComponent>();
	Stove->FireboxShape = NewObject<UBoxComponent>();
	FHitResult Hit; Hit.Component = Stove->FireboxShape;
	IInteractable::Execute_AcceptsHit(Stove, Hit); Click(Stove);
	TestTrue(TEXT("Closed stove retains held fuel"), Hand->Fuel == Wood);
	TestEqual(TEXT("Closed stove adds no burn time"), Stove->GetBurnTimeLeft(), 0.f);
	TestFalse(TEXT("Closed mouth leaves door closed"), Stove->IsDoorOpen());
	Hit.Component.Reset(); Hit.BoneName = Stove->DoorBone;
	IInteractable::Execute_AcceptsHit(Stove, Hit); Click(Stove);
	TestTrue(TEXT("Door click opens stove before feeding"), Stove->IsDoorOpen());
	Hit.BoneName = NAME_None; Hit.Component = Stove->FireboxShape;
	IInteractable::Execute_AcceptsHit(Stove, Hit); Click(Stove);
	TestEqual(TEXT("Open stove receives fuel"), Stove->GetBurnTimeLeft(), 180.f);
	TestTrue(TEXT("Feeding empties hand"), Pawn->IsHandEmpty());
	TestFalse(TEXT("Feeding hides prop immediately"), Hand->FuelProp->IsVisible());
	Advance(1.f);
	TestTrue(TEXT("Consumed hand stays empty after updates"), Pawn->IsHandEmpty());
	TestTrue(TEXT("Taking rifle still works"), Pawn->DrawWeapon(HookRack));
	Advance(1.f);
	const int32 Before = Supplies[0]->GetCount(); Click(Supplies[0]);
	TestEqual(TEXT("Bucket rejects gun in hand"), Supplies[0]->GetCount(), Before);
	return true;
}
#endif
