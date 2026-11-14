#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Grabbable.h"
#include "BalloonWingLeverComponent.generated.h"

class UBalloonFlightComponent;
class UPrimitiveComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnWingLeverSlipped, int32, FromState, int32, ToState);

UENUM(BlueprintType)
enum class EBalloonSide : uint8 { Left, Right };
UENUM(BlueprintType)
enum class EWingLeverStepMode : uint8 { PingPong, Cycle };

UENUM(BlueprintType)
enum class EWingLeverReleaseMode : uint8 { ReturnToLastDetent, SnapToNearestDetent, Free };

USTRUCT(BlueprintType)
struct FBalloonWingState
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float LeverAngle = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0")) float DetentBreakAngle = 5.f; // TBD: tuning
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0", ClampMax="1")) float SlipChance = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float WingAngle = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0", ClampMax="1")) float TurnInput = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0", ClampMax="1")) float SpeedMultiplier = 1.f;
};

/** Persistent wing brake, with independently tunable detents. Angles are relative to bone rest pose. */
UCLASS(ClassGroup=(Balloon), meta=(BlueprintSpawnableComponent))
class GUNSANDBALLOONS_API UBalloonWingLeverComponent : public UActorComponent, public IGrabbable
{
	GENERATED_BODY()
public:
	UBalloonWingLeverComponent();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual bool CanGrab_Implementation(APawn* Pawn, const FHitResult& Hit) const override;
	virtual void BeginGrab_Implementation(APawn* Pawn, const FHitResult& Hit) override;
	virtual void UpdateGrab_Implementation(APawn* Pawn, FVector WorldDragDelta) override;
	virtual void EndGrab_Implementation(APawn* Pawn) override;
	virtual FVector GetGrabLocation_Implementation() const override;
	bool AcceptsHit_Implementation(const FHitResult& Hit) const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Balloon|Wing") EBalloonSide Side = EBalloonSide::Left;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Balloon|Wing") FName LeverBone = TEXT("lever_L");
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Balloon|Wing") FName WingBone = TEXT("stopper_L");
	/** Optional query-only hit shape, when the physics asset does not expose a lever body. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Balloon|Wing") TObjectPtr<UPrimitiveComponent> InteractShape;
	/** Changing any generator parameter overwrites manual edits to States. */
	UPROPERTY(EditAnywhere, Category="Balloon|Wing|Generator", meta=(ClampMin="2", ClampMax="256")) int32 NumStates = 4; // TBD: prototype value
	UPROPERTY(EditAnywhere, Category="Balloon|Wing|Generator") float LeverClosedAngle = -20.f; // TBD: prototype value
	UPROPERTY(EditAnywhere, Category="Balloon|Wing|Generator") float LeverOpenAngle = 70.f; // TBD: prototype value
	UPROPERTY(EditAnywhere, Category="Balloon|Wing|Generator", meta=(ClampMin="0")) float MaxWingAngle = 90.f; // TBD: prototype value
	UPROPERTY(EditAnywhere, Category="Balloon|Wing|Generator", meta=(ClampMin="0", ClampMax="1")) float MaxTurnInput = 1.f; // TBD: prototype value
	UPROPERTY(EditAnywhere, Category="Balloon|Wing|Generator", meta=(ClampMin="0", ClampMax="1")) float OpenSpeedMultiplier = .8f; // TBD: prototype value
	/** Source of truth. Regenerating or changing any generator parameter overwrites these manual edits. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Balloon|Wing") TArray<FBalloonWingState> States;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Balloon|Wing") EWingLeverStepMode StepMode = EWingLeverStepMode::PingPong;
	UPROPERTY(EditAnywhere, Category="Balloon|Wing", meta=(ClampMin="0.01")) float LeverMoveSpeed = 400.f; // TBD: prototype value
	UPROPERTY(EditAnywhere, Category="Balloon|Wing", meta=(ClampMin="0.01")) float WingMoveSpeed = 60.f; // TBD: prototype value


	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Balloon|Wing|Grab") EWingLeverReleaseMode ReleaseMode = EWingLeverReleaseMode::ReturnToLastDetent;
	// TBD: prototype grab and detent parameters; tune in the component defaults.
	UPROPERTY(EditAnywhere, Category="Balloon|Wing|Generator", meta=(ClampMin="0")) float DetentBreakAngle = 5.f;
	UPROPERTY(EditAnywhere, Category="Balloon|Wing|Generator", meta=(ClampMin="0", ClampMax="1")) float MaxSlipChance = .05f; // TBD: prototype value
	UPROPERTY(EditAnywhere, Category="Balloon|Wing|Slip", meta=(ClampMin="0.01")) float SlipCheckInterval = 1.f; // TBD: prototype value
	UPROPERTY(EditAnywhere, Category="Balloon|Wing|Slip", meta=(ClampMin="0.01")) float SlipMoveSpeed = 600.f; // TBD: prototype value
	UPROPERTY(EditAnywhere, Category="Balloon|Wing|Slip") bool bSlipWhileGrabbed = false; // TBD: prototype value
	/** Zero generates a random seed at BeginPlay; other values give reproducible rolls. */
	UPROPERTY(EditAnywhere, Category="Balloon|Wing|Slip") int32 SlipSeed = 0; // TBD: prototype value
	UPROPERTY(BlueprintAssignable, Category="Balloon|Wing") FOnWingLeverSlipped OnSlipped;
	UPROPERTY(EditAnywhere, Category="Balloon|Wing|Grab", meta=(ClampMin="0")) float MinGrabRadius = 2.f;
	UPROPERTY(EditAnywhere, Category="Balloon|Wing|Grab", meta=(ClampMin="0")) float HandleLength = 20.f;
	UPROPERTY(EditAnywhere, Category="Balloon|Wing|Grab", meta=(ClampMin="0")) float LeverDragMaxSpeed = 400.f;
	UPROPERTY(EditAnywhere, Category="Balloon|Wing|Grab", meta=(ClampMin="0")) float DetentStrainDecay = 10.f;
	UPROPERTY(EditAnywhere, Category="Balloon|Wing|Grab", meta=(ClampMin="0")) float DetentGive = .2f;
	UPROPERTY(EditAnywhere, Category="Balloon|Wing|Grab") float DragSign = 1.f; // TBD: rig direction
	UPROPERTY(EditAnywhere, Category="Balloon|Wing|Grab") FVector LeverAxis = FVector::RightVector; // TBD: bone-local axis

	UFUNCTION(BlueprintCallable, Category="Balloon|Wing") void SetState(int32 NewState);
	UFUNCTION(BlueprintCallable, Category="Balloon|Wing") void StepState();
	/** Roll a chance for wind, hits or shaking; returns whether the lever slipped. */
	UFUNCTION(BlueprintCallable, Category="Balloon|Wing") bool TrySlip(float Chance);
	/** Release the lever and close fully, bypassing intermediate detents. */
	UFUNCTION(BlueprintCallable, Category="Balloon|Wing") bool Slip();
	UFUNCTION(BlueprintPure, Category="Balloon|Wing") int32 GetState() const { return LastDetent; }
	UFUNCTION(BlueprintPure, Category="Balloon|Wing") int32 GetNumStates() const { return States.Num(); }
	UFUNCTION(BlueprintPure, Category="Balloon|Wing") float GetLeverAngle() const;
	UFUNCTION(BlueprintPure, Category="Balloon|Wing") float GetPosition() const { return Position; }
	bool IsGrabbed() const { return bGrabbed; }
	virtual bool IsGrabbed_Implementation() const override { return bGrabbed; }
	UFUNCTION(BlueprintPure, Category="Balloon|Wing") float GetWingAngle() const { return WingAngle; }
	UFUNCTION(BlueprintPure, Category="Balloon|Wing") float GetWingOpen01() const;
	/** Resolve by side, independent of Blueprint component ordering or display names. */
	UFUNCTION(BlueprintPure, Category="Balloon|Wing")
	static UBalloonWingLeverComponent* FindWingLever(AActor* Actor, EBalloonSide InSide);
	UFUNCTION(CallInEditor, BlueprintCallable, Category="Balloon|Wing|Generator") void RegenerateStates();
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
private:
	friend class FPlayerGrabTest;
	friend class FWingLeverSlipTest;
	UPROPERTY(Transient) TObjectPtr<UBalloonFlightComponent> Flight;
	float Position = 0.f;
	float TargetPosition = 0.f;
	bool bGrabbed = false;
	int32 LastDetent = 0;
	int32 HeldDetent = INDEX_NONE;
	float DetentStrain = 0.f;
	int32 EscapedDetent = INDEX_NONE;
	float EscapeDirection = 0.f;
	float MoveSpeedOverride = 0.f;
	float SlipCheckElapsed = 0.f;
	FRandomStream SlipRandom;
	FVector LocalGrabPoint = FVector::ZeroVector;
	TWeakObjectPtr<class USkeletalMeshComponent> GrabMesh;
	TWeakObjectPtr<APawn> GrabPawn;
	FBalloonWingState SampleByPosition(float S) const;
	float PositionFromLeverAngle(float Angle) const;
	FBalloonWingState SampleEffectByWingAngle(float Angle) const;
	void ApplyDetents(float OldAngle, float DeltaAngle);
	int32 StepDirection = 1;
	float LeverAngle = 0.f;
	float WingAngle = 0.f;
	float CurrentTurn = 0.f;
	float CurrentMultiplier = 1.f;
	float LastTurn = 0.f;
	float LastMultiplier = 1.f;
	bool bPublished = false;
	void PublishModifier();
};
