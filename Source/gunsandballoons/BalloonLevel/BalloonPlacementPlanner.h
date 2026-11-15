#pragma once
#include "BalloonLevelConfig.h"

namespace BalloonPlacementPlanning
{
 struct FResolvedRow
 {
  FBalloonCatalogEntry Entry;
  FBalloonPlacementMetadata Metadata;
  bool BlueprintActor = false;
  FBox LocalCollisionBounds = FBox(ForceInit);
 };
 struct FSurface { double Z = 0.; FVector Normal = FVector::UpVector; };
 using FGroundSampler = TFunction<bool(const FVector&, FSurface&)>;
 /** Game thread only. Resolves bounds and class kind; Build itself never loads UObjects. */
 GUNSANDBALLOONS_API bool Resolve(const UBalloonLevelConfig& Config, const TArray<FBalloonCatalogEntry>& Catalog,
  TArray<FResolvedRow>& Rows, TArray<FString>& Diagnostics);
 /** Pure transactional dispatcher. Empty sampler uses configured flat ground. All units in descriptors cm. */
 GUNSANDBALLOONS_API bool Build(const UBalloonLevelConfig& Config, FBalloonLevelPlan& Plan,
  const TArray<FResolvedRow>& Rows, TArray<FString>& Diagnostics, const FGroundSampler& Ground = FGroundSampler(),
  const TArray<TArray<FVector>>& FieldBorders = {}, const TArray<FVector>& ReservedRoute = {});
}
