#include "BalloonLevelGenerator.h"
#include "BalloonLevelRealizer.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "../NemesisAirship.h"
#include "../NemesisBeacon.h"
#include "../NemesisBirdFlock.h"
#include "../NemesisFlak.h"
#include "../NemesisTowerCannon.h"

void ABalloonLevelGenerator::SpawnLevelEnemies()
{
 if (!SpawnEnemies || !ActiveConfig || !GetWorld() || Plan.LengthCm <= 0.) return;
 FRandomStream Random(Plan.Seed ^ 0x454E454D);
 auto Spawn = [&](UClass* Class, const FVector& Position, const FRotator& Rotation) -> AActor*
 {
  const FTransform Transform(Rotation, Position);
  auto* Enemy = GetWorld()->SpawnActorDeferred<AActor>(Class, Transform, this, nullptr,
      ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
  if (!Enemy)
  {
   Diagnostics.Add(FString::Printf(TEXT("Enemies: failed to spawn %s"), *GetNameSafe(Class)));
   return nullptr;
  }
  Enemy->SetFlags(RF_Transient | RF_DuplicateTransient);
  Enemy->Tags.AddUnique(TEXT("BalloonLevel.Generated"));
  Enemy->Tags.AddUnique(TEXT("BalloonLevel.Enemy"));
  // The generated transform is authoritative, including when BeginPlay happens later in PIE.
  if (auto* Airship = Cast<ANemesisAirship>(Enemy)) Airship->SetStartDistance(0.f);
  Enemy->FinishSpawning(Transform);
  if (!IsValid(Enemy)) return nullptr;
  Realizer->Actors.Add(Enemy);
  // Flying enemies move independently; attachment is for editor organization only.
  if (!GetWorld()->IsGameWorld()) Enemy->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
  return Enemy;
 };
 auto AlongRoute = [&](UClass* Class, int32 RequestedCount, bool Edge, bool Ground)
 {
  const int32 Count = FMath::Clamp(RequestedCount, 0, 100);
  const double Clearance = FMath::Max(0.f, EnemyStartClearanceMeters) * 100.;
  if (Clearance >= Plan.LengthCm) return;
  for (int32 Index = 0; Index < Count; ++Index)
  {
   // Seeded jitter within separate route intervals avoids a rigid grid and preserves reproducibility.
   const double S = Clearance + (Plan.LengthCm - Clearance) * (Index + Random.FRandRange(.2f, .8f)) / Count;
   const FVector Center = BalloonSplinePlanning::PositionAt(Plan, S);
   FVector Forward = BalloonSplinePlanning::PositionAt(Plan, FMath::Min(S + 100., Plan.LengthCm)) -
       BalloonSplinePlanning::PositionAt(Plan, FMath::Max(0., S - 100.));
   Forward.Z = 0.;
   Forward = Forward.GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector);
   const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward);
   const double HalfWidth = BalloonSplinePlanning::WidthAt(Plan, S) * .5;
   const double Side = Edge ? HalfWidth : FMath::Min(double(FMath::Max(0.f, EnemySideOffsetMeters)) * 100., HalfWidth * .75);
   FVector Position = Center + Right * Side * (Index % 2 ? -1. : 1.);
   if (Class == ANemesisBirdFlock::StaticClass()) Position = Center;
   if (Ground)
   {
    TArray<FHitResult> Hits;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(LevelEnemyGround), true, RoutePawn);
    const FVector Top(Position.X, Position.Y, ActiveConfig->TerrainTraceTopMeters * 100.);
    const FVector Bottom(Position.X, Position.Y, ActiveConfig->TerrainTraceBottomMeters * 100.);
    GetWorld()->LineTraceMultiByObjectType(Hits, Top, Bottom, FCollisionObjectQueryParams(ECC_WorldStatic), Query);
    const auto* Hit = Hits.FindByPredicate([&](const FHitResult& H)
    {
     return H.GetActor() && (H.GetActor()->ActorHasTag(TEXT("BalloonLevel.Terrain")) ||
         H.GetActor()->ActorHasTag(ActiveConfig->TerrainActorTag));
    });
    if (!Hit)
    {
     Diagnostics.Add(FString::Printf(TEXT("Enemies: skipped %s; no tagged terrain at route %.1fm"), *GetNameSafe(Class), S / 100.));
     continue;
    }
    Position.Z = Hit->ImpactPoint.Z;
   }
   Spawn(Class, Position, (-Right * (Index % 2 ? -1. : 1.)).Rotation());
  }
 };
 if (SpawnSharedEnemies)
 {
  bool HasAirship = false;
  for (TActorIterator<ANemesisAirship> It(GetWorld()); It; ++It) { HasAirship = true; break; }
  if (!HasAirship)
   Spawn(ANemesisAirship::StaticClass(), Plan.Start.GetLocation() - Plan.Start.GetRotation().GetForwardVector() *
       FMath::Max(0.f, AirshipStartDistanceMeters) * 100. + FVector(0., 0., AirshipHeightOffsetMeters * 100.), Plan.Start.Rotator());
  // An authored border class already supplies edge enemies; do not double-populate the edges.
  if (ActiveConfig->BorderActorClass.IsNull() || Plan.Borders.IsEmpty()) AlongRoute(ANemesisFlak::StaticClass(), EdgeAACount, true, false);
 }
 if (SpawnBiomeEnemies)
 {
  if (ActiveConfig->BiomeId == TEXT("Field")) AlongRoute(ANemesisBirdFlock::StaticClass(), BirdFlockCount, false, false);
  else if (ActiveConfig->BiomeId == TEXT("Desert"))
  {
   AlongRoute(ANemesisTowerCannon::StaticClass(), DesertCannonCount, false, true);
   AlongRoute(ANemesisBeacon::StaticClass(), DesertBeaconCount, false, true);
  }
 }
}
