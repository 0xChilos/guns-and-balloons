#include "RevolverAnimInstance.h"

#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimNodeBase.h"
#include "Animation/AnimSequenceBase.h"
#include "Animation/AnimationPoseData.h"
#include "BonePose.h"

namespace
{
	const FName CylinderBone(TEXT("cylinder"));
	const FName CaseEjectBone(TEXT("case_eject"));
	const FName ChamberBones[URevolverAnimInstance::NumChambers] = {
		TEXT("chamber_1"), TEXT("chamber_2"), TEXT("chamber_3"),
		TEXT("chamber_4"), TEXT("chamber_5"), TEXT("chamber_6"),
	};

	/** Copies the game-thread values in PreUpdate, then builds the pose on the (possibly worker) evaluation thread. */
	struct FRevolverAnimInstanceProxy : public FAnimInstanceProxy
	{
		explicit FRevolverAnimInstanceProxy(UAnimInstance* InAnimInstance)
			: FAnimInstanceProxy(InAnimInstance)
		{
		}

		virtual void PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds) override
		{
			FAnimInstanceProxy::PreUpdate(InAnimInstance, DeltaSeconds);

			const URevolverAnimInstance* Instance = CastChecked<URevolverAnimInstance>(InAnimInstance);
			Sequence = Instance->Sequence;
			Time = Instance->Time;
			CylinderAngle = Instance->CylinderAngle;
			CaseEjectScale = Instance->CaseEjectScale;
			for (int32 i = 0; i < URevolverAnimInstance::NumChambers; ++i)
			{
				ChamberScale[i] = Instance->ChamberScale[i];
			}
		}

		virtual bool Evaluate(FPoseContext& Output) override
		{
			if (Sequence)
			{
				FAnimationPoseData PoseData(Output);
				const double SampleTime = FMath::Clamp(double(Time), 0.0, double(Sequence->GetPlayLength()));
				Sequence->GetAnimationPose(PoseData, FAnimExtractContext(SampleTime));
			}
			else
			{
				Output.ResetToRefPose();
			}

			const FBoneContainer& Bones = Output.Pose.GetBoneContainer();
			auto FindBone = [&Bones](FName Name) -> FCompactPoseBoneIndex
			{
				const int32 MeshIndex = Bones.GetPoseBoneIndexForBoneName(Name);
				return MeshIndex == INDEX_NONE ? FCompactPoseBoneIndex(INDEX_NONE) : Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(MeshIndex));
			};

			// The clips never turn the cylinder; it spins about its local Y axis (the barrel axis).
			// In the clips' pose chamber_1 sits under the hammer and chamber_6 at the loading gate;
			// each +60 degrees brings the next chamber up and moves the previous one to the gate.
			const FCompactPoseBoneIndex Cylinder = FindBone(CylinderBone);
			if (Cylinder.IsValid())
			{
				FTransform& Transform = Output.Pose[Cylinder];
				Transform.SetRotation(Transform.GetRotation() * FQuat(FVector::YAxisVector, FMath::DegreesToRadians(CylinderAngle)));
			}

			for (int32 i = 0; i < URevolverAnimInstance::NumChambers; ++i)
			{
				const FCompactPoseBoneIndex Chamber = FindBone(ChamberBones[i]);
				if (Chamber.IsValid())
				{
					FTransform& Transform = Output.Pose[Chamber];
					Transform.SetScale3D(Transform.GetScale3D() * ChamberScale[i]);
				}
			}

			const FCompactPoseBoneIndex CaseEject = FindBone(CaseEjectBone);
			if (CaseEject.IsValid())
			{
				FTransform& Transform = Output.Pose[CaseEject];
				Transform.SetScale3D(Transform.GetScale3D() * CaseEjectScale);
			}

			return true;
		}

		const UAnimSequenceBase* Sequence = nullptr;
		float Time = 0.f;
		float CylinderAngle = 0.f;
		float ChamberScale[URevolverAnimInstance::NumChambers] = { 1.f, 1.f, 1.f, 1.f, 1.f, 1.f };
		float CaseEjectScale = 1.f;
	};
}

FAnimInstanceProxy* URevolverAnimInstance::CreateAnimInstanceProxy()
{
	return new FRevolverAnimInstanceProxy(this);
}

void URevolverAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy)
{
	delete static_cast<FRevolverAnimInstanceProxy*>(InProxy);
}
