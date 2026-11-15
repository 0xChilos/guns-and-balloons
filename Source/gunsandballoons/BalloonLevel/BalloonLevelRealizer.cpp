#include "BalloonLevelRealizer.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"
#include "BalloonBorderThreat.h"

namespace
{
void Remove(TArray<TObjectPtr<UHierarchicalInstancedStaticMeshComponent>>& Groups, TArray<TWeakObjectPtr<AActor>>& Actors)
{
 for(auto& A:Actors) if(A.IsValid()) A->Destroy();
 for(auto& C:Groups) if(IsValid(C)) { C->GetOwner()->RemoveInstanceComponent(C); C->DestroyComponent(); }
 Actors.Reset(); Groups.Reset();
}
}
void UBalloonLevelRealizer::Clear() { Remove(Groups,Actors); }
void UBalloonLevelRealizer::EndPlay(const EEndPlayReason::Type Reason) { Clear(); Super::EndPlay(Reason); }
void UBalloonLevelRealizer::OnComponentDestroyed(bool Hierarchy) { Clear(); Super::OnComponentDestroyed(Hierarchy); }
bool UBalloonLevelRealizer::Realize(const FBalloonLevelPlan& Plan,TArray<FString>& Diagnostics,UClass* BorderActorClass)
{
 check(IsInGameThread());
 const double Start=FPlatformTime::Seconds();
 TArray<TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> NewGroups;
 TArray<TWeakObjectPtr<AActor>> NewActors;
 TMap<UStaticMesh*,UHierarchicalInstancedStaticMeshComponent*> ByMesh;
 auto Fail=[&](const FString& Reason) { Diagnostics.Add(Reason); Remove(NewGroups,NewActors); return false; };
 if(!GetWorld() || !GetOwner()) return Fail(TEXT("M8: realization requires a world/owner"));
 int32 Instances=0;
 for(const auto& D:Plan.Placements)
 {
  UObject* Asset=D.Asset.TryLoad();
  if(!Asset) return Fail(FString::Printf(TEXT("M8 row=%s segment=%d: asset unavailable %s"),*D.RowName.ToString(),D.SegmentId,*D.Asset.ToString()));
  if(D.Representation==EBalloonPlacementRepresentation::HISM)
  {
   auto* Mesh=Cast<UStaticMesh>(Asset);
   if(!Mesh || D.Collision || (D.CullEndCm!=0 && D.CullEndCm!=15000 && D.CullEndCm!=30000 && D.CullEndCm!=80000)) return Fail(TEXT("M8: invalid HISM mesh/collision/cull descriptor"));
   auto* C=ByMesh.FindRef(Mesh);
   if(C && C->InstanceEndCullDistance!=D.CullEndCm) return Fail(TEXT("M8: same mesh has conflicting cull profiles"));
   if(!C)
   {
    C=NewObject<UHierarchicalInstancedStaticMeshComponent>(GetOwner(),NAME_None,RF_Transient | RF_DuplicateTransient);
    NewGroups.Add(C); GetOwner()->AddInstanceComponent(C);
    C->SetupAttachment(GetOwner()->GetRootComponent());
    C->SetStaticMesh(Mesh); C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    C->SetGenerateOverlapEvents(false); C->SetCanEverAffectNavigation(false);
    C->bAllowCullDistanceVolume=false; C->SetCullDistances(0,D.CullEndCm);
    C->RegisterComponent(); ByMesh.Add(Mesh,C);
   }
   if(C->AddInstance(D.Transform,true)==INDEX_NONE) return Fail(TEXT("M8: AddInstance failed"));
   ++Instances;
  }
  else
  {
   UClass* Class=D.Representation==EBalloonPlacementRepresentation::MeshActor ? AStaticMeshActor::StaticClass() : Cast<UClass>(Asset);
   if(!Class || !Class->IsChildOf(AActor::StaticClass()) || (D.Representation==EBalloonPlacementRepresentation::MeshActor && !Cast<UStaticMesh>(Asset))) return Fail(TEXT("M8: Actor descriptor asset has wrong type"));
   if(!D.Collision && D.Rule!=TEXT("flock")) return Fail(TEXT("M8: only flock may be a decorative Actor"));
   auto* A=GetWorld()->SpawnActorDeferred<AActor>(Class,D.Transform,GetOwner(),nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
   if(!A) return Fail(TEXT("M8: Actor spawn failed"));
   NewActors.Add(A); A->SetFlags(RF_Transient | RF_DuplicateTransient); A->Tags.AddUnique(TEXT("BalloonLevel.Generated"));
   if(auto* M=Cast<AStaticMeshActor>(A)) { M->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable); M->GetStaticMeshComponent()->SetStaticMesh(Cast<UStaticMesh>(Asset)); M->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAllDynamic")); }
   A->FinishSpawning(D.Transform);
   if(!IsValid(A)) return Fail(TEXT("M8: Actor destroyed during construction"));
   if (A->GetRootComponent() && A->GetRootComponent()->Mobility == EComponentMobility::Movable)
    A->AttachToActor(GetOwner(), FAttachmentTransformRules::KeepWorldTransform);
   A->SetActorEnableCollision(D.Collision);
   TInlineComponentArray<UPrimitiveComponent*> Primitives(A);
   bool HasCollision=false;
   for(auto* P:Primitives)
   {
    P->bAllowCullDistanceVolume=false; P->SetCullDistance(0.f);
    if(!D.Collision) P->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    HasCollision |= P->IsQueryCollisionEnabled();
   }
   if(D.Collision && !HasCollision) return Fail(TEXT("M8: gameplay Actor requires authored query collision"));
  }
 }
 // Border descriptors use the same staged ownership and rollback as other generated actors.
 if (BorderActorClass && (!BorderActorClass->IsChildOf(AActor::StaticClass()) || BorderActorClass->HasAnyClassFlags(CLASS_Abstract)))
  return Fail(TEXT("Borders: selected threat class must be a concrete Actor"));
 for (const auto& Border : Plan.Borders)
 {
  const FTransform Transform(FQuat::Identity, Border.Center);
  auto* Proxy = GetWorld()->SpawnActorDeferred<ABalloonBorderThreat>(ABalloonBorderThreat::StaticClass(), Transform,
      GetOwner(), nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
  if (!Proxy) return Fail(TEXT("Borders: influence proxy spawn failed"));
  NewActors.Add(Proxy);
  Proxy->SetFlags(RF_Transient | RF_DuplicateTransient);
  Proxy->Tags.AddUnique(TEXT("BalloonLevel.Generated"));
  if (!Proxy->InitializeInfluence(Border)) return Fail(TEXT("Borders: invalid influence descriptor"));
  Proxy->FinishSpawning(Transform);
  if (!IsValid(Proxy)) return Fail(TEXT("Borders: proxy destroyed during construction"));
  Proxy->AttachToActor(GetOwner(), FAttachmentTransformRules::KeepWorldTransform);
  if (BorderActorClass)
  {
   auto* Threat = GetWorld()->SpawnActorDeferred<AActor>(BorderActorClass, Transform, GetOwner(), nullptr,
       ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
   if (!Threat) return Fail(TEXT("Borders: rocket balloon spawn failed"));
   NewActors.Add(Threat);
   Threat->SetFlags(RF_Transient | RF_DuplicateTransient);
   Threat->Tags.AddUnique(TEXT("BalloonLevel.Generated"));
   // Set before construction/BeginPlay so no frame uses the Blueprint's wider default range.
   if (!Proxy->BindThreatActor(Threat)) return Fail(TEXT("Borders: rocket class requires floating-point AggroRangeM and DriftRangeM"));
   Threat->FinishSpawning(Transform);
   if (!IsValid(Threat) || !Proxy->BindThreatActor(Threat)) return Fail(TEXT("Borders: rocket balloon initialization failed"));
   Threat->AttachToActor(Proxy, FAttachmentTransformRules::KeepWorldTransform);
   // The certified influence supplies collision. Decorative mesh bounds must not add unplanned blockers.
   Threat->SetActorEnableCollision(false);
   if (Border.Policy != EBalloonBorderInfluencePolicy::Damaging) Threat->SetActorTickEnabled(false);
  }
 }
 Clear(); Groups=MoveTemp(NewGroups); Actors=MoveTemp(NewActors);
 LastMilliseconds=(FPlatformTime::Seconds()-Start)*1000.;
 Diagnostics.Add(FString::Printf(TEXT("M8 realized seed=%d descriptors=%d HISM=%d instances=%d Actors=%d time=%.3fms (includes synchronous loading)"),Plan.Seed,Plan.Placements.Num(),Groups.Num(),Instances,Actors.Num(),LastMilliseconds));
 return true;
}
