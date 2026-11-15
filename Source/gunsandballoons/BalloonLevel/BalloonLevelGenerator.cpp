#include "BalloonLevelGenerator.h"
#include "BalloonEnvironmentPresetController.h"
#include "EngineUtils.h"
#include "BalloonLevelRealizer.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "BalloonAssetCatalog.h"
#include "BalloonBorderPlanner.h"
#include "BalloonPlacementPlanner.h"
#include "BalloonHorizontalRouteValidator.h"
#include "BalloonBorderThreat.h"
#include "DrawDebugHelpers.h"
#include "Components/SplineComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Components/SphereComponent.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "BalloonTerrainBuilder.h"
#include "ProceduralMeshComponent.h"
#include "Misc/ScopeExit.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/DataTable.h"

ABalloonLevelGenerator::ABalloonLevelGenerator()
{
	PrimaryActorTick.bCanEverTick = true;
 Realizer = CreateDefaultSubobject<UBalloonLevelRealizer>(TEXT("Realizer"));
 EventManager = CreateDefaultSubobject<UBalloonLevelEventManager>(TEXT("EventManager"));
	Spline = CreateDefaultSubobject<USplineComponent>(TEXT("ProgressionSpline"));
	SetRootComponent(Spline);
 CompletionZone = CreateDefaultSubobject<UBoxComponent>(TEXT("CompletionZone"));
 CompletionZone->SetupAttachment(Spline);
 CompletionZone->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 CompletionZone->SetGenerateOverlapEvents(false);
 CompletionZone->SetBoxExtent(FVector::ZeroVector);
 CompletionZone->ShapeColor = FColor::Green;
 FinishAirship = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FinishAirship"));
 FinishAirship->SetupAttachment(CompletionZone);
 FinishAirship->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 FinishAirship->SetGenerateOverlapEvents(false);
 FinishAirship->SetCanEverAffectNavigation(false);
 FinishAirship->SetVisibility(false);
 static ConstructorHelpers::FObjectFinder<UStaticMesh> FinishMesh(TEXT("/Game/Assets/Nemesis/SM_Nemesis_Airship.SM_Nemesis_Airship"));
 if (FinishMesh.Succeeded()) FinishAirship->SetStaticMesh(FinishMesh.Object);
	Spline->ClearSplinePoints();
 const auto* Defaults = GetDefault<ABalloonEnvironmentPresetController>();
 Day = Defaults->Day; Night = Defaults->Night; BiomeFog = Defaults->BiomeFog;
}

namespace
{
template<typename T>
T* ControllerActor(ABalloonLevelGenerator& Controller, T* Assigned, TArray<TObjectPtr<AActor>>& Owned, TArray<FString>& Diagnostics)
{
 T* Result = IsValid(Assigned) ? Assigned : nullptr;
 if (!Result)
 {
  for (TActorIterator<T> It(Controller.GetWorld()); It; ++It)
  {
   if (!IsValid(*It) || (It->GetOwner() && It->GetOwner() != &Controller)) continue;
   if (const auto* Environment = Cast<ABalloonEnvironmentPresetController>(static_cast<AActor*>(*It)))
    if (Environment->Generator && Environment->Generator != &Controller) continue;
   if (Result) { Diagnostics.Add(FString::Printf(TEXT("Controller: multiple %s actors; assign one explicitly"), *T::StaticClass()->GetName())); return nullptr; }
   Result = *It;
  }
 }
 if (!Result)
 {
  FActorSpawnParameters Params; Params.Owner = &Controller; Params.ObjectFlags |= RF_Transient;
  Result = Controller.GetWorld()->SpawnActor<T>(T::StaticClass(), FTransform::Identity, Params);
  if (Result) Owned.Add(Result);
 }
 if (Result && Result->GetRootComponent())
 {
  Result->GetRootComponent()->SetMobility(EComponentMobility::Movable);
  Result->AttachToActor(&Controller, FAttachmentTransformRules::KeepWorldTransform);
 }
 return Result;
}
}

void ABalloonLevelGenerator::Tick(float DeltaSeconds)
{
 Super::Tick(DeltaSeconds);
 if (!GetWorld()->IsGameWorld() || Status != EBalloonLevelGenerationStatus::Ready || LevelCompleted || !IsValid(RoutePawn)) return;
 const FVector Local = CompletionZone->GetComponentTransform().InverseTransformPosition(RoutePawn->GetActorLocation());
 if (FBox(-CompletionZone->GetUnscaledBoxExtent(), CompletionZone->GetUnscaledBoxExtent()).IsInsideOrOn(Local))
 {
  const FString Message=FString::Printf(TEXT("CompleteLevel area entered: balloon=%s position=%s generator=%s biome=%s seed=%d"),
   *GetNameSafe(RoutePawn.Get()), *RoutePawn->GetActorLocation().ToString(), *GetName(), *Plan.BiomeId.ToString(), Plan.Seed);
  UE_LOG(LogTemp, Display, TEXT("%s"), *Message);
  Diagnostics.Add(Message);
#if !UE_BUILD_SHIPPING
  if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, TEXT("Balloon entered CompleteLevel area"));
#endif
  CompleteLevel();
 }
}

void ABalloonLevelGenerator::CompleteLevel()
{
 if (Status != EBalloonLevelGenerationStatus::Ready || LevelCompleted) return;
 LevelCompleted = true;
 EventManager->PauseEventTimeline(true);
 OnLevelCompleted.Broadcast();
}

