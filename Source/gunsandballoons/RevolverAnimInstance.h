#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "RevolverAnimInstance.generated.h"

class UAnimSequenceBase;

/**
 * Code-driven pose for SK_Pax (the loading-gate revolver); no Animation Blueprint needed.
 *
 * Samples one sequence at a time chosen by the game code, then layers on what the separate
 * fire/reload clips leave to the game: the cylinder's rotation, which chambers hold a live round
 * (chamber_N) or a fired case (chamber_N_spent), and hiding the ejected case once a physics casing
 * has taken its place. The clips key every round and case visible, so these always decide.
 * URevolverComponent fills these in every frame.
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

	/** Cylinder rotation about its axis (degrees), on top of the clips' pose. Each chamber is 60 degrees apart. */
	float CylinderAngle = 0.f;

	/** Per-chamber scale multiplier for the live round (bones "chamber_N"): 1 shows it, 0 hides it. */
	float ChamberScale[NumChambers] = { 1.f, 1.f, 1.f, 1.f, 1.f, 1.f };

	/**
	 * Per-chamber scale multiplier for the fired case (bones "chamber_N_spent"). It sits in the same
	 * chamber as the round, so at most one of ChamberScale/SpentScale should be 1 for a chamber.
	 */
	float SpentScale[NumChambers] = { 0.f, 0.f, 0.f, 0.f, 0.f, 0.f };

	/** Scale multiplier for the case being pushed out of the gate (bone "case_eject"). */
	float CaseEjectScale = 0.f;

protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override;
};
