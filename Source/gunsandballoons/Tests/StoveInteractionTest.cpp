#include "../StoveComponent.h"
#include "Components/BoxComponent.h"
#include "../FirstPersonCameraPawn.h"
#include "../FuelHandComponent.h"
#include "../StoveFuelType.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/World.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStoveInteractionTest, "GunsAndBalloons.Stove.Interaction", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FStoveInteractionTest::RunTest(const FString& Parameters)
{
	auto* Stove = NewObject<UStoveComponent>();
	FHitResult Hit;
	TestFalse(TEXT("Unset shape does not accept null component"), IInteractable::Execute_AcceptsHit(Stove, Hit));
	Hit.BoneName = TEXT("gauge_needle");
	TestFalse(TEXT("Gauge is not clickable"), IInteractable::Execute_AcceptsHit(Stove, Hit));
	Hit.BoneName = Stove->DoorBone;
	TestTrue(TEXT("Door bone accepted"), IInteractable::Execute_AcceptsHit(Stove, Hit));
	TestTrue(TEXT("Clicks always consumed"), IInteractable::Execute_CanInteract(Stove, nullptr));
	IInteractable::Execute_Interact(Stove, nullptr);
	TestTrue(TEXT("Door click opens"), Stove->IsDoorOpen());
	IInteractable::Execute_Interact(Stove, nullptr);
	TestTrue(TEXT("Cached hit consumed once"), Stove->IsDoorOpen());
	IInteractable::Execute_AcceptsHit(Stove, Hit);
	IInteractable::Execute_Interact(Stove, nullptr);
	TestFalse(TEXT("Second door click closes"), Stove->IsDoorOpen());
	Stove->FireboxShape = NewObject<UBoxComponent>();
	Hit.BoneName = NAME_None;
	Hit.Component = Stove->FireboxShape;
	TestTrue(TEXT("Firebox component accepted"), IInteractable::Execute_AcceptsHit(Stove, Hit));
	IInteractable::Execute_Interact(Stove, nullptr);
	TestFalse(TEXT("Closed firebox click leaves door closed"), Stove->IsDoorOpen());
	Stove->SetDoorOpen(true);
	IInteractable::Execute_AcceptsHit(Stove, Hit);
	IInteractable::Execute_Interact(Stove, nullptr);
	TestEqual(TEXT("No fuel created by empty click"), Stove->GetBurnTimeLeft(), 0.f);
	Hit.Component.Reset();
	IInteractable::Execute_AcceptsHit(Stove, Hit);
	IInteractable::Execute_Interact(Stove, nullptr);
	TestTrue(TEXT("Rejected hit clears cache"), Stove->IsDoorOpen());
	TestEqual(TEXT("Sweep from rig source"), Stove->GaugeSweepDeg, 240.f);
	if (!TestNotNull(TEXT("World"), GWorld.GetReference())) return false;
	auto* Pawn = GWorld->SpawnActor<AFirstPersonCameraPawn>();
	if (!TestNotNull(TEXT("Pawn"), Pawn)) return false;
	ON_SCOPE_EXIT { GWorld->DestroyActor(Pawn); };
	auto* RuntimeMouth = NewObject<UBoxComponent>(Pawn, TEXT("StoveFirebox"));
	RuntimeMouth->RegisterComponentWithWorld(GWorld);
	auto* RuntimeStove = NewObject<UStoveComponent>(Pawn);
	RuntimeStove->FireboxShape = Stove->FireboxShape; // Simulate an unremapped template reference.
	RuntimeStove->RegisterComponentWithWorld(GWorld);
	if (!RuntimeStove->HasBegunPlay()) RuntimeStove->BeginPlay();
	TestEqual(TEXT("BeginPlay resolves mouth on its own actor"), RuntimeStove->FireboxShape.Get(), static_cast<UPrimitiveComponent*>(RuntimeMouth));
	auto* Hand = NewObject<UFuelHandComponent>(Pawn);
	auto* Fuel = NewObject<UStoveFuelType>();
	Fuel->BurnTime = 180.f; Fuel->HeatAdd = 20.f;
	Hand->Fuel = Fuel;
	Pawn->ActiveGun = Hand;
	Pawn->HandState = EPlayerHandState::Armed;
	Hit.Component = Stove->FireboxShape;
	Stove->SetDoorOpen(false);
	IInteractable::Execute_AcceptsHit(Stove, Hit);
	IInteractable::Execute_Interact(Stove, Pawn);
	TestTrue(TEXT("Closed mouth retains fuel"), Hand->Fuel == Fuel);
	TestFalse(TEXT("Fuel click on closed mouth leaves door closed"), Stove->IsDoorOpen());
	TestEqual(TEXT("Closed mouth does not accept fuel"), Stove->GetBurnTimeLeft(), 0.f);
	Stove->SetDoorOpen(true);
	Stove->MaxBurnTime = 100.f;
	IInteractable::Execute_AcceptsHit(Stove, Hit);
	IInteractable::Execute_Interact(Stove, Pawn);
	TestTrue(TEXT("Overfill retains fuel"), Hand->Fuel == Fuel);
	TestFalse(TEXT("Overfill keeps occupied hand"), Pawn->IsHandEmpty());
	Stove->MaxBurnTime = 600.f;
	IInteractable::Execute_AcceptsHit(Stove, Hit);
	IInteractable::Execute_Interact(Stove, Pawn);
	TestEqual(TEXT("Accepted fuel added exactly once"), Stove->GetBurnTimeLeft(), 180.f);
	TestNull(TEXT("Accepted item cleared"), Hand->Fuel.Get());
	TestTrue(TEXT("Successful feed empties hand"), Pawn->IsHandEmpty());
	return true;
}
#endif