bool ABalloonLevelGenerator::ConfigureEnvironment()
{
 if (!ManageEnvironment || !GetWorld()) return !ManageEnvironment;
 if (!IsValid(EnvironmentController))
 {
  for (TActorIterator<ABalloonEnvironmentPresetController> It(GetWorld()); It; ++It)
  {
   if (It->Generator && It->Generator != this) continue;
   if (EnvironmentController) { Diagnostics.Add(TEXT("Controller: multiple environment actors; assign one explicitly")); EnvironmentController = nullptr; return false; }
   EnvironmentController = *It;
  }
 }
 EnvironmentController = ControllerActor(*this, EnvironmentController.Get(), OwnedEnvironmentActors, Diagnostics);
 if (!EnvironmentController) return false;
 // Preserve explicit legacy bindings when first consolidating an existing map.
 Sun = ControllerActor(*this, Sun ? Sun.Get() : EnvironmentController->Sun.Get(), OwnedEnvironmentActors, Diagnostics);
 Sky = ControllerActor(*this, Sky ? Sky.Get() : EnvironmentController->Sky.Get(), OwnedEnvironmentActors, Diagnostics);
 Fog = ControllerActor(*this, Fog ? Fog.Get() : EnvironmentController->Fog.Get(), OwnedEnvironmentActors, Diagnostics);
 if (!Sun || !Sky || !Fog) return false;
 EnvironmentController->Generator = this;
 EnvironmentController->ApplyOnBeginPlay = false;
 EnvironmentController->SetActorTickEnabled(false);
 EnvironmentController->Sun = Sun; EnvironmentController->Sky = Sky; EnvironmentController->Fog = Fog;
 const auto* Config = ActiveConfig ? ActiveConfig.Get() : LevelConfig.Get();
 const auto Mode = OverrideTerrain ? TerrainMode : (Config ? Config->TerrainMode : TerrainMode);
 if (!AuthoredGround && EnvironmentController->Ground) AuthoredGround = EnvironmentController->Ground;
 EnvironmentController->Ground = Mode == EBalloonTerrainMode::WorldCollision ? AuthoredGround.Get() : nullptr;
 if (OverrideEnvironmentSettings)
 { EnvironmentController->Day = Day; EnvironmentController->Night = Night; EnvironmentController->BiomeFog = BiomeFog; }
 return true;
}

void ABalloonLevelGenerator::SetupLevelController() { ConfigureEnvironment(); }

void ABalloonLevelGenerator::PreviewEnvironment()
{
 if (!GetWorld() || GetWorld()->IsGameWorld() || !ConfigureEnvironment() || !EnvironmentController) return;
 const auto* Config = BiomeSelection == EBalloonBiomeSelection::Selected ? LevelConfig.Get() : ActiveConfig.Get();
 if (!Config) { Diagnostics.Add(TEXT("Controller: select a config, or generate a random biome before previewing")); return; }
 EnvironmentController->ApplyEnvironment(Config->BiomeId, OverrideLightingPreset ? LightingPreset : Config->Preset);
 Diagnostics.Append(EnvironmentController->GetDiagnostics());
}

void ABalloonLevelGenerator::ClearOwnedEnvironment()
{
 for (auto& Actor : OwnedEnvironmentActors) if (IsValid(Actor)) Actor->Destroy();
 OwnedEnvironmentActors.Reset();
}

void ABalloonLevelGenerator::EndPlay(const EEndPlayReason::Type Reason)
{
 ClearOwnedEnvironment();
 Super::EndPlay(Reason);
}

void ABalloonLevelGenerator::BeginPlay()
{
 Super::BeginPlay();
 if (auto* Instance = GetGameInstance())
  if (auto* Launch = Instance->GetSubsystem<UBalloonLevelLaunchSubsystem>())
   if (Launch->Consume(StartOptions)) { UseStartOptions = true; GenerationMode = EBalloonGenerationMode::OnLevelStart; }
 if (GenerationMode == EBalloonGenerationMode::OnLevelStart)
 {
  StartingLevel = true;
  GenerateLevel();
  StartingLevel = false;
 }
}

void ABalloonLevelGenerator::ClearGeneratedLevel()
{
 FinishAirship->SetVisibility(false);
 LevelCompleted = false;
 CompletionZone->SetBoxExtent(FVector::ZeroVector);
 Realizer->Clear();
 EventManager->ResetEventTimeline();
 BorderProxies.Reset();
 ActiveConfig = nullptr;
	Spline->ClearSplinePoints();
	Plan = FBalloonLevelPlan();
	Progress = FBalloonLevelProgress();
	Diagnostics.Reset();
	Status = EBalloonLevelGenerationStatus::NotStarted;
}

bool ABalloonLevelGenerator::RegenerateLevel() { return GenerateLevel(); }

void ABalloonLevelGenerator::GenerateLevelInEditor() { GenerateLevel(); }
void ABalloonLevelGenerator::RegenerateLevelInEditor() { RegenerateLevel(); }

void ABalloonLevelGenerator::CheckSeedInEditor() { CheckSeed(); }

