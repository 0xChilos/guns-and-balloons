#pragma once
#include "CoreMinimal.h"
#include "BalloonLevelTypes.h"

class UBalloonLevelConfig;
class UProceduralMeshComponent;
class AActor;

namespace BalloonTerrain
{
 struct FMesh
 {
  TArray<FVector> Vertices;
  TArray<int32> Triangles;
  TArray<FVector> Normals;
  TArray<FVector2D> UVs;
 };
 /** Pure deterministic mesh construction in world coordinates, with bounded allocation. */
 bool Build(const UBalloonLevelConfig& Config, const FBalloonLevelPlan& Plan, FMesh& Mesh, TArray<FString>& Diagnostics);
 /** Creates a staged actor: caller owns rollback until it hands the actor to the realizer. */
 AActor* Spawn(AActor& Owner, const UBalloonLevelConfig& Config, const FMesh& Mesh, UProceduralMeshComponent*& Component, TArray<FString>& Diagnostics);
}
