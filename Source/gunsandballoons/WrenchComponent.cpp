#include "WrenchComponent.h"

#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Texture.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GunAnimInstance.h"
#include "StoveComponent.h"
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

	// The clips place the wrench themselves (its bone moves up to ~20 cm about the component). The component's
	// origin is the work spot: the nut the jaw sits on in Fix, on a wall plane the wrench lies flat against.
	// The artist animated it seen face-on from the wall's open side (Blender: looking along +Y), which after
	// import looks along the component's -Y: yaw +90 turns that toward the camera, so the wrench's face and the
	// Fix swing are square to the view. At rest the work spot sits a little right of and below the centre, out
	// of the way; brought onto the work it slides onto the crosshair (GetCrosshairPin: only the depth, X, is
	// kept), so the jaw turns where the player aims. The handle reaches to the left.
	// TBD: first-pass pose, needs tuning in game.
	ViewOffset = FVector(28.f, 8.f, -4.f);
	ViewRotation = FRotator(0.f, 90.f, 0.f);
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

	// A turn ends where a rest starts after a swing: a gap in the rest samples (the opening hold isn't one).
	FixTurnEndTimes.Reset();
	for (int32 i = 1; i < FixRestTimes.Num(); ++i)
	{
		if (FixRestTimes[i] - FixRestTimes[i - 1] > FixRestSampleStep * 1.5f)
		{
			FixTurnEndTimes.Add(FixRestTimes[i]);
		}
	}
	// No clip data to find the turns in: the whole clip is one turn.
	if (FixTurnEndTimes.IsEmpty() && Length > 0.f)
	{
		FixTurnEndTimes.Add(Length);
	}
}

int32 UWrenchComponent::CountTurnsEnded(float From, float To) const
{
	const float Length = AnimLength(FixAnim);
	int32 Count = 0;
	if (Length <= 0.f || To <= From)
	{
		return 0;
	}
	// To can run past the clip's end within a tick (Fix wraps after this): count the next loop's turns too.
	for (float Loop = 0.f; Loop <= To; Loop += Length)
	{
		for (const float End : FixTurnEndTimes)
		{
			const float T = Loop + End;
			Count += (T > From && T <= To) ? 1 : 0;
		}
	}
	return Count;
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

// --- Repair ----------------------------------------------------------------------------------

namespace
{
	/** The stove on the hit actor that accepts this hit as a repair target, or null. */
	UStoveComponent* FindRepairableStove(const FHitResult& Hit)
	{
		AActor* HitActor = Hit.GetActor();
		if (!HitActor)
		{
			return nullptr;
		}
		TInlineComponentArray<UStoveComponent*> Stoves(HitActor);
		for (UStoveComponent* Stove : Stoves)
		{
			if (Stove->AcceptsRepairHit(Hit))
			{
				return Stove;
			}
		}
		return nullptr;
	}
}

bool UWrenchComponent::ShouldUseInsteadOfInteract(const FHitResult& Hit) const
{
	return FindRepairableStove(Hit) != nullptr;
}

float UWrenchComponent::ApplyTurnRepair(const FVector& Start, const FVector& Direction)
{
	const bool bOnWork = Action == EWrenchAction::Fix || Action == EWrenchAction::FinishTurn;
	if (!bOnWork || !GetWorld() || !FMath::IsFinite(RepairPerTurn) || RepairPerTurn <= 0.f
		|| !FMath::IsFinite(RepairRange) || RepairRange <= 0.f || Direction.IsNearlyZero())
	{
		return 0.f;
	}
	FHitResult Hit;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(WrenchRepair), false, GetOwner());
	if (!GetWorld()->LineTraceSingleByChannel(Hit, Start, Start + Direction.GetSafeNormal() * RepairRange,
		RepairTraceChannel, Query))
	{
		return 0.f;
	}
	UStoveComponent* Stove = FindRepairableStove(Hit);
	const float Restored = Stove ? Stove->ApplyRepair(RepairPerTurn, GetOwner()) : 0.f;
	if (Restored > 0.f)
	{
		OnTurnRepaired.Broadcast();
	}
	return Restored;
}

float UWrenchComponent::GetCrosshairPin() const
{
	switch (Action)
	{
	case EWrenchAction::ToFix:
	case EWrenchAction::FromFix:
	{
		// Along with the clip bringing the jaw onto the work (FromFix counts its time down).
		const float Length = AnimLength(ToFixAnim);
		return Length > 0.f ? FMath::SmoothStep(0.f, 1.f, ActionTime / Length) : 1.f;
	}
	case EWrenchAction::Fix:
	case EWrenchAction::FinishTurn:
		return 1.f;
	default:
		return 0.f;
	}
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
	const float PreviousTime = ActionTime;
	ActionTime = bBackwards ? FMath::Max(0.f, ActionTime - DeltaTime) : ActionTime + DeltaTime;

	// Each ratchet turn that comes back to rest repairs what the jaw is on. It is pinned under the crosshair,
	// so trace along the view.
	if (Action == EWrenchAction::Fix || Action == EWrenchAction::FinishTurn)
	{
		if (FixTurnEndTimes.IsEmpty())
		{
			FindFixRestTimes();
		}
		const float TurnedTo = Action == EWrenchAction::FinishTurn ? FMath::Min(ActionTime, FinishTime) : ActionTime;
		const int32 Turns = Camera ? CountTurnsEnded(PreviousTime, TurnedTo) : 0;
		for (int32 i = 0; i < Turns; ++i)
		{
			ApplyTurnRepair(Camera->GetComponentLocation(), Camera->GetForwardVector());
		}
	}

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