bool ABalloonLevelGenerator::CheckSeed()
{
 const auto PreviousPlan = Plan;
 const auto PreviousProgress = Progress;
 auto* PreviousConfig = ActiveConfig.Get();
 const auto PreviousStatus = Status;
 const bool PreviousRetryable = SeedRetryable;
 TArray<TPair<TWeakObjectPtr<AActor>, bool>> Collision;
 for (auto& Ref : Realizer->Actors)
  if (Ref.IsValid()) { Collision.Emplace(Ref, Ref->GetActorEnableCollision()); Ref->SetActorEnableCollision(false); }
 ON_SCOPE_EXIT
 {
  for (auto& Entry : Collision) if (Entry.Key.IsValid()) Entry.Key->SetActorEnableCollision(Entry.Value);
  Plan = PreviousPlan; Progress = PreviousProgress; ActiveConfig = PreviousConfig;
  Status = PreviousStatus; SeedRetryable = PreviousRetryable;
  Spline->ClearSplinePoints(false);
  for (const auto& Point : Plan.SplineSamples)
  {
   const int32 Index = Spline->GetNumberOfSplinePoints();
   Spline->AddSplinePoint(Point, ESplineCoordinateSpace::World, false);
   Spline->SetSplinePointType(Index, ESplinePointType::Linear, false);
  }
  Spline->UpdateSpline();
 };
 LastSeedCheckPassed = BuildAndRealizeLevel(false, true);
 Diagnostics.Add(LastSeedCheckPassed ? TEXT("Seed check passed; no level committed") : TEXT("Seed check failed; no level committed"));
 return LastSeedCheckPassed;
}

bool ABalloonLevelGenerator::GenerateLevel()
{
 if (StartingLevel && (LevelStartSeedRetries < 0 || LevelStartSeedRetries > 12))
 { Diagnostics = {TEXT("LevelStartSeedRetries must be [0,12]")}; return false; }
 const bool HadLevel = Status == EBalloonLevelGenerationStatus::Ready;
 const auto OldPlan = Plan;
 const auto OldProgress = Progress;
 auto* OldConfig = ActiveConfig.Get();
 TArray<TPair<TWeakObjectPtr<AActor>, bool>> Collision;
 for(auto& Ref : Realizer->Actors) if(Ref.IsValid()) { Collision.Emplace(Ref,Ref->GetActorEnableCollision()); Ref->SetActorEnableCollision(false); }
 bool Built = BuildAndRealizeLevel();
 TArray<FString> AttemptDiagnostics = Diagnostics;
 for (int32 Retry = 0; !Built && StartingLevel && Retry < LevelStartSeedRetries &&
      SeedRetryable; ++Retry)
 {
  // Keep the biome and resolved overrides; change only the snapshot seed.
  Built = BuildAndRealizeLevel(true);
  AttemptDiagnostics.Append(Diagnostics);
 }
 Diagnostics = MoveTemp(AttemptDiagnostics);
 if(Built)
 {
  LevelCompleted = false;
  CompletionZone->SetWorldLocationAndRotation(Plan.Finish.GetLocation(), Plan.Finish.GetRotation());
  CompletionZone->SetWorldScale3D(FVector::OneVector);
  CompletionZone->SetBoxExtent(FVector(FMath::Max(1.f, CompletionZoneHalfDepthMeters) * 100.f,
   GetWidthAtDistanceCm(Plan.LengthCm) * 1.25f * .5f, FMath::Max(1000., Plan.BalloonHalfHeightCm)));
  // Place the whole mesh beyond the far face of the completion zone, accounting for its pivot.
  FTransform MeshShape = FinishAirship->GetRelativeTransform();
  MeshShape.SetTranslation(FVector::ZeroVector);
  const double MeshMinX = FinishAirship->GetStaticMesh()
   ? FinishAirship->GetStaticMesh()->GetBoundingBox().TransformBy(MeshShape).Min.X : 0.;
  FinishAirship->SetRelativeLocation(FVector(CompletionZone->GetUnscaledBoxExtent().X - MeshMinX +
   FMath::Max(0.f, FinishAirshipGapMeters) * 100., 0., 0.));
  FinishAirship->SetVisibility(true);
  EventManager->ResetEventTimeline();
  BorderProxies.Reset();
  for (const auto& Actor : Realizer->Actors)
   if (auto* Proxy = Cast<ABalloonBorderThreat>(Actor.Get())) RegisterBorderProxy(Proxy);
  // All entry points share this path: Details, Blueprint, BeginPlay and regeneration.
  // Use the successful snapshot, including the config chosen by Random mode.
  if (ManageEnvironment) ConfigureEnvironment();
  SpawnLevelEnemies();
  for (TActorIterator<ABalloonEnvironmentPresetController> It(GetWorld()); It; ++It)
  {
   if (It->Generator != this) continue;
   if (!It->ApplyEnvironment(ActiveConfig->BiomeId, ActiveConfig->Preset))
   {
    Diagnostics.Add(TEXT("Environment application failed; inspect the bound environment controller diagnostics"));
    Diagnostics.Append(It->GetDiagnostics());
   }
  }
  return true;
 }
 for(auto& Entry : Collision) if(Entry.Key.IsValid()) Entry.Key->SetActorEnableCollision(Entry.Value);
 if(HadLevel)
 {
  Plan=OldPlan; Progress=OldProgress; ActiveConfig=OldConfig; Status=EBalloonLevelGenerationStatus::Ready;
  Spline->ClearSplinePoints(false);
  for(const auto& Point:Plan.SplineSamples) { const int32 I=Spline->GetNumberOfSplinePoints(); Spline->AddSplinePoint(Point,ESplineCoordinateSpace::World,false); Spline->SetSplinePointType(I,ESplinePointType::Linear,false); }
  Spline->UpdateSpline();
  Diagnostics.Add(TEXT("M8: regeneration failed; previous realized level retained"));
 }
 return false;
}

