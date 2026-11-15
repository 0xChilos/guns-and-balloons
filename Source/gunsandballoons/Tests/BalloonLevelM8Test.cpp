#include "../BalloonLevel/BalloonLevelGenerator.h"
#include "../BalloonLevel/BalloonLevelRealizer.h"
#include "../BalloonLevel/BalloonBorderThreat.h"
#include "Components/SphereComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Engine/StaticMeshSocket.h"
#include "../HookPickup.h"
#include "Components/StaticMeshComponent.h"
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
 // Authored loot sockets work on individual HISM instances and gameplay mesh actors.
 auto* SocketMesh=DuplicateObject<UStaticMesh>(Mesh,GetTransientPackage());
 auto* Socket=NewObject<UStaticMeshSocket>(SocketMesh);
 Socket->SocketName=TEXT("loot_01"); Socket->RelativeLocation=FVector(0,0,200);
 SocketMesh->Sockets.Add(Socket);
 auto LootDescriptor=Scenery; LootDescriptor.Asset=FSoftObjectPath(SocketMesh);
 LootDescriptor.Transform=FTransform(FRotator(0,45,0),FVector(1000,2000,3000),FVector(2));
 for (auto Representation:{EBalloonPlacementRepresentation::HISM,EBalloonPlacementRepresentation::MeshActor})
 {
  LootDescriptor.Representation=Representation;
  LootDescriptor.Collision=Representation==EBalloonPlacementRepresentation::MeshActor;
  Plan.Placements={LootDescriptor};
  TestTrue(TEXT("realize socket loot"),G->Realizer->Realize(Plan,Diag));
  AHookPickup* Pickup=nullptr;
  for (const auto& Ref:G->Realizer->Actors) if (auto* P=Cast<AHookPickup>(Ref.Get())) Pickup=P;
  TestNotNull(TEXT("socket has a hook pickup"),Pickup);
  if (Pickup)
  {
   TestTrue(TEXT("socket transform includes host scale"),Pickup->GetActorLocation().Equals(LootDescriptor.Transform.TransformPosition(Socket->RelativeLocation)));
   TestTrue(TEXT("pickup keeps authored size"),Pickup->GetActorScale3D().Equals(FVector::OneVector));
   TestTrue(TEXT("pickup hookable with query collision"),Pickup->CanBeHooked() && Pickup->Mesh->IsQueryCollisionEnabled());
   TestTrue(TEXT("pickup contains resources"),Pickup->Contents.Num()>0);
   TestTrue(TEXT("pickup stays on its platform"),Pickup->bFloating && !Pickup->Mesh->IsSimulatingPhysics());
   auto Weak=TWeakObjectPtr<AHookPickup>(Pickup);
   G->Realizer->Clear();
   TestFalse(TEXT("cleanup removes socket loot"),Weak.IsValid());
  }
 }
 // Density controls socket occupancy without changing scenery or breaking replacement rollback.
 LootDescriptor.Representation=EBalloonPlacementRepresentation::HISM; LootDescriptor.Collision=false;
 Plan.Placements.Reset();
 for (int32 I=0; I<100; ++I)
 {
  LootDescriptor.Transform.SetLocation(FVector(I*1000,0,0));
  Plan.Placements.Add(LootDescriptor);
 }
 TestTrue(TEXT("zero item density"),G->Realizer->Realize(Plan,Diag,nullptr,0.f));
 TestEqual(TEXT("zero has no items"),G->Realizer->Actors.Num(),0);
 TestEqual(TEXT("density preserves scenery"),G->Realizer->Groups[0]->GetInstanceCount(),100);
 TestTrue(TEXT("partial item density"),G->Realizer->Realize(Plan,Diag,nullptr,.5f));
 const int32 PartialCount=G->Realizer->Actors.Num();
 TestTrue(TEXT("half density populates a subset"),PartialCount>0 && PartialCount<100);
 TArray<FVector> Locations;
 for (const auto& Ref:G->Realizer->Actors) Locations.Add(Ref->GetActorLocation());
 TestTrue(TEXT("repeat partial item density"),G->Realizer->Realize(Plan,Diag,nullptr,.5f));
 TestEqual(TEXT("seed keeps item count stable"),G->Realizer->Actors.Num(),PartialCount);
 for (int32 I=0; I<FMath::Min(PartialCount,G->Realizer->Actors.Num()); ++I)
  TestTrue(TEXT("seed keeps occupied sockets stable"),Locations[I].Equals(G->Realizer->Actors[I]->GetActorLocation()));
 const auto PreviousLoot=G->Realizer->Actors;
 TestFalse(TEXT("invalid density rejected"),G->Realizer->Realize(Plan,Diag,nullptr,2.f));
 TestTrue(TEXT("invalid density preserves old items"),PreviousLoot==G->Realizer->Actors);
 TestTrue(TEXT("full item density"),G->Realizer->Realize(Plan,Diag,nullptr,1.f));
 TestEqual(TEXT("full density fills every spot"),G->Realizer->Actors.Num(),100);
 // Borders are realized and replaced transactionally, even without optional visual content.
 Plan.Placements.Reset();
 FBalloonBorderInfluence Border; Border.InstanceId=7; Border.Center=FVector(10000.,30000.,10000.); Border.RadiusCm=2500.f;
 Plan.Borders={Border};
 TestTrue(TEXT("realize generated border"),G->Realizer->Realize(Plan,Diag));
 TestEqual(TEXT("one border proxy"),G->Realizer->Actors.Num(),1);
 auto* Proxy=Cast<ABalloonBorderThreat>(G->Realizer->Actors[0].Get());
 TestNotNull(TEXT("border influence actor"),Proxy);
 if(Proxy)
 {
  TestTrue(TEXT("border at planned location"),Proxy->GetActorLocation().Equals(Border.Center));
  TestEqual(TEXT("border at planned radius"),Proxy->InfluenceCollision->GetUnscaledSphereRadius(),Border.RadiusCm);
 }
 const auto OldBorder=G->Realizer->Actors[0];
 TestFalse(TEXT("incompatible rocket class rolls back"),G->Realizer->Realize(Plan,Diag,AActor::StaticClass()));
 TestTrue(TEXT("failed border replacement retains old proxy"),OldBorder.IsValid() && G->Realizer->Actors[0]==OldBorder);
 Plan.Borders[0].RadiusCm=-1.f;
 TestFalse(TEXT("invalid border rolls back"),G->Realizer->Realize(Plan,Diag));
 TestTrue(TEXT("invalid border retains old proxy"),OldBorder.IsValid());
 Plan.Borders[0]=Border; Plan.Borders[0].Center.X+=5000.;
 TestTrue(TEXT("regenerate borders"),G->Realizer->Realize(Plan,Diag));
 TestFalse(TEXT("regeneration destroys old border"),OldBorder.IsValid());
 TestEqual(TEXT("regeneration does not accumulate borders"),G->Realizer->Actors.Num(),1);
 Plan.Borders.Reset();
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
