#include "GunAnimInstance.h"

#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimNodeBase.h"
#include "Animation/AnimSequenceBase.h"
#include "Animation/AnimationPoseData.h"
#include "BonePose.h"

namespace
{
	/** Copies the game-thread values in PreUpdate, then builds the pose on the (possibly worker) evaluation thread. */
	struct FGunAnimInstanceProxy : public FAnimInstanceProxy
	{
		explicit FGunAnimInstanceProxy(UAnimInstance* InAnimInstance)
			: FAnimInstanceProxy(InAnimInstance)
		{
		}

		virtual void PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds) override
		{
			FAnimInstanceProxy::PreUpdate(InAnimInstance, DeltaSeconds);

			const UGunAnimInstance* Instance = CastChecked<UGunAnimInstance>(InAnimInstance);
			Sequence = Instance->Sequence;
			Time = Instance->Time;
			BoneScales = Instance->GetBoneScales();
			BonePoses = Instance->GetBonePoses();
		}

		virtual bool Evaluate(FPoseContext& Output) override
		{
			if (Sequence)
			{
				SamplePose(Output, Sequence, Time);
			}
			else
			{
				Output.ResetToRefPose();
			}

			const FBoneContainer& Bones = Output.Pose.GetBoneContainer();

			for (const FGunBonePose& BonePose : BonePoses)
			{
				const FCompactPoseBoneIndex Bone = FindBone(Bones, BonePose.Bone);
				if (BonePose.Source && Bone.IsValid())
				{
					// The rigs are tiny, so sampling a whole second pose for one bone is cheap.
					FPoseContext SourcePose(Output);
					SamplePose(SourcePose, BonePose.Source, BonePose.Time);
					Output.Pose[Bone] = SourcePose.Pose[Bone];
				}
			}

			for (const FGunBoneScale& BoneScale : BoneScales)
			{
				const FCompactPoseBoneIndex Bone = FindBone(Bones, BoneScale.Bone);
				if (Bone.IsValid())
				{
					FTransform& Transform = Output.Pose[Bone];
					Transform.SetScale3D(Transform.GetScale3D() * BoneScale.Scale);
				}
			}

			return true;
		}

		static void SamplePose(FPoseContext& Pose, const UAnimSequenceBase* Anim, float AtTime)
		{
			FAnimationPoseData PoseData(Pose);
			const double SampleTime = FMath::Clamp(double(AtTime), 0.0, double(Anim->GetPlayLength()));
			Anim->GetAnimationPose(PoseData, FAnimExtractContext(SampleTime));
		}

		static FCompactPoseBoneIndex FindBone(const FBoneContainer& Bones, FName Name)
		{
			const int32 MeshIndex = Bones.GetPoseBoneIndexForBoneName(Name);
			return MeshIndex == INDEX_NONE ? FCompactPoseBoneIndex(INDEX_NONE) : Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(MeshIndex));
		}

		const UAnimSequenceBase* Sequence = nullptr;
		float Time = 0.f;
		TArray<FGunBoneScale> BoneScales;
		TArray<FGunBonePose> BonePoses;
	};
}

void UGunAnimInstance::SetBoneScale(FName Bone, float Scale)
{
	for (FGunBoneScale& Entry : BoneScales)
	{
		if (Entry.Bone == Bone)
		{
			Entry.Scale = Scale;
			return;
		}
	}
	BoneScales.Add({ Bone, Scale });
}

void UGunAnimInstance::SetBonePose(FName Bone, const UAnimSequenceBase* Source, float InTime)
{
	const int32 Index = BonePoses.IndexOfByPredicate([Bone](const FGunBonePose& Entry) { return Entry.Bone == Bone; });
	if (!Source)
	{
		if (Index != INDEX_NONE)
		{
			BonePoses.RemoveAtSwap(Index);
		}
		return;
	}

	if (Index != INDEX_NONE)
	{
		BonePoses[Index].Source = Source;
		BonePoses[Index].Time = InTime;
		return;
	}
	BonePoses.Add({ Bone, Source, InTime });
}

FAnimInstanceProxy* UGunAnimInstance::CreateAnimInstanceProxy()
{
	return new FGunAnimInstanceProxy(this);
}

void UGunAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy)
{
	delete static_cast<FGunAnimInstanceProxy*>(InProxy);
}
