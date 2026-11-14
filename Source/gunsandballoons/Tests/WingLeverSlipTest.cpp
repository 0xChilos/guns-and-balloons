#include "../BalloonWingLeverComponent.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "Misc/ScopeExit.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWingLeverSlipTest, "GunsAndBalloons.WingLever.Slip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWingLeverSlipTest::RunTest(const FString& Parameters)
{
	auto* Lever = NewObject<UBalloonWingLeverComponent>();
	if (!TestNotNull(TEXT("Editor world is available"), GWorld.GetReference())) return false;
	Lever->RegisterComponentWithWorld(GWorld);
	ON_SCOPE_EXIT { Lever->UnregisterComponent(); };
	Lever->MaxSlipChance = 0.f;
	Lever->RegenerateStates();
	auto Tick = [Lever](float Dt) { Lever->TickComponent(Dt, LEVELTICK_All, nullptr); };
	auto Open = [&]() { Lever->SetState(3); Tick(2.f); };
	TestFalse(TEXT("Closed lever cannot slip"), Lever->TrySlip(1.f));
	Open();
	TestFalse(TEXT("Zero chance never slips"), Lever->TrySlip(0.f));
	TestFalse(TEXT("Negative chance never slips"), Lever->TrySlip(-1.f));
	TestEqual(TEXT("Zero chance preserves detent"), Lever->GetState(), 3);
	TestTrue(TEXT("Certain external slip succeeds"), Lever->TrySlip(1.f));
	TestEqual(TEXT("Slip targets fully closed immediately"), Lever->GetState(), 0);
	TestEqual(TEXT("Slip does not teleport wing"), Lever->GetWingAngle(), 90.f);
	TestFalse(TEXT("Repeated slip is a no-op"), Lever->Slip());
	Tick(.05f);
	TestEqual(TEXT("Lever uses slip speed"), Lever->GetLeverAngle(), 40.f);
	TestEqual(TEXT("Wing closes at its own speed"), Lever->GetWingAngle(), 87.f);
	TestEqual(TEXT("Intermediate detent does not catch lever"), Lever->GetState(), 0);
	Tick(.1f);
	TestEqual(TEXT("Lever reaches closed"), Lever->GetPosition(), 0.f);
	TestEqual(TEXT("Speed override clears on arrival"), Lever->MoveSpeedOverride, 0.f);
	Tick(2.f);
	TestEqual(TEXT("Wing eventually closes"), Lever->GetWingAngle(), 0.f);

	Open();
	Lever->bGrabbed = true;
	TestFalse(TEXT("Grab protects against guaranteed slip"), Lever->TrySlip(1.f));
	TestFalse(TEXT("Grab protects against direct slip"), Lever->Slip());
	Lever->bSlipWhileGrabbed = true;
	TestTrue(TEXT("Opt-in allows grabbed slip"), Lever->Slip());
	TestFalse(TEXT("Slip ends grab first"), Lever->IsGrabbed());
	TestEqual(TEXT("EndGrab release target is overridden"), Lever->TargetPosition, 0.f);

	Open();
	Lever->SlipCheckElapsed = 0.f;
	Lever->States[3].SlipChance = 1.f;
	Tick(.5f);
	TestEqual(TEXT("Automatic check respects interval"), Lever->GetState(), 3);
	Tick(.5f);
	TestEqual(TEXT("Certain automatic check closes open lever"), Lever->GetState(), 0);
	Lever->States[3].SlipChance = 0.f;
	Open();
	Tick(10.f);
	TestEqual(TEXT("Zero automatic chance stays open"), Lever->GetState(), 3);
	Lever->States[3].SlipChance = 1.f;
	Lever->LeverAngle = Lever->SampleByPosition(2.5f).LeverAngle;
	Lever->Position = Lever->TargetPosition = 2.5f;
	Tick(1.f);
	TestEqual(TEXT("Free release between detents does not roll"), Lever->GetState(), 3);

	Lever->States[3].SlipChance = 0.f;
	FRandomStream Expected(12345);
	Lever->SlipRandom.Initialize(12345);
	int32 Successes = 0;
	for (int32 I = 0; I < 32; ++I)
	{
		Lever->SetState(3);
		const bool bExpected = Expected.FRand() < .5f;
		const bool bSlipped = Lever->TrySlip(.5f);
		TestEqual(TEXT("Seeded rolls follow reproducible stream"), bSlipped, bExpected);
		Successes += bSlipped ? 1 : 0;
	}
	TestTrue(TEXT("Seeded sample covers both outcomes"), Successes > 0 && Successes < 32);
	return true;
}
#endif
