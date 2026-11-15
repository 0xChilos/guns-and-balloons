#include "../BalloonPlayerController.h"
#include "../FirstPersonCameraGameMode.h"
#include "../FirstPersonCameraPawn.h"
#include "../PlayerInteractionComponent.h"
#include "../StoveComponent.h"
#include "../AmmoCrate.h"
#include "../InteractSwap.h"
#include "../ExtinguisherComponent.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Engine/World.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "EngineGlobals.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlayerArchitectureTest, "GunsAndBalloons.Player.Architecture",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPlayerArchitectureTest::RunTest(const FString& Parameters)
{
	UWorld* World = GWorld;
	if (!TestNotNull(TEXT("Editor world available"), World)) return false;
	// Actor::ProcessEvent otherwise suppresses reflected interface events in an editor world.
	TGuardValue<bool> AllowActorEvents(GAllowActorScriptExecutionInEditor, true);
	auto* Pawn = World->SpawnActor<AFirstPersonCameraPawn>();
	auto* PC = World->SpawnActor<ABalloonPlayerController>();
	auto* Balloon = World->SpawnActor<AActor>();
	auto* Crate = World->SpawnActor<AAmmoCrate>();
	auto* Swap = World->SpawnActor<AInteractSwap>();
	auto* OwnedMesh = World->SpawnActor<AStaticMeshActor>();
	ON_SCOPE_EXIT
	{
		for (AActor* Actor : TArray<AActor*>{PC, Pawn, Balloon, Crate, OwnedMesh, Swap})
			if (Actor) World->DestroyActor(Actor);
	};
	if (!Pawn || !PC || !Balloon || !Crate || !Swap || !OwnedMesh) return false;
	TestEqual(TEXT("GameMode uses the input policy controller"),
		GetDefault<AFirstPersonCameraGameMode>()->PlayerControllerClass.Get(), ABalloonPlayerController::StaticClass());
	auto* Interaction = Pawn->FindComponentByClass<UPlayerInteractionComponent>();
	if (!TestNotNull(TEXT("Pawn owns an interaction router"), Interaction)) return false;

	// A stove component on a shared balloon actor owns the interaction and state.
	auto* Stove = NewObject<UStoveComponent>(Balloon);
	Balloon->AddInstanceComponent(Stove);
	FHitResult DoorHit(Balloon, nullptr, FVector::ZeroVector, FVector::UpVector);
	DoorHit.BoneName = TEXT("furnace_door");
	TestEqual(TEXT("Component resolves to its actor"), Interaction->FindAimedActor(DoorHit), Balloon);
	TestTrue(TEXT("Door click routed to stove"), Interaction->TryInteract(DoorHit, Pawn));
	TestTrue(TEXT("Stove owns the changed door state"), Stove->IsDoorOpen());
	TestTrue(TEXT("Second click routed"), Interaction->TryInteract(DoorHit, Pawn));
	TestFalse(TEXT("Second click closes door"), Stove->IsDoorOpen());
	DoorHit.BoneName = TEXT("gauge_needle");
	TestFalse(TEXT("Unrelated balloon bone is ignored"), Interaction->TryInteract(DoorHit, Pawn));

	FHitResult AmmoHit(Crate, nullptr, FVector::ZeroVector, FVector::UpVector);
	TestEqual(TEXT("Ammo remains discoverable"), Interaction->FindAimedActor(AmmoHit), static_cast<AActor*>(Crate));
	TestFalse(TEXT("LMB does not take ammo"), Interaction->TryInteract(AmmoHit, Pawn));
	OwnedMesh->SetOwner(Swap);
	FHitResult OwnedHit(OwnedMesh, OwnedMesh->GetStaticMeshComponent(), FVector::ZeroVector, FVector::UpVector);
	TestTrue(TEXT("Mesh has the interaction owner"), OwnedMesh->GetOwner() == Swap);
	TestFalse(TEXT("Swap starts unused"), Swap->HasSwapped());
	TestTrue(TEXT("Native actor permits interaction"), Swap->CanInteract_Implementation(Pawn));
	auto* NativeInterface = static_cast<IInteractable*>(Swap->GetNativeInterfaceAddress(UInteractable::StaticClass()));
	TestNotNull(TEXT("Native interface address"), NativeInterface);
	if (NativeInterface)
		TestTrue(TEXT("Interface vtable permits interaction"), NativeInterface->CanInteract_Implementation(Pawn));
	TestTrue(TEXT("Actor interface is enabled"), IInteractable::Execute_CanInteract(Swap, Pawn));
	TestTrue(TEXT("Owned mesh dispatches to actor interface"), Interaction->TryInteract(OwnedHit, Pawn));
	TestTrue(TEXT("Owner handles interaction"), Swap->HasSwapped());

	PC->Possess(Pawn);
	Pawn->ActiveGun = Pawn->GetExtinguisher();
	Pawn->HandState = EPlayerHandState::Armed;
	Pawn->ActiveGun->SetEquipped(true);
	Pawn->GetExtinguisher()->SetTriggerHeld(true);
	for (int32 Frame = 0; Frame < 240; ++Frame)
		Pawn->GetExtinguisher()->TickComponent(1.f / 60.f, LEVELTICK_All, nullptr);
	TestTrue(TEXT("Tool sprays before menu entry"), Pawn->GetExtinguisher()->IsSpraying());
	PC->UseMenuInput();
	TestFalse(TEXT("Menu disables gameplay polling"), PC->IsGameplayInputEnabled());
	TestTrue(TEXT("Menu keeps the player possessed"), PC->GetPawn() == Pawn);
	Pawn->GetExtinguisher()->TickComponent(1.f / 60.f, LEVELTICK_All, nullptr);
	TestFalse(TEXT("Menu releases tool trigger"), Pawn->GetExtinguisher()->IsSpraying());
	PC->UseGameplayInput();
	TestTrue(TEXT("Closing menu restores gameplay"), PC->IsGameplayInputEnabled());
	PC->UnPossess();
	return true;
}
#endif
