# Balloon level generation: API reference

> **For agents and Blueprint authors.** Reference for every public type of the procedural level generator in `Source/gunsandballoons/BalloonLevel/`. Pipeline and rules: [level-generation.md](../level-generation.md). Status log: [STATUS.md](STATUS.md) (milestone history; not updated for the 2026-11-15 validation, see [decisions.md](../../decisions.md)). Design rationale: [DESIGN_DECISIONS.md](DESIGN_DECISIONS.md). **Verified against source:** 2026-11-15.

Units: config and Blueprint-facing fields are **meters**; plan descriptors and C++ planner APIs are **centimeters** (cm/s, rad/s in the route validator). Every planner fails transactionally: on failure it leaves no partial output and appends a diagnostic that names the table, row, rule or segment.

## Contents
1. [Quick start](#quick-start)
2. [`ABalloonLevelGenerator`](#aballoonlevelgenerator)
3. [`FBalloonLevelStartOptions` and `UBalloonLevelLaunchSubsystem`](#start-options)
4. [`UBalloonLevelConfig`](#uballoonlevelconfig)
5. [Asset tables (`FBalloonLevelAssetRow`)](#asset-tables)
6. [`UBalloonLevelEventManager`](#uballoonleveleventmanager)
7. [`ABalloonBorderThreat`](#aballoonborderthreat)
8. [`ABalloonEnvironmentPresetController`](#aballoonenvironmentpresetcontroller)
9. [`UBalloonLevelRealizer`](#uballoonlevelrealizer)
10. [Enums and plan types](#enums-and-plan-types)
11. [Pure C++ planner APIs](#pure-c-planner-apis)
12. [Diagnostics and status codes](#diagnostics-and-status-codes)
13. [Automation](#automation)

## Quick start
**New level (editor).** Create a `UBalloonLevelConfig` DataAsset, place an `ABalloonLevelGenerator`, set `LevelConfig` and `RoutePawn` (the placed `BP_Balloon`), press **Generate Level**, then read `GetGenerationStatus()` / `GetDiagnostics()`. A successful generation moves the balloon to the route start, so no manual alignment is needed.

**From a menu (runtime).**
```
Get Game Instance Subsystem (BalloonLevelLaunchSubsystem) -> Start Level(Options, Level=None, out Error)
```
Empty `Level` opens `/Game/Scenes/MainLevel`. The generator in that map consumes the options once in `BeginPlay` and generates.

**Events.** After `Ready`: `RegisterEventDefinitions`, bind `OnLevelEventTriggered`, then call `StartEventTimeline()`. Generation never starts the clock.

**Completion.** Bind `OnLevelCompleted`. It fires once when `RoutePawn`'s origin enters the finish box.

## `ABalloonLevelGenerator`
The only entry point. Root component is the progression `Spline`; owns the `Realizer`, `EventManager` and `CompletionZone`.

### Functions
| Function | Access | Behaviour |
| --- | --- | --- |
| `GenerateLevel()` → bool | BlueprintCallable | Full plan → validate → realize pipeline. `true` means `Ready`. A failure retains the previous Ready level (plan, config, progress, realized actors, balloon position). On success: resets completion and the event timeline, registers border proxies, applies the environment, spawns functional enemies. |
| `RegenerateLevel()` → bool | BlueprintCallable | Same as `GenerateLevel()`. |
| `GenerateLevelInEditor()`, `RegenerateLevelInEditor()` | CallInEditor | Details buttons. |
| `ClearGeneratedLevel()` | BlueprintCallable, CallInEditor | Destroys realized actors/HISM, resets plan, spline, progress, event timeline, completion; status → `NotStarted`. Also runs on `Destroyed`. |
| `GetGenerationStatus()` → `EBalloonLevelGenerationStatus` | BlueprintPure | See [status codes](#diagnostics-and-status-codes). |
| `GetDiagnostics()` → `TArray<FString>` | BlueprintPure | Human-readable log of the last attempt, including `Generation biome=<id> seed=<n>`. |
| `GetStartTransform()`, `GetFinishTransform()` | BlueprintPure | Plan start/finish at flight Z. |
| `GetWidthAtDistanceCm(S)` | BlueprintPure | Corridor width (cm) at arc length `S` (cm). |
| `GetBorderInfluences()` → `TArray<FBalloonBorderInfluence>` | BlueprintPure | Copy of the current border descriptors. |
| `GetNormalizedProgress(Pos)` → float 0..1 | BlueprintCallable | Stateless projection onto the centerline (full search). |
| `UpdateProgress(Pos, Teleported=false)` | BlueprintCallable | Tracked progress (local search; full search on teleport); feeds the event manager. |
| `GetCurrentProgressCm()`, `GetMaxReachedProgressCm()` | BlueprintPure | Current and monotonic maximum arc length (cm). |
| `RegisterBorderProxy(Proxy)` → bool | BlueprintCallable | Links an initialized `ABalloonBorderThreat` to its descriptor. Requires an exact match (id, center, radius, policy), unit scale, same world, not already registered. Generated proxies register automatically. |
| `CompleteLevel()` | BlueprintCallable | Idempotent. Needs `Ready` and not completed; sets `LevelCompleted`, pauses the event timeline, broadcasts `OnLevelCompleted`. |
| `SetupLevelController()` | BlueprintCallable, CallInEditor | Binds or creates the environment controller, sun, sky and fog (see below). Idempotent. |
| `PreviewEnvironment()` | CallInEditor | Applies the selected config's biome/preset in the editor world only. |
| `DrawBorderDebug(Duration=10)`, `DrawGenerationDebug()` | BlueprintCallable / CallInEditor | Debug drawing, honouring the `Debug*` toggles. |
| `GetPlan()` | C++ | `const FBalloonLevelPlan&`. |
| `TryEventBorderChange(Event, Reason)` → bool | C++ | Radius transaction used by the event manager. |

### Delegates
| Delegate | Fires |
| --- | --- |
| `OnLevelCompleted` | Once per generated level, when `RoutePawn`'s origin enters `CompletionZone` (checked in the generator tick, game worlds only). |
| `OnBorderChanged` | After a validated radius change has been committed to the plan and every registered proxy. |

### Properties
| Group | Property | Default | Notes |
| --- | --- | --- | --- |
| Level | `LevelConfig` | none | Source `UBalloonLevelConfig`. Never modified. |
| | `RoutePawn` | none | Placed balloon actor. Required: supplies collision envelope and movement limits. |
| | `Spline` | | Read-only progression spline (linear points). |
| | `Realizer`, `EventManager` | | Default subobjects. |
| Generation | `GenerationMode` | `Manual` | `OnLevelStart` generates in `BeginPlay`. |
| | `BiomeSelection` | `Selected` | `Random` picks a valid entry of `RandomBiomeConfigs` per generation. |
| | `RandomBiomeConfigs` | empty | One config per biome. Also used to look up the config for `StartOptions.Biome`. |
| | `RandomizeSeed` | false | Replaces `ActiveConfig->Seed` with a global-random value. The value is logged in diagnostics. |
| | `ActiveConfig` | transient | The per-generation copy actually used. |
| Start options | `UseStartOptions`, `StartOptions` | false | Set automatically when a menu launch is consumed. |
| Height | `OverrideHeights`, `FlightHeightMeters` (100), `GroundHeightMeters` (0) | false | Absolute world Z. |
| Terrain | `OverrideTerrain`, `TerrainMode`, `TerrainHeightVariationMeters` (15), `TerrainWavelengthMeters` (200), `TerrainCellMeters` (20), `TerrainPaddingMeters` (100), `TerrainMaterial` | false | Overrides apply to the copy. |
| Environment | `ManageEnvironment` (true), `EnvironmentController`, `Sun`, `Sky`, `Fog`, `AuthoredGround`, `OverrideLightingPreset`, `LightingPreset`, `OverrideEnvironmentSettings`, `Day`, `Night`, `BiomeFog` | | See [environment](#aballoonenvironmentpresetcontroller). |
| Enemies | `SpawnEnemies` (true), `SpawnSharedEnemies`, `SpawnBiomeEnemies` | true | `SpawnEnemies=false` also clears `EnemiesEnabled` and `BorderActorClass`. |
| | `EdgeAACount` 8, `BirdFlockCount` 3, `DesertCannonCount` 3, `DesertBeaconCount` 2 | | Prototype counts (TBD); scaled by `ActiveConfig->EnemyDensity`, clamped to 100. |
| | `EnemySideOffsetMeters` 45, `EnemyStartClearanceMeters` 150, `AirshipStartDistanceMeters` 250, `AirshipHeightOffsetMeters` 15 | | Prototype distances (TBD). |
| Completion | `CompletionZone` (box), `CompletionZoneHalfDepthMeters` (10), `LevelCompleted` | | Box is collision-free. Width = corridor width at the finish; half-height = max(10 m, balloon half-height). |
| Debug | `DebugPlacements`, `DebugRoute`, `DebugSpline`, `DebugSegments`, `DebugHISM` | true | Used by `DrawGenerationDebug`. |

### Config resolution (what `GenerateLevel` actually does)
1. **Pick the source config.** With `UseStartOptions`: `LevelConfig` if its biome matches, else the first `RandomBiomeConfigs` entry with that biome, else a native-default config. Otherwise `BiomeSelection=Random` picks from `RandomBiomeConfigs`; otherwise `LevelConfig`. None → `InvalidConfig`.
2. **Duplicate** into the transient `ActiveConfig`.
3. **Apply overrides in order:** `SpawnEnemies`, `OverrideHeights`, `OverrideTerrain`, `OverrideLightingPreset`, then `StartOptions` (wins over the legacy overrides), then `RandomizeSeed`.
4. **Pipeline:** spline → catalog merge → borders → pawn inspection → placement resolve → terrain staging → up to `RouteAttempts` placement/route-proof tries → commit plan → move balloon → realize → environment → enemies.
5. **On any failure** the staged terrain is destroyed, the balloon is restored, and the previous level (if any) is kept.

### `StartOptions` effects on the config copy
| Option | Effect |
| --- | --- |
| `Biome`, `Cycle` | `BiomeId`, `Preset`. A biome change loads `/Game/BalloonLevel/Rules/DT_BalloonAssets_<Biome>` and clears `TerrainMaterial`. A missing shared table loads `DT_BalloonAssets_Shared`. |
| `FlightHeight` | `FlightHeightMeters`. |
| `LevelWidth` | Constant corridor: `BaseWidthMeters = MinWidthMeters = LevelWidth`; width curve cleared. |
| `DistanceToNemesis` | Airship spawns this far behind the route start. |

## Start options
`FBalloonLevelStartOptions` (struct, BlueprintType). Defaults are prototype values (TBD).

| Field | Default | Valid range |
| --- | --- | --- |
| `Biome` | `Forest` | `Forest`, `Field`, `City`, `Desert` |
| `Cycle` | `Day` | `Day`, `Night` (fixed for the whole level) |
| `FlightHeight` (m) | 100 | finite |
| `LevelWidth` (m) | 600 | finite, ≥ 1 |
| `DistanceToNemesis` (m) | 250 | finite, ≥ 0 |

`Validate(Errors)` is C++ only. The generator calls it again at generation time.

`UBalloonLevelLaunchSubsystem` (`UGameInstanceSubsystem`):
| Member | Behaviour |
| --- | --- |
| `StartLevel(Options, Level, OutError)` → bool | Validates options, requires a running game world, resolves the map (empty → `MainLevel`) and checks it exists, queues the one-shot handoff, then requests travel. `false` leaves any queued launch untouched. `true` means travel was requested, not that generation finished. |
| `QueueLevel(Options)` → bool | Validates and queues without travel, for callers that manage their own travel. |
| `Options`, `Pending` | Read-only queue state. |
| `Consume(Out)` | C++. Called by the generator in `BeginPlay`; clears `Pending`. |

## `UBalloonLevelConfig`
DataAsset. Every numeric value is a **prototype tuning input** (TBD) except the approved 600 m width. `Validate(Errors)` rejects non-finite or out-of-range values and returns false without clamping.

| Group | Fields |
| --- | --- |
| Geometry | `Seed` 12345, `LengthMeters` 1500, `BaseWidthMeters` 600, `MinWidthMeters` 40, `SegmentTargetMeters` 250, `FlightHeightMeters` 100 (absolute world Z), `StartTransform` (XY + yaw only), `MaxHeadingDegrees` 25 ([0,45]), `MinimumTurningRadiusMeters` 100, `GeometrySampleMeters` 10, `WidthProfile` (normalized distance → width multiplier; empty = 1) |
| Biome | `BiomeId` (`Forest`), `SharedAssetTable`, `BiomeAssetTable`, `CatalogVersion` ("1"), `Preset` (Day/Night) |
| Borders | `BordersEnabled` false, `BorderActorClass` (soft class; needs float `AggroRangeM` and `DriftRangeM`; empty = proxies only), `BorderExactCount` -1 (-1 density, 0 none, ≤ 10000), `BorderDensityPerKm` 8, `BorderRadiusMin/MaxMeters` 12/35, `BorderIntrusionFraction` 0.5, `BorderMinSpacingMeters` 0, `BorderAllowOverlap` true, `BorderPolicy` Damaging, `BorderHeightOffsetMeters` 0, `BalloonRadiusMeters`/`BalloonHalfHeightMeters` (border-planning diagnostics only; the route proof uses the live pawn) |
| Placement | `SceneryEnabled` true, `ObstaclesEnabled` false, `EnemiesEnabled` false, `EnemyDensity` 1, `ObstacleDensity` 1 ([0,10]), `ItemDensity` 1 ([0,1]), `ObstacleExactCount`/`EnemyExactCount` -1, `PlacementMetadata` (row name → `FBalloonPlacementMetadata`), `ClusterRadiusMeters` 30, `StreetCellMeters` 100, `StreetRoadMeters` 20, `StreetLotDepthMeters` 30, `StreetLampIntervalMeters` 20, `PlacementGroundZMeters` 0 |
| Terrain | `TerrainMode` `Generated`/`Flat`/`WorldCollision`, `TerrainHeightVariationMeters` 15, `TerrainWavelengthMeters` 200, `TerrainCellMeters` 20, `TerrainPaddingMeters` 100, `TerrainMaterial`, `TerrainActorTag` (`Terrain`), `TerrainTraceTopMeters` 1000, `TerrainTraceBottomMeters` -1000 |
| Route | `RouteCellMeters` 10, `RouteSafetyMeters` 1, `RouteWindMeters` 0, `RouteAttempts` 12 ([1,12]) |

`FBalloonPlacementMetadata`: `InteriorEnemy`, `StreetLamp`, `SizeMeters` (unscaled; zero = measured from a StaticMesh; Blueprint Actor rows require an authored value), `CullEndCm` (15000 clutter, 30000 short trees, 80000 buildings/dunes, 0 never culled).

**Terrain.** `Generated` builds a seeded two-frequency Perlin grid over the corridor bounds plus padding (≤ 250000 vertices) with horizon rings, as a transient `ProceduralMeshComponent` actor with `BlockAll` complex collision. `Flat` is the same grid without relief. `WorldCollision` samples actors tagged `TerrainActorTag`. Generated terrain is staged before the route proof, so it can block routes. It transfers to the realizer on success.

## Asset tables
`FBalloonLevelAssetRow` (DataTable row, exactly 12 columns): `Asset, Category, Collision, DensityPerKm2, ScaleMin, ScaleMax, MinSpacingM, RandomYaw, AlignToSlope, ClusterSize, Rule, Notes`. Shared and biome tables merge by row name (the biome row wins). Authoring contract, categories and rule semantics: [Content/BalloonLevel/Rules/README.md](../../../Content/BalloonLevel/Rules/README.md). Editable sources are the `BalloonAssets_*.json` files beside it.

| Rule | Biome gate | Result |
| --- | --- | --- |
| `scatter` | any | density × corridor area, clusters, spacing |
| `ring` | Forest | ≥ 3 inward-facing noncolliding Background meshes in a 1.5–3 km annulus |
| `line` | Field | chained meshes along both corridor edges; **count must exactly cover the border** |
| `street` | City | world-axis grid, back-to-back buildings, lamps |
| `wind_aligned` | Desert | one seeded level yaw ± 10° |
| `flock` | Field | one 3–6 Actor budget shared by all active flock rows |

Rows of a gated rule in another biome are skipped silently. Placement representation: static meshes without collision → one HISM per mesh (`NoCollision`); static meshes in Obstacle/Hazard → `AStaticMeshActor` (`BlockAllDynamic`); Blueprint classes → spawned Actors. Colliding placements are certified by `CollisionBounds`: StaticMesh mesh bounds plus simple collision, or for Blueprint Actors the union of the primitive components the class will construct (native CDO components plus `SimpleConstructionScript` templates with an SCS parent chain) that have a query collision profile. A colliding Blueprint that constructs no query-colliding primitive fails the route proof with "colliding placement lacks certified 3D bounds".

## `UBalloonLevelEventManager`
ActorComponent on the generator. It ticks itself, samples `RoutePawn` through `Generator->UpdateProgress`, and drives the clock. Disable its tick before driving the update methods manually.

| Function | Behaviour |
| --- | --- |
| `RegisterEventDefinitions(Defs)` → bool | Atomic replace while stopped. Rejects duplicate ids (even disabled), NaN, unknown enums, `RequestsLightingSwap` and Day/Night swap ids. |
| `StartEventTimeline()` | Needs generator `Ready`. Idempotent. **Not called by generation.** |
| `PauseEventTimeline(bool)`, `ResetEventTimeline()` | Freeze; reset elapsed/progress/fired state (definitions and bindings persist). |
| `UpdateElapsedGameTime(dt)`, `UpdatePlayerProgress(Normalized, Meters)`, `EmitCustomSignal(Signal, Target)` | Manual drivers. |
| `GetSnapshot()` | Elapsed, progress, watermark, running/paused, fixed preset, per-event fired/cooldown. |
| `OnLevelEventTriggered(Event)`, `OnLevelEventRejected(EventId, Reason)` | Blueprint receivers. Filter `TargetTag`/`EventId` yourself. |

`FBalloonLevelEventDefinition`: `EventId`, `Enabled`, `Trigger` (`ElapsedGameTime` s, `NormalizedProgress` 0..1, `AbsoluteProgress` m, `CustomSignal`), `Threshold`, `SignalId`, `TargetTag`, `Action` (`Blueprint`, `SetBorderRadius` m, `AddBorderRadius` m, `ScaleBorderRadius` ×), `Value`, `BorderId`/`SegmentId` (-1 = all), `Priority`, `Repeat` (`Once`, `RepeatAfterCooldown`, `OnEveryCrossing`), `CooldownSeconds`.

Ordering inside one sample: threshold, then priority, then `EventId`. `Once` fires at most once, even when rejected. Large steps never skip an event. A radius action is applied only if the route proof still passes from the balloon's exact position; otherwise `OnLevelEventRejected` fires with a reason and nothing changes. No event can change the lighting preset.

## `ABalloonBorderThreat`
Stationary, non-ticking proxy for one border descriptor. Generated automatically by the realizer; spawn manually only for authored borders: spawn deferred → `InitializeInfluence(Descriptor)` before `BeginPlay` → finish spawning → `Generator->RegisterBorderProxy(Proxy)`.

| Member | Behaviour |
| --- | --- |
| `InitializeInfluence(Descriptor)` → bool | Rejected after `BeginPlay`. |
| `InfluenceCollision` | Sphere. `Blocked` blocks `Pawn` and `WorldDynamic`; `Damaging`/`Pushback` overlap them and call `OnBalloonInfluenceEntered`; `VisualOnly` has no collision. |
| `GetInfluence()` | Current descriptor. |
| `OnInfluenceInitialized(Descriptor)`, `OnBalloonInfluenceEntered(Balloon, Policy)` | BlueprintImplementableEvent. Filter `Balloon` to the intended actor. |

With `BorderActorClass` set, the realizer also spawns the rocket balloon, binds `AggroRangeM` to the descriptor radius, sets `DriftRangeM = 0`, disables the rocket's collision, and disables its tick unless the policy is `Damaging`.

## `ABalloonEnvironmentPresetController`
Applies one fixed Day or Night preset, the biome fog and the biome ground/sky once per play session. No clock or transition exists.

| Member | Behaviour |
| --- | --- |
| `ApplyEnvironment(Biome, Preset)` → bool | Validates all, then applies. In play, only the first successful call applies; repeating the same pair is a no-op; a different pair is rejected. Outside play it reapplies freely. |
| `ApplyFromGenerator()` → bool | Editor: selected `LevelConfig`. Play: `ActiveConfig` once Ready. |
| `PreviewEnvironment()` | Editor button. |
| `IsApplied()`, `GetSelectedLightingPreset()`, `GetAppliedBiome()`, `GetDiagnostics()`, `GetSpawnedFog()` | Queries. |
| Properties | `Generator`, `FallbackBiomeId`, `FallbackPreset`, `ApplyOnBeginPlay`, `Ground`, `GroundMaterialPathFormat`, `GroundMaterialOverrides`, `Fog`, `Sun`, `Sky`, `BiomeFog` (`FBalloonFogPreset`: color, density), `Day`/`Night` (`FBalloonLightingPresetValues`: sun rotation/intensity/color, sky intensity), `DaySkyMaterials`, `NightSkyMaterials`, `SkyDome`. |

`ABalloonLevelGenerator::SetupLevelController()` reuses the assigned or single existing environment, sun, sky and fog actors, creates missing ones transiently (owned and removed with the generator), and attaches them below the generator while preserving world transforms. Several candidates without an explicit assignment fail with a diagnostic. **A generation does not fail if environment application fails**; check the diagnostics.

## `UBalloonLevelRealizer`
Component that turns the plan into world objects, transactionally on the game thread. Not intended to be called directly. `Realize(Plan, Diagnostics, BorderActorClass, ItemDensity)` stages new objects and replaces the old ones only on success; on failure the staged objects are removed.

| Output | Details |
| --- | --- |
| Scenery | One `UHierarchicalInstancedStaticMeshComponent` per mesh, `NoCollision`, no overlaps/navigation, cull start 0, end from the descriptor. Same mesh with conflicting cull profiles fails. |
| Gameplay actors | `AStaticMeshActor` (`BlockAllDynamic`) or spawned Blueprint classes. A colliding actor must have query collision; for a Blueprint, the class must construct a query-colliding primitive (native component or `SimpleConstructionScript` template) and the row needs authored `SizeMeters`. Only `flock` may be a decorative actor. Tag `BalloonLevel.Generated`. |
| Border proxies | One `ABalloonBorderThreat` per descriptor, plus optional rocket balloons. |
| Loot | Every mesh socket named `loot_*` (HISM instances, actor meshes, skinned bones) gets one `AHookPickup` attached to it; a Skeleton socket and a mesh socket with the same name are the same spot, so names are deduplicated per component. Skinned assets take loot from Skeleton sockets, not bone names: `SK_Windmill` and `SK_Lighthouse` need `loot_01`/`loot_02` sockets on their Skeleton. Blueprint Actors carrying their own `AC_LootSpots` spawner keep it only for manual placements: on generated instances the realizer zeroes `SpawnChance` on its class templates for the synchronous spawn, then clears the instance component right after construction (its drops are timer-delayed) and destroys anything it already dropped, so the generator stays the single loot source (`ItemDensity`, seeded item types). `ItemDensity` keeps the fraction of spots whose roll is below it; item type and roll use separate seeded streams, so lowering density keeps a nested subset with stable item types. Pickups are tagged `BalloonLevel.Loot`. |
| Terrain | The generator hands over its staged terrain actor (tag `BalloonLevel.Terrain`). |
| Enemies | `SpawnLevelEnemies` adds `ANemesisAirship`, edge `ANemesisFlak` (skipped when the border rocket class already populates the edges), and per biome: Field `ANemesisBirdFlock`, Desert `ANemesisTowerCannon` and `ANemesisBeacon` (ground-snapped; skipped with a diagnostic when no tagged terrain is hit). Tags `BalloonLevel.Generated` and `BalloonLevel.Enemy`. They are not part of the route proof. |

`Groups` (HISM components) and `Actors` (weak refs) expose what is tracked; `Clear()` removes exactly those.

## Enums and plan types
| Type | Values / members |
| --- | --- |
| `EBalloonLevelGenerationStatus` | `NotStarted`, `Planning`, `Validating`, `Ready`, `Realizing`, `Complete` (reserved, never set), `InvalidConfig`, `InsufficientSpace`, `Unreachable`, `Failed` |
| `EBalloonLightingPreset` | `Day`, `Night` |
| `EBalloonBorderInfluencePolicy` | `Blocked`, `Damaging`, `Pushback`, `VisualOnly` |
| `EBalloonGenerationMode` | `Manual`, `OnLevelStart` |
| `EBalloonBiomeSelection` | `Selected`, `Random` |
| `EBalloonTerrainMode` | `Flat`, `WorldCollision`, `Generated` |
| `EBalloonEventTrigger` / `Repeat` / `Action` | see the event manager |
| `FBalloonBorderInfluence` | `InstanceId`, `SegmentId`, `Side` (-1 left, +1 right), `DistanceCm`, `Center`, `RadiusCm`, `IntrusionCm`, `Policy` |
| `FBalloonLevelPlan` (C++) | `Seed`, `BiomeId`, `CatalogVersion`, `LightingPreset`, `SplineSamples`, `SampleDistances`, `SampleWidths`, `Start`, `Finish`, `LengthCm`, `WidthCurve`, `BaseWidthCm`, `MinWidthCm`, `Segments`, `Catalog`, `Placements`, `PrevailingWindYaw`, `PlacementAttempt`, `BalloonRadiusCm`, `BalloonHalfHeightCm`, `Borders`, `LeftBoundary`, `RightBoundary`, `RoutePoints` |
| `FBalloonPlacementDescriptor` (C++) | `Biome`, `RowName`, `Rule`, `InstanceIndex`, `SegmentId`, `ClusterId`, `BirdsPerFlock`, `Asset`, `Transform`, `Representation` (`HISM`/`MeshActor`/`BlueprintActor`), `Collision`, `InteriorEnemy`, `CollisionBounds`, `CullEndCm` |

## Pure C++ planner APIs
Headers in `Source/gunsandballoons/BalloonLevel/`. All are free of UObject ownership except where marked.

| Namespace / function | Purpose |
| --- | --- |
| `BalloonSplinePlanning::Build(Config, Plan, Errors)` | Validates the config, builds the centerline (bounded sine heading field: no self-crossing by construction), widths, segments, start and finish. |
| `PositionAt`, `WidthAt`, `SegmentAt`, `NearestDistance(Plan, Pos, Edge, FullSearch)` | Queries by arc length / position. |
| `FBalloonLevelProgress::Update(Plan, Pos, Teleported)` | Tracked and monotonic progress. |
| `BalloonAssetCatalog::ValidateRow`, `Merge(Shared, Biome, Out, Diag)` | Row validation and shared + biome merge. |
| `BalloonBorderPlanning::Build`, `RightAt`, `ProjectedRadius`, `CrossSection`, `UnsafeWidth` | Border spheres, slab projection, unsafe-width union. |
| `BalloonPlacementPlanning::Resolve(Config, Catalog, Rows, Diag)` | **Game thread**; loads assets, measures StaticMesh bounds and derives Blueprint Actor bounds from the class's native components and `SimpleConstructionScript` templates. |
| `BalloonPlacementPlanning::Build(Config, Plan, Rows, Diag, GroundSampler?, FieldBorders?)` | The six rules → descriptors. Deterministic per `Seed` and `PlacementAttempt`. |
| `BalloonTerrain::Build(Config, Plan, Mesh, Diag)`, `Spawn(Owner, Config, Mesh, Component, Diag)` | Terrain mesh and transient actor. |
| `BalloonHorizontalRoute::InspectPawn(Pawn, Envelope, Error)` | Collision envelope (cylinder → box over every query shape) and `BalloonFlightComponent` limits read by reflected names `MaxSpeed`, `MaxTurnRate`, `TurnAcceleration`, `TurnDeceleration`, `Acceleration`, `Deceleration`. Renaming or retyping these floats makes them read as 0 and generation fails. |
| `BalloonHorizontalRoute::WorldSweep(World, Ignore)` | Box overlap/sweep against all object types, ignoring the pawn and its attached actors. |
| `BalloonHorizontalRoute::Validate(Plan, Envelope, Settings, Sweep, Current?)` | Fixed-Z route proof: direct quintic-Bezier attempt, then A* over (progress, lateral) with curvature and yaw-acceleration bounds. Never accepts an infeasible level; may reject a feasible one. |
| `BalloonHorizontalRoute::TryBorders(Plan, Proposed, Current, Envelope, Settings, Sweep)` | Transactional border proposal. |

Hard limits (explicit failure, never clamped): 100000 samples, segments or lattice states; 10000 borders or placements; 256 attempts per item; 500000 planning/sweep operations; curve subdivision depth 10; terrain 250000 vertices.

## Determinism
| Stream | Seed |
| --- | --- |
| Geometry | `Seed ^ 0x47454F31` |
| Border *i* | `Seed ^ 0x424F5231 ^ (i × 0x9E3779B9)` |
| Placement (wind, flock budget, per row, per item) | FNV-1a of `biome|catalogVersion|Placement/v1|tag`, XOR `Seed`, XOR `attempt × 0x9E3779B9` |
| Terrain relief | `Seed` |
| Enemies | `Seed ^ 0x454E454D` |
| Loot type and occupancy | `Seed` (two independent streams) |

Same seed, config and catalog give the same level. `RandomizeSeed` and `BiomeSelection=Random` use global random; reproduce a run from the logged `Generation biome=… seed=…` line.

## Diagnostics and status codes
| Status | Meaning |
| --- | --- |
| `Ready` | Plan proven and realized. |
| `InvalidConfig` | Config, start options, catalog, placement resolve or terrain build rejected. |
| `InsufficientSpace` | Border planning failed (spacing or count impossible). |
| `Unreachable` | No pawn/envelope, bad `RouteAttempts`, or no fixed-Z route in any placement attempt. |
| `Failed` | Terrain spawn, balloon placement, rocket class load or realization failed. |

When regeneration fails with a previous level, the status stays `Ready` and the diagnostics end with `M8: regeneration failed; previous realized level retained`.

## Automation
All tests are `GunsAndBalloons.BalloonLevel.*`: `M1`–`M5`, `M6.Events`, `M6.RuntimeSafety`, `M8.Realization`, `GenerationModes`, `EnvironmentConfig`, `Terrain`, `Enemies`, `Completion`, `Controller`, `StartOptions`, `StartMethod` (16). Run `Scripts/Verify-BalloonLevel.ps1` (editor closed) or:
```text
UnrealEditor-Cmd.exe gunsandballoons.uproject /Engine/Maps/Entry -unattended -NullRHI -ExecCmds="Automation RunTests GunsAndBalloons.BalloonLevel;Quit"
```
Real-content smoke test (needs the editor; prints `LEVEL_GENERATION_PIE_PASS`): `py Scripts/Editor/test_level_generation_pie.py`. Biome matrix: `py Scripts/Editor/test_biome_asset_catalogs.py`.

### Level-start route fallback (2026-11-15)

`LevelStartSeedRetries` on the generator allows 0-12 additional attempts at BeginPlay (default 3; tuning TBD). Each retry copies the resolved config and increments its seed by `0x9E3779B9` with uint32 wraparound, preserving biome and overrides. The successful snapshot seed is logged for manual replay. Manual generation/regeneration never retries seeds. Route/space failures are retryable; invalid configuration and realization failures stop. All attempt diagnostics are retained.

`BalloonSplinePlanning::Build` accepts optional `PawnMinimumRadiusCm` (default 0). Geometry uses max(config radius, pawn radius), reserving curvature through config `RouteCurvatureMargin` (finite >= 1, default 3; tuning TBD). Before placements, the generator proves the empty-placement route with staged terrain, authored collision and borders. A failure skips placement retries. Full placement proof and transactional rollback remain required.
