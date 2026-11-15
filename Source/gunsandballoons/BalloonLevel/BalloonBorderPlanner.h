#pragma once
#include "BalloonLevelConfig.h"

namespace BalloonBorderPlanning
{
 GUNSANDBALLOONS_API FVector RightAt(const FBalloonLevelPlan& Plan, double S);
 /** -1 means no vertical intersection; otherwise sphere slice against full envelope Z slab. */
 GUNSANDBALLOONS_API double ProjectedRadius(const FBalloonBorderInfluence& Threat, double FlightZ, double HalfHeightCm);
 /** Exact line/disc intersection, inflated by horizontal envelope radius. */
 GUNSANDBALLOONS_API bool CrossSection(const FBalloonBorderInfluence& Threat, const FVector& Origin, const FVector& Right, double HalfHeightCm, double BalloonRadiusCm, double& MinLateral, double& MaxLateral);
 /** Union of unsafe intervals clipped to corridor; visual-only never occupies safe space. */
 GUNSANDBALLOONS_API double UnsafeWidth(const FBalloonLevelPlan& Plan, double S, double HalfHeightCm, double BalloonRadiusCm);
 GUNSANDBALLOONS_API bool Build(const UBalloonLevelConfig& Config, FBalloonLevelPlan& Plan, TArray<FString>& Diagnostics);
}
