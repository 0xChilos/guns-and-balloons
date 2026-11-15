# Level generation — technical reference (C++)

> **For agents.** How the procedural balloon level generator works, its API, and how other systems connect to it. Design lives in [biomes](../features/biomes.md); the full spec is [docs/temp/…Implementation_Plan.v1.md](../temp/Balloon_Level_Generation_LLM_Implementation_Plan.v1.md). Progress and known defects are in [BalloonLevel/STATUS.md](BalloonLevel/STATUS.md). If code and design disagree, the design wins. **Updated:** 2026-11-15

## Core rules
- **Generated borders (2026-11-15).** Realization now creates one transient `ABalloonBorderThreat` proxy per seeded border descriptor and optionally a rocket balloon from `UBalloonLevelConfig::BorderActorClass`. The class is a soft reference for cooking; an empty reference gives proxies only. Rocket classes must expose floating-point `AggroRangeM` and `DriftRangeM`. Engagement range matches the descriptor in meters; drift is zero. The proxy supplies collision, while the rocket actor's collision is disabled. Non-damaging policies disable the rocket actor's tick. Proxies register automatically after a successful commit, and validated radius changes synchronize the bound rocket's range. Borders share the realizer's staging, rollback, regeneration and Clear ownership. `RandomizeSeed` controls fresh positions; fixed seeds remain deterministic. The preview setup script removes legacy authored markers and enables randomized seeds.
- **Plan → validate → realize.** Planners are pure C++ functions over a plain `FBalloonLevelPlan` with no UObject ownership. Validation proves a route before the plan is committed. **Realize (spawning HISM/actors, M8) is not in the committed code yet; `BalloonLevelRealizer` is in progress. Update this page when it lands.** A generated level is currently data plus debug lines only.
- **No vertical axis.** The balloon flies at one absolute world Z (`FlightHeightMeters`). Routing is 2D (progress × lateral) with full 3D collision sweeps at that Z. Never add altitude nodes, climb paths or high/low bypasses.
- **Lighting is fixed.** `Day` or `Night` is stored in the plan. No event, API or tick may change it.
- **Deterministic.** Same seed + config + catalog version gives the same plan. Each subsystem has its own stream; never use global random for layout.
- **Units.** Config fields are meters. Everything in the plan, descriptors and APIs is centimeters (cm/s, radians/s inside the route validator).

## Files
| File | Role |
| --- | --- |
| `BalloonLevel/BalloonLevelGenerator.h/.cpp` | `ABalloonLevelGenerator`: the only entry point; orchestrates the pipeline, owns spline, progress, proxies, events |
| `BalloonLevel/BalloonLevelConfig.h/.cpp` | `UBalloonLevelConfig` DataAsset and `Validate()` |
| `BalloonLevel/BalloonLevelTypes.h` | Enums, `FBalloonLevelAssetRow` (DataTable row), `FBalloonBorderInfluence`, plan/segment/placement descriptors |
| `BalloonLevel/BalloonSplinePlanner.h/.cpp` | `BalloonSplinePlanning::*`: centerline, widths, segments, nearest-distance; `FBalloonLevelProgress` |
| `BalloonLevel/BalloonAssetCatalog.h/.cpp` | Row validation and shared + biome table merge |
| `BalloonLevel/BalloonBorderPlanner.h/.cpp` | `BalloonBorderPlanning::*`: border threat placement, flight-plane projection, unsafe width |
| `BalloonLevel/BalloonPlacementPlanner.h/.cpp` | `BalloonPlacementPlanning::Resolve/Build`: six placement rules → descriptors |
| `BalloonLevel/BalloonHorizontalRouteValidator.h/.cpp` | `BalloonHorizontalRoute::*`: pawn envelope, world sweep, A* proof, border transactions |
| `BalloonLevel/BalloonLevelEventManager.h/.cpp` | `UBalloonLevelEventManager`: time/progress/signal events |
| `BalloonLevel/BalloonBorderThreat.h/.cpp` | `ABalloonBorderThreat`: optional stationary world proxy for one border threat |
| `Tests/BalloonLevelM1…M6Test.cpp`, `BalloonLevelModesTest.cpp` | Automation tests `GunsAndBalloons.BalloonLevel.*` |
| `Content/BalloonLevel/Rules/README.md` | DataTable authoring contract and exact rule semantics |
| `Scripts/Editor/setup_level_generation_preview.py`, `test_level_generation_pie.py` | Preview fixture in `/Game/Scenes/MainLevel` and PIE smoke test |
| `Scripts/Verify-BalloonLevelM6.ps1` | Isolated clean build + all BalloonLevel tests |

