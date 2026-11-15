#include "BalloonLevelRealizer.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/ActorComponent.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "HAL/PlatformTime.h"
#include "BalloonBorderThreat.h"
#include "../HookPickup.h"
#include "Components/SkinnedMeshComponent.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/UnrealType.h"

namespace
{
void Remove(TArray<TObjectPtr<UHierarchicalInstancedStaticMeshComponent>>& Groups, TArray<TWeakObjectPtr<AActor>>& Actors)
{
 for(auto& A:Actors) if(A.IsValid()) A->Destroy();
 for(auto& C:Groups) if(IsValid(C)) { C->GetOwner()->RemoveInstanceComponent(C); C->DestroyComponent(); }
 Actors.Reset(); Groups.Reset();
}
}
UBalloonLevelRealizer::UBalloonLevelRealizer()
{
 const TCHAR* MeshNames[] = {TEXT("AmmoBox"), TEXT("MoneyPouch"), TEXT("Log"), TEXT("Coal"), TEXT("Crystal")};
 const TCHAR* SoundNames[] = {TEXT("Ammo"), TEXT("Money"), TEXT("Wood"), TEXT("Coal"), TEXT("Crystal")};
 for (int32 I=0; I<5; ++I)
 {
  ConstructorHelpers::FObjectFinder<UStaticMesh> Mesh(*FString::Printf(TEXT("/Game/Assets/Locations/Loot/SM_Loot_%s"), MeshNames[I]));
  ConstructorHelpers::FObjectFinder<USoundBase> Sound(*FString::Printf(TEXT("/Game/Audio/Sounds/SFX_Loot_%s"), SoundNames[I]));
  LootMeshes.Add(Mesh.Object); LootSounds.Add(Sound.Object);
 }
 ConstructorHelpers::FObjectFinder<UNiagaraSystem> Sparkle(TEXT("/Game/Assets/Locations/Blueprints/NS_LootSparkle"));
 LootSparkle=Sparkle.Object;
}
void UBalloonLevelRealizer::Clear() { Remove(Groups,Actors); }
void UBalloonLevelRealizer::EndPlay(const EEndPlayReason::Type Reason) { Clear(); Super::EndPlay(Reason); }
void UBalloonLevelRealizer::OnComponentDestroyed(bool Hierarchy) { Clear(); Super::OnComponentDestroyed(Hierarchy); }
bool UBalloonLevelRealizer::Realize(const FBalloonLevelPlan& Plan,TArray<FString>& Diagnostics,UClass* BorderActorClass,float ItemDensity)
{
 check(IsInGameThread());
 const double Start=FPlatformTime::Seconds();
 TArray<TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> NewGroups;
 TArray<TWeakObjectPtr<AActor>> NewActors;
 TMap<UStaticMesh*,UHierarchicalInstancedStaticMeshComponent*> ByMesh;
 auto Fail=[&](const FString& Reason) { Diagnostics.Add(Reason); Remove(NewGroups,NewActors); return false; };
 if(!GetWorld() || !GetOwner()) return Fail(TEXT("M8: realization requires a world/owner"));
 if (!FMath::IsFinite(ItemDensity) || ItemDensity < 0.f || ItemDensity > 1.f)
  return Fail(TEXT("Loot: item density must be finite and in [0,1]"));
 FRandomStream LootRandom(Plan.Seed);
 FRandomStream LootOccupancy(Plan.Seed);
 auto SpawnLoot=[&](const FTransform& Spot, USceneComponent* Parent, FName Socket)
 {
  const int32 Kind=LootRandom.RandRange(0,4);
  // Separate streams keep item types and nested occupied subsets stable as density changes.
  const float Roll=LootOccupancy.FRand();
  if (ItemDensity <= 0.f || Roll >= ItemDensity) return true;
  if (!LootMeshes[Kind]) return false;
  // Socket scale belongs to the host; pickups use their authored size and base-centred pivot.
  FTransform Transform(Spot.GetRotation(), Spot.GetLocation(), FVector::OneVector);
  auto* Pickup=GetWorld()->SpawnActorDeferred<AHookPickup>(AHookPickup::StaticClass(),Transform,GetOwner(),nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
  if (!Pickup) return false;
  NewActors.Add(Pickup);
  Pickup->SetFlags(RF_Transient | RF_DuplicateTransient);
  Pickup->Tags.AddUnique(TEXT("BalloonLevel.Generated"));
  Pickup->Tags.AddUnique(TEXT("BalloonLevel.Loot"));
  Pickup->Mesh->SetStaticMesh(LootMeshes[Kind]);
  Pickup->Mesh->EmptyOverrideMaterials();
  Pickup->bFloating=true;
  Pickup->bConsumeOnDelivery=true;
  Pickup->SparkleSystem=LootSparkle;
  Pickup->GrabSound=LootSounds[Kind];
  const FLinearColor Colors[]={FLinearColor(1,.7f,.2f),FLinearColor(1,.85f,.25f),FLinearColor(.6f,1,.4f),FLinearColor(1,.35f,.1f),FLinearColor(.35f,.9f,1)};
  Pickup->Color=Colors[Kind];
  FResourceYield Yield; Yield.ResourceType=ResourceTypes::All[Kind];
  if (Kind==0) Yield.MaxAmount=3;
  Pickup->Contents.Add(Yield);
  if (Kind==0) { Yield.ResourceType=EResourceType::Money; Yield.MaxAmount=10; Pickup->Contents.Add(Yield); }
  Pickup->FinishSpawning(Transform);
  if (!IsValid(Pickup)) return false;
  Pickup->SetActorScale3D(FVector::OneVector);
  Pickup->AttachToComponent(Parent,FAttachmentTransformRules::KeepWorldTransform,Socket);
  return true;
 };
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
   // HISM sockets need the individual instance transform, not the group's transform.
   for (FName Socket:C->GetAllSocketNames())
    if (Socket.ToString().StartsWith(TEXT("loot_")))
     if (!SpawnLoot(C->GetSocketTransform(Socket,RTS_Component)*D.Transform,GetOwner()->GetRootComponent(),NAME_None))
      return Fail(TEXT("Loot: failed to spawn socket pickup"));
  }
  else
  {
   UClass* Class=D.Representation==EBalloonPlacementRepresentation::MeshActor ? AStaticMeshActor::StaticClass() : Cast<UClass>(Asset);
   if(!Class || !Class->IsChildOf(AActor::StaticClass()) || (D.Representation==EBalloonPlacementRepresentation::MeshActor && !Cast<UStaticMesh>(Asset))) return Fail(TEXT("M8: Actor descriptor asset has wrong type"));
   if(!D.Collision && D.Rule!=TEXT("flock")) return Fail(TEXT("M8: only flock may be a decorative Actor"));
   // Generated levels own their loot: the realizer spawns AHookPickup on every loot_* socket.
   // Blueprint structures may carry AC_LootSpots, which would spawn a second item per socket.
   // Zero its SpawnChance on the class templates for this synchronous spawn, then shut the
   // instance component down right after construction, before its first drop timer fires.
   auto IsLootSpawner=[](const UActorComponent* C) { return C && C->GetClass()->GetName().StartsWith(TEXT("AC_LootSpots")); };
   auto ChanceProperty=[](const UActorComponent* C) { return FindFProperty<FNumericProperty>(C->GetClass(),TEXT("SpawnChance")); };
   TArray<TPair<UActorComponent*,double>> Suppressed;
   if (D.Representation==EBalloonPlacementRepresentation::BlueprintActor)
    if (UBlueprintGeneratedClass* BP=Cast<UBlueprintGeneratedClass>(Class))
     for (UClass* C=BP; C; C=C->GetSuperClass())
      if (UBlueprintGeneratedClass* Base=Cast<UBlueprintGeneratedClass>(C))
       if (Base->SimpleConstructionScript)
        for (USCS_Node* Node: Base->SimpleConstructionScript->GetAllNodes())
         if (UActorComponent* Template=Node ? Node->GetActualComponentTemplate(BP) : nullptr)
          if (IsLootSpawner(Template))
           if (FNumericProperty* Chance=ChanceProperty(Template))
           {
            void* Value=Chance->ContainerPtrToValuePtr<void>(Template);
            Suppressed.Emplace(Template,Chance->GetFloatingPointPropertyValue(Value));
            Chance->SetFloatingPointPropertyValue(Value,0.);
           }
   auto RestoreTemplates=[&]
   {
    for(const auto& S:Suppressed) if(FNumericProperty* Chance=ChanceProperty(S.Key))
     Chance->SetFloatingPointPropertyValue(Chance->ContainerPtrToValuePtr<void>(S.Key),S.Value);
    Suppressed.Reset();
   };
   auto* A=GetWorld()->SpawnActorDeferred<AActor>(Class,D.Transform,GetOwner(),nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
   if(!A) { RestoreTemplates(); return Fail(TEXT("M8: Actor spawn failed")); }
   NewActors.Add(A); A->SetFlags(RF_Transient | RF_DuplicateTransient); A->Tags.AddUnique(TEXT("BalloonLevel.Generated"));
   if(auto* M=Cast<AStaticMeshActor>(A)) { M->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable); M->GetStaticMeshComponent()->SetStaticMesh(Cast<UStaticMesh>(Asset)); M->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAllDynamic")); }
   A->FinishSpawning(D.Transform);
   RestoreTemplates();
   if(!IsValid(A)) return Fail(TEXT("M8: Actor destroyed during construction"));
   TInlineComponentArray<UActorComponent*> Owned(A);
   for(UActorComponent* C:Owned) if(IsLootSpawner(C))
   {
    if(FNumericProperty* Chance=ChanceProperty(C)) Chance->SetFloatingPointPropertyValue(Chance->ContainerPtrToValuePtr<void>(C),0.);
    if(FBoolProperty* Respawn=FindFProperty<FBoolProperty>(C->GetClass(),TEXT("bRespawn"))) Respawn->SetPropertyValue_InContainer(C,false);
    if(FArrayProperty* Array=FindFProperty<FArrayProperty>(C->GetClass(),TEXT("LootActors")))
     if(FObjectProperty* Entry=CastField<FObjectProperty>(Array->Inner))
     {
      TArray<AActor*> Dropped;
      FScriptArrayHelper Loot(Array,Array->ContainerPtrToValuePtr<void>(C));
      for(int32 I=0; I<Loot.Num(); ++I)
       if(AActor* Spawned=Cast<AActor>(Entry->GetObjectPropertyValue(Loot.GetRawPtr(I)))) Dropped.AddUnique(Spawned);
      for(AActor* Spawned:Dropped) Spawned->Destroy();
     }
    C->DestroyComponent();
   }
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
   TInlineComponentArray<USceneComponent*> Components(A);
   for (auto* C:Components)
   {
    if (!Cast<UStaticMeshComponent>(C) && !Cast<USkinnedMeshComponent>(C)) continue;
    // A Skeleton socket and a mesh socket may share a name: one pickup per spot.
    TSet<FName> Spots;
    for (FName Socket:C->GetAllSocketNames())
     if (Socket.ToString().StartsWith(TEXT("loot_")) && !Spots.Contains(Socket))
     {
      Spots.Add(Socket);
      if (!SpawnLoot(C->GetSocketTransform(Socket),C,Socket))
       return Fail(TEXT("Loot: failed to spawn socket pickup"));
     }
   }
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
