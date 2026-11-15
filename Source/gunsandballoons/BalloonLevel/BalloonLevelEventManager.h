#pragma once
#include "Components/ActorComponent.h"
#include "BalloonLevelTypes.h"
#include "BalloonLevelEventManager.generated.h"

UENUM(BlueprintType)
enum class EBalloonEventTrigger : uint8 { ElapsedGameTime, NormalizedProgress, AbsoluteProgress, CustomSignal };
UENUM(BlueprintType)
enum class EBalloonEventRepeat : uint8 { Once, RepeatAfterCooldown, OnEveryCrossing };
UENUM(BlueprintType)
enum class EBalloonEventAction : uint8 { Blueprint, SetBorderRadius, AddBorderRadius, ScaleBorderRadius };

USTRUCT(BlueprintType)
struct GUNSANDBALLOONS_API FBalloonLevelEventDefinition
{
 GENERATED_BODY()
 UPROPERTY(EditAnywhere, BlueprintReadWrite) FName EventId;
 UPROPERTY(EditAnywhere, BlueprintReadWrite) bool Enabled = true;
 UPROPERTY(EditAnywhere, BlueprintReadWrite) EBalloonEventTrigger Trigger = EBalloonEventTrigger::ElapsedGameTime;
 /** Seconds, normalized fraction, or meters. */
 UPROPERTY(EditAnywhere, BlueprintReadWrite) double Threshold = 0.;
 UPROPERTY(EditAnywhere, BlueprintReadWrite) FName SignalId;
 UPROPERTY(EditAnywhere, BlueprintReadWrite) FName TargetTag;
 UPROPERTY(EditAnywhere, BlueprintReadWrite) EBalloonEventAction Action = EBalloonEventAction::Blueprint;
 UPROPERTY(EditAnywhere, BlueprintReadWrite) double Value = 0.;
 /** -1 means all. When TargetTag is set, only registered proxies with that Actor tag match. */
 UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 BorderId = -1;
 UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 SegmentId = -1;
 UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Priority = 0;
 UPROPERTY(EditAnywhere, BlueprintReadWrite) EBalloonEventRepeat Repeat = EBalloonEventRepeat::Once;
 UPROPERTY(EditAnywhere, BlueprintReadWrite) double CooldownSeconds = 0.;
 /** Explicit input validation flag for imported requests. Lighting swaps are always rejected. */
 UPROPERTY(EditAnywhere, BlueprintReadWrite) bool RequestsLightingSwap = false;
};

USTRUCT(BlueprintType)
struct GUNSANDBALLOONS_API FBalloonEventState
{
 GENERATED_BODY()
 UPROPERTY(BlueprintReadOnly) FName EventId;
 UPROPERTY(BlueprintReadOnly) bool Fired = false;
 UPROPERTY(BlueprintReadOnly) double NextEligibleTime = 0.;
};
USTRUCT(BlueprintType)
struct GUNSANDBALLOONS_API FBalloonEventSnapshot
{
 GENERATED_BODY()
 UPROPERTY(BlueprintReadOnly) double ElapsedSeconds = 0.;
 UPROPERTY(BlueprintReadOnly) double NormalizedProgress = 0.;
 UPROPERTY(BlueprintReadOnly) double AbsoluteMeters = 0.;
 UPROPERTY(BlueprintReadOnly) double ProgressWatermark = 0.;
 UPROPERTY(BlueprintReadOnly) bool Running = false;
 UPROPERTY(BlueprintReadOnly) bool Paused = false;
 UPROPERTY(BlueprintReadOnly) EBalloonLightingPreset FixedPreset = EBalloonLightingPreset::Day;
 UPROPERTY(BlueprintReadOnly) TArray<FBalloonEventState> States;
};
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBalloonEventTriggered, const FBalloonLevelEventDefinition&, Event);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FBalloonEventRejected, FName, EventId, const FString&, Reason);

/** One authoritative game-time update. Blueprint receivers bind delegates and filter TargetTag.
 * Encounter/wind actions belong to receivers; they must perform their own spawn safety checks. */
UCLASS(ClassGroup=(Balloon), meta=(BlueprintSpawnableComponent))
class GUNSANDBALLOONS_API UBalloonLevelEventManager : public UActorComponent
{
 GENERATED_BODY()
public:
 UBalloonLevelEventManager();
 UPROPERTY(BlueprintAssignable) FBalloonEventTriggered OnLevelEventTriggered;
 UPROPERTY(BlueprintAssignable) FBalloonEventRejected OnLevelEventRejected;
 UFUNCTION(BlueprintCallable) bool RegisterEventDefinitions(const TArray<FBalloonLevelEventDefinition>& Definitions);
 /** Explicit gameplay-ready boundary; generation never starts the clock. Start while running is idempotent. */
 UFUNCTION(BlueprintCallable) void StartEventTimeline();
 UFUNCTION(BlueprintCallable) void PauseEventTimeline(bool Paused);
 UFUNCTION(BlueprintCallable) void ResetEventTimeline();
 UFUNCTION(BlueprintCallable) void UpdateElapsedGameTime(double DeltaSeconds);
 UFUNCTION(BlueprintCallable) void UpdatePlayerProgress(double NormalizedProgress, double AbsoluteMeters);
 UFUNCTION(BlueprintCallable) void EmitCustomSignal(FName SignalId, FName TargetTag);
 UFUNCTION(BlueprintPure) FBalloonEventSnapshot GetSnapshot() const { return Snapshot; }
 virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
 virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
 UPROPERTY() TArray<FBalloonLevelEventDefinition> Events;
 UPROPERTY() FBalloonEventSnapshot Snapshot;
 bool Dispatching = false;
 uint64 Revision = 0;
 void Evaluate(EBalloonEventTrigger Trigger, double Previous, double Current, bool Initial = false, FName Signal = NAME_None, FName Target = NAME_None);
};
