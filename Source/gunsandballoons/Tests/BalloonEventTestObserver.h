#pragma once
#include "../BalloonLevel/BalloonLevelEventManager.h"
#include "BalloonEventTestObserver.generated.h"
UCLASS(Transient, NotBlueprintable)
class UBalloonEventTestObserver : public UObject
{
 GENERATED_BODY()
public:
 TArray<FName> Fired;
 TArray<FName> Rejected;
 UBalloonLevelEventManager* Manager = nullptr;
 bool ResetOnFire = false;
 UFUNCTION() void Triggered(const FBalloonLevelEventDefinition& Event)
 {
  Fired.Add(Event.EventId);
  if(Manager) { Manager->EmitCustomSignal(TEXT("nested"),NAME_None); if(ResetOnFire) Manager->ResetEventTimeline(); }
 }
 UFUNCTION() void Reject(FName Id, const FString& Reason) { Rejected.Add(Id); }
};
