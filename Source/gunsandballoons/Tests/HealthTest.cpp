#include "../HealthComponent.h"
#include "HealthTestObserver.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Engine/World.h"
#include "Engine/DamageEvents.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHealthTest, "GunsAndBalloons.Health",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHealthTest::RunTest(const FString& Parameters)
{
	UWorld* World = GWorld;
	if (!TestNotNull(TEXT("Editor world available"), World)) return false;
	AActor* Owner = World->SpawnActor<AActor>();
	if (!TestNotNull(TEXT("Owner spawned"), Owner)) return false;
	ON_SCOPE_EXIT { World->DestroyActor(Owner); };
	auto* Health = NewObject<UHealthComponent>(Owner);
	Health->RegisterComponentWithWorld(World);
	Health->MaxHealth = 80.f;
	Health->BeginPlay();
	ON_SCOPE_EXIT { Health->EndPlay(EEndPlayReason::Destroyed); Health->UnregisterComponent(); };
	auto* Observer = NewObject<UHealthTestObserver>(Health);
	Health->OnHealthChanged.AddDynamic(Observer, &UHealthTestObserver::Changed);
	Health->OnDepleted.AddDynamic(Observer, &UHealthTestObserver::Depleted);
	Health->OnRestored.AddDynamic(Observer, &UHealthTestObserver::Restored);
	TestEqual(TEXT("BeginPlay uses configured maximum"), Health->GetHealth(), 80.f);
	TestEqual(TEXT("Full normalized health"), Health->GetHealth01(), 1.f);
	TestEqual(TEXT("Applied damage returned"), Health->ApplyDamage(30.f), 30.f);
	TestEqual(TEXT("Damage delta"), Observer->LastDelta, -30.f);
	TestEqual(TEXT("Event value"), Observer->LastHealth, 50.f);
	TestTrue(TEXT("Event source"), Observer->LastSource == Health);
	TestEqual(TEXT("Overkill returns remaining health"), Health->ApplyDamage(100.f), 50.f);
	TestTrue(TEXT("Depleted"), Health->IsDepleted());
	TestEqual(TEXT("Depleted damage is ignored"), Health->ApplyDamage(5.f), 0.f);
	Health->SetHealth(-10.f);
	TestEqual(TEXT("Depletion fires once"), Observer->Depletions, 1);
	TestEqual(TEXT("No duplicate change"), Observer->Changes, 2);
	TestEqual(TEXT("Repair from zero"), Health->Repair(20.f), 20.f);
	TestEqual(TEXT("Restoration fires"), Observer->Restorations, 1);
	TestEqual(TEXT("Repair delta"), Observer->LastDelta, 20.f);
	TestEqual(TEXT("Partial normalized health"), Health->GetHealth01(), .25f);
	TestEqual(TEXT("Repair clamps and returns actual amount"), Health->Repair(100.f), 60.f);
	TestEqual(TEXT("Full repair returns zero"), Health->Repair(1.f), 0.f);
	const int32 Changes = Observer->Changes;
	for (float Invalid : {0.f, -1.f, std::numeric_limits<float>::infinity(),
		-std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()})
	{
		TestEqual(TEXT("Invalid damage no-op"), Health->ApplyDamage(Invalid), 0.f);
		TestEqual(TEXT("Invalid repair no-op"), Health->Repair(Invalid), 0.f);
		if (!FMath::IsFinite(Invalid)) Health->SetHealth(Invalid);
	}
	Health->SetHealth(1000.f);
	TestEqual(TEXT("No-op operations emit nothing"), Observer->Changes, Changes);
	Health->SetHealth(0.f);
	Health->SetHealth(200.f);
	TestEqual(TEXT("SetHealth restores once"), Observer->Restorations, 2);
	TestEqual(TEXT("Second depletion"), Observer->Depletions, 2);

	Health->DamageBones = {TEXT("stove"), TEXT("furnace_door")};
	FPointDamageEvent Point;
	Point.HitInfo.BoneName = TEXT("basket");
	Owner->TakeDamage(10.f, Point, nullptr, nullptr);
	TestEqual(TEXT("Rejected bone also skips AnyDamage"), Health->GetHealth(), 80.f);
	Point.HitInfo.BoneName = TEXT("stove");
	Owner->TakeDamage(10.f, Point, nullptr, nullptr);
	TestEqual(TEXT("Point damage counted once"), Health->GetHealth(), 70.f);
	Point.HitInfo.BoneName = TEXT("furnace_door");
	Owner->TakeDamage(10.f, Point, nullptr, nullptr);
	TestEqual(TEXT("Second allowed bone"), Health->GetHealth(), 60.f);
	Owner->TakeDamage(5.f, FDamageEvent(), nullptr, nullptr);
	TestEqual(TEXT("Generic damage bypasses bone filter"), Health->GetHealth(), 55.f);
	Health->DamageBones.Empty();
	Point.HitInfo.BoneName = TEXT("basket");
	Owner->TakeDamage(10.f, Point, nullptr, nullptr);
	TestEqual(TEXT("Empty filter accepts any bone once"), Health->GetHealth(), 45.f);
	Health->EndPlay(EEndPlayReason::RemovedFromWorld);
	Owner->TakeDamage(5.f, FDamageEvent(), nullptr, nullptr);
	TestEqual(TEXT("EndPlay unbinds damage"), Health->GetHealth(), 45.f);
	Health->bListenToActorDamage = false;
	Health->BeginPlay();
	Owner->TakeDamage(10.f, Point, nullptr, nullptr);
	Owner->TakeDamage(10.f, FDamageEvent(), nullptr, nullptr);
	TestEqual(TEXT("Listening can be disabled"), Health->GetHealth(), 80.f);
	return true;
}
#endif
