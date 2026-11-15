#include "BalloonTerrainBuilder.h"
#include "BalloonLevelConfig.h"
#include "ProceduralMeshComponent.h"
#include "Engine/World.h"
#include "Materials/Material.h"
#include "MaterialDomain.h"

namespace
{
 // Prototype horizon distances (metres), requiring art tuning. Sparse rings avoid
 // increasing the detailed terrain grid or moving the playable corridor borders.
 constexpr double HorizonDistancesMeters[] = {500., 2000., 10000., 100000.};
}

bool BalloonTerrain::Build(const UBalloonLevelConfig& Config, const FBalloonLevelPlan& Plan, FMesh& Mesh, TArray<FString>& Diagnostics)
{
 Mesh = FMesh();
 if (!Config.Validate(Diagnostics)) return false;
 FBox Bounds(ForceInit);
 for (const auto& Segment : Plan.Segments) Bounds += Segment.BaseBounds;
 if (!Bounds.IsValid) { Diagnostics.Add(TEXT("Terrain: corridor bounds are empty")); return false; }
 Bounds = Bounds.ExpandBy(Config.TerrainPaddingMeters * 100.);
 const double Cell = Config.TerrainCellMeters * 100.;
 const double NX = FMath::Max(1., FMath::CeilToDouble(Bounds.GetSize().X / Cell));
 const double NY = FMath::Max(1., FMath::CeilToDouble(Bounds.GetSize().Y / Cell));
 if (!FMath::IsFinite(NX * NY) || (NX + 1.) * (NY + 1.) + 8. * (NX + NY) > 250000.)
 { Diagnostics.Add(TEXT("Terrain: vertex limit exceeded (250000); increase TerrainCellMeters or reduce padding")); return false; }
 const int32 XCount = int32(NX), YCount = int32(NY);
 const double StepX = Bounds.GetSize().X / XCount, StepY = Bounds.GetSize().Y / YCount;
 FRandomStream Random(Config.Seed);
 const FVector2D NoiseOffset(Random.FRandRange(-1000.f, 1000.f), Random.FRandRange(-1000.f, 1000.f));
 const FVector Origin = Config.StartTransform.GetLocation();
 const FQuat InverseYaw = FRotator(0., -Config.StartTransform.Rotator().Yaw, 0.).Quaternion();
 Mesh.Vertices.Reserve((XCount + 1) * (YCount + 1));
 for (int32 Y = 0; Y <= YCount; ++Y)
 for (int32 X = 0; X <= XCount; ++X)
 {
  FVector V(Bounds.Min.X + X * StepX, Bounds.Min.Y + Y * StepY, Config.PlacementGroundZMeters * 100.);
  const FVector Local = InverseYaw.RotateVector(V - Origin);
  const FVector2D P = FVector2D(Local.X, Local.Y) / (Config.TerrainWavelengthMeters * 100.) + NoiseOffset;
  if (Config.TerrainMode == EBalloonTerrainMode::Generated)
   V.Z += Config.TerrainHeightVariationMeters * 100. * (0.75 * FMath::PerlinNoise2D(P) + 0.25 * FMath::PerlinNoise2D(P * 2.));
  Mesh.Vertices.Add(V);
  Mesh.UVs.Add(FVector2D(Local.X, Local.Y) / 1000.);
 }
 for (int32 Y = 0; Y < YCount; ++Y)
 for (int32 X = 0; X < XCount; ++X)
 {
  const int32 A = Y * (XCount + 1) + X, B = A + 1, C = A + XCount + 1, D = C + 1;
  Mesh.Triangles.Append({A, C, B, B, C, D});
 }
 // Stitch the exact grid perimeter to sparse, progressively wider horizon rings.
 // The shared inner vertices prevent cracks, even on seeded rolling terrain.
 TArray<int32> Perimeter;
 for (int32 X = 0; X < XCount; ++X) Perimeter.Add(X);
 for (int32 Y = 0; Y < YCount; ++Y) Perimeter.Add(Y * (XCount + 1) + XCount);
 for (int32 X = XCount; X > 0; --X) Perimeter.Add(YCount * (XCount + 1) + X);
 for (int32 Y = YCount; Y > 0; --Y) Perimeter.Add(Y * (XCount + 1));
 const TArray<int32> InnerPerimeter = Perimeter;
 const FVector Center = Bounds.GetCenter();
 const FVector Extent = Bounds.GetExtent();
 for (double DistanceMeters : HorizonDistancesMeters)
 {
  TArray<int32> Outer;
  for (int32 Index : InnerPerimeter)
  {
   const FVector Inner = Mesh.Vertices[Index];
   FVector V = Inner;
   V.X = Center.X + (Inner.X - Center.X) * (1. + DistanceMeters * 100. / Extent.X);
   V.Y = Center.Y + (Inner.Y - Center.Y) * (1. + DistanceMeters * 100. / Extent.Y);
   V.Z = FMath::Lerp(Inner.Z, Config.PlacementGroundZMeters * 100., FMath::Min(1., DistanceMeters / 10000.));
   Outer.Add(Mesh.Vertices.Add(V));
   const FVector Local = InverseYaw.RotateVector(V - Origin);
   Mesh.UVs.Add(FVector2D(Local.X, Local.Y) / 1000.);
  }
  for (int32 I = 0; I < Perimeter.Num(); ++I)
  {
   const int32 Next = (I + 1) % Perimeter.Num();
   Mesh.Triangles.Append({Perimeter[I], Perimeter[Next], Outer[I], Perimeter[Next], Outer[Next], Outer[I]});
  }
  Perimeter = MoveTemp(Outer);
 }
 Mesh.Normals.Init(FVector::ZeroVector, Mesh.Vertices.Num());
 for (int32 I = 0; I < Mesh.Triangles.Num(); I += 3)
 {
  const int32 A = Mesh.Triangles[I], B = Mesh.Triangles[I + 1], C = Mesh.Triangles[I + 2];
  const FVector Normal = FVector::CrossProduct(Mesh.Vertices[C] - Mesh.Vertices[A], Mesh.Vertices[B] - Mesh.Vertices[A]);
  Mesh.Normals[A] += Normal; Mesh.Normals[B] += Normal; Mesh.Normals[C] += Normal;
 }
 for (FVector& Normal : Mesh.Normals) Normal.Normalize();
 return true;
}