## Pipeline (`GenerateLevel`)
1. `ClearGeneratedLevel()`: resets the plan, spline, progress, proxies, event timeline and `ActiveConfig`.
2. Choose a config: `LevelConfig`, or a random valid entry of `RandomBiomeConfigs` (`BiomeSelection=Random`). Duplicate it into the transient `ActiveConfig` and optionally replace the seed (`RandomizeSeed`). Source assets are never modified.
3. `BalloonSplinePlanning::Build`: validates the config and builds the centerline, sample widths, segments, start/finish. Failure → `InvalidConfig`.
4. `BalloonAssetCatalog::Merge(Shared, Biome)`. Failure → `InvalidConfig`.
5. `BalloonBorderPlanning::Build`: border threats. Failure → `InsufficientSpace`.
6. `InspectPawn(RoutePawn)`: collision envelope and movement limits. No pawn → `Unreachable`.
7. `BalloonPlacementPlanning::Resolve`: loads assets and measures bounds (game thread).
8. Up to `RouteAttempts` (1–12) times: `BalloonPlacementPlanning::Build` (salted by attempt), then `BalloonHorizontalRoute::Validate`. The first reachable attempt wins; otherwise → `Unreachable`.
9. Commit the plan, write the spline points, set status `Ready`. `Complete` is reserved for M8.

The status passes through `Planning`, `Validating`, then ends at `Ready` or an error. `GetDiagnostics()` lists every step, including the biome/seed used and the reason for each failed attempt.

## `ABalloonLevelGenerator` API
| Member | Kind | Behaviour |
| --- | --- | --- |
| `LevelConfig`, `RoutePawn` | EditAnywhere | Config asset and the **placed balloon actor** (`BP_Balloon` instance) used for collision/movement inspection |
| `GenerationMode` | EditAnywhere | `Manual` (default) or `OnLevelStart` (generate in `BeginPlay`) |
| `BiomeSelection`, `RandomBiomeConfigs`, `RandomizeSeed` | EditAnywhere | Biome/seed choice (pipeline step 2) |
| `ActiveConfig` | Transient, read-only | The config copy actually used |
| `GenerateLevel()` / `RegenerateLevel()` | BlueprintCallable → bool | Full pipeline; `true` = `Ready` |
| `GenerateLevelInEditor()` / `RegenerateLevelInEditor()` | CallInEditor | Details-panel buttons (buttons must have no params/return) |
| `ClearGeneratedLevel()` | BlueprintCallable, CallInEditor | Drops the plan; spawns/destroys nothing |
| `GetGenerationStatus()`, `GetDiagnostics()` | BlueprintPure | Status enum, human-readable log |
| `GetStartTransform()`, `GetFinishTransform()` | BlueprintPure | Plan start/finish at flight Z |
| `GetWidthAtDistanceCm(S)` | BlueprintPure | Corridor width at arc length S |
| `GetBorderInfluences()` | BlueprintPure | Copy of current border descriptors |
| `GetNormalizedProgress(Pos)` | BlueprintCallable | Stateless 0..1 projection (full search) |
| `UpdateProgress(Pos, bTeleported)` | BlueprintCallable | Tracked progress (local search; full search on teleport) and feeds the event manager |
| `GetCurrentProgressCm()`, `GetMaxReachedProgressCm()` | BlueprintPure | Current and monotonic max arc length |
| `RegisterBorderProxy(Proxy)` | BlueprintCallable → bool | Link an initialized `ABalloonBorderThreat` to its descriptor (exact match, unscaled, same world) |
| `OnBorderChanged` | BlueprintAssignable | Fires after a validated radius change has been committed to plan and proxies |
| `DrawBorderDebug(Duration)` | BlueprintCallable | Green route proof, cyan corridor edges, red/silver threat spheres + flight-plane slices |
| `EventManager` | Component | See below |
| `GetPlan()` | C++ only | Const `FBalloonLevelPlan&` (route points, placements, segments, etc.) |
| `TryEventBorderChange(Event, Reason)` | C++ only | Radius transaction used by the event manager |