bool ABalloonLevelGenerator::BuildAndRealizeLevel(bool RetrySeed, bool ValidateOnly)
{
 SeedRetryable = false;
 Diagnostics.Reset(); Plan=FBalloonLevelPlan(); Progress=FBalloonLevelProgress(); if (!RetrySeed) ActiveConfig=nullptr; Spline->ClearSplinePoints();
	Status = EBalloonLevelGenerationStatus::Planning;
 if (!RetrySeed)
 {
 UBalloonLevelConfig* SelectedConfig = LevelConfig;
 if (UseStartOptions)
 {
  if (!StartOptions.Validate(Diagnostics)) { Status = EBalloonLevelGenerationStatus::InvalidConfig; return false; }
  if (!SelectedConfig || SelectedConfig->BiomeId != StartOptions.Biome)
   for (const auto& Config : RandomBiomeConfigs)
    if (IsValid(Config) && Config->BiomeId == StartOptions.Biome) { SelectedConfig = Config; break; }
  // Direct level setup also works without the deleted preview fixture.
  if (!SelectedConfig) SelectedConfig = NewObject<UBalloonLevelConfig>(this);
 }
 else if (BiomeSelection == EBalloonBiomeSelection::Random)
 {
  TArray<UBalloonLevelConfig*> Choices;
  for (const auto& Entry : RandomBiomeConfigs)
  {
   if (IsValid(Entry.Get())) Choices.AddUnique(Entry.Get());
  }
  if (Choices.IsEmpty())
  {
   Diagnostics.Add(TEXT("Random biome selection requires at least one valid RandomBiomeConfigs entry"));
   Status = EBalloonLevelGenerationStatus::InvalidConfig;
   return false;
  }
  SelectedConfig = Choices[FMath::RandHelper(Choices.Num())];
 }
	if (!IsValid(SelectedConfig))
	{
		Diagnostics.Add(TEXT("LevelConfig is required"));
		Status = EBalloonLevelGenerationStatus::InvalidConfig;
		return false;
	}
 ActiveConfig = DuplicateObject<UBalloonLevelConfig>(SelectedConfig, this);
 ActiveConfig->SetFlags(RF_Transient);
 if (!SpawnEnemies) ActiveConfig->EnemiesEnabled = false;
 if (!SpawnEnemies || !SpawnSharedEnemies) ActiveConfig->BorderActorClass.Reset();
 if (OverrideHeights)
 { ActiveConfig->FlightHeightMeters = FlightHeightMeters; ActiveConfig->PlacementGroundZMeters = GroundHeightMeters; }
 if (OverrideTerrain)
 {
  ActiveConfig->TerrainMode = TerrainMode;
  ActiveConfig->TerrainHeightVariationMeters = TerrainHeightVariationMeters;
  ActiveConfig->TerrainWavelengthMeters = TerrainWavelengthMeters;
  ActiveConfig->TerrainCellMeters = TerrainCellMeters;
  ActiveConfig->TerrainPaddingMeters = TerrainPaddingMeters;
  ActiveConfig->TerrainMaterial = TerrainMaterial;
 }
 if (OverrideLightingPreset) ActiveConfig->Preset = LightingPreset;
 if (UseStartOptions)
 {
  const bool ChangedBiome = ActiveConfig->BiomeId != StartOptions.Biome;
  ActiveConfig->BiomeId = StartOptions.Biome;
  ActiveConfig->Preset = StartOptions.Cycle;
  ActiveConfig->FlightHeightMeters = StartOptions.FlightHeight;
  ActiveConfig->BaseWidthMeters = ActiveConfig->MinWidthMeters = StartOptions.LevelWidth;
  ActiveConfig->WidthProfile.GetRichCurve()->Reset();
  if (ChangedBiome || !ActiveConfig->BiomeAssetTable)
   ActiveConfig->BiomeAssetTable = LoadObject<UDataTable>(nullptr, *FString::Printf(TEXT("/Game/BalloonLevel/Rules/DT_BalloonAssets_%s.DT_BalloonAssets_%s"), *StartOptions.Biome.ToString(), *StartOptions.Biome.ToString()));
  if (!ActiveConfig->SharedAssetTable)
   ActiveConfig->SharedAssetTable = LoadObject<UDataTable>(nullptr, TEXT("/Game/BalloonLevel/Rules/DT_BalloonAssets_Shared.DT_BalloonAssets_Shared"));
  if (ChangedBiome) ActiveConfig->TerrainMaterial.Reset();
 }
 if (RandomizeSeed) ActiveConfig->Seed = FMath::Rand();
 }
 else
 {
  // Odd increment produces distinct seeds, including across signed integer wraparound.
  const int32 NextSeed = static_cast<int32>(static_cast<uint32>(ActiveConfig->Seed) + 0x9E3779B9u);
  ActiveConfig = DuplicateObject<UBalloonLevelConfig>(ActiveConfig, this);
  ActiveConfig->SetFlags(RF_Transient);
  ActiveConfig->Seed = NextSeed;
 }
 Diagnostics.Add(FString::Printf(TEXT("Generation biome=%s seed=%d"), *ActiveConfig->BiomeId.ToString(), ActiveConfig->Seed));
 if (!ActiveConfig->Validate(Diagnostics)) { Status = EBalloonLevelGenerationStatus::InvalidConfig; return false; }
 BalloonHorizontalRoute::FEnvelope Envelope;
 FString EnvelopeError;
 if(!RoutePawn || !GetWorld() || !BalloonHorizontalRoute::InspectPawn(*RoutePawn,Envelope,EnvelopeError))
 { Diagnostics.Add(RoutePawn?EnvelopeError:TEXT("M5: RoutePawn instance is required for collision/movement inspection")); Status=EBalloonLevelGenerationStatus::Unreachable;return false; }
 Envelope.Safety=ActiveConfig->RouteSafetyMeters*100.;Envelope.Wind=ActiveConfig->RouteWindMeters*100.;
 Envelope.MinimumRadius=FMath::Max(Envelope.MinimumRadius,double(ActiveConfig->MinimumTurningRadiusMeters)*100.);
	FBalloonLevelPlan Candidate;
	if (!BalloonSplinePlanning::Build(*ActiveConfig, Candidate, Diagnostics, Envelope.MinimumRadius))
	{
		Status = EBalloonLevelGenerationStatus::InvalidConfig;
		return false;
	}
	Status = EBalloonLevelGenerationStatus::Validating;
	if (!BalloonAssetCatalog::Merge(ActiveConfig->SharedAssetTable, ActiveConfig->BiomeAssetTable, Candidate.Catalog, Diagnostics))
	{
		Status = EBalloonLevelGenerationStatus::InvalidConfig;
		return false;
	}
	if (!BalloonBorderPlanning::Build(*ActiveConfig, Candidate, Diagnostics))
 {
  SeedRetryable = true;
  Status = EBalloonLevelGenerationStatus::InsufficientSpace;
  return false;
 }
 Candidate.BalloonRadiusCm=(Envelope.Radius+Envelope.Safety+Envelope.Wind)*UE_DOUBLE_SQRT_2;
 Candidate.BalloonHalfHeightCm=Envelope.HalfHeight+Envelope.Safety;
 BalloonHorizontalRoute::FSettings Settings;Settings.CellCm=ActiveConfig->RouteCellMeters*100.;Settings.MaxAttempts=1;
 if(ActiveConfig->RouteAttempts<1 || ActiveConfig->RouteAttempts>12)
 {Diagnostics.Add(TEXT("M5: RouteAttempts must be [1,12]"));Status=EBalloonLevelGenerationStatus::InvalidConfig;return false;}
 TArray<BalloonPlacementPlanning::FResolvedRow> Rows;
 if(!BalloonPlacementPlanning::Resolve(*ActiveConfig,Candidate.Catalog,Rows,Diagnostics))
 {Status=EBalloonLevelGenerationStatus::InvalidConfig;return false;}
 AActor* StagedTerrain = nullptr;
 UProceduralMeshComponent* TerrainComponent = nullptr;
 ON_SCOPE_EXIT { if (IsValid(StagedTerrain)) StagedTerrain->Destroy(); };
 double TerrainTop = 0., TerrainBottom = 0.;
 if (ActiveConfig->TerrainMode != EBalloonTerrainMode::WorldCollision)
 {
  BalloonTerrain::FMesh TerrainMesh;
  if (!BalloonTerrain::Build(*ActiveConfig, Candidate, TerrainMesh, Diagnostics))
  { Status = EBalloonLevelGenerationStatus::InvalidConfig; return false; }
  FBox TerrainBounds(TerrainMesh.Vertices);
  TerrainTop = TerrainBounds.Max.Z + 100.; TerrainBottom = TerrainBounds.Min.Z - 100.;
  StagedTerrain = BalloonTerrain::Spawn(*this, *ActiveConfig, TerrainMesh, TerrainComponent, Diagnostics);
  if (!StagedTerrain) { Status = EBalloonLevelGenerationStatus::Failed; return false; }
 }
 const auto Sweep=BalloonHorizontalRoute::WorldSweep(*GetWorld(),RoutePawn);
 // An empty-placement failure cannot be repaired by re-rolling placements. Keep
 // terrain, authored collision and borders in this proof; never weaken validation.
 const auto Geometry = BalloonHorizontalRoute::Validate(Candidate, Envelope, Settings, Sweep);
 if (!Geometry.Reachable)
 {
  Diagnostics.Add(FString::Printf(TEXT("M5 seed=%d geometry preflight failed; placement attempts skipped: %s"), Candidate.Seed, *Geometry.FailureReason));
  SeedRetryable = true;
  Status = EBalloonLevelGenerationStatus::Unreachable;
  return false;
 }
 bool Reachable=false;
 for(int32 Attempt=0;Attempt<ActiveConfig->RouteAttempts;++Attempt)
 {
  Candidate.PlacementAttempt=Attempt;
  BalloonPlacementPlanning::FGroundSampler TerrainSampler;
  if (TerrainComponent)
  {
   TerrainSampler = [TerrainComponent, TerrainTop, TerrainBottom](const FVector& Position, BalloonPlacementPlanning::FSurface& Surface)
   {
    FHitResult Hit;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(BalloonGeneratedTerrain), true);
    if (!TerrainComponent->LineTraceComponent(Hit, FVector(Position.X, Position.Y, TerrainTop), FVector(Position.X, Position.Y, TerrainBottom), Query)) return false;
    Surface.Z = Hit.ImpactPoint.Z; Surface.Normal = Hit.ImpactNormal;
    return true;
   };
  }
  if (ActiveConfig->TerrainMode == EBalloonTerrainMode::WorldCollision)
  {
   TerrainSampler = [this](const FVector& Position, BalloonPlacementPlanning::FSurface& Surface)
   {
    // Query tagged components directly so unrelated blocking geometry cannot hide the terrain.
    FHitResult BestHit;
    bool Found = false;
    const FVector Top(Position.X, Position.Y, ActiveConfig->TerrainTraceTopMeters * 100.);
    const FVector Bottom(Position.X, Position.Y, ActiveConfig->TerrainTraceBottomMeters * 100.);
    FCollisionQueryParams Query(SCENE_QUERY_STAT(BalloonTerrain), true);
    for (TActorIterator<AActor> It(GetWorld()); It; ++It)
    {
     if (!It->GetActorEnableCollision() || !It->ActorHasTag(ActiveConfig->TerrainActorTag)) continue;
     TInlineComponentArray<UPrimitiveComponent*> Components(*It);
     for (UPrimitiveComponent* Component : Components)
     {
      FHitResult Hit;
      if (Component->IsQueryCollisionEnabled() && Component->LineTraceComponent(Hit, Top, Bottom, Query) &&
          (!Found || Hit.Distance < BestHit.Distance))
      { BestHit = Hit; Found = true; }
     }
    }
    if (!Found) return false;
    Surface.Z = BestHit.ImpactPoint.Z;
    Surface.Normal = BestHit.ImpactNormal;
    return true;
   };
  }
  if(!BalloonPlacementPlanning::Build(*ActiveConfig,Candidate,Rows,Diagnostics,TerrainSampler,{},Geometry.Points))continue;
  const auto Result=BalloonHorizontalRoute::Validate(Candidate,Envelope,Settings,Sweep);
  Diagnostics.Add(FString::Printf(TEXT("M5 seed=%d placement attempt=%d visited=%d: %s"),Candidate.Seed,Attempt+1,Result.CellsVisited,Result.Reachable?TEXT("fixed-Z route proved"):*Result.FailureReason));
  if(Result.Reachable) {Candidate.RoutePoints=Result.Points;Reachable=true;break;}
 }
 if(!Reachable) {SeedRetryable=true;Status=EBalloonLevelGenerationStatus::Unreachable;return false;}
 // Both checks and generation must pass the same complete planning proof.
 if (ValidateOnly) return true;
 Plan = MoveTemp(Candidate);
	for (const FVector& Point : Plan.SplineSamples)
	{
		const int32 Index = Spline->GetNumberOfSplinePoints();
		Spline->AddSplinePoint(Point, ESplineCoordinateSpace::World, false);
		Spline->SetSplinePointType(Index, ESplinePointType::Linear, false);
	}
	Spline->SetClosedLoop(false, false);
	Spline->UpdateSpline();
	Status = EBalloonLevelGenerationStatus::Realizing;
 // The route is certified at its configured start, so place the balloon there as part of the commit.
 // Move its root to carry the basket, camera and attached player together; retain authored scale.
 const FTransform PreviousBalloonTransform = RoutePawn->GetActorTransform();
 bool BalloonPlacementCommitted = false;
 ON_SCOPE_EXIT
 {
  if (!BalloonPlacementCommitted && IsValid(RoutePawn))
   RoutePawn->SetActorTransform(PreviousBalloonTransform, false, nullptr, ETeleportType::TeleportPhysics);
 };
 if (!RoutePawn->SetActorLocationAndRotation(Plan.Start.GetLocation(), Plan.Start.GetRotation(), false, nullptr, ETeleportType::TeleportPhysics))
 {
  Diagnostics.Add(TEXT("Controller: unable to place the balloon at the generated start"));
  Status = EBalloonLevelGenerationStatus::Failed;
  return false;
 }
 UClass* BorderClass = nullptr;
 if (!Plan.Borders.IsEmpty() && !ActiveConfig->BorderActorClass.IsNull())
 {
  BorderClass = ActiveConfig->BorderActorClass.LoadSynchronous();
  if (!BorderClass)
  {
   Diagnostics.Add(TEXT("Borders: configured rocket-balloon class could not be loaded"));
   Status = EBalloonLevelGenerationStatus::Failed;
   return false;
  }
 }
 if(!Realizer->Realize(Plan,Diagnostics,BorderClass,ActiveConfig->ItemDensity)) {Status=EBalloonLevelGenerationStatus::Failed; return false;}
 BalloonPlacementCommitted = true;
 if (StagedTerrain)
 {
  Realizer->Actors.Add(StagedTerrain);
  StagedTerrain = nullptr;
  Diagnostics.Add(TEXT("Terrain: generated surface realized with collision and biome ground material"));
 }
 Diagnostics.Add(TEXT("M8 Ready: fixed-Z route certified; placements realized."));
	Status = EBalloonLevelGenerationStatus::Ready;
	return true;
}

