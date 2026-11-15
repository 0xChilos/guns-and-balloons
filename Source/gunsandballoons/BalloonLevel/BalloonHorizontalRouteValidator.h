#pragma once
#include "BalloonLevelTypes.h"

class AActor;
class UWorld;

namespace BalloonHorizontalRoute
{
 /** Cm and seconds. Yaw-invariant enclosure of every query-enabled Pawn shape.
  * Filling hollow spaces is intentionally conservative. Z is always the actor datum. */
 struct FEnvelope
 {
  double Radius = 0., HalfHeight = 0., Safety = 0., Wind = 0.;
  double Speed = 0., MaxYawRate = 0., YawAcceleration = 0., MinimumRadius = 0.;
  FVector InitialDirection = FVector::ForwardVector;
  double InitialYawRate = 0.;
  double SpeedAcceleration = 0.;
 };
 struct FSettings { double CellCm = 1000.; int32 MaxStates = 100000, MaxAttempts = 12; };
 struct FResult
 {
  bool Reachable = false;
  FString FailureReason;
  TArray<FVector> Points;
  double FixedZ = 0., MinimumClearanceCm = 0., MaximumCurvature = 0.;
  int32 CellsVisited = 0, AttemptCount = 0;
 };
 /** Returns true when the swept enclosing box is blocked, including stationary overlap. */
 using FSweep = TFunction<bool(const FVector&, const FVector&, const FVector&)>;
 GUNSANDBALLOONS_API bool InspectPawn(const AActor& Pawn, FEnvelope& Out, FString& Error);
 GUNSANDBALLOONS_API FSweep WorldSweep(UWorld& World, const AActor* Ignore);
 /** Pure fixed-Z A*. Optional current XY is exact, not snapped, and must already be at fixed Z. */
 GUNSANDBALLOONS_API FResult Validate(const FBalloonLevelPlan& Plan, const FEnvelope& Envelope,
  const FSettings& Settings, const FSweep& Sweep, const FVector* Current = nullptr);
 /** Transactional border proposal. Failure leaves both plan and previous proof untouched. */
 GUNSANDBALLOONS_API FResult TryBorders(FBalloonLevelPlan& Plan, const TArray<FBalloonBorderInfluence>& Proposed,
  const FVector& Current, const FEnvelope& Envelope, const FSettings& Settings, const FSweep& Sweep);
}
