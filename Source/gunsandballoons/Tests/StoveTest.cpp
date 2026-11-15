#include "../StoveComponent.h"
#include "../StoveFuelType.h"
#include "../HealthComponent.h"
#include "../BalloonFlightComponent.h"
#include "StoveTestObserver.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "Misc/ScopeExit.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStoveHeatTest, "GunsAndBalloons.Stove.Heat", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FStoveHeatTest::RunTest(const FString& Parameters)
{
	if (!TestNotNull(TEXT("World"), GWorld.GetReference())) return false;
	auto* Stove = NewObject<UStoveComponent>();
	Stove->RegisterComponentWithWorld(GWorld);
	ON_SCOPE_EXIT { Stove->UnregisterComponent(); };
	auto* Wood = NewObject<UStoveFuelType>(); Wood->BurnTime = 180.f; Wood->HeatAdd = 20.f;
	auto* Crystal = NewObject<UStoveFuelType>(); Crystal->BurnTime = 10.f; Crystal->HeatAdd = 50.f; Crystal->bInstantHeat = true;
	auto Tick = [Stove](float Dt) { Stove->TickComponent(Dt, LEVELTICK_All, nullptr); };
	TestFalse(TEXT("Null refused"), Stove->AddFuel(nullptr));
	TestFalse(TEXT("Closed door refuses fuel"), Stove->AddFuel(Wood));
	Stove->SetDoorOpen(true); Tick(1.f);
	TestEqual(TEXT("Door reaches open angle"), Stove->GetDoorAngle(), 100.f);
	TestTrue(TEXT("Wood accepted"), Stove->AddFuel(Wood)); Stove->SetDoorOpen(false);
	Tick(10.f);
	TestEqual(TEXT("Ten second ignition stays cold"), Stove->GetTemperature(), 0.f);
	TestEqual(TEXT("Ignition preserves burn time"), Stove->GetBurnTimeLeft(), 180.f);
	Tick(4.f); TestEqual(TEXT("Wood heats at five degrees per second"), Stove->GetTemperature(), 20.f);
	TestEqual(TEXT("Door closes"), Stove->GetDoorAngle(), 0.f);
	Tick(5.f); TestEqual(TEXT("Pause holds temperature"), Stove->GetTemperature(), 20.f);
	Tick(1.f); TestEqual(TEXT("Pause ends without retroactive cooling"), Stove->GetTemperature(), 20.f);
	Tick(4.f); TestEqual(TEXT("Burning decay reaches sixteen"), Stove->GetTemperature(), 16.f);
	Tick(1.f); TestEqual(TEXT("Decay never reheats"), Stove->GetTemperature(), 15.f);
	Stove->SetDoorOpen(true);
	TestTrue(TEXT("Second wood"), Stove->AddFuel(Wood));
	TestTrue(TEXT("Third wood"), Stove->AddFuel(Wood));
	TestFalse(TEXT("Fourth wood exceeds cap"), Stove->AddFuel(Wood));
	Stove->bClampBurnTimeOnOverfill = true;
	TestTrue(TEXT("Optional clamp"), Stove->AddFuel(Wood));
	TestEqual(TEXT("Clamped burn time"), Stove->GetBurnTimeLeft(), 600.f);

	auto* Instant = NewObject<UStoveComponent>(); Instant->RegisterComponentWithWorld(GWorld);
	ON_SCOPE_EXIT { Instant->UnregisterComponent(); };
	Instant->SetDoorOpen(true); Instant->AddFuel(Wood); Instant->AddFuel(Crystal);
	TestEqual(TEXT("Crystal includes pending wood heat"), Instant->GetTemperature(), 70.f);
	Instant->SetDoorOpen(false); Instant->TickComponent(1.f, LEVELTICK_All, nullptr);
	TestEqual(TEXT("Crystal cancels ignition"), Instant->GetBurnTimeLeft(), 189.f);

	auto* Rate = NewObject<UStoveComponent>(); Rate->RegisterComponentWithWorld(GWorld);
	ON_SCOPE_EXIT { Rate->UnregisterComponent(); };
	Rate->SetDoorOpen(true); Rate->AddFuel(Crystal);
	TestEqual(TEXT("Crystal from cold is immediate"), Rate->GetTemperature(), 50.f);
	Rate->HeatRate = 10.f; Rate->AddFuel(Wood); Rate->SetDoorOpen(false);
	Rate->TickComponent(2.f, LEVELTICK_All, nullptr);
	TestEqual(TEXT("Runtime default rate"), Rate->GetTemperature(), 70.f);
	Wood->HeatRateOverride = 2.f; Rate->SetDoorOpen(true); Rate->AddFuel(Wood); Rate->SetDoorOpen(false);
	Rate->TickComponent(2.f, LEVELTICK_All, nullptr);
	TestEqual(TEXT("Fuel override"), Rate->GetTemperature(), 74.f);
	// A second fuel must never slow an already running heat-up.
	Wood->BurnTime = 0.f;
	Wood->HeatRateOverride = 0.f;
	Rate->SetDoorOpen(true); Rate->AddFuel(Wood); Rate->SetDoorOpen(false);
	Rate->TickComponent(1.f, LEVELTICK_All, nullptr);
	TestEqual(TEXT("Default rate joins slower override"), Rate->GetTemperature(), 84.f);
	Wood->HeatRateOverride = 2.f;
	Rate->SetDoorOpen(true); Rate->AddFuel(Wood); Rate->SetDoorOpen(false);
	Rate->TickComponent(1.f, LEVELTICK_All, nullptr);
	TestEqual(TEXT("Slower override preserves active default rate"), Rate->GetTemperature(), 94.f);
	Rate->HeatRate = 12.f;
	Rate->TickComponent(1.f, LEVELTICK_All, nullptr);
	TestEqual(TEXT("Default rate remains adjustable during heating"), Rate->GetTemperature(), 106.f);

	auto* Empty = NewObject<UStoveComponent>(); Empty->RegisterComponentWithWorld(GWorld);
	ON_SCOPE_EXIT { Empty->UnregisterComponent(); };
	auto* Observer = NewObject<UStoveTestObserver>();
	Empty->OnExtinguished.AddDynamic(Observer, &UStoveTestObserver::Extinguished);
	Empty->DecayPause = 0.f; Empty->SetDoorOpen(true); Empty->AddFuel(Crystal); Empty->SetDoorOpen(false);
	Empty->TickComponent(10.f, LEVELTICK_All, nullptr);
	TestEqual(TEXT("Fuel exhaustion does not cool retroactively"), Empty->GetTemperature(), 40.f);
	Empty->TickComponent(8.f, LEVELTICK_All, nullptr);
	TestEqual(TEXT("Empty decay is five per second"), Empty->GetTemperature(), 0.f);
	TestEqual(TEXT("Extinguished event"), Observer->Extinctions, 1);
	Empty->AddFuel(Crystal); // closed: refused
	Empty->SetDoorOpen(true); Empty->AddFuel(Crystal); Empty->TickComponent(10.f, LEVELTICK_All, nullptr);
	Empty->TickComponent(3.f, LEVELTICK_All, nullptr);
	TestEqual(TEXT("Open door empty decay clamps at zero"), Empty->GetTemperature(), 0.f);
	Empty->DecayPause = 10.f;
	Empty->AddFuel(Crystal);
	Empty->TickComponent(1.f, LEVELTICK_All, nullptr);
	TestEqual(TEXT("Open door loses heat even during pause"), Empty->GetTemperature(), 49.f);
	Empty->SetDoorOpen(false);
	Empty->TickComponent(1.f, LEVELTICK_All, nullptr);
	TestEqual(TEXT("Closing door preserves remaining pause"), Empty->GetTemperature(), 49.f);
	Empty->DecayPause = 0.f;
	Crystal->BurnTime = 0.f; Crystal->HeatAdd = 0.f;
	Empty->SetDoorOpen(true); Empty->AddFuel(Crystal); Empty->SetDoorOpen(false);
	Empty->TickComponent(8.f, LEVELTICK_All, nullptr);
	TestEqual(TEXT("Empty stove decays at five degrees per second"), Empty->GetTemperature(), 9.f);
	Empty->SetDoorOpen(true);
	Empty->TickComponent(.5f, LEVELTICK_All, nullptr);
	TestEqual(TEXT("Open door doubles empty stove decay"), Empty->GetTemperature(), 4.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStoveTickSizeTest, "GunsAndBalloons.Stove.TickSize", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FStoveTickSizeTest::RunTest(const FString& Parameters)
{
	if (!TestNotNull(TEXT("World"), GWorld.GetReference())) return false;
	auto Check = [this](const TCHAR* Label, float BurnTime, float Pause, bool bOpen, bool bInstant, float Elapsed, float Expected)
	{
		auto* Coarse = NewObject<UStoveComponent>();
		auto* Fine = NewObject<UStoveComponent>();
		Coarse->RegisterComponentWithWorld(GWorld);
		Fine->RegisterComponentWithWorld(GWorld);
		ON_SCOPE_EXIT { Coarse->UnregisterComponent(); Fine->UnregisterComponent(); };
		auto* Fuel = NewObject<UStoveFuelType>();
		Fuel->BurnTime = BurnTime; Fuel->HeatAdd = 50.f; Fuel->bInstantHeat = bInstant;
		for (auto* Stove : { Coarse, Fine })
		{
			Stove->DecayPause = Pause;
			Stove->SetDoorOpen(true);
			Stove->AddFuel(Fuel);
			Stove->SetDoorOpen(bOpen);
		}
		Coarse->TickComponent(Elapsed, LEVELTICK_All, nullptr);
		for (int32 I = 0; I < FMath::RoundToInt(Elapsed * 8.f); ++I)
			Fine->TickComponent(.125f, LEVELTICK_All, nullptr);
		TestEqual(FString(Label) + TEXT(" expected temperature"), Coarse->GetTemperature(), Expected, .001f);
		TestEqual(FString(Label) + TEXT(" temperature across tick sizes"), Coarse->GetTemperature(), Fine->GetTemperature(), .001f);
		TestEqual(FString(Label) + TEXT(" burn time across tick sizes"), Coarse->GetBurnTimeLeft(), Fine->GetBurnTimeLeft(), .001f);
	};
	Check(TEXT("Fuel exhaustion"), 2.f, 0.f, false, true, 3.f, 43.f);
	Check(TEXT("Pause expiry"), 20.f, 2.f, false, true, 3.f, 49.f);
	Check(TEXT("Both boundaries, open door"), 2.f, 1.f, true, true, 3.f, 37.f);
	Check(TEXT("Fuel expires before pause"), 1.f, 2.f, false, true, 3.f, 45.f);
	Check(TEXT("Exact fuel boundary"), 2.f, 0.f, false, true, 2.f, 48.f);
	Check(TEXT("Exact pause boundary"), 20.f, 2.f, false, true, 2.f, 50.f);
	Check(TEXT("Ignition and heat completion"), 20.f, 2.f, false, false, 21.f, 49.f);
	Check(TEXT("Extinction stops fuel consumption"), 100.f, 0.f, true, true, 30.f, 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStoveOverheatTest, "GunsAndBalloons.Stove.Overheat", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FStoveOverheatTest::RunTest(const FString& Parameters)
{
	if (!TestNotNull(TEXT("World"), GWorld.GetReference())) return false;
	auto* Stove = NewObject<UStoveComponent>(); Stove->RegisterComponentWithWorld(GWorld);
	ON_SCOPE_EXIT { if (Stove->HasBegunPlay()) Stove->EndPlay(EEndPlayReason::Destroyed); Stove->UnregisterComponent(); };
	auto* Health = NewObject<UHealthComponent>();
	auto* Flight = NewObject<UBalloonFlightComponent>(); Flight->SetTargetSpeed(100.f);
	Stove->Health = Health; Stove->Flight = Flight;
	Stove->BeginPlay();
	auto* Observer = NewObject<UStoveTestObserver>();
	Stove->OnOverheatChanged.AddDynamic(Observer, &UStoveTestObserver::Overheat);
	Stove->OnIgnited.AddDynamic(Observer, &UStoveTestObserver::Ignited);
	auto* Fuel = NewObject<UStoveFuelType>(); Fuel->BurnTime = 120.f; Fuel->bInstantHeat = true;
	Stove->SetDoorOpen(true);
	TestEqual(TEXT("Cold multiplier"), Stove->GetSpeedMultiplier(), 1.f);
	Fuel->HeatAdd = 90.f; Stove->AddFuel(Fuel);
	TestEqual(TEXT("Ninety multiplier"), Stove->GetSpeedMultiplier(), 1.6f);
	TestEqual(TEXT("Flight at ninety"), Flight->GetEffectiveTargetSpeed(), 160.f);
	Fuel->HeatAdd = 1.f; Stove->AddFuel(Fuel); Stove->SetDoorOpen(false);
	TestEqual(TEXT("Ninety-one multiplier"), Stove->GetSpeedMultiplier(), 2.f);
	TestEqual(TEXT("Flight overheated"), Flight->GetEffectiveTargetSpeed(), 200.f);
	Stove->TickComponent(1.f, LEVELTICK_All, nullptr);
	TestEqual(TEXT("Overheat damage for one second"), Health->GetHealth(), 98.f);
	TestEqual(TEXT("One rising event"), Observer->OverheatChanges, 1);
	Stove->DecayPause = 0.f; Fuel->HeatAdd = 0.f; Stove->SetDoorOpen(true); Stove->AddFuel(Fuel); Stove->SetDoorOpen(false);
	Stove->TickComponent(1.f, LEVELTICK_All, nullptr);
	TestFalse(TEXT("Ninety is not overheated"), Stove->IsOverheated());
	TestEqual(TEXT("One falling event"), Observer->OverheatChanges, 2);
	TestEqual(TEXT("One ignition"), Observer->Ignitions, 1);
	Stove->EndPlay(EEndPlayReason::Destroyed);
	TestEqual(TEXT("Cleanup removes modifier"), Flight->GetEffectiveTargetSpeed(), 100.f);
	return true;
}
#endif
