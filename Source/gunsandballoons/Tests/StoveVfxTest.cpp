#include "../StoveVfxComponent.h"
#include "../StoveComponent.h"
#include "../StoveFuelType.h"
#include "Components/BoxComponent.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/World.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStoveVfxTest, "GunsAndBalloons.Stove.VFX", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FStoveVfxTest::RunTest(const FString& Parameters)
{
	if (!TestNotNull(TEXT("World"), GWorld.GetReference())) return false;
	auto* Owner = GWorld->SpawnActor<AActor>();
	if (!TestNotNull(TEXT("Owner"), Owner)) return false;
	auto* Mouth = NewObject<UBoxComponent>(Owner, TEXT("StoveFirebox"));
	Owner->SetRootComponent(Mouth);
	Mouth->RegisterComponentWithWorld(GWorld);
	auto* Stove = NewObject<UStoveComponent>(Owner);
	Stove->RegisterComponentWithWorld(GWorld);
	if (!Stove->HasBegunPlay()) Stove->BeginPlay();
	auto* Vfx = NewObject<UStoveVfxComponent>(Owner);
	Vfx->RegisterComponentWithWorld(GWorld);
	if (!Vfx->HasBegunPlay()) Vfx->BeginPlay();
	ON_SCOPE_EXIT
	{
		if (Vfx->HasBegunPlay()) Vfx->EndPlay(EEndPlayReason::Destroyed);
		GWorld->DestroyActor(Owner);
	};
	auto* Fuel = NewObject<UStoveFuelType>();
	Fuel->BurnTime = 180.f; Fuel->HeatAdd = 95.f; Fuel->bInstantHeat = true;
	TestFalse(TEXT("Closed stove rejects fuel"), Stove->AddFuel(Fuel));
	TestNull(TEXT("Rejected fuel emits no effect"), Vfx->GetFuelEffect());
	TestNull(TEXT("Cold stove has no overheat effect"), Vfx->GetOverheatEffect());
	Stove->DecayPause = 0.f;
	Stove->SetDoorOpen(true);
	TestTrue(TEXT("Fuel accepted"), Stove->AddFuel(Fuel));
	if (!TestNotNull(TEXT("Accepted fuel creates effect"), Vfx->GetFuelEffect())) return false;
	TestEqual(TEXT("One fuel burst"), Vfx->GetFuelEffect()->GetNumPuffs(), 12);
	if (!TestNotNull(TEXT("Overheat creates effect"), Vfx->GetOverheatEffect())) return false;
	TestTrue(TEXT("Overheat starts continuous emission"), Vfx->IsComponentTickEnabled());
	const int32 Before = Vfx->GetOverheatEffect()->GetNumPuffs();
	Vfx->TickComponent(.5f, LEVELTICK_All, nullptr);
	TestEqual(TEXT("Overheat emits at configured rate"), Vfx->GetOverheatEffect()->GetNumPuffs(), Before + 4);
	Stove->SetDoorOpen(false); Stove->DecayPause = 0.f;
	Stove->TickComponent(5.f, LEVELTICK_All, nullptr);
	TestFalse(TEXT("Cooling stops overheat emission"), Vfx->IsComponentTickEnabled());
	const int32 After = Vfx->GetOverheatEffect()->GetNumPuffs();
	Vfx->TickComponent(1.f, LEVELTICK_All, nullptr);
	TestEqual(TEXT("Cooling does not spawn additional smoke"), Vfx->GetOverheatEffect()->GetNumPuffs(), After);
	auto* Burst = Vfx->GetFuelEffect();
	auto* Smoke = Vfx->GetOverheatEffect();
	Vfx->EndPlay(EEndPlayReason::Destroyed);
	TestTrue(TEXT("Fuel effect cleaned up"), Burst->IsActorBeingDestroyed());
	TestTrue(TEXT("Overheat effect cleaned up"), Smoke->IsActorBeingDestroyed());
	return true;
}
#endif