float ABalloonLevelGenerator::GetNormalizedProgress(const FVector& WorldPosition) const
{
 int32 Edge=INDEX_NONE;
 return Plan.LengthCm>0. ? BalloonSplinePlanning::NearestDistance(Plan,WorldPosition,Edge,true)/Plan.LengthCm : 0.f;
}
void ABalloonLevelGenerator::UpdateProgress(const FVector& WorldPosition, bool Teleported)
{
 Progress.Update(Plan,WorldPosition,Teleported);
 if(Plan.LengthCm>0.) EventManager->UpdatePlayerProgress(Progress.CurrentS/Plan.LengthCm,Progress.CurrentS/100.);
}

bool ABalloonLevelGenerator::RegisterBorderProxy(ABalloonBorderThreat* Proxy)
{
 if(!IsValid(Proxy) || !IsValid(Proxy->InfluenceCollision) || Proxy->GetWorld()!=GetWorld()) return false;
 const auto T=Proxy->GetInfluence();
 const auto* Descriptor=Plan.Borders.FindByPredicate([&](const auto& B){return B.InstanceId==T.InstanceId;});
 if(!Descriptor || !Descriptor->Center.Equals(T.Center) || Descriptor->RadiusCm!=T.RadiusCm || Descriptor->Policy!=T.Policy ||
    !Proxy->GetActorLocation().Equals(T.Center) || !Proxy->GetActorScale3D().Equals(FVector::OneVector) ||
    !Proxy->InfluenceCollision->GetComponentLocation().Equals(T.Center) || !Proxy->InfluenceCollision->GetComponentScale().Equals(FVector::OneVector) ||
    Proxy->InfluenceCollision->GetUnscaledSphereRadius()!=T.RadiusCm || BorderProxies.Contains(T.InstanceId)) return false;
 Proxy->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
 BorderProxies.Add(T.InstanceId,Proxy);return true;
}

