#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "GunAnimInstance.generated.h"

class UAnimSequenceBase;

/** A bone scaled on top of the sampled pose (not reflected; filled from code every frame). */
struct FGunBoneScale
{
	FName Bone;
	float Scale = 1.f;
};

/**
 * A bone whose local transform is taken from another clip at a fixed time instead of the sampled one
 * (not reflected; filled from code every frame). The gun component holds the clip, keeping it alive.
 */
struct FGunBonePose
{
	FName Bone;
	const UAnimSequenceBase* Source = nullptr;
	float Time = 0.f;
};

/**
 * A bone's local transform changed on top of the sampled pose (not reflected; filled from code every frame):
 * an optional absolute translation, and a rotation applied in the bone's own frame (Rotation = Sampled * Offset).
 */
struct FGunBoneOverride
{
	FName Bone;
	TOptional<FVector> Translation;
	FQuat RotationOffset = FQuat::Identity;
};

/**
 * Code-driven pose for a simple gun rig; no Animation Blueprint needed.
 *
 * Samples one sequence at a time chosen by the game code, then scales named bones on top of it,
 * which is how the rigs show or hide the rounds and cases they carry (the clips key them all
 * visible, so the game state always decides). The gun component fills these in every frame.
 * Used by URifleComponent (SK_MartiniHenry) and UShotgunComponent (SK_Homestead78); the revolver has its own URevolverAnimInstance
 * because it also turns the cylinder.
 */
UCLASS(Transient, NotBlueprintable)
class GUNSANDBALLOONS_API UGunAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	/** Sequence to sample. Null shows the reference pose. */
	UPROPERTY(Transient)
	TObjectPtr<UAnimSequenceBase> Sequence;

	/** Time in Sequence (seconds). Clamped to its length when sampled. */
	float Time = 0.f;

	/** Scale multiplier for a bone on top of the sampled pose: 1 shows what it carries, 0 hides it. */
	void SetBoneScale(FName Bone, float Scale);

	const TArray<FGunBoneScale>& GetBoneScales() const { return BoneScales; }

	/**
	 * Pose a bone as it is in Source at Time, whatever Sequence is playing (e.g. a hammer held cocked
	 * through clips that key it down). Applied before the bone scales. Null Source hands the bone back
	 * to Sequence.
	 */
	void SetBonePose(FName Bone, const UAnimSequenceBase* Source, float InTime = 0.f);

	const TArray<FGunBonePose>& GetBonePoses() const { return BonePoses; }

	/** Replace a bone's local translation (applied after the bone poses). Unset hands it back to the clip. */
	void SetBoneTranslation(FName Bone, TOptional<FVector> Translation);

	/** Turn a bone in its own frame on top of the sampled pose (e.g. a gauge needle). Identity removes it. */
	void SetBoneRotationOffset(FName Bone, const FQuat& RotationOffset);

	const TArray<FGunBoneOverride>& GetBoneOverrides() const { return BoneOverrides; }

protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override;

private:
	TArray<FGunBoneScale> BoneScales;
	TArray<FGunBonePose> BonePoses;
	TArray<FGunBoneOverride> BoneOverrides;

	FGunBoneOverride& FindOrAddOverride(FName Bone);
	void PruneOverride(FName Bone);
};