## `UBalloonLevelConfig` (DataAsset)
All values are prototype tuning inputs (`TBD` in design), except the 600 m width from [biomes](../features/biomes.md).

| Group | Fields |
| --- | --- |
| Geometry | `Seed`, `LengthMeters` 1500, `BaseWidthMeters` 600, `SegmentTargetMeters` 250, `FlightHeightMeters` 100 (**absolute world Z**), `StartTransform` (XY + yaw only), `MaxHeadingDegrees` 25 (0 = straight, max 45), `MinimumTurningRadiusMeters` 100, `GeometrySampleMeters` 10, `MinWidthMeters` 40, `WidthProfile` (normalized distance → width multiplier) |
| Biome | `BiomeId`, `SharedAssetTable`, `BiomeAssetTable`, `CatalogVersion` |
| Lighting | `Preset` Day/Night (stored, not yet applied) |
| Borders | `BordersEnabled` (false), `BorderExactCount` (-1 = density, 0 = none), `BorderDensityPerKm` 8, `BorderRadiusMin/MaxMeters` 12/35, `BorderIntrusionFraction` 0.5, `BorderMinSpacingMeters`, `BorderAllowOverlap`, `BorderPolicy`, `BorderHeightOffsetMeters`, `BalloonRadius/HalfHeightMeters` (M3 preview only) |
| Placement | `SceneryEnabled`, `ObstaclesEnabled`, `EnemiesEnabled`, `ObstacleExactCount`/`EnemyExactCount` (-1 = density), `PlacementMetadata` (row name → `InteriorEnemy`, `StreetLamp`, `SizeMeters`, `CullEndCm`), `ClusterRadiusMeters`, `Street*Meters`, `PlacementGroundZMeters` |
| Terrain | `TerrainMode`: `Generated` (default, seeded height variation), `Flat` (generated plane), or `WorldCollision` (tagged authored ground). Generated settings: `TerrainHeightVariationMeters` 15, `TerrainWavelengthMeters` 200, `TerrainCellMeters` 20, `TerrainPaddingMeters` 100; all prototype tuning. `PlacementGroundZMeters` is the elevation datum. `TerrainMaterial` overrides `MI_<Biome>_Ground`. World collision uses `TerrainActorTag` (`Terrain`) and absolute `TerrainTraceTopMeters` / `TerrainTraceBottomMeters` (1000 / -1000 m). |

`BalloonTerrainBuilder` builds a bounded grid covering corridor bounds plus padding (250000 vertices maximum), with seed-stable two-frequency Perlin relief. A transient ProceduralMeshComponent actor supplies rendering and synchronous triangle collision before placement/route proof. Scenery samples the actual triangles; horizon rings keep their datum outside the terrain footprint. Failed generation destroys the staged ground and restores previous terrain collision. Successful generation transfers terrain ownership to the realizer for regeneration, clear and EndPlay. Existing authored ground is not removed; keep it clear of the generated surface. Fixture creation chooses rolling Forest/Desert and flat Field/City as prototype tuning.
| Route | `RouteCellMeters` 10, `RouteSafetyMeters` 1, `RouteWindMeters` 0, `RouteAttempts` 12 |

The existing `ABalloonLevelGenerator` is the unified controller. Details exposes `LevelConfig`, biome pool, generation buttons, `OverrideHeights` (absolute flight/ground meters), `OverrideTerrain` (mode, variation, wavelength, grid, padding, material), and lighting/environment overrides. `SetupLevelController` reuses explicit or unique environment/light/fog actors, creates missing actors transiently, and attaches them below the generator while preserving world transforms. Ambiguous actor searches require explicit assignment. `PreviewEnvironment` uses the selected config and controller lighting override. Generated actors, terrain and HISM components attach to the generator. Setup is idempotent; deletion/EndPlay removes only environment actors it spawned. Existing authored actors are retained. Successful generation teleports RoutePawn to Plan.Start (configured XY, absolute flight height and initial yaw), preserving scale and carrying attached riders. Balloon placement rolls back if realization fails; earlier failures never move it. Flight height remains fixed during play.

DataTable rows use `FBalloonLevelAssetRow` with exactly 12 columns: `Asset, Category, Collision, DensityPerKm2, ScaleMin, ScaleMax, MinSpacingM, RandomYaw, AlignToSlope, ClusterSize, Rule, Notes`. See the [Rules README](../../Content/BalloonLevel/Rules/README.md) for categories, rules and validation. A biome row replaces a shared row with the same name.

