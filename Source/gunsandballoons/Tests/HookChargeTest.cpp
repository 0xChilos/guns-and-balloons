#include "../HookComponent.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHookChargeTest, "GunsAndBalloons.Hook.ChargeAndPump",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHookChargeTest::RunTest(const FString& Parameters)
{
	UWorld* World = GWorld;
	if (!TestNotNull(TEXT("Editor world available"), World)) return false;
	AActor* Owner = World->SpawnActor<AActor>();
	if (!Owner) return false;
	ON_SCOPE_EXIT { World->DestroyActor(Owner); };

	UHookComponent* Hook = NewObject<UHookComponent>(Owner);
	Hook->RegisterComponent();
	ON_SCOPE_EXIT { Hook->DestroyComponent(); };

	auto Tick = [Hook](float Seconds)
	{
		for (float T = 0.f; T < Seconds; T += 1.f / 60.f)
		{
			Hook->TickComponent(1.f / 60.f, LEVELTICK_All, nullptr);
		}
	};

	// Comes fully charged (default InitialCharge) - set explicitly since BeginPlay does not run here.
	Hook->SetCharge(100.f);
	TestTrue(TEXT("Starts ready"), Hook->IsFullyCharged());

	// Below 100 the hook does not fire.
	Hook->SetCharge(99.f);
	Hook->PullTrigger();
	TestEqual(TEXT("No throw below 100"), Hook->GetState(), EHookState::Stowed);
	TestEqual(TEXT("Dry pull keeps the pressure"), Hook->GetCharge(), 99.f);

	// Pumping from 0: exactly PumpsToFullCharge strokes reach 100. Easy up to the middle, heavier after:
	// with 4 strokes and the lever heavy from 50, strokes 1-2 take the clip's length, 3 and 4 longer and longer.
	Hook->SetPumpsToFullCharge(4);
	Hook->SetHardPumpFrom(50.f);
	Hook->SetHardPumpPlayRate(0.4f);
	Hook->SetCharge(0.f);
	Hook->SetSecondaryHeld(true);
	TArray<float> StrokeTimes;
	float Elapsed = 0.f;
	float LastCharge = Hook->GetCharge();
	for (int32 Step = 0; Step < 60 * 10 && StrokeTimes.Num() < 4; ++Step)
	{
		Hook->TickComponent(1.f / 60.f, LEVELTICK_All, nullptr);
		Elapsed += 1.f / 60.f;
		if (Hook->GetCharge() > LastCharge)
		{
			StrokeTimes.Add(Elapsed);
			Elapsed = 0.f;
			LastCharge = Hook->GetCharge();
			if (StrokeTimes.Num() < 4)
			{
				TestFalse(TEXT("Not ready before the last stroke"), Hook->IsFullyCharged());
			}
		}
	}
	if (TestEqual(TEXT("Four strokes to 100"), StrokeTimes.Num(), 4))
	{
		AddInfo(FString::Printf(TEXT("Stroke times: %.3f %.3f %.3f %.3f s"), StrokeTimes[0], StrokeTimes[1], StrokeTimes[2], StrokeTimes[3]));
		TestTrue(TEXT("Easy strokes before the middle are the same"), FMath::Abs(StrokeTimes[1] - StrokeTimes[0]) <= 1.5f / 60.f);
		TestTrue(TEXT("Past the middle the lever gets heavier"), StrokeTimes[2] > StrokeTimes[1] + 1.f / 60.f);
		TestTrue(TEXT("The last stroke is the heaviest"), StrokeTimes[3] > StrokeTimes[2] + 1.f / 60.f);
		TestTrue(TEXT("Last stroke at 40% speed: ~2.5x the easy one"), FMath::IsNearlyEqual(StrokeTimes[3] / StrokeTimes[0], 2.5f, 0.15f));
	}
	TestTrue(TEXT("Four strokes: ready"), Hook->IsFullyCharged());
	TestEqual(TEXT("Pressure capped at 100"), Hook->GetCharge(), 100.f);
	Tick(1.f);
	TestEqual(TEXT("Holding RMB at 100 does not pump on"), Hook->GetAction() == EHookAction::Pump, false);
	Hook->SetSecondaryHeld(false);

	// Firing at 100 empties it and sends the head out.
	Tick(0.5f);
	Hook->PullTrigger();
	TestEqual(TEXT("Fired: pressure 0"), Hook->GetCharge(), 0.f);
	TestEqual(TEXT("Fired: head out"), Hook->GetState(), EHookState::Out);

	// No pumping while the head is out.
	Hook->SetSecondaryHeld(true);
	Tick(0.1f);
	TestFalse(TEXT("No pump while out"), Hook->GetAction() == EHookAction::Pump);
	return true;
}
#endif
