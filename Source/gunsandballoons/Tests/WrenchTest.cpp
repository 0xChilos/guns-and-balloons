#include "../WrenchComponent.h"
#include "../StoveComponent.h"
#include "../HealthComponent.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Components/BoxComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Camera/CameraComponent.h"
#include "EngineUtils.h"
#include "../FirstPersonCameraPawn.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWrenchFixTest, "GunsAndBalloons.Wrench.FixCycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWrenchFixTest::RunTest(const FString& Parameters)
{
	UWorld* World = GWorld;
	if (!TestNotNull(TEXT("Editor world available"), World)) return false;
	AActor* Owner = World->SpawnActor<AActor>();
	if (!Owner) return false;
	ON_SCOPE_EXIT { World->DestroyActor(Owner); };

	UWrenchComponent* Wrench = NewObject<UWrenchComponent>(Owner);
	Wrench->RegisterComponent();
	ON_SCOPE_EXIT { Wrench->DestroyComponent(); };

	auto Tick = [Wrench](float Seconds)
	{
		for (float T = 0.f; T < Seconds - 1.e-4f; T += 1.f / 60.f)
		{
			Wrench->TickComponent(1.f / 60.f, LEVELTICK_All, nullptr);
		}
	};

	// --- The Fix clip's rest poses (SK_Wrench_Fix: four turns, back at rest at 1.25, 2.25, 3.25, 4.25 s).
	Wrench->FindFixRestTimes();
	TestTrue(TEXT("In the opening hold: at rest now"), FMath::IsNearlyEqual(Wrench->GetNextFixRestTime(0.1f), 0.1f, 0.02f));
	TestTrue(FString::Printf(TEXT("Mid first turn: rest at 1.25 (got %.2f)"), Wrench->GetNextFixRestTime(0.5f)),
		FMath::IsNearlyEqual(Wrench->GetNextFixRestTime(0.5f), 1.25f, 0.05f));
	TestTrue(FString::Printf(TEXT("Mid third turn: rest at 3.25 (got %.2f)"), Wrench->GetNextFixRestTime(2.6f)),
		FMath::IsNearlyEqual(Wrench->GetNextFixRestTime(2.6f), 3.25f, 0.05f));

	// --- LMB held: onto the work (ToFix 0.67 s), then turning.
	TestEqual(TEXT("Starts ready"), Wrench->GetAction(), EWrenchAction::Idle);
	TestEqual(TEXT("Ready: off the crosshair"), Wrench->GetCrosshairPin(), 0.f);
	Wrench->SetTriggerHeld(true);
	Tick(0.1f);
	TestEqual(TEXT("Held: ToFix"), Wrench->GetAction(), EWrenchAction::ToFix);
	const float PinDuringToFix = Wrench->GetCrosshairPin();
	TestTrue(FString::Printf(TEXT("ToFix: on its way to the crosshair (pin %.2f)"), PinDuringToFix), PinDuringToFix > 0.f && PinDuringToFix < 1.f);
	Tick(0.7f);
	TestTrue(TEXT("Held past ToFix: turning"), Wrench->IsFixing());
	TestEqual(TEXT("Turning: on the crosshair"), Wrench->GetCrosshairPin(), 1.f);

	// Let go part-way into the first turn: it finishes the turn (to 1.25 s in Fix), then comes off the work.
	Tick(0.3f);
	Wrench->SetTriggerHeld(false);
	Tick(1.f / 60.f);
	TestEqual(TEXT("Released mid-turn: finishes the turn"), Wrench->GetAction(), EWrenchAction::FinishTurn);
	TestEqual(TEXT("Finishing the turn: still on the crosshair"), Wrench->GetCrosshairPin(), 1.f);
	Tick(0.4f);
	TestEqual(TEXT("Still finishing"), Wrench->GetAction(), EWrenchAction::FinishTurn);
	Tick(0.6f);
	TestEqual(TEXT("Turn done: off the work"), Wrench->GetAction(), EWrenchAction::FromFix);
	Tick(0.75f);
	TestEqual(TEXT("Back to ready"), Wrench->GetAction(), EWrenchAction::Idle);
	TestEqual(TEXT("Back to ready: off the crosshair"), Wrench->GetCrosshairPin(), 0.f);

	// Let go during ToFix: straight back.
	Wrench->SetTriggerHeld(true);
	Tick(0.2f);
	Wrench->SetTriggerHeld(false);
	Tick(1.f / 60.f);
	TestEqual(TEXT("Released during ToFix: back off"), Wrench->GetAction(), EWrenchAction::FromFix);
	Tick(0.3f);
	TestEqual(TEXT("Backed off as far as it got"), Wrench->GetAction(), EWrenchAction::Idle);

	// --- Putting it away plays Equip backwards; calling it off brings it back up.
	Wrench->PrepareToHolster();
	Tick(1.f / 60.f);
	TestEqual(TEXT("Holstering: Unequip"), Wrench->GetAction(), EWrenchAction::Unequip);
	TestFalse(TEXT("Not ready mid-Unequip"), Wrench->IsReadyToHolster());
	Wrench->CancelHolster();
	TestEqual(TEXT("Called off: Equip"), Wrench->GetAction(), EWrenchAction::Equip);
	Tick(1.1f);
	TestEqual(TEXT("Up again: ready"), Wrench->GetAction(), EWrenchAction::Idle);
	Wrench->PrepareToHolster();
	Tick(1.1f);
	TestTrue(TEXT("Ready after Equip backwards"), Wrench->IsReadyToHolster());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWrenchStoveRepairTest, "GunsAndBalloons.Wrench.StoveRepair",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWrenchStoveRepairTest::RunTest(const FString& Parameters)
{
	UWorld* World = GWorld;
	if (!TestNotNull(TEXT("Editor world available"), World)) return false;
	AActor* Balloon = World->SpawnActor<AActor>();
	AActor* Player = World->SpawnActor<AActor>();
	AActor* Other = World->SpawnActor<AActor>();
	ON_SCOPE_EXIT
	{
		for (AActor* Actor : {Balloon, Player, Other}) if (Actor) World->DestroyActor(Actor);
	};
	if (!Balloon || !Player || !Other) return false;

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

	// The balloon: a stove (its firebox box 1.5 m ahead) sharing the balloon's health.
	const FVector Origin(100000.f, -100000.f, 100000.f);
	auto* Health = NewObject<UHealthComponent>(Balloon);
	Balloon->AddInstanceComponent(Health);
	Health->RegisterComponent();
	auto* Stove = NewObject<UStoveComponent>(Balloon);
	Balloon->AddInstanceComponent(Stove);
	Stove->RegisterComponent();
	Stove->FireboxShape = MakeBox(Balloon, Origin + FVector(150.f, 0.f, 0.f));
	MakeBox(Other, Origin + FVector(0.f, 150.f, 0.f));

	TestEqual(TEXT("Stove without health repairs nothing"), Stove->ApplyRepair(10.f), 0.f);
	Stove->Health = Health;
	Health->SetHealth(50.f);
	TestEqual(TEXT("Stove repair restores the shared health"), Stove->ApplyRepair(10.f), 10.f);
	TestEqual(TEXT("Health after the stove repair"), Health->GetHealth(), 60.f);
	TestEqual(TEXT("Negative repair ignored"), Stove->ApplyRepair(-5.f), 0.f);

	UWrenchComponent* Wrench = NewObject<UWrenchComponent>(Player);
	Player->AddInstanceComponent(Wrench);
	Wrench->RegisterComponent();
	ON_SCOPE_EXIT { Wrench->DestroyComponent(); };

	// LMB on the stove goes to the wrench (not the door); other things keep their interaction.
	const FHitResult StoveHit(Balloon, Stove->FireboxShape, Origin, FVector::ForwardVector);
	const FHitResult OtherHit(Other, Cast<UPrimitiveComponent>(Other->GetRootComponent()), Origin, FVector::ForwardVector);
	TestTrue(TEXT("LMB on the stove turns the wrench"), Wrench->ShouldUseInsteadOfInteract(StoveHit));
	TestFalse(TEXT("LMB elsewhere keeps the interaction"), Wrench->ShouldUseInsteadOfInteract(OtherHit));

	// Not on the work yet: no repair.
	TestEqual(TEXT("Idle wrench repairs nothing"), Wrench->ApplyTurnRepair(Origin, FVector::ForwardVector), 0.f);

	// The Fix clip's four ratchet turns come back to rest at ~1.25, 2.25, 3.25, 4.25 s (BeginPlay finds them).
	Wrench->FindFixRestTimes();
	const TArray<float>& TurnEnds = Wrench->GetFixTurnEndTimes();
	FString Ends;
	for (const float T : TurnEnds) Ends += FString::Printf(TEXT(" %.2f"), T);
	TestEqual(FString::Printf(TEXT("Four turns per Fix loop (ends:%s)"), *Ends), TurnEnds.Num(), 4);
	if (TurnEnds.Num() == 4)
	{
		TestTrue(TEXT("First turn ends at 1.25 s"), FMath::IsNearlyEqual(TurnEnds[0], 1.25f, 0.05f));
		TestTrue(TEXT("Last turn ends at 4.25 s"), FMath::IsNearlyEqual(TurnEnds[3], 4.25f, 0.05f));
	}

	// LMB held past ToFix (0.67 s): turning. No camera here, so the tick itself doesn't repair.
	Wrench->SetTriggerHeld(true);
	for (int32 i = 0; i < 60; ++i) Wrench->TickComponent(1.f / 60.f, LEVELTICK_All, nullptr);
	if (!TestTrue(TEXT("Turning"), Wrench->IsFixing())) return false;
	TestEqual(TEXT("Tick without a camera repairs nothing"), Health->GetHealth(), 60.f);

	// A turn on the stove closes one hole: 10 HP.
	const float PerTurn = Wrench->RepairPerTurn;
	TestEqual(TEXT("A turn on the stove repairs RepairPerTurn"), Wrench->ApplyTurnRepair(Origin, FVector::ForwardVector), PerTurn);
	TestEqual(TEXT("Health after a turn"), Health->GetHealth(), 60.f + PerTurn);
	TestEqual(TEXT("A turn on something else repairs nothing"), Wrench->ApplyTurnRepair(Origin, FVector::RightVector), 0.f);
	TestEqual(TEXT("A turn at nothing repairs nothing"), Wrench->ApplyTurnRepair(Origin, FVector::UpVector), 0.f);

	const float Range = Wrench->RepairRange;
	Wrench->RepairRange = 100.f;
	TestEqual(TEXT("Stove out of reach: nothing"), Wrench->ApplyTurnRepair(Origin, FVector::ForwardVector), 0.f);
	Wrench->RepairRange = Range;

	// Repairs up to full and no further (the last hole closes at 100 HP).
	Health->SetHealth(Health->MaxHealth - 2.f);
	TestEqual(TEXT("Repair stops at full health"), Wrench->ApplyTurnRepair(Origin, FVector::ForwardVector), 2.f);
	TestEqual(TEXT("At full health"), Health->GetHealth(), Health->MaxHealth);
	TestEqual(TEXT("Nothing to repair at full health"), Wrench->ApplyTurnRepair(Origin, FVector::ForwardVector), 0.f);

	// Let go: the turn under way still finishes on the work; once off it, nothing.
	Wrench->SetTriggerHeld(false);
	Wrench->TickComponent(1.f / 60.f, LEVELTICK_All, nullptr);
	TestFalse(TEXT("Released: not turning"), Wrench->IsFixing());
	for (int32 i = 0; i < 75; ++i) Wrench->TickComponent(1.f / 60.f, LEVELTICK_All, nullptr);
	TestEqual(TEXT("Off the work"), Wrench->GetAction(), EWrenchAction::FromFix);
	Health->SetHealth(50.f);
	TestEqual(TEXT("Off the work: no repair"), Wrench->ApplyTurnRepair(Origin, FVector::ForwardVector), 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWrenchBalloonStoveTargetTest, "GunsAndBalloons.Wrench.BalloonStoveTarget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/** On the real BP_Balloon: aimed from the basket at the stove's bullet holes, the wrench's trace lands on a repair target. */
bool FWrenchBalloonStoveTargetTest::RunTest(const FString& Parameters)
{
	UWorld* World = GWorld;
	if (!TestNotNull(TEXT("Editor world available"), World)) return false;
	UClass* BalloonClass = LoadClass<AActor>(nullptr, TEXT("/Game/Assets/Balloon/BP_Balloon.BP_Balloon_C"));
	if (!TestNotNull(TEXT("BP_Balloon loads"), BalloonClass)) return false;
	FActorSpawnParameters Spawn;
	Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AActor* Balloon = World->SpawnActor<AActor>(BalloonClass, FVector(0.f, 200000.f, 100000.f), FRotator::ZeroRotator, Spawn);
	if (!TestNotNull(TEXT("BP_Balloon spawns"), Balloon)) return false;
	ON_SCOPE_EXIT { World->DestroyActor(Balloon); };

	UStoveComponent* Stove = Balloon->FindComponentByClass<UStoveComponent>();
	USkeletalMeshComponent* Mesh = Balloon->FindComponentByClass<USkeletalMeshComponent>();
	if (!TestNotNull(TEXT("Stove"), Stove) || !TestNotNull(TEXT("Mesh"), Mesh)) return false;

	// Eye height in the middle of the basket (the pawn stands there).
	const FVector Eye = Mesh->GetComponentTransform().TransformPosition(FVector(0.f, 0.f, 165.f));
	FCollisionQueryParams Query(SCENE_QUERY_STAT(WrenchBalloonTarget), false);
	TArray<FName> Targets = {TEXT("stove"), TEXT("furnace_door")};
	for (int32 i = 1; i <= 10; ++i) Targets.Add(*FString::Printf(TEXT("stove_damage_%d"), i));
	for (const FName& Bone : Targets)
	{
		if (Mesh->GetBoneIndex(Bone) == INDEX_NONE)
		{
			AddWarning(FString::Printf(TEXT("%s: no such bone"), *Bone.ToString()));
			continue;
		}
		const FVector Aim = Mesh->GetSocketLocation(Bone);
		FHitResult Hit;
		const bool bHit = World->LineTraceSingleByChannel(Hit, Eye, Eye + (Aim - Eye) * 1.5f, ECC_Visibility, Query);
		const FString What = FString::Printf(TEXT("%s (%.0f cm away): first hit %s, bone %s at %.0f cm"), *Bone.ToString(),
			FVector::Dist(Eye, Aim), bHit && Hit.GetComponent() ? *Hit.GetComponent()->GetName() : TEXT("nothing"),
			*Hit.BoneName.ToString(), Hit.Distance);
		AddInfo(What);
		TestTrue(What + TEXT(" is a repair target"), bHit && Stove->AcceptsRepairHit(Hit));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWrenchBalloonHolesTest, "GunsAndBalloons.Wrench.BalloonHoles",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/** On the real BP_Balloon in a playing world: damage opens bullet holes, repairing through the stove closes them. */
bool FWrenchBalloonHolesTest::RunTest(const FString& Parameters)
{
	UClass* BalloonClass = LoadClass<AActor>(nullptr, TEXT("/Game/Assets/Balloon/BP_Balloon.BP_Balloon_C"));
	if (!TestNotNull(TEXT("BP_Balloon loads"), BalloonClass)) return false;

	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
	Context.SetCurrentWorld(World);
	ON_SCOPE_EXIT
	{
		World->DestroyWorld(false);
		GEngine->DestroyWorldContext(World);
	};
	World->InitializeActorsForPlay(FURL());
	World->BeginPlay();
	// No game mode here to start play: begin it directly, so spawned actors run BeginPlay.
	World->GetWorldSettings()->NotifyBeginPlay();

	AActor* Balloon = World->SpawnActor<AActor>(BalloonClass, FTransform::Identity);
	if (!TestNotNull(TEXT("BP_Balloon spawns"), Balloon)) return false;
	TestTrue(TEXT("BP_Balloon has begun play"), Balloon->HasActorBegunPlay());
	UStoveComponent* Stove = Balloon->FindComponentByClass<UStoveComponent>();
	UHealthComponent* Health = Balloon->FindComponentByClass<UHealthComponent>();
	USkeletalMeshComponent* Mesh = Balloon->FindComponentByClass<USkeletalMeshComponent>();
	if (!TestNotNull(TEXT("Stove"), Stove) || !TestNotNull(TEXT("Health"), Health) || !TestNotNull(TEXT("Mesh"), Mesh)) return false;
	TestTrue(TEXT("Stove repairs the balloon's Health"), Stove->Health == Health);

	auto VisibleHoles = [Mesh]()
	{
		int32 Count = 0;
		for (int32 i = 1; i <= 10; ++i)
		{
			const FName Bone(*FString::Printf(TEXT("stove_damage_%d"), i));
			if (Mesh->GetBoneIndex(Bone) != INDEX_NONE && !Mesh->IsBoneHiddenByName(Bone)) ++Count;
		}
		return Count;
	};
	AddInfo(FString::Printf(TEXT("Start: %.0f HP, %d holes visible"), Health->GetHealth(), VisibleHoles()));
	TestEqual(TEXT("Full health: no holes"), VisibleHoles(), 0);
	Health->ApplyDamage(35.f);
	AddInfo(FString::Printf(TEXT("After 35 damage: %.0f HP, %d holes visible"), Health->GetHealth(), VisibleHoles()));
	TestEqual(TEXT("65 HP: three holes"), VisibleHoles(), 3);
	Stove->ApplyRepair(20.f);
	AddInfo(FString::Printf(TEXT("After 20 repair: %.0f HP, %d holes visible"), Health->GetHealth(), VisibleHoles()));
	TestEqual(TEXT("85 HP: one hole"), VisibleHoles(), 1);
	Stove->ApplyRepair(100.f);
	TestEqual(TEXT("Repaired: no holes"), VisibleHoles(), 0);

	// End to end: the balloon's own player, wrench in hand, camera on a hole, LMB held.
	AFirstPersonCameraPawn* Player = nullptr;
	for (TActorIterator<AFirstPersonCameraPawn> It(World); It; ++It) Player = *It;
	if (!TestNotNull(TEXT("BP_Balloon carries a player pawn"), Player)) return true;
	UWrenchComponent* Wrench = Player->GetWrench();
	UCameraComponent* Camera = Player->GetCamera();
	if (!TestNotNull(TEXT("Wrench"), Wrench) || !TestNotNull(TEXT("Camera"), Camera)) return true;
	Health->ApplyDamage(35.f);
	const FVector Hole = Mesh->GetSocketLocation(TEXT("stove_damage_1"));
	Camera->SetWorldRotation((Hole - Camera->GetComponentLocation()).Rotation());
	FHitResult Hit;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(WrenchEndToEnd), false, Player);
	World->LineTraceSingleByChannel(Hit, Camera->GetComponentLocation(), Camera->GetComponentLocation() + Camera->GetForwardVector() * 400.f, ECC_Visibility, Query);
	AddInfo(FString::Printf(TEXT("Camera at %s, hole %.0f cm away; first hit %s / %s / bone %s at %.0f cm"),
		*Camera->GetComponentLocation().ToString(), FVector::Dist(Camera->GetComponentLocation(), Hole),
		Hit.GetActor() ? *Hit.GetActor()->GetName() : TEXT("none"), Hit.GetComponent() ? *Hit.GetComponent()->GetName() : TEXT("none"),
		*Hit.BoneName.ToString(), Hit.Distance));
	Wrench->SetEquipped(true);
	auto Tick = [Wrench](float Seconds)
	{
		for (float T = 0.f; T < Seconds - 1.e-4f; T += 1.f / 60.f) Wrench->TickComponent(1.f / 60.f, LEVELTICK_All, nullptr);
	};
	Tick(1.2f);
	const float Before = Health->GetHealth();
	Wrench->SetTriggerHeld(true);
	Tick(3.f);
	// ToFix 0.67 s, then 2.33 s of Fix: the turns ending at 1.25 and 2.25 s each close a hole.
	AddInfo(FString::Printf(TEXT("Wrench held 3 s: fixing %d, %.1f -> %.1f HP, %d holes"),
		Wrench->IsFixing(), Before, Health->GetHealth(), VisibleHoles()));
	TestEqual(TEXT("Held 3 s on the stove: two turns, 20 HP"), Health->GetHealth(), Before + 2.f * Wrench->RepairPerTurn);
	TestEqual(TEXT("Held 3 s on the stove: one hole left"), VisibleHoles(), 1);
	// Let go: the third turn finishes (at 3.25 s) and closes the last hole.
	Wrench->SetTriggerHeld(false);
	Tick(2.f);
	AddInfo(FString::Printf(TEXT("Released: %.1f HP, %d holes"), Health->GetHealth(), VisibleHoles()));
	TestEqual(TEXT("Finished turn after release repairs too"), Health->GetHealth(), Before + 3.f * Wrench->RepairPerTurn);
	TestEqual(TEXT("All holes closed"), VisibleHoles(), 0);
	return true;
}
#endif
