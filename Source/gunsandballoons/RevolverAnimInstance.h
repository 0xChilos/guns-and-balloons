#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "RevolverAnimInstance.generated.h"

class UAnimSequenceBase;

/**
 * Code-driven pose for SK_Revolver; no Animation Blueprint needed.
 *
 * Samples one sequence at a time chosen by the game code, then layers on what the separate
 * reload/fire clips leave to the game: the cylinder's rotation, which chambers hold a round,
 * and whether the round being loaded is visible. URevolverComponent fills these in every frame.
 */
UCLASS(Transient, NotBlueprintable)
class GUNSANDBALLOONS_API URevolverAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	static constexpr int32 NumChambers = 6;

	/** Sequence to sample. Null shows the reference pose. */
	UPROPERTY(Transient)
	TObjectPtr<UAnimSequenceBase> Sequence;

	/** Time in Sequence (seconds). Clamped to its length when sampled. */
	float Time = 0.f;

	/** Cylinder rotation about its axis (degrees). Each chamber is 60 degrees apart. */
	float CylinderAngle = 0.f;

	/** Per-chamber scale multiplier: 1 shows the round in that chamber, 0 hides it. */
	float ChamberScale[NumChambers] = { 1.f, 1.f, 1.f, 1.f, 1.f, 1.f };

	/** Scale multiplier for the round being loaded (bone "round_load"). */
	float RoundLoadScale = 1.f;

protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override;
};
