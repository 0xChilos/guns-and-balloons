#include "../BalloonLevel/BalloonLevelGenerator.h"
#include "../BalloonLevel/BalloonLevelRealizer.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "HAL/PlatformMisc.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBalloonLevelM8Test,"GunsAndBalloons.BalloonLevel.M8.Realization",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FBalloonLevelM8Test::RunTest(const FString& Parameters)
{
 auto* W=UWorld::CreateWorld(EWorldType::Game,false);
 auto* G=W->SpawnActor<ABalloonLevelGenerator>();
 auto* Authored=W->SpawnActor<AActor>();
 auto* Mesh=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
 TestNotNull(TEXT("real engine mesh"),Mesh);
 FBalloonLevelPlan Plan;Plan.Seed=12345;
 FBalloonPlacementDescriptor D;D.Asset=FSoftObjectPath(Mesh);D.Rule=TEXT("scatter");D.Transform=FTransform::Identity;
 TArray<FString> Diag;
 for(int32 Cull:{15000,30000,80000,0})
 {
  Plan.Placements={D,D};for(auto& P:Plan.Placements)P.CullEndCm=Cull;
  TestTrue(TEXT("realize"),G->Realizer->Realize(Plan,Diag));
  TestEqual(TEXT("one HISM per mesh"),G->Realizer->Groups.Num(),1);
  auto* H=G->Realizer->Groups[0].Get();
  TestEqual(TEXT("instances"),H->GetInstanceCount(),2);
  TestEqual(TEXT("cull end applied"),H->InstanceEndCullDistance,Cull);
  TestTrue(TEXT("scenery no collision"),H->GetCollisionEnabled()==ECollisionEnabled::NoCollision);
 }
 Plan.Placements[1].CullEndCm=15000;
 TestFalse(TEXT("conflicting mesh culls fail transaction"),G->Realizer->Realize(Plan,Diag));
 TestEqual(TEXT("old group retained"),G->Realizer->Groups.Num(),1);
 D.Collision=true;D.Representation=EBalloonPlacementRepresentation::MeshActor;Plan.Placements={D,D};
 TestTrue(TEXT("gameplay meshes are Actors"),G->Realizer->Realize(Plan,Diag));
 TestEqual(TEXT("no gameplay HISM"),G->Realizer->Groups.Num(),0);
 TestEqual(TEXT("two Actors"),G->Realizer->Actors.Num(),2);
 for(auto& Ref:G->Realizer->Actors)
 {
  TestTrue(TEXT("owned colliding Actor"),Ref.IsValid() && Ref->GetOwner()==G && Ref->GetActorEnableCollision());
  TInlineComponentArray<UPrimitiveComponent*> Components(Ref.Get());
  for(auto* C:Components) TestEqual(TEXT("no Actor distance cull"),C->LDMaxDrawDistance,0.f);
 }
 const auto OldActors=G->Realizer->Actors;
 // Allocate both staging representations before failing; neither may leak or replace the old level.
 auto Scenery=D;Scenery.Collision=false;Scenery.Representation=EBalloonPlacementRepresentation::HISM;
 auto Invalid=D;Invalid.Asset=FSoftObjectPath();
 Plan.Placements={Scenery,D,Invalid};
 TestFalse(TEXT("partial realization rolls back"),G->Realizer->Realize(Plan,Diag));
 TestEqual(TEXT("rollback removes staged HISM"),G->Realizer->Groups.Num(),0);
 TestEqual(TEXT("rollback retains actor count"),G->Realizer->Actors.Num(),OldActors.Num());
 for(int32 I=0;I<OldActors.Num();++I)
  TestTrue(TEXT("rollback retains exact colliding Actors"),OldActors[I].IsValid() && G->Realizer->Actors[I]==OldActors[I] && OldActors[I]->GetActorEnableCollision());
 // Representative content-free workload: actual component allocation and insertion, no GPU claim.
 D.Collision=false;D.Representation=EBalloonPlacementRepresentation::HISM;D.CullEndCm=30000;
 Plan.Placements.Reset();for(int32 I=0;I<10000;++I){D.Transform.SetLocation(FVector(I%100*200,I/100*200,0));Plan.Placements.Add(D);}
 TestTrue(TEXT("10000-instance profile"),G->Realizer->Realize(Plan,Diag));
 for(const auto& Ref:OldActors) TestFalse(TEXT("successful replacement destroys old Actors"),Ref.IsValid());
 AddInfo(FString::Printf(TEXT("CPU=%s seed=12345 instances=10000 groups=1 realization=%.3fms NullRHI"),*FPlatformMisc::GetCPUBrand(),G->Realizer->LastMilliseconds));
 G->ClearGeneratedLevel();G->ClearGeneratedLevel();
 TestTrue(TEXT("authored Actor survives cleanup"),IsValid(Authored));
 TestEqual(TEXT("owned groups cleared"),G->Realizer->Groups.Num(),0);
 TestEqual(TEXT("owned Actors cleared"),G->Realizer->Actors.Num(),0);
 Plan.Placements={D};TestTrue(TEXT("recreate before owner deletion"),G->Realizer->Realize(Plan,Diag));
 auto WeakGroup=TWeakObjectPtr<UHierarchicalInstancedStaticMeshComponent>(G->Realizer->Groups[0].Get());
 G->Destroy();
 TestTrue(TEXT("owner deletion unregisters generated component"),!WeakGroup.IsValid() || !WeakGroup->IsRegistered());
 W->DestroyWorld(false);return true;
}
#endif
