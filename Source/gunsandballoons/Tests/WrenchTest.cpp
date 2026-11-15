#include "../WrenchComponent.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWrenchFixTest, "GunsAndBalloons.Wrench.FixCycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWrenchFixTest::RunTest(const FString& Parameters)
{
	UWorld* World = GWorld;
	if (!TestNotNull(TEXT("Editor world available"), World)) return false;
	AActor* Owner = World->SpawnActor<AActor>();
	if (!Owner) return false;
	ON_SCOPE_EXIT { World->DestroyActor(Owner); };

	UWrenchComponent* Wrench = NewObject<UWrenchComponent>(Owner);
	Wrench->RegisterComponent();
	ON_SCOPE_EXIT { Wrench->DestroyComponent(); };

	auto Tick = [Wrench](float Seconds)
	{
		for (float T = 0.f; T < Seconds - 1.e-4f; T += 1.f / 60.f)
		{
			Wrench->TickComponent(1.f / 60.f, LEVELTICK_All, nullptr);
		}
	};

	// --- The Fix clip's rest poses (SK_Wrench_Fix: four turns, back at rest at 1.25, 2.25, 3.25, 4.25 s).
	Wrench->FindFixRestTimes();
	TestTrue(TEXT("In the opening hold: at rest now"), FMath::IsNearlyEqual(Wrench->GetNextFixRestTime(0.1f), 0.1f, 0.02f));
	TestTrue(FString::Printf(TEXT("Mid first turn: rest at 1.25 (got %.2f)"), Wrench->GetNextFixRestTime(0.5f)),
		FMath::IsNearlyEqual(Wrench->GetNextFixRestTime(0.5f), 1.25f, 0.05f));
	TestTrue(FString::Printf(TEXT("Mid third turn: rest at 3.25 (got %.2f)"), Wrench->GetNextFixRestTime(2.6f)),
		FMath::IsNearlyEqual(Wrench->GetNextFixRestTime(2.6f), 3.25f, 0.05f));

	// --- LMB held: onto the work (ToFix 0.67 s), then turning.
	TestEqual(TEXT("Starts ready"), Wrench->GetAction(), EWrenchAction::Idle);
	TestEqual(TEXT("Ready: off the crosshair"), Wrench->GetCrosshairPin(), 0.f);
	Wrench->SetTriggerHeld(true);
	Tick(0.1f);
	TestEqual(TEXT("Held: ToFix"), Wrench->GetAction(), EWrenchAction::ToFix);
	const float PinDuringToFix = Wrench->GetCrosshairPin();
	TestTrue(FString::Printf(TEXT("ToFix: on its way to the crosshair (pin %.2f)"), PinDuringToFix), PinDuringToFix > 0.f && PinDuringToFix < 1.f);
	Tick(0.7f);
	TestTrue(TEXT("Held past ToFix: turning"), Wrench->IsFixing());
	TestEqual(TEXT("Turning: on the crosshair"), Wrench->GetCrosshairPin(), 1.f);

	// Let go part-way into the first turn: it finishes the turn (to 1.25 s in Fix), then comes off the work.
	Tick(0.3f);
	Wrench->SetTriggerHeld(false);
	Tick(1.f / 60.f);
	TestEqual(TEXT("Released mid-turn: finishes the turn"), Wrench->GetAction(), EWrenchAction::FinishTurn);
	TestEqual(TEXT("Finishing the turn: still on the crosshair"), Wrench->GetCrosshairPin(), 1.f);
	Tick(0.4f);
	TestEqual(TEXT("Still finishing"), Wrench->GetAction(), EWrenchAction::FinishTurn);
	Tick(0.6f);
	TestEqual(TEXT("Turn done: off the work"), Wrench->GetAction(), EWrenchAction::FromFix);
	Tick(0.75f);
	TestEqual(TEXT("Back to ready"), Wrench->GetAction(), EWrenchAction::Idle);
	TestEqual(TEXT("Back to ready: off the crosshair"), Wrench->GetCrosshairPin(), 0.f);

	// Let go during ToFix: straight back.
	Wrench->SetTriggerHeld(true);
	Tick(0.2f);
	Wrench->SetTriggerHeld(false);
	Tick(1.f / 60.f);
	TestEqual(TEXT("Released during ToFix: back off"), Wrench->GetAction(), EWrenchAction::FromFix);
	Tick(0.3f);
	TestEqual(TEXT("Backed off as far as it got"), Wrench->GetAction(), EWrenchAction::Idle);

	// --- Putting it away plays Equip backwards; calling it off brings it back up.
	Wrench->PrepareToHolster();
	Tick(1.f / 60.f);
	TestEqual(TEXT("Holstering: Unequip"), Wrench->GetAction(), EWrenchAction::Unequip);
	TestFalse(TEXT("Not ready mid-Unequip"), Wrench->IsReadyToHolster());
	Wrench->CancelHolster();
	TestEqual(TEXT("Called off: Equip"), Wrench->GetAction(), EWrenchAction::Equip);
	Tick(1.1f);
	TestEqual(TEXT("Up again: ready"), Wrench->GetAction(), EWrenchAction::Idle);
	Wrench->PrepareToHolster();
	Tick(1.1f);
	TestTrue(TEXT("Ready after Equip backwards"), Wrench->IsReadyToHolster());
	return true;
}
#endif
