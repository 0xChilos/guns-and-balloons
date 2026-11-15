#include "../BalloonLevel/BalloonTerrainBuilder.h"
#include "../BalloonLevel/BalloonLevelConfig.h"
#include "../BalloonLevel/BalloonLevelGenerator.h"
#include "../BalloonLevel/BalloonLevelRealizer.h"
#include "Engine/World.h"
#include "ProceduralMeshComponent.h"
#include "Materials/Material.h"
#include "MaterialDomain.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBalloonLevelTerrainTest, "GunsAndBalloons.BalloonLevel.Terrain",
 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBalloonLevelTerrainTest::RunTest(const FString& Parameters)
{
 auto* Config = NewObject<UBalloonLevelConfig>();
 Config->LengthMeters = 100.f; Config->BaseWidthMeters = 100.f;
 Config->TerrainPaddingMeters = 10.f; Config->TerrainCellMeters = 10.f;
 Config->TerrainMaterial = UMaterial::GetDefaultMaterial(MD_Surface);
 FBalloonLevelPlan Plan;
 TArray<FString> Diagnostics;
 if (!TestTrue(TEXT("terrain geometry"), BalloonLevelPlanning::BuildStraightPlan(*Config, Plan, Diagnostics))) return false;
 BalloonTerrain::FMesh Mesh, Replay;
 if (!TestTrue(TEXT("build terrain"), BalloonTerrain::Build(*Config, Plan, Mesh, Diagnostics))) return false;
 BalloonTerrain::Build(*Config, Plan, Replay, Diagnostics);
 TestTrue(TEXT("same seed repeats terrain"), Mesh.Vertices == Replay.Vertices);
 FBox HorizonBounds(ForceInit);
 for (const FVector& V : Mesh.Vertices) HorizonBounds += V;
 TestTrue(TEXT("horizon extends at least 100 km beyond corridor on all sides"),
  HorizonBounds.Min.X < -9990000. && HorizonBounds.Min.Y < -9990000. &&
  HorizonBounds.Max.X > 10000000. && HorizonBounds.Max.Y > 10000000.);
 TestTrue(TEXT("horizon uses sparse geometry"), Mesh.Vertices.Num() < 1000);
 TestEqual(TEXT("each horizon vertex has UVs"), Mesh.Vertices.Num(), Mesh.UVs.Num());
 double Minimum = Mesh.Vertices[0].Z, Maximum = Minimum;
 for (const FVector& V : Mesh.Vertices) { Minimum = FMath::Min(Minimum, V.Z); Maximum = FMath::Max(Maximum, V.Z); }
 TestTrue(TEXT("generated terrain is uneven"), Maximum - Minimum > 1.);
 Config->Seed++;
 BalloonTerrain::Build(*Config, Plan, Replay, Diagnostics);
 TestFalse(TEXT("new seed changes terrain"), Mesh.Vertices == Replay.Vertices);
 Config->TerrainMode = EBalloonTerrainMode::Flat;
 BalloonTerrain::Build(*Config, Plan, Replay, Diagnostics);
 for (const FVector& V : Replay.Vertices) TestEqual(TEXT("flat mode uses ground datum"), V.Z, double(Config->PlacementGroundZMeters) * 100.);
 Config->TerrainPaddingMeters = 1000000.f;
 TestFalse(TEXT("huge terrain rejected before allocation"), BalloonTerrain::Build(*Config, Plan, Replay, Diagnostics));
 auto* World = UWorld::CreateWorld(EWorldType::Game, false);
 auto* Generator = World->SpawnActor<ABalloonLevelGenerator>();
 UProceduralMeshComponent* Component = nullptr;
 AActor* Terrain = BalloonTerrain::Spawn(*Generator, *Config, Mesh, Component, Diagnostics);
 if (TestNotNull(TEXT("terrain actor spawned"), Terrain))
 {
  TestTrue(TEXT("visible terrain section exists"), Component && Component->GetNumSections() == 1);
  FHitResult Hit;
  const FVector Vertex = Mesh.Vertices[0];
  FCollisionQueryParams Query(SCENE_QUERY_STAT(TerrainTest), true);
  TestTrue(TEXT("generated terrain has query collision"), Component->LineTraceComponent(Hit,
   FVector(Vertex.X + 1., Vertex.Y + 1., Maximum + 100.), FVector(Vertex.X + 1., Vertex.Y + 1., Minimum - 100.), Query));
  TestTrue(TEXT("terrain collision normal points up"), Hit.ImpactNormal.Z > 0.);
  Generator->Realizer->Actors.Add(Terrain);
  Generator->ClearGeneratedLevel();
  TestFalse(TEXT("clear removes terrain collision"), IsValid(Terrain) && Component->IsRegistered());
 }
 World->DestroyWorld(false);
 return true;
}
#endif
