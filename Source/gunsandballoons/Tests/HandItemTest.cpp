#include "../FirstPersonCameraPawn.h"
#include "../HandItemPickup.h"
#include "../HookComponent.h"
#include "../RevolverComponent.h"
#include "../RifleComponent.h"
#include "../WeaponRackComponent.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Components/SkeletalMeshComponent.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/World.h"

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
	return true;
}
#endif
