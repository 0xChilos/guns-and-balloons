#include "../BalloonRunSubsystem.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/GameInstance.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBalloonRunShopTest, "GunsAndBalloons.Run.Shop", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBalloonRunShopTest::RunTest(const FString& Parameters)
{
	// Gold and extinguisher rules only: no world, so CompleteLevel / ContinueTravel are covered in PIE.
	UGameInstance* GameInstance = NewObject<UGameInstance>(GetTransientPackage());
	UBalloonRunSubsystem* Run = NewObject<UBalloonRunSubsystem>(GameInstance);
	if (!TestNotNull(TEXT("Run subsystem"), Run)) return false;

	Run->StartNewRun();
	TestTrue(TEXT("New Run starts a run"), Run->IsRunActive());
	TestEqual(TEXT("New Run: no gold"), Run->GetGold(), 0);
	TestFalse(TEXT("New Run: no extinguisher"), Run->HasExtinguisher());
	TestFalse(TEXT("Cannot refill an extinguisher that is not owned"), Run->RefillExtinguisher(1, 10.f));

	TestFalse(TEXT("Cannot buy with too little gold"), Run->BuyExtinguisher(10));
	Run->AddGold(-5);
	TestEqual(TEXT("Negative gold is ignored"), Run->GetGold(), 0);
	Run->AddGold(12);
	TestTrue(TEXT("Buys the extinguisher"), Run->BuyExtinguisher(10));
	TestEqual(TEXT("Price is paid"), Run->GetGold(), 2);
	TestTrue(TEXT("Extinguisher owned"), Run->HasExtinguisher());
	TestEqual(TEXT("Bought extinguisher is full"), Run->GetExtinguisherAmount(), UBalloonRunSubsystem::ExtinguisherFull);
	Run->AddGold(20);
	TestFalse(TEXT("Cannot buy a second extinguisher"), Run->BuyExtinguisher(10));
	TestFalse(TEXT("Cannot refill a full extinguisher"), Run->RefillExtinguisher(1, 10.f));
	TestEqual(TEXT("Failed purchases cost nothing"), Run->GetGold(), 22);

	Run->StartNewRun();
	TestEqual(TEXT("A second New Run clears gold"), Run->GetGold(), 0);
	TestFalse(TEXT("A second New Run clears the extinguisher"), Run->HasExtinguisher());
	TestEqual(TEXT("A second New Run empties the extinguisher"), Run->GetExtinguisherAmount(), 0.f);
	TestFalse(TEXT("Shop is closed"), Run->IsShopOpen());
	return true;
}
#endif
