#include "WrenchComponent.h"

#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Texture.h"
#include "GunAnimInstance.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	/** Steps (s) the Fix clip is sampled at to find its rest poses. */
	constexpr float FixRestSampleStep = 1.f / 60.f;
}

UWrenchComponent::UWrenchComponent()
{
	// Imported by Scripts/Editor/import_wrench.py: every take of Wrench.fbx is bound to SK_Wrench_Skeleton.
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> Equip(TEXT("/Game/Animations/Wrench/SK_Wrench_Equip.SK_Wrench_Equip"));
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> Idle(TEXT("/Game/Animations/Wrench/SK_Wrench_Idle.SK_Wrench_Idle"));
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> ToFix(TEXT("/Game/Animations/Wrench/SK_Wrench_ToFix.SK_Wrench_ToFix"));
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> Fix(TEXT("/Game/Animations/Wrench/SK_Wrench_Fix.SK_Wrench_Fix"));
	static ConstructorHelpers::FObjectFinder<UTexture> Texture(TEXT("/Game/Textures/Wrench/T_Wrench.T_Wrench"));

	EquipAnim = Equip.Object;
	IdleAnim = Idle.Object;
	ToFixAnim = ToFix.Object;
	FixAnim = Fix.Object;
	WeaponTexture = Texture.Object;

	// Nothing comes out of a wrench.
	SmokeClass = nullptr;
	CasingMesh = nullptr;

	// The clips place the wrench themselves (its bone moves a few cm about the component); this is where that
	// origin sits in front of the eyes: low on the right. First-pass pose, needs tuning in game.
	ViewOffset = FVector(30.f, 14.f, -12.f);
	ViewRotation = FRotator(0.f, -4.f, 0.f);
	ReloadViewOffset = ViewOffset;
	ReloadViewRotation = ViewRotation;
	// A tool, not a gun: nothing to aim down.
	bCanAimDownSights = false;

	// No kick; a light tool, it sways a little in the hand.
	RecoilKickBack = 0.f;
	RecoilKickPitch = 0.f;
	HandSwayPitch = 0.4f;
	HandSwayYaw = 0.5f;
	HandSwaySpeed = 0.6f;
}

void UWrenchComponent::BeginPlay()
{
	Super::BeginPlay();

	FindFixRestTimes();
	// The pawn equips (Equip clip) or holsters it right after this.
	SetAction(EWrenchAction::Idle, IdleAnim);
	UpdatePose();
}

void UWrenchComponent::FindFixRestTimes()
{
	FixRestTimes.Reset();
	const float Length = AnimLength(FixAnim);
	const UAnimSequence* Sequence = Cast<UAnimSequence>(FixAnim);
	const USkeleton* Skeleton = Sequence ? Sequence->GetSkeleton() : nullptr;
	const int32 BoneIndex = Skeleton ? Skeleton->GetReferenceSkeleton().FindBoneIndex(WrenchBone) : INDEX_NONE;
	if (BoneIndex != INDEX_NONE)
	{
		auto Sample = [Sequence, BoneIndex](float Time)
		{
			FTransform Transform;
			Sequence->GetBoneTransform(Transform, FSkeletonPoseBoneIndex(BoneIndex), FAnimExtractContext(double(Time)), /*bUseRawData=*/false);
			return Transform;
		};
		const FTransform Rest = Sample(0.f);
		for (float Time = 0.f; Time < Length; Time += FixRestSampleStep)
		{
			const FTransform Pose = Sample(Time);
			if (FMath::RadiansToDegrees(Rest.GetRotation().AngularDistance(Pose.GetRotation())) <= FixRestTolerance
				&& FVector::Dist(Rest.GetLocation(), Pose.GetLocation()) <= FixRestTolerance)
			{
				FixRestTimes.Add(Time);
			}
		}
	}
	// The clip loops back to its first frame, so its end is always a rest.
	FixRestTimes.Add(Length);
}

float UWrenchComponent::GetNextFixRestTime(float Time) const
{
	for (const float RestTime : FixRestTimes)
	{
		// At rest right now (within a sample of a rest), or the next one ahead.
		if (RestTime >= Time - FixRestSampleStep * 0.5f)
		{
			return FMath::Max(RestTime, Time);
		}
	}
	return AnimLength(FixAnim);
}

// --- Input -----------------------------------------------------------------------------------

void UWrenchComponent::SetTriggerHeld(bool bHeld)
{
	bTriggerHeld = bHeld;
}

void UWrenchComponent::SetEquipped(bool bInEquipped)
{
	const bool bWasFixing = IsFixing() || Action == EWrenchAction::FinishTurn;
	Super::SetEquipped(bInEquipped);

	bTriggerHeld = false;
	if (bInEquipped)
	{
		// Brings itself up into view.
		bHolstering = false;
		SetAction(EWrenchAction::Equip, EquipAnim);
	}
	else
	{
		// Put away: rests at the start of Equip, out of view.
		SetAction(EWrenchAction::Unequip, EquipAnim, 0.f);
		if (bWasFixing)
		{
			OnFixStopped.Broadcast();
		}
	}
	UpdatePose();
}

