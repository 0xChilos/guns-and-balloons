#include "../BalloonRunSubsystem.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/GameInstance.h"
#include "../HookComponent.h"
#include "../RevolverComponent.h"
#include "../RifleComponent.h"
#include "../ShotgunComponent.h"

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

	// Gear: bought once. A run starts with none.
	TestFalse(TEXT("New Run: no hook"), Run->OwnsGear(UHookComponent::StaticClass()));
	TestFalse(TEXT("New Run: no rifle"), Run->OwnsGear(URifleComponent::StaticClass()));
	TestFalse(TEXT("New Run: no shotgun"), Run->OwnsGear(UShotgunComponent::StaticClass()));
	TestTrue(TEXT("Buys the hook"), Run->BuyGear(UHookComponent::StaticClass(), 10));
	TestTrue(TEXT("Hook owned"), Run->OwnsGear(UHookComponent::StaticClass()));
	TestEqual(TEXT("Hook price is paid"), Run->GetGold(), 12);
	TestFalse(TEXT("Cannot buy the hook twice"), Run->BuyGear(UHookComponent::StaticClass(), 0));
	TestFalse(TEXT("Cannot buy gear that is not sold"), Run->BuyGear(URevolverComponent::StaticClass(), 1));
	Run->AddGold(10);
	TestTrue(TEXT("Buys the shotgun"), Run->BuyGear(UShotgunComponent::StaticClass(), 20));
	TestTrue(TEXT("Shotgun owned"), Run->OwnsGear(UShotgunComponent::StaticClass()));
	TestEqual(TEXT("Shotgun price is paid"), Run->GetGold(), 2);
	TestFalse(TEXT("Cannot buy the rifle with too little gold"), Run->BuyGear(URifleComponent::StaticClass(), 30));
	TestFalse(TEXT("Cannot buy the shotgun twice"), Run->BuyGear(UShotgunComponent::StaticClass(), 0));

	// Supplies: New Run starts with 4 of each fuel and 40 rounds.
	TestEqual(TEXT("New Run: 4 wood"), Run->GetSupply(EResourceType::Wood), 4);
	TestEqual(TEXT("New Run: 4 coal"), Run->GetSupply(EResourceType::Coal), 4);
	TestEqual(TEXT("New Run: 4 crystals"), Run->GetSupply(EResourceType::Crystal), 4);
	TestEqual(TEXT("New Run: 40 rounds"), Run->GetSupply(EResourceType::Ammo), 40);
	TestTrue(TEXT("Buys a wood"), Run->BuySupply(EResourceType::Wood, 1, 1));
	TestTrue(TEXT("Buys another wood"), Run->BuySupply(EResourceType::Wood, 1, 1));
	TestEqual(TEXT("Wood is full"), Run->GetSupply(EResourceType::Wood), 6);
	TestEqual(TEXT("Supply price is paid"), Run->GetGold(), 0);
	Run->AddGold(5);
	TestFalse(TEXT("Cannot buy a full supply"), Run->BuySupply(EResourceType::Wood, 1, 1));
	TestFalse(TEXT("Money is not a supply"), Run->BuySupply(EResourceType::Money, 1, 1));
	TestEqual(TEXT("Fuel max"), UBalloonRunSubsystem::GetSupplyMax(EResourceType::Coal), 6);
	TestEqual(TEXT("Ammo max"), UBalloonRunSubsystem::GetSupplyMax(EResourceType::Ammo), 60);
	TestEqual(TEXT("Failed supply purchases cost nothing"), Run->GetGold(), 5);

	Run->StartNewRun();
	TestFalse(TEXT("A second New Run clears bought gear"), Run->OwnsGear(UShotgunComponent::StaticClass()));
	TestFalse(TEXT("A second New Run clears the hook"), Run->OwnsGear(UHookComponent::StaticClass()));
	TestEqual(TEXT("A second New Run resets wood"), Run->GetSupply(EResourceType::Wood), 4);
	TestEqual(TEXT("A second New Run clears gold"), Run->GetGold(), 0);
	TestFalse(TEXT("A second New Run clears the extinguisher"), Run->HasExtinguisher());
	TestEqual(TEXT("A second New Run empties the extinguisher"), Run->GetExtinguisherAmount(), 0.f);
	TestFalse(TEXT("Shop is closed"), Run->IsShopOpen());
	return true;
}
#endif