## Completion zone
Successful generation positions the generator's green `CompletionZone` box at `Plan.Finish`, aligned to the final route direction. Its full width matches the corridor; half-depth is editable as `CompletionZoneHalfDepthMeters` (10 m prototype), and half-height covers the balloon envelope with a 10 m prototype minimum. These values require tuning. The box is visible when selected in the editor and has no physics/query collision. During gameplay the generator tick tests `RoutePawn`'s origin against the box, independently of the event timeline. Entry calls Blueprint-callable `CompleteLevel()` once, sets `LevelCompleted`, pauses the event timeline and broadcasts `OnLevelCompleted`. Bind that delegate for the future safe-zone transition; no level travel is implemented. Successful regeneration resets completion and moves the zone; failed regeneration retains the previous zone/state. Clear disables completion.

## `UBalloonLevelEventManager` API
| Method / member | Behaviour |
| --- | --- |
| `RegisterEventDefinitions(Array)` | Atomic replace while stopped; rejects duplicates, NaN, unknown enums and any Day/Night swap request |
| `StartEventTimeline()` | Starts the clock. Requires generator `Ready`. **Not called automatically, even with `OnLevelStart`.** |
| `PauseEventTimeline(bool)`, `ResetEventTimeline()` | Freeze; reset elapsed/progress/fired state (definitions and bindings persist) |
| `UpdateElapsedGameTime(dt)`, `UpdatePlayerProgress(N, m)`, `EmitCustomSignal(Id, Target)` | Manual drivers. The component ticks by itself and samples `RoutePawn`; disable its tick before driving it manually |
| `OnLevelEventTriggered(Event)`, `OnLevelEventRejected(Id, Reason)` | Blueprint receivers. Filter `TargetTag`/`EventId` yourself |
| `GetSnapshot()` | Elapsed, progress, watermark, per-event fired/cooldown, fixed preset |

`FBalloonLevelEventDefinition`: `EventId`, `Enabled`, `Trigger` (ElapsedGameTime s / NormalizedProgress 0..1 / AbsoluteProgress m / CustomSignal), `Threshold`, `SignalId`, `TargetTag`, `Action` (`Blueprint`, `SetBorderRadius` m, `AddBorderRadius` m, `ScaleBorderRadius` ×), `Value`, `BorderId`/`SegmentId` (-1 = all), `Priority`, `Repeat` (Once / RepeatAfterCooldown / OnEveryCrossing), `CooldownSeconds`. Order within a sample is threshold, then priority, then EventId. `Once` fires at most once, even if rejected. Large steps never skip an event.

## `ABalloonBorderThreat`
Stationary, non-ticking proxy for one border descriptor. Spawn deferred, call `InitializeInfluence(Descriptor)` before BeginPlay, finish spawning, then `Generator->RegisterBorderProxy(Proxy)`. Policy → collision on `ECC_Pawn` and `ECC_WorldDynamic` (the balloon is `BlockAllDynamic`): `Blocked` blocks, `Damaging`/`Pushback` overlap and call BP `OnBalloonInfluenceEntered(Actor, Policy)`, `VisualOnly` has no collision. `OnInfluenceInitialized` is the visual hook. Radius changes come only through the validated transaction. Nothing spawns proxies automatically yet.

## Pure C++ APIs (for tests and tools)
- `BalloonSplinePlanning::Build / PositionAt / WidthAt / SegmentAt / NearestDistance`
- `BalloonAssetCatalog::ValidateRow / Merge`
- `BalloonBorderPlanning::Build / RightAt / ProjectedRadius / CrossSection / UnsafeWidth`
- `BalloonPlacementPlanning::Resolve` (game thread, loads assets) / `Build(Config, Plan, Rows, Diag, GroundSampler?, FieldBorders?)`
- `BalloonHorizontalRoute::InspectPawn / WorldSweep / Validate(Plan, Envelope, Settings, Sweep, Current?) / TryBorders`

All of them fail transactionally: on failure they leave no partial output and append a diagnostic naming the table, row, rule or segment.