bool ABalloonLevelGenerator::TryEventBorderChange(const FBalloonLevelEventDefinition& Event,FString& Reason)
{
 if(Status!=EBalloonLevelGenerationStatus::Ready || !IsValid(RoutePawn) || !ActiveConfig || !GetWorld() || Event.RequestsLightingSwap ||
    !FMath::IsFinite(Event.Value) || Event.Action==EBalloonEventAction::Blueprint || uint8(Event.Action)>uint8(EBalloonEventAction::ScaleBorderRadius))
 {Reason=TEXT("Border transaction requires Ready, a live Pawn, and a valid radius action; lighting swaps forbidden");return false;}
 auto Candidate=Plan.Borders;
 bool Matched=false;
 for(auto& B:Candidate)
 {
  if(Event.BorderId>=0 && B.InstanceId!=Event.BorderId)continue;
  if(Event.SegmentId>=0 && B.SegmentId!=Event.SegmentId)continue;
  auto* Entry=BorderProxies.Find(B.InstanceId);
  ABalloonBorderThreat* Proxy=Entry?Entry->Get():nullptr;
  if(!Event.TargetTag.IsNone() && (!IsValid(Proxy) || !Proxy->ActorHasTag(Event.TargetTag)))continue;
  // A registered but destroyed/moved/rescaled proxy invalidates the transaction before any commit.
  if(Entry && (!IsValid(Proxy) || !IsValid(Proxy->InfluenceCollision) || !Proxy->GetActorLocation().Equals(B.Center) || !Proxy->GetActorScale3D().Equals(FVector::OneVector) ||
     Proxy->GetInfluence().RadiusCm!=B.RadiusCm || Proxy->GetInfluence().Policy!=B.Policy ||
     !Proxy->InfluenceCollision->GetComponentLocation().Equals(B.Center) || !Proxy->InfluenceCollision->GetComponentScale().Equals(FVector::OneVector) ||
     Proxy->InfluenceCollision->GetUnscaledSphereRadius()!=B.RadiusCm))
  {Reason=TEXT("Registered border proxy is stale or transformed");return false;}
  const double Radius=Event.Action==EBalloonEventAction::ScaleBorderRadius ? B.RadiusCm*Event.Value :
   Event.Action==EBalloonEventAction::AddBorderRadius ? B.RadiusCm+Event.Value*100. : Event.Value*100.;
  if(!FMath::IsFinite(Radius) || Radius<=0. || Radius>MAX_flt) {Reason=TEXT("Radius must be finite and positive");return false;}
  B.RadiusCm=Radius;
  const FVector Center=BalloonSplinePlanning::PositionAt(Plan,B.DistanceCm);
  const FVector Right=BalloonBorderPlanning::RightAt(Plan,B.DistanceCm);
  double A=0.,Z=0.;B.IntrusionCm=0.f;
  if(BalloonBorderPlanning::CrossSection(B,Center,Right,Plan.BalloonHalfHeightCm,0.,A,Z))
  {
   const double Half=BalloonSplinePlanning::WidthAt(Plan,B.DistanceCm)*.5;
   B.IntrusionCm=FMath::Max(0.,FMath::Min(Z,Half)-FMath::Max(A,-Half));
  }
  Matched=true;
 }
 if(!Matched) {Reason=TEXT("Border selector matched no threats");return false;}
 BalloonHorizontalRoute::FEnvelope Envelope;
 if(!BalloonHorizontalRoute::InspectPawn(*RoutePawn,Envelope,Reason))return false;
 Envelope.Safety=ActiveConfig->RouteSafetyMeters*100.;Envelope.Wind=ActiveConfig->RouteWindMeters*100.;
 Envelope.MinimumRadius=FMath::Max(Envelope.MinimumRadius,double(ActiveConfig->MinimumTurningRadiusMeters)*100.);
 BalloonHorizontalRoute::FSettings Settings;Settings.CellCm=ActiveConfig->RouteCellMeters*100.;Settings.MaxAttempts=1;
 const auto Result=BalloonHorizontalRoute::TryBorders(Plan,Candidate,RoutePawn->GetActorLocation(),Envelope,Settings,BalloonHorizontalRoute::WorldSweep(*GetWorld(),RoutePawn));
 if(!Result.Reachable) {Reason=Result.FailureReason;return false;}
 // No BP callbacks until every native representation has committed.
 for(const auto& B:Plan.Borders) if(auto* Entry=BorderProxies.Find(B.InstanceId); Entry && IsValid(Entry->Get())) Entry->Get()->CommitValidatedInfluence(B);
 OnBorderChanged.Broadcast();
 return true;
}