void UWrenchComponent::PrepareToHolster()
{
	// Let go; once it is back in the ready pose the wrench takes itself out of view (Tick).
	bHolstering = true;
	bTriggerHeld = false;
}

void UWrenchComponent::CancelHolster()
{
	bHolstering = false;
	if (Action == EWrenchAction::Unequip)
	{
		// Come back up from as far down as it got: Equip forward from where the backwards play is.
		SetAction(EWrenchAction::Equip, EquipAnim, ActionTime);
	}
}

bool UWrenchComponent::IsReadyToHolster() const
{
	return Action == EWrenchAction::Unequip && ActionTime <= 0.f;
}

// --- Tick ------------------------------------------------------------------------------------

void UWrenchComponent::SetAction(EWrenchAction NewAction, UAnimSequenceBase* Anim, float Time)
{
	Action = NewAction;
	ActionAnim = Anim;
	ActionTime = Time;
}

bool UWrenchComponent::IsActionDone() const
{
	return ActionTime >= AnimLength(ActionAnim);
}

void UWrenchComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const bool bWantFix = bTriggerHeld && !bHolstering;
	const bool bBackwards = Action == EWrenchAction::FromFix || Action == EWrenchAction::Unequip;
	ActionTime = bBackwards ? FMath::Max(0.f, ActionTime - DeltaTime) : ActionTime + DeltaTime;

	switch (Action)
	{
	case EWrenchAction::Equip:
		if (IsActionDone())
		{
			SetAction(EWrenchAction::Idle, IdleAnim);
		}
		break;

	case EWrenchAction::Idle:
		if (bHolstering)
		{
			// Equip backwards, from its end (the ready pose).
			SetAction(EWrenchAction::Unequip, EquipAnim, AnimLength(EquipAnim));
		}
		else if (bWantFix)
		{
			SetAction(EWrenchAction::ToFix, ToFixAnim);
		}
		break;

	case EWrenchAction::ToFix:
		if (!bWantFix)
		{
			// Back off the work from as far as it got.
			SetAction(EWrenchAction::FromFix, ToFixAnim, FMath::Min(ActionTime, AnimLength(ToFixAnim)));
		}
		else if (IsActionDone())
		{
			SetAction(EWrenchAction::Fix, FixAnim);
			OnFixStarted.Broadcast();
		}
		break;

	case EWrenchAction::Fix:
	{
		const float Length = AnimLength(FixAnim);
		if (Length > 0.f && ActionTime >= Length)
		{
			ActionTime = FMath::Fmod(ActionTime, Length);
		}
		if (!bWantFix)
		{
			// Finish the turn under way, so it comes off the work from its rest pose.
			if (FixRestTimes.IsEmpty())
			{
				FindFixRestTimes();
			}
			FinishTime = GetNextFixRestTime(ActionTime);
			Action = EWrenchAction::FinishTurn;
			OnFixStopped.Broadcast();
		}
		break;
	}

	case EWrenchAction::FinishTurn:
		if (bWantFix)
		{
			Action = EWrenchAction::Fix;
			OnFixStarted.Broadcast();
		}
		else if (ActionTime >= FinishTime)
		{
			SetAction(EWrenchAction::FromFix, ToFixAnim, AnimLength(ToFixAnim));
		}
		break;

	case EWrenchAction::FromFix:
		if (bWantFix)
		{
			// Straight back onto the work from where it is.
			SetAction(EWrenchAction::ToFix, ToFixAnim, ActionTime);
		}
		else if (ActionTime <= 0.f)
		{
			SetAction(EWrenchAction::Idle, IdleAnim);
		}
		break;

	case EWrenchAction::Unequip:
		// Holds its last frame (Equip's first) until the pawn swaps (or the swap is called off: CancelHolster).
		break;
	}

	UpdatePose();
}

// --- Pose ------------------------------------------------------------------------------------

void UWrenchComponent::UpdatePose()
{
	UGunAnimInstance* Anim = Mesh ? Cast<UGunAnimInstance>(Mesh->GetAnimInstance()) : nullptr;
	if (!Anim)
	{
		return;
	}

	Anim->Sequence = ActionAnim ? ActionAnim.Get() : IdleAnim.Get();
	float Time = FMath::Max(0.f, ActionTime);
	if (Action == EWrenchAction::Idle)
	{
		const float Length = AnimLength(IdleAnim);
		Time = Length > 0.f ? FMath::Fmod(ActionTime, Length) : 0.f;
	}
	else if (Action == EWrenchAction::FinishTurn)
	{
		Time = FMath::Min(ActionTime, FinishTime);
	}
	Anim->Time = Time;
}