## Interaction with other systems
- **Balloon (`BP_Balloon` + `UBalloonFlightComponent`).** `InspectPawn` encloses every query-enabled `UShapeComponent` (yaw-invariant cylinder → box). It also reads the flight component's protected `MaxSpeed`, `MaxTurnRate`, `TurnAcceleration`, `TurnDeceleration`, `Acceleration`, `Deceleration` **by reflected name**. Renaming or retyping those floats makes them read as 0, and generation then fails with "valid movement limits". It never modifies movement. Tilted pawns or a non-zero current turn rate are rejected.
- **Balloon placement.** The generator does **not** move the balloon. Its proof starts from `GetStartTransform()`. Runtime border changes validate from the balloon's real position and require its Z to equal `FlightHeightMeters × 100`. Keep the config start/height and the placed balloon in sync, as `setup_level_generation_preview.py` does, or move the balloon to `GetStartTransform()` before play.
- **Player / riders.** Actors attached to the balloon (the `BP_BalloonPlayer` camera pawn) are ignored by route sweeps.
- **World collision.** `WorldSweep` treats every query-enabled object type as a blocker, including ground, authored level geometry and border proxies. Only `RoutePawn` and its attached actors are ignored.
- **Progress → events.** The event manager tick calls `Generator->UpdateProgress(RoutePawn location)` every frame, then advances game time. Pause (world or explicit) freezes everything.
- **Blueprint gameplay** (enemy spawns, wind, fog hazards). Implement it in `OnLevelEventTriggered` receivers keyed by `EventId`. Receivers must validate their own colliding spawns; an event is not spawn authorization.
- **Editor.** Use the Details buttons on the placed generator. Plans are transient: they are not saved in the map and not copied into PIE, so generate again after loading or in PIE (or use `OnLevelStart`).

## Determinism
| Stream | Seed |
| --- | --- |
| Geometry | `Seed ^ 0x47454F31` |
| Border item *i* | `Seed ^ 0x424F5231 ^ (i × 0x9E3779B9)` |
| Placement (wind yaw, flock budget, per row, per item) | FNV-1a of `biome|catalogVersion|Placement/v1|tag`, XOR `Seed`, XOR `attempt × 0x9E3779B9` |

Changing obstacles or catalogs never moves geometry or borders. Retries change only placement. The biome choice under `Random` and `RandomizeSeed` use global random; read the chosen values from diagnostics or `ActiveConfig`.

## Recipes
- **New level:** create a `BalloonLevelConfig` DataAsset, place an `ABalloonLevelGenerator`, set `LevelConfig` and `RoutePawn`, and align the config's `StartTransform`/`FlightHeightMeters` with the balloon. Press **Generate Level** and read Status/Diagnostics.
- **Events:** on BeginPlay (after `Ready`), call `RegisterEventDefinitions`, bind `OnLevelEventTriggered`/`Rejected`, then call `StartEventTimeline`.
- **Border radius event:** `Action=ScaleBorderRadius`, `Value=1.15`, an optional `SegmentId`/`BorderId`/`TargetTag`. An unsafe change arrives as `OnLevelEventRejected` with a reason; nothing is committed.
- **New biome:** set a new `BiomeId` and table. Only `scatter` works for it; the other rules are tied to their biome (see STATUS F7).

## Limits and pitfalls
- Nothing is realized in the world: no HISM, actors, fog, ground or lighting (M7/M8). See STATUS for the known defects F1–F9. The most important ones: enemies and Blueprint obstacles cannot pass validation, `line` needs an exact count, and failure wipes the last good plan.
- Hard limits: 100000 samples, segments or lattice states; 10000 borders or placements; 256 attempts per item; 500000 planning/sweep operations; curve subdivision depth 10. They fail explicitly and never clamp.
- The route validator is deliberately conservative: it uses the global minimum width, a box around a cylinder, and all object types. It can reject feasible levels and never accepts an infeasible one.

## Verification
`powershell -ExecutionPolicy Bypass -File Scripts/Verify-BalloonLevelM6.ps1` runs an isolated clean build and all `GunsAndBalloons.BalloonLevel.*` tests (8). For the full project (the editor may stay open), run `UnrealEditor-Cmd.exe gunsandballoons.uproject -unattended -NullRHI -ExecCmds="Automation RunTests GunsAndBalloons.BalloonLevel" -TestExit="Automation Test Queue Empty"`. Last result: 2026-11-15, 8/8 passed in both. The PIE smoke test is `py Scripts/Editor/test_level_generation_pie.py` (prints `LEVEL_GENERATION_PIE_PASS`). History is in [TEST_REPORT.md](BalloonLevel/TEST_REPORT.md).