AActor* BalloonTerrain::Spawn(AActor& Owner, const UBalloonLevelConfig& Config, const FMesh& Mesh, UProceduralMeshComponent*& Component, TArray<FString>& Diagnostics)
{
 Component = nullptr;
 FActorSpawnParameters Params;
 Params.Owner = &Owner;
 Params.ObjectFlags |= RF_Transient | RF_DuplicateTransient;
 Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
 AActor* Actor = Owner.GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, Params);
 if (!Actor) { Diagnostics.Add(TEXT("Terrain: actor spawn failed")); return nullptr; }
 Actor->Tags.Add(TEXT("BalloonLevel.Generated"));
 Actor->Tags.Add(TEXT("BalloonLevel.Terrain"));
 Component = NewObject<UProceduralMeshComponent>(Actor, NAME_None, RF_Transient | RF_DuplicateTransient);
 Actor->SetRootComponent(Component);
 Actor->AddInstanceComponent(Component);
 Component->bUseAsyncCooking = false;
 Component->bUseComplexAsSimpleCollision = true;
 Component->SetCollisionProfileName(TEXT("BlockAll"));
 Component->SetCanEverAffectNavigation(false);
 Component->RegisterComponent();
 Actor->AttachToActor(&Owner, FAttachmentTransformRules::KeepWorldTransform);
 Component->CreateMeshSection(0, Mesh.Vertices, Mesh.Triangles, Mesh.Normals, Mesh.UVs, {}, {}, true);
 UMaterialInterface* Material = Config.TerrainMaterial.LoadSynchronous();
 if (!Material && Config.TerrainMaterial.IsNull())
 {
  const FString Path = FString::Printf(TEXT("/Game/Assets/Locations/Materials/MI_%s_Ground.MI_%s_Ground"), *Config.BiomeId.ToString(), *Config.BiomeId.ToString());
  Material = LoadObject<UMaterialInterface>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
 }
 if (!Material)
 {
  Diagnostics.Add(TEXT("Terrain: ground material unavailable; using engine default surface"));
  Material = UMaterial::GetDefaultMaterial(MD_Surface);
 }
 Component->SetMaterial(0, Material);
#if WITH_EDITOR
 Actor->SetActorLabel(TEXT("Generated Terrain"));
#endif
 return Actor;
}