TArray<FBalloonBorderInfluence> ABalloonLevelGenerator::GetBorderInfluences() const { return Plan.Borders; }

void ABalloonLevelGenerator::DrawBorderDebug(float Duration) const
{
 if(!GetWorld() || Plan.SplineSamples.Num()<2 || !FMath::IsFinite(Duration) || Duration<0.f) return;
 for(int32 I=1;I<Plan.RoutePoints.Num();++I)
  DrawDebugLine(GetWorld(),Plan.RoutePoints[I-1],Plan.RoutePoints[I],FColor::Green,false,Duration,0,3.f);
 for(int32 I=1;I<Plan.LeftBoundary.Num();++I)
 {
  DrawDebugLine(GetWorld(),Plan.LeftBoundary[I-1],Plan.LeftBoundary[I],FColor::Cyan,false,Duration);
  DrawDebugLine(GetWorld(),Plan.RightBoundary[I-1],Plan.RightBoundary[I],FColor::Cyan,false,Duration);
 }
 for(const auto& T:Plan.Borders)
 {
  const double Radius=BalloonBorderPlanning::ProjectedRadius(T,Plan.Start.GetLocation().Z,Plan.BalloonHalfHeightCm);
  const FColor Color=T.Policy==EBalloonBorderInfluencePolicy::VisualOnly?FColor::Silver:FColor::Red;
  DrawDebugSphere(GetWorld(),T.Center,T.RadiusCm,24,Color,false,Duration);
  if(Radius>=0.)
  {
   const FVector Center(T.Center.X,T.Center.Y,Plan.Start.GetLocation().Z);
   DrawDebugCircle(GetWorld(),Center,Radius,48,Color,false,Duration,0,2.f,FVector::ForwardVector,FVector::RightVector,false);
   DrawDebugString(GetWorld(),Center,FString::Printf(TEXT("Border %d side %d R=%.1fm intrusion=%.1fm"),T.InstanceId,T.Side,T.RadiusCm/100.,T.IntrusionCm/100.),nullptr,Color,Duration);
  }
 }
}

