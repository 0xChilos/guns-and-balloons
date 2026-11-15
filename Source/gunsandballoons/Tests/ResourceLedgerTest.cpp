#include "../ResourceLedger.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "ResourceLedgerTestObserver.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FResourceLedgerTest, "GunsAndBalloons.Resources.Ledger",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FResourceLedgerTest::RunTest(const FString& Parameters)
{
	// Not registered with a world: only the counting is under test here (no world is touched).
	UResourceLedger* Ledger = NewObject<UResourceLedger>(GetTransientPackage());
	UResourceLedgerTestObserver* Observer = NewObject<UResourceLedgerTestObserver>();
	Ledger->OnCountChanged.AddDynamic(Observer, &UResourceLedgerTestObserver::Changed);

	for (EResourceType Type : ResourceTypes::All)
	{
		TestEqual(TEXT("Starts at 0"), Ledger->GetCount(Type), 0);
	}

	// Each resource counts on its own.
	Ledger->Add(EResourceType::Coal, 1);
	Ledger->Add(EResourceType::Coal, 2);
	Ledger->Add(EResourceType::Crystal, 1);
	TestEqual(TEXT("Coal 3"), Ledger->GetCount(EResourceType::Coal), 3);
	TestEqual(TEXT("Crystal 1"), Ledger->GetCount(EResourceType::Crystal), 1);
	TestEqual(TEXT("Ammo untouched"), Ledger->GetCount(EResourceType::Ammo), 0);
	TestEqual(TEXT("One event per add"), Observer->Changes, 3);
	TestEqual(TEXT("Event type"), Observer->LastType, EResourceType::Crystal);
	TestEqual(TEXT("Event count"), Observer->LastCount, 1);
	TestEqual(TEXT("Event delta"), Observer->LastDelta, 1);

	// None and non-positive amounts change nothing and say nothing.
	Ledger->Add(EResourceType::None, 5);
	Ledger->Add(EResourceType::Wood, 0);
	Ledger->Add(EResourceType::Wood, -2);
	TestEqual(TEXT("None not counted"), Ledger->GetCount(EResourceType::None), 0);
	TestEqual(TEXT("Wood unchanged"), Ledger->GetCount(EResourceType::Wood), 0);
	TestEqual(TEXT("No events for ignored adds"), Observer->Changes, 3);

	TestTrue(TEXT("No pickup, nothing collected"), !Ledger->CollectPickup(nullptr));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FResourceYieldTest, "GunsAndBalloons.Resources.YieldRoll",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FResourceYieldTest::RunTest(const FString& Parameters)
{
	// The ammo box in Hook_Loot_Grab: Ammo 1-3 and Money 1-10. Every roll stays in range and, over many
	// rolls, both ends come up (missing one in 500 rolls is vanishingly unlikely).
	auto CheckRange = [this](const TCHAR* Name, int32 Min, int32 Max)
	{
		FResourceYield Yield;
		Yield.MinAmount = Min;
		Yield.MaxAmount = Max;
		int32 Lowest = TNumericLimits<int32>::Max();
		int32 Highest = TNumericLimits<int32>::Lowest();
		for (int32 i = 0; i < 500; ++i)
		{
			const int32 Amount = Yield.RollAmount();
			Lowest = FMath::Min(Lowest, Amount);
			Highest = FMath::Max(Highest, Amount);
		}
		TestEqual(FString::Printf(TEXT("%s lowest roll"), Name), Lowest, Min);
		TestEqual(FString::Printf(TEXT("%s highest roll"), Name), Highest, Max);
	};
	CheckRange(TEXT("Ammo 1-3"), 1, 3);
	CheckRange(TEXT("Money 1-10"), 1, 10);

	// A fixed amount, and a range set the wrong way round (counts as the minimum).
	FResourceYield Fixed;
	Fixed.MinAmount = Fixed.MaxAmount = 1;
	TestEqual(TEXT("1-1 is always 1"), Fixed.RollAmount(), 1);
	FResourceYield Backwards;
	Backwards.MinAmount = 4;
	Backwards.MaxAmount = 2;
	TestEqual(TEXT("Max below Min gives Min"), Backwards.RollAmount(), 4);
	return true;
}
#endif
