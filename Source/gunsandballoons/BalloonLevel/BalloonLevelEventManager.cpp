#include "BalloonLevelEventManager.h"
#include "BalloonLevelGenerator.h"
#include "Engine/World.h"

namespace
{
 bool IsLightingCommand(FName Name)
 {
  FString Text=Name.ToString().ToLower();Text.ReplaceInline(TEXT("_"),TEXT(""));Text.ReplaceInline(TEXT("-"),TEXT(""));
  return Text.Contains(TEXT("transitiontoday")) || Text.Contains(TEXT("transitiontonight")) || Text.Contains(TEXT("swapdaynight")) ||
   Text.Contains(TEXT("daynightswap")) || Text.Contains(TEXT("setlightingpreset")) || Text.Contains(TEXT("changelightingpreset"));
 }
}

UBalloonLevelEventManager::UBalloonLevelEventManager()
{
 PrimaryComponentTick.bCanEverTick = true;
 PrimaryComponentTick.bTickEvenWhenPaused = false;
}
bool UBalloonLevelEventManager::RegisterEventDefinitions(const TArray<FBalloonLevelEventDefinition>& Definitions)
{
 if(Dispatching || Snapshot.Running) return false;
 TSet<FName> Ids;
 for(const auto& E : Definitions)
 {
  FString Error;
  if(E.RequestsLightingSwap || IsLightingCommand(E.EventId) || IsLightingCommand(E.SignalId)) Error=TEXT("Day/Night swaps are forbidden; lighting is fixed for the session");
  else if(E.EventId.IsNone() || Ids.Contains(E.EventId)) Error=TEXT("EventId must be unique and nonempty");
  else if(uint8(E.Trigger)>uint8(EBalloonEventTrigger::CustomSignal) || uint8(E.Action)>uint8(EBalloonEventAction::ScaleBorderRadius) || uint8(E.Repeat)>uint8(EBalloonEventRepeat::OnEveryCrossing)) Error=TEXT("Unknown trigger, action or repeat policy (lighting transitions are unsupported)");
  else if(!FMath::IsFinite(E.Threshold) || E.Threshold<0. || (E.Trigger==EBalloonEventTrigger::NormalizedProgress && E.Threshold>1.) || !FMath::IsFinite(E.Value) || !FMath::IsFinite(E.CooldownSeconds) || E.CooldownSeconds<0.) Error=TEXT("Invalid threshold, value or cooldown");
  else if(E.Trigger==EBalloonEventTrigger::CustomSignal && E.SignalId.IsNone()) Error=TEXT("Custom signal requires SignalId");
  else if(E.Repeat==EBalloonEventRepeat::RepeatAfterCooldown && E.CooldownSeconds<=0.) Error=TEXT("Repeated events require positive cooldown");
  else if(E.BorderId < -1 || E.SegmentId < -1 || ((E.Action==EBalloonEventAction::SetBorderRadius || E.Action==EBalloonEventAction::ScaleBorderRadius) && E.Value<=0.)) Error=TEXT("Invalid border selector/radius");
  if(!Error.IsEmpty()) { OnLevelEventRejected.Broadcast(E.EventId,Error); return false; }
  Ids.Add(E.EventId);
 }
 Events=Definitions;
 ResetEventTimeline();
 return true;
}
void UBalloonLevelEventManager::ResetEventTimeline()
{
 ++Revision;
 Snapshot=FBalloonEventSnapshot();
 for(const auto& E:Events) { FBalloonEventState S; S.EventId=E.EventId; Snapshot.States.Add(S); }
}
void UBalloonLevelEventManager::StartEventTimeline()
{
 if(Snapshot.Running || Dispatching) return;
 if(const auto* G=Cast<ABalloonLevelGenerator>(GetOwner()))
 {
  if(G->GetGenerationStatus()!=EBalloonLevelGenerationStatus::Ready) return;
  Snapshot.FixedPreset=G->GetPlan().LightingPreset;
 }
 Snapshot.Running=true;
 Evaluate(EBalloonEventTrigger::ElapsedGameTime,0.,Snapshot.ElapsedSeconds,true);
 Evaluate(EBalloonEventTrigger::NormalizedProgress,0.,Snapshot.NormalizedProgress,true);
 Evaluate(EBalloonEventTrigger::AbsoluteProgress,0.,Snapshot.AbsoluteMeters,true);
}
void UBalloonLevelEventManager::PauseEventTimeline(bool Paused) { Snapshot.Paused=Paused; }
void UBalloonLevelEventManager::UpdateElapsedGameTime(double DeltaSeconds)
{
 if(Dispatching || !Snapshot.Running || Snapshot.Paused || (GetWorld() && GetWorld()->IsPaused()) || !FMath::IsFinite(DeltaSeconds) || DeltaSeconds<0. || !FMath::IsFinite(Snapshot.ElapsedSeconds+DeltaSeconds)) return;
 const double Old=Snapshot.ElapsedSeconds; Snapshot.ElapsedSeconds+=DeltaSeconds;
 Evaluate(EBalloonEventTrigger::ElapsedGameTime,Old,Snapshot.ElapsedSeconds);
 // Level-triggered cooldown repeats stay eligible while above the progress threshold.
 Evaluate(EBalloonEventTrigger::NormalizedProgress,Snapshot.NormalizedProgress,Snapshot.NormalizedProgress);
 Evaluate(EBalloonEventTrigger::AbsoluteProgress,Snapshot.AbsoluteMeters,Snapshot.AbsoluteMeters);
}
void UBalloonLevelEventManager::UpdatePlayerProgress(double Normalized, double Meters)
{
 if(Dispatching || !Snapshot.Running || Snapshot.Paused || (GetWorld() && GetWorld()->IsPaused()) || !FMath::IsFinite(Normalized) || !FMath::IsFinite(Meters) || Normalized<0. || Normalized>1. || Meters<0.) return;
 const double OldN=Snapshot.NormalizedProgress, OldM=Snapshot.AbsoluteMeters;
 Snapshot.NormalizedProgress=Normalized; Snapshot.AbsoluteMeters=Meters;
 Snapshot.ProgressWatermark=FMath::Max(Snapshot.ProgressWatermark,Normalized);
 const uint64 Before=Revision;
 Evaluate(EBalloonEventTrigger::NormalizedProgress,OldN,Normalized);
 if(Before==Revision) Evaluate(EBalloonEventTrigger::AbsoluteProgress,OldM,Meters);
}
void UBalloonLevelEventManager::EmitCustomSignal(FName Signal, FName Target)
{ Evaluate(EBalloonEventTrigger::CustomSignal,0.,0.,false,Signal,Target); }
void UBalloonLevelEventManager::Evaluate(EBalloonEventTrigger Trigger, double Previous, double Current, bool Initial, FName Signal, FName Target)
{
 if(Dispatching || !Snapshot.Running || Snapshot.Paused || (GetWorld() && GetWorld()->IsPaused())) return;
 TArray<int32> Due;
 for(int32 I=0;I<Events.Num();++I)
 {
  const auto& E=Events[I]; const auto& S=Snapshot.States[I];
  if(!E.Enabled || E.Trigger!=Trigger || (S.Fired && E.Repeat==EBalloonEventRepeat::Once) || Snapshot.ElapsedSeconds<S.NextEligibleTime) continue;
  const bool Crossing=(Previous<E.Threshold && Current>=E.Threshold) || (Initial && E.Threshold==0.);
  const bool Eligible=Trigger==EBalloonEventTrigger::CustomSignal ? E.SignalId==Signal && (Target.IsNone() || E.TargetTag==Target) :
   E.Repeat==EBalloonEventRepeat::OnEveryCrossing ? Crossing : Current>=E.Threshold;
  if(Eligible) Due.Add(I);
 }
 Due.Sort([&](int32 A,int32 B) { const auto& X=Events[A];const auto& Y=Events[B]; if(X.Threshold!=Y.Threshold)return X.Threshold<Y.Threshold;if(X.Priority!=Y.Priority)return X.Priority<Y.Priority;return X.EventId.LexicalLess(Y.EventId); });
 const uint64 Before=Revision;
 TGuardValue<bool> Guard(Dispatching,true);
 for(int32 I:Due)
 {
  if(Before!=Revision || !Snapshot.Running || Snapshot.Paused) break;
  const auto E=Events[I]; auto& S=Snapshot.States[I];
  // Consume attempts before callbacks: rejected safety proposals are exact-once too.
  S.Fired=true;S.NextEligibleTime=Snapshot.ElapsedSeconds+E.CooldownSeconds;
  FString Reason;
  bool Accepted=true;
  if(E.Action!=EBalloonEventAction::Blueprint)
  {
   auto* Generator=Cast<ABalloonLevelGenerator>(GetOwner());
   Accepted=Generator && Generator->TryEventBorderChange(E,Reason);
   if(!Generator) Reason=TEXT("Border action requires a generator owner");
  }
  if(Before!=Revision) break;
  if(Accepted) OnLevelEventTriggered.Broadcast(E);
  else OnLevelEventRejected.Broadcast(E.EventId,Reason);
 }
}
void UBalloonLevelEventManager::TickComponent(float DeltaTime,ELevelTick TickType,FActorComponentTickFunction* Function)
{
 Super::TickComponent(DeltaTime,TickType,Function);
 if(!GetWorld() || GetWorld()->IsPaused()) return;
 if(auto* G=Cast<ABalloonLevelGenerator>(GetOwner()); G && IsValid(G->RoutePawn)) G->UpdateProgress(G->RoutePawn->GetActorLocation());
 UpdateElapsedGameTime(DeltaTime);
}
void UBalloonLevelEventManager::EndPlay(const EEndPlayReason::Type Reason)
{
 ResetEventTimeline();OnLevelEventTriggered.Clear();OnLevelEventRejected.Clear();Super::EndPlay(Reason);
}
