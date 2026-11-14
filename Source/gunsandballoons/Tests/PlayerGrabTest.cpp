#include "../FirstPersonCameraPawn.h"
#include "../BalloonWingLeverComponent.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlayerGrabTest, "GunsAndBalloons.Player.GrabLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPlayerGrabTest::RunTest(const FString& Parameters)
{
	UWorld* World = GWorld;
	if (!TestNotNull(TEXT("Editor world available"), World)) return false;
	AFirstPersonCameraPawn* Pawn = World->SpawnActor<AFirstPersonCameraPawn>();
	APlayerController* PC = World->SpawnActor<APlayerController>();
	if (!Pawn || !PC) return false;
	ON_SCOPE_EXIT { World->DestroyActor(PC); World->DestroyActor(Pawn); };
	auto* Lever = NewObject<UBalloonWingLeverComponent>(Pawn);
	Lever->RegenerateStates();
	Pawn->HandState = EPlayerHandState::Empty;
	auto Hold = [&]()
	{
		Pawn->GrabTarget = Lever;
		Lever->GrabPawn = Pawn;
		Lever->bGrabbed = true;
	};
	Hold();
	TestTrue(TEXT("Interface reports held state"), IGrabbable::Execute_IsGrabbed(Lever));
	Pawn->UpdateHeldGrab(PC); // No LMB held: release through the normal frame path.
	TestFalse(TEXT("Mouse release clears pawn target"), Pawn->GrabTarget.IsValid());
	TestFalse(TEXT("Mouse release ends target grab"), Lever->IsGrabbed());

	Hold();
	Lever->SetState(3);
	Lever->bSlipWhileGrabbed = true;
	TestTrue(TEXT("Slip succeeds while held when enabled"), Lever->Slip());
	Pawn->UpdateHeldGrab(PC);
	TestFalse(TEXT("Pawn notices target ended its grab"), Pawn->GrabTarget.IsValid());
	TestEqual(TEXT("Pawn preserves slip target"), Lever->TargetPosition, 0.f);

	Hold();
	Pawn->UnPossessed();
	TestFalse(TEXT("UnPossessed clears pawn target"), Pawn->GrabTarget.IsValid());
	TestFalse(TEXT("UnPossessed releases target"), Lever->IsGrabbed());
	Pawn->EndGrab();
	TestFalse(TEXT("Repeated cleanup is safe"), Lever->IsGrabbed());
	return true;
}
#endif
