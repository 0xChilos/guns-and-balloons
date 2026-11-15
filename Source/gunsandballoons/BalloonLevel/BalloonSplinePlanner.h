#pragma once
#include "BalloonLevelConfig.h"

/** Pure descriptor operations. Distances and widths are centimeters; queries ignore world Z. */
namespace BalloonSplinePlanning
{
 GUNSANDBALLOONS_API bool Build(const UBalloonLevelConfig& Config, FBalloonLevelPlan& Plan, TArray<FString>& Errors);
 GUNSANDBALLOONS_API int32 SegmentAt(const FBalloonLevelPlan& Plan, double S);
 GUNSANDBALLOONS_API FVector PositionAt(const FBalloonLevelPlan& Plan, double S);
 GUNSANDBALLOONS_API double WidthAt(const FBalloonLevelPlan& Plan, double S);
 GUNSANDBALLOONS_API double NearestDistance(const FBalloonLevelPlan& Plan, const FVector& Position, int32& Edge, bool FullSearch);
}

/** Explicit per-player state; reset on regeneration. MaxReachedS is for later one-shot events. */
struct GUNSANDBALLOONS_API FBalloonLevelProgress
{
 double CurrentS = 0.;
 double MaxReachedS = 0.;
 int32 Edge = INDEX_NONE;
 void Update(const FBalloonLevelPlan& Plan, const FVector& Position, bool Teleported = false);
};
