#include "../StoveVfxComponent.h"
#include "../StoveComponent.h"
#include "../HealthComponent.h"
#include "Components/BoxComponent.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/World.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStoveDamageSmokeTest, "GunsAndBalloons.Stove.DamageSmoke", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FStoveDamageSmokeTest::RunTest(const FString& Parameters)
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
	auto* Health = NewObject<UHealthComponent>(Owner);
	Health->RegisterComponentWithWorld(GWorld);
	UActorComponent* HealthBase = Health;   // BeginPlay / EndPlay are protected on UHealthComponent
	if (!Health->HasBegunPlay()) HealthBase->BeginPlay();
	auto* Vfx = NewObject<UStoveVfxComponent>(Owner);
	Vfx->RegisterComponentWithWorld(GWorld);
	if (!Vfx->HasBegunPlay()) Vfx->BeginPlay();
	ON_SCOPE_EXIT
	{
		if (Vfx->HasBegunPlay()) Vfx->EndPlay(EEndPlayReason::Destroyed);
		if (Health->HasBegunPlay()) HealthBase->EndPlay(EEndPlayReason::Destroyed);
		GWorld->DestroyActor(Owner);
	};
	TestFalse(TEXT("Full health, cold stove: no tick"), Vfx->IsComponentTickEnabled());
	Health->ApplyDamage(10.f);
	TestTrue(TEXT("Damage starts the hole-smoke tick"), Vfx->IsComponentTickEnabled());
	Vfx->TickComponent(1.f, LEVELTICK_All, nullptr);
	TestNull(TEXT("No skeletal mesh / hole bones: no smoke and no crash"), Vfx->GetDamageEffect());
	Health->Repair(5.f);
	TestTrue(TEXT("Still damaged: keeps ticking"), Vfx->IsComponentTickEnabled());
	Health->Repair(100.f);
	TestFalse(TEXT("Fully repaired: tick stops"), Vfx->IsComponentTickEnabled());
	return true;
}
#endif
