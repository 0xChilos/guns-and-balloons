#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Interactable.h"
#include "BalloonWingLeverComponent.generated.h"

class UBalloonFlightComponent;
class UPrimitiveComponent;

UENUM(BlueprintType)
enum class EBalloonSide : uint8 { Left, Right };
UENUM(BlueprintType)
enum class EWingLeverStepMode : uint8 { PingPong, Cycle };

USTRUCT(BlueprintType)
struct FBalloonWingState
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float LeverAngle = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float WingAngle = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0", ClampMax="1")) float TurnInput = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0", ClampMax="1")) float SpeedMultiplier = 1.f;
};

/** Persistent wing brake, with independently tunable detents. Angles are relative to bone rest pose. */
UCLASS(ClassGroup=(Balloon), meta=(BlueprintSpawnableComponent))
class GUNSANDBALLOONS_API UBalloonWingLeverComponent : public UActorComponent, public IInteractable
{
	GENERATED_BODY()
public:
	UBalloonWingLeverComponent();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void Interact_Implementation(APawn* InstigatorPawn) override;
	virtual bool CanInteract_Implementation(APawn* InstigatorPawn) const override;
	virtual bool AcceptsHit_Implementation(const FHitResult& Hit) const override;

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

	UFUNCTION(BlueprintCallable, Category="Balloon|Wing") void SetState(int32 NewState);
	UFUNCTION(BlueprintCallable, Category="Balloon|Wing") void StepState();
	UFUNCTION(BlueprintPure, Category="Balloon|Wing") int32 GetState() const { return State; }
	UFUNCTION(BlueprintPure, Category="Balloon|Wing") int32 GetNumStates() const { return States.Num(); }
	UFUNCTION(BlueprintPure, Category="Balloon|Wing") float GetLeverAngle() const { return LeverAngle; }
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
	UPROPERTY(Transient) TObjectPtr<UBalloonFlightComponent> Flight;
	int32 State = 0;
	int32 StepDirection = 1;
	float LeverAngle = 0.f;
	float WingAngle = 0.f;
	float TransitionWingStart = 0.f;
	float TransitionTurnStart = 0.f;
	float TransitionSpeedStart = 1.f;
	float CurrentTurn = 0.f;
	float CurrentMultiplier = 1.f;
	float LastTurn = 0.f;
	float LastMultiplier = 1.f;
	bool bPublished = false;
	void PublishModifier();
};
