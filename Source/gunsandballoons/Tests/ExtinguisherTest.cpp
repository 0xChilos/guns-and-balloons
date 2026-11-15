#include "../ExtinguisherComponent.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FExtinguisherSprayTest, "GunsAndBalloons.Extinguisher.SprayAndCounter",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FExtinguisherSprayTest::RunTest(const FString& Parameters)
{
	// --- The counter's drums roll like an odometer.
	TestEqual(TEXT("73 -> 0 7 3"), UExtinguisherComponent::GetDrumDigits(73.f), FVector(3.f, 7.f, 0.f));
	TestEqual(TEXT("100 -> 1 0 0"), UExtinguisherComponent::GetDrumDigits(100.f), FVector(0.f, 0.f, 1.f));
	TestEqual(TEXT("0 -> 0 0 0"), UExtinguisherComponent::GetDrumDigits(0.f), FVector::ZeroVector);
	TestTrue(TEXT("99.5: every drum half way to the next digit"), UExtinguisherComponent::GetDrumDigits(99.5f).Equals(FVector(9.5f, 9.5f, 0.5f), 1.e-3f));
	TestTrue(TEXT("95.5: only the ones drum moves"), UExtinguisherComponent::GetDrumDigits(95.5f).Equals(FVector(5.5f, 9.f, 0.f), 1.e-3f));

	// --- The drum axis and direction match the artist's reference poses: Ref_000 turned 36 degrees per
	// digit about the drum's own +Y gives Ref_073 (tens 7, ones 3) and Ref_100 (hundreds 1).
	{
		const TCHAR* Root = TEXT("/Game/Assets/Fire_extinguisher/SK_ExtinguisherSK_Extinguisher_Counter_Ref_");
		UAnimSequence* Zero = LoadObject<UAnimSequence>(nullptr, *FString::Printf(TEXT("%s000.SK_ExtinguisherSK_Extinguisher_Counter_Ref_000"), Root));
		UAnimSequence* Ref073 = LoadObject<UAnimSequence>(nullptr, *FString::Printf(TEXT("%s073.SK_ExtinguisherSK_Extinguisher_Counter_Ref_073"), Root));
		UAnimSequence* Ref100 = LoadObject<UAnimSequence>(nullptr, *FString::Printf(TEXT("%s100.SK_ExtinguisherSK_Extinguisher_Counter_Ref_100"), Root));
		if (TestNotNull(TEXT("Counter_Ref_000"), Zero) && TestNotNull(TEXT("Counter_Ref_073"), Ref073) && TestNotNull(TEXT("Counter_Ref_100"), Ref100)
			&& TestNotNull(TEXT("Skeleton"), Zero->GetSkeleton()))
		{
			const FReferenceSkeleton& RefSkeleton = Zero->GetSkeleton()->GetReferenceSkeleton();
			auto Rotation = [&RefSkeleton](const UAnimSequence* Anim, const TCHAR* Bone)
			{
				FTransform Transform;
				Anim->GetBoneTransform(Transform, FSkeletonPoseBoneIndex(RefSkeleton.FindBoneIndex(Bone)), FAnimExtractContext(0.0), /*bUseRawData=*/true);
				return Transform.GetRotation();
			};
			struct FCheck { const UAnimSequence* Anim; const TCHAR* Bone; float Digit; };
			const FCheck Checks[] = {
				{ Ref073, TEXT("digit_1"), 3.f }, { Ref073, TEXT("digit_10"), 7.f }, { Ref073, TEXT("digit_100"), 0.f },
				{ Ref100, TEXT("digit_1"), 0.f }, { Ref100, TEXT("digit_10"), 0.f }, { Ref100, TEXT("digit_100"), 1.f },
			};
			for (const FCheck& Check : Checks)
			{
				if (!TestTrue(FString::Printf(TEXT("Bone %s exists"), Check.Bone), RefSkeleton.FindBoneIndex(Check.Bone) != INDEX_NONE))
				{
					continue;
				}
				const FQuat Expected = Rotation(Zero, Check.Bone) * FQuat(FVector::YAxisVector, FMath::DegreesToRadians(36.f * Check.Digit));
				const float ErrorDegrees = FMath::RadiansToDegrees(Expected.AngularDistance(Rotation(Check.Anim, Check.Bone)));
				TestTrue(FString::Printf(TEXT("%s %s at %.0f: off by %.2f deg"), *Check.Anim->GetName(), Check.Bone, Check.Digit, ErrorDegrees), ErrorDegrees < 0.5f);
			}
		}
	}

	// --- Spraying uses 5 a second while LMB is held; empty, it only sputters.
	UWorld* World = GWorld;
	if (!TestNotNull(TEXT("Editor world available"), World)) return false;
	AActor* Owner = World->SpawnActor<AActor>();
	if (!Owner) return false;
	ON_SCOPE_EXIT { World->DestroyActor(Owner); };

	UExtinguisherComponent* Extinguisher = NewObject<UExtinguisherComponent>(Owner);
	Extinguisher->RegisterComponent();
	ON_SCOPE_EXIT { Extinguisher->DestroyComponent(); };

	auto Tick = [Extinguisher](float Seconds)
	{
		for (float T = 0.f; T < Seconds - 1.e-4f; T += 1.f / 60.f)
		{
			Extinguisher->TickComponent(1.f / 60.f, LEVELTICK_All, nullptr);
		}
	};

	// BeginPlay does not run here: start full by hand.
	Extinguisher->SetAmount(100.f);
	Tick(0.1f);
	TestFalse(TEXT("Not spraying without LMB"), Extinguisher->IsSpraying());
	TestEqual(TEXT("Nothing used without LMB"), Extinguisher->GetAmount(), 100.f);

	Extinguisher->PullTrigger();
	Extinguisher->SetTriggerHeld(true);
	Tick(1.f);
	TestTrue(TEXT("Spraying while held"), Extinguisher->IsSpraying());
	TestTrue(FString::Printf(TEXT("One second uses 5 (left %.3f)"), Extinguisher->GetAmount()), FMath::IsNearlyEqual(Extinguisher->GetAmount(), 95.f, 0.1f));

	Extinguisher->SetTriggerHeld(false);
	Tick(0.5f);
	TestFalse(TEXT("Released: stops"), Extinguisher->IsSpraying());
	const float AfterRelease = Extinguisher->GetAmount();
	Tick(1.f);
	TestEqual(TEXT("Nothing used after release"), Extinguisher->GetAmount(), AfterRelease);
	TestEqual(TEXT("Counter settles on the whole number left"), Extinguisher->GetDisplayedCount(), FMath::CeilToFloat(AfterRelease - 1.e-3f));

	// Running dry mid-spray: down to 0, the Empty clip, no more foam.
	Extinguisher->SetAmount(1.f);
	Extinguisher->PullTrigger();
	Extinguisher->SetTriggerHeld(true);
	Tick(0.25f);
	TestEqual(TEXT("Ran dry"), Extinguisher->GetAmount(), 0.f);
	TestFalse(TEXT("No spray when empty"), Extinguisher->IsSpraying());
	TestEqual(TEXT("Sputters when it runs dry"), Extinguisher->GetAction(), EExtinguisherAction::Empty);
	Tick(1.f);
	TestFalse(TEXT("Holding on empty does nothing"), Extinguisher->IsSpraying());
	Extinguisher->SetTriggerHeld(false);

	// A click on empty sputters again.
	Tick(0.1f);
	Extinguisher->PullTrigger();
	TestEqual(TEXT("Click on empty: Empty clip"), Extinguisher->GetAction(), EExtinguisherAction::Empty);

	// Putting it away waits for the Unequip clip.
	Tick(1.f);
	Extinguisher->PrepareToHolster();
	Tick(1.f / 60.f);
	TestEqual(TEXT("Holstering: Unequip"), Extinguisher->GetAction(), EExtinguisherAction::Unequip);
	TestFalse(TEXT("Not ready mid-Unequip"), Extinguisher->IsReadyToHolster());
	Tick(1.f);
	TestTrue(TEXT("Ready after Unequip"), Extinguisher->IsReadyToHolster());
	return true;
}
#endif