void ABalloonLevelGenerator::DrawGenerationDebug() const
{
 if(DebugRoute) DrawBorderDebug();
 if(!GetWorld()) return;
 if(DebugSpline) for(int32 I=1;I<Plan.SplineSamples.Num();++I) DrawDebugLine(GetWorld(),Plan.SplineSamples[I-1],Plan.SplineSamples[I],FColor::White,false,10.f,0,2.f);
 if(DebugHISM) for(const auto& Group:Realizer->Groups) if(IsValid(Group))
 {
  DrawDebugBox(GetWorld(),Group->Bounds.Origin,Group->Bounds.BoxExtent,FColor::Purple,false,10.f);
  DrawDebugString(GetWorld(),Group->Bounds.Origin,FString::Printf(TEXT("HISM %s instances=%d end=%dcm (0=unlimited)"),*GetNameSafe(Group->GetStaticMesh()),Group->GetInstanceCount(),Group->InstanceEndCullDistance),nullptr,FColor::Purple,10.f);
 }
 if(DebugPlacements) for(const auto& D:Plan.Placements)
 {
  const FColor Color=D.Collision?FColor::Orange:FColor::Silver;
  DrawDebugCoordinateSystem(GetWorld(),D.Transform.GetLocation(),D.Transform.Rotator(),200.f,false,10.f);
  if(D.CollisionBounds.IsValid) DrawDebugBox(GetWorld(),D.CollisionBounds.GetCenter(),D.CollisionBounds.GetExtent(),Color,false,10.f);
  DrawDebugString(GetWorld(),D.Transform.GetLocation(),FString::Printf(TEXT("%s / %s #%d segment=%d cull=%d"),*D.Biome.ToString(),*D.Rule.ToString(),D.InstanceIndex,D.SegmentId,D.CullEndCm),nullptr,Color,10.f);
 }
 if(DebugSegments) for(const auto& Segment:Plan.Segments) if(Segment.BaseBounds.IsValid) DrawDebugBox(GetWorld(),Segment.BaseBounds.GetCenter(),Segment.BaseBounds.GetExtent(),FColor::Blue,false,10.f);
 DrawDebugSphere(GetWorld(),Plan.Start.GetLocation(),200.f,12,FColor::Green,false,10.f);
 DrawDebugSphere(GetWorld(),Plan.Finish.GetLocation(),200.f,12,FColor::Yellow,false,10.f);
}

void ABalloonLevelGenerator::Destroyed()
{
 ClearGeneratedLevel();
 ClearOwnedEnvironment();
 Super::Destroyed();
}
