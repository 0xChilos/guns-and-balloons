#include "../StoveComponent.h"
#include "../StoveFuelType.h"
#include "../ExtinguisherComponent.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStoveCoolingTest, "GunsAndBalloons.Extinguisher.StoveCooling",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FStoveCoolingTest::RunTest(const FString& Parameters)
{
	if (!TestNotNull(TEXT("World"), GWorld.GetReference())) return false;
	auto* Target = GWorld->SpawnActor<AActor>();
	auto* Shooter = GWorld->SpawnActor<AActor>();
	auto* Blocker = GWorld->SpawnActor<AActor>();
	ON_SCOPE_EXIT
	{
		for (AActor* Actor : {Target, Shooter, Blocker}) if (Actor) GWorld->DestroyActor(Actor);
	};
	if (!Target || !Shooter || !Blocker) return false;
	auto* Stove = NewObject<UStoveComponent>(Target);
	Target->AddInstanceComponent(Stove);
	Stove->RegisterComponent();
	auto* Fuel = NewObject<UStoveFuelType>();
	Fuel->bInstantHeat = true;
	Fuel->HeatAdd = 100.f;
	Fuel->BurnTime = 180.f;
	Stove->SetDoorOpen(true);
	Stove->AddFuel(Fuel);
	Stove->SetDoorOpen(false);
	const float BurnTime = Stove->GetBurnTimeLeft();
	TestTrue(TEXT("Initially overheated"), Stove->IsOverheated());
	TestEqual(TEXT("Cooling removes heat"), Stove->ApplyCooling(20.f), 20.f);
	TestEqual(TEXT("Temperature falls"), Stove->GetTemperature(), 80.f);
	TestFalse(TEXT("Cooling immediately clears overheat"), Stove->IsOverheated());
	TestEqual(TEXT("Cooling preserves fuel"), Stove->GetBurnTimeLeft(), BurnTime);
	Stove->TickComponent(.1f, LEVELTICK_All, nullptr);
	TestEqual(TEXT("Old heat target does not undo cooling"), Stove->GetTemperature(), 80.f);
	TestEqual(TEXT("Negative cooling ignored"), Stove->ApplyCooling(-10.f), 0.f);
	TestEqual(TEXT("NaN cooling ignored"), Stove->ApplyCooling(std::numeric_limits<float>::quiet_NaN()), 0.f);

	auto MakeBox = [](AActor* Owner, FVector Location)
	{
		auto* Box = NewObject<UBoxComponent>(Owner);
		Owner->AddInstanceComponent(Box);
		Owner->SetRootComponent(Box);
		Box->SetBoxExtent(FVector(15.f));
		Box->SetCollisionProfileName(TEXT("BlockAll"));
		Box->RegisterComponent();
		Box->SetWorldLocation(Location);
		return Box;
	};
	const FVector Origin(100000.f, 100000.f, 100000.f);
	Stove->FireboxShape = MakeBox(Target, Origin + FVector(200.f, 0.f, 0.f));
	auto* Wall = MakeBox(Blocker, Origin + FVector(100.f, 100.f, 0.f));
	auto* Extinguisher = NewObject<UExtinguisherComponent>(Shooter);
	Shooter->AddInstanceComponent(Extinguisher);
	Extinguisher->RegisterComponent();
	FHitResult FireboxHit(Target, Stove->FireboxShape, Origin, FVector::ForwardVector);
	TestTrue(TEXT("Spraying at the stove takes priority over opening its door"), Extinguisher->ShouldUseInsteadOfInteract(FireboxHit));
	FHitResult BlockerHit(Blocker, Wall, Origin, FVector::ForwardVector);
	TestFalse(TEXT("Other interactions retain priority"), Extinguisher->ShouldUseInsteadOfInteract(BlockerHit));
	Extinguisher->SetAmount(100.f);
	Extinguisher->SetTriggerHeld(true);
	Extinguisher->TickComponent(.01f, LEVELTICK_All, nullptr);
	TestTrue(TEXT("Spray active"), Extinguisher->IsSpraying());
	Extinguisher->ApplySprayCooling(Origin, FVector::ForwardVector, .5f);
	TestEqual(TEXT("Nozzle hit cools stove"), Stove->GetTemperature(), 70.f);
	Extinguisher->ApplySprayCooling(Origin, FVector::RightVector, .5f);
	TestEqual(TEXT("Miss does not cool"), Stove->GetTemperature(), 70.f);
	Wall->SetWorldLocation(Origin + FVector(100.f, 0.f, 0.f));
	Extinguisher->ApplySprayCooling(Origin, FVector::ForwardVector, .5f);
	TestEqual(TEXT("Obstacle blocks cooling"), Stove->GetTemperature(), 70.f);
	Wall->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Extinguisher->StoveCoolingRange = 50.f;
	Extinguisher->ApplySprayCooling(Origin, FVector::ForwardVector, .5f);
	TestEqual(TEXT("Out of range does not cool"), Stove->GetTemperature(), 70.f);
	Extinguisher->StoveCoolingRange = 400.f;
	Extinguisher->SetAmount(0.f);
	Extinguisher->ApplySprayCooling(Origin, FVector::ForwardVector, .5f);
	TestEqual(TEXT("Empty extinguisher does not cool"), Stove->GetTemperature(), 70.f);
	FHitResult Hit;
	Hit.BoneName = TEXT("basket");
	TestFalse(TEXT("Basket is not a cooling surface"), Stove->AcceptsCoolingHit(Hit));
	Hit.BoneName = TEXT("stove");
	TestTrue(TEXT("Stove body accepts cooling"), Stove->AcceptsCoolingHit(Hit));
	Stove->ApplyCooling(1000.f);
	TestEqual(TEXT("Temperature clamps to zero"), Stove->GetTemperature(), 0.f);
	Stove->TickComponent(1.f, LEVELTICK_All, nullptr);
	TestEqual(TEXT("Extinguished stove stays cold"), Stove->GetTemperature(), 0.f);
	return true;
}
#endif
