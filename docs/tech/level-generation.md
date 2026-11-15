# Level generation: technical reference (C++)

> **For agents.** How the procedural balloon level generator works and how other systems connect to it. Every type and function is in the [API reference](BalloonLevel/API.md). Design lives in [biomes](../features/biomes.md); the original specification is [docs/temp/…Implementation_Plan.v1.md](../temp/Balloon_Level_Generation_LLM_Implementation_Plan.v1.md). Defects and validation: [BalloonLevel/STATUS.md](BalloonLevel/STATUS.md). If code and design disagree, the design wins. **Updated:** 2026-11-15

## Core rules
- **Plan → validate → realize.** Planners are pure C++ functions over a plain `FBalloonLevelPlan`. A route is proven before the plan is committed, and only then are world objects created. Realization is transactional: a failed generation keeps the previous level.
- **No vertical axis.** The balloon flies at one absolute world Z (`FlightHeightMeters`). Routing is 2D (progress × lateral) with full 3D box sweeps at that Z. Never add altitude nodes, climb paths or high/low bypasses.
- **Lighting is fixed.** `Day` or `Night` is stored in the plan and applied once. No event, API or tick may change it.
- **Deterministic.** Same seed, config and catalog version give the same level. Each subsystem has its own stream; never use global random for layout. See [determinism](BalloonLevel/API.md#determinism).
- **Units.** Config fields are meters. Plan descriptors and planner APIs are centimeters.
- **Prototype values.** Every numeric default is a tuning input (TBD) except the approved 600 m corridor width. Do not present them as design.

## Files
| File | Role |
| --- | --- |
| `BalloonLevelGenerator` | `ABalloonLevelGenerator`: the only entry point. Orchestrates the pipeline, owns spline, progress, proxies, completion zone and the environment binding. `BalloonLevelEnemies.cpp` holds `SpawnLevelEnemies`. |
| `BalloonLevelConfig` | `UBalloonLevelConfig` DataAsset and `Validate()`. |
| `BalloonLevelStartOptions` | `FBalloonLevelStartOptions` and `UBalloonLevelLaunchSubsystem` (menu → level handoff). |
| `BalloonLevelTypes.h` | Enums, `FBalloonLevelAssetRow` (DataTable row), border, placement and plan structs. |
| `BalloonSplinePlanner` | Centerline, widths, segments, nearest-distance queries, `FBalloonLevelProgress`. |
| `BalloonAssetCatalog` | Row validation and shared + biome table merge. |
| `BalloonBorderPlanner` | Border threat spheres, flight-slab projection, unsafe width. |
| `BalloonPlacementPlanner` | Six placement rules → descriptors. |
| `BalloonTerrainBuilder` | Seeded terrain mesh and transient terrain actor. |
| `BalloonHorizontalRouteValidator` | Pawn envelope, world sweep, route proof, border transactions. |
| `BalloonLevelRealizer` | HISM scenery, gameplay actors, border proxies, loot pickups; staging and rollback. |
| `BalloonLevelEventManager` | Time, progress and signal events. |
| `BalloonBorderThreat` | Stationary border influence proxy. |
| `BalloonEnvironmentPresetController` | Fixed Day/Night, biome fog, ground and sky. |
| `Tests/BalloonLevel*Test.cpp`, `BalloonEnvironmentConfigTest.cpp` | Automation tests `GunsAndBalloons.BalloonLevel.*`. |
| `Content/BalloonLevel/Rules/` | Native `DT_BalloonAssets_*` tables, their JSON sources and the authoring contract. |

## Pipeline (`GenerateLevel`)
1. **Choose and copy the config.** Start options, biome pool or `LevelConfig` → transient `ActiveConfig`. Overrides and `RandomizeSeed` apply to the copy; assets are never modified. At BeginPlay only, unreachable/space failures allow `LevelStartSeedRetries` additional seeds (default 3, valid 0-12, tuning TBD), preserving the chosen biome and resolved overrides and retaining diagnostics for all attempts. Manual calls use one seed. Config resolution order is in the [API](BalloonLevel/API.md#config-resolution-what-generatelevel-actually-does).
2. **Pawn inspection and spline** (`BalloonSplinePlanning::Build`). The planner uses the same effective turning radius as the prover (max of pawn and config), with a configurable curvature reserve (`RouteCurvatureMargin`, default 3, tuning TBD). Config invalid → `InvalidConfig`.
3. **Catalog merge**, then **borders** (`InsufficientSpace` on failure).
4. **Envelope settings.** `RoutePawn` supplies the collision envelope and movement limits in step 2. Missing pawn → `Unreachable`.
5. **Placement resolve** (loads assets, measures bounds), then **terrain staging** (generated terrain is spawned before the proof so it can block routes).
6. **Up to `RouteAttempts` (1–12) tries:** build placements (salted by attempt) and prove the fixed-Z route against the world, obstacles, borders and the corridor. The first reachable attempt wins; otherwise `Unreachable`. Geometry and borders are **not** re-rolled between placement attempts. An empty-placement preflight against staged terrain, world collision and borders must pass first; failure skips all placement attempts.
7. **Commit** the plan and spline, then **move the balloon** to the route start (position and yaw; scale and attached riders preserved).
8. **Realize.** HISM scenery, colliding actors, border proxies and rockets, loot pickups. Any failure restores the balloon, removes staged objects and keeps the old level.
9. **Finish.** Terrain ownership moves to the realizer; the completion box moves to the finish; the event timeline resets; border proxies register; environment applied; functional enemies spawned. Status `Ready`.

While old content is being replaced, its collision is disabled so it cannot block the new route proof.

## Interaction with other systems
- **Balloon (`BP_Balloon`, `UBalloonFlightComponent`).** The generator reads `MaxSpeed`, `MaxTurnRate`, `TurnAcceleration`, `TurnDeceleration`, `Acceleration`, `Deceleration` by reflected name. Renaming or retyping them breaks generation ("valid movement limits"). A tilted pawn or a nonzero current turn rate is rejected. Runtime border changes additionally require the balloon's Z to equal the flight height and zero current yaw rate.
- **Player and riders.** Actors attached to the balloon are ignored by route sweeps and move with it when the balloon is placed.
- **World collision.** Sweeps treat every query-enabled object type as a blocker, including authored geometry and authored border proxies. Generated content is excluded only while it is being replaced.
- **Progress and events.** The event manager tick calls `Generator->UpdateProgress(RoutePawn location)` each frame, then advances game time. Implement gameplay (enemy spawns, wind, fog hazards) in `OnLevelEventTriggered` receivers keyed by `EventId`. Receivers must validate their own colliding spawns: an event is not spawn authorization.
- **Completion.** The generator tick tests the pawn's origin against the finish box and calls `CompleteLevel()` once. Bind `OnLevelCompleted` for the safe-zone transition (no travel is implemented here).
- **Nemesis airship and biome enemies.** Spawned by `SpawnLevelEnemies` after the level is Ready; they are not part of the route proof. See [nemesis-airship.md](nemesis-airship.md).
- **Main menu.** The menu only collects settings and calls `StartLevel`. See [main-menu.md](main-menu.md) and [menus](../features/ui-menus.md).
- **Editor.** Plans are transient: not saved in the map and not copied into PIE. Generate again after loading, or use **Every Level Start**.

## Recipes
- **New biome.** Add rows to a new `DT_BalloonAssets_<Biome>` table and a config with that `BiomeId`. Only `scatter` works for a new biome; the specialized rules are tied to their biome. Start options accept only the four shipped biomes.
- **Radius event.** `Action=ScaleBorderRadius`, `Value=1.15`, optional `SegmentId`/`BorderId`/`TargetTag`. An unsafe change arrives as `OnLevelEventRejected`; nothing is committed.
- **Reproduce a run.** Read `Generation biome=… seed=…` from the diagnostics, disable `RandomizeSeed`, and set the config seed.

## Limits and pitfalls
- The route validator is deliberately conservative: it uses the global minimum width, a box around a cylinder and all object types. It can reject feasible levels and never accepts an infeasible one. **With random seeds a small share of generations is rejected** (see [decisions.md](../decisions.md)); level-start generation now retries a bounded number of distinct seeds. Exhaustion still fails explicitly; it cannot fix an impossible config or blocked world.
- Hard limits fail explicitly and never clamp: 100000 samples, segments or lattice states; 10000 borders or placements; 256 attempts per item; 500000 planning/sweep operations; curve subdivision depth 10; terrain 250000 vertices.
- Rows of a biome-gated rule in another biome are skipped without a diagnostic.
- Blueprint-actor rows that collide cannot pass the route proof (no collision bounds are resolved for Blueprint classes). Shipped tables contain only static meshes, so this is latent.
- `Complete` status is reserved and never set.

## Verification
`powershell -ExecutionPolicy Bypass -File Scripts/Verify-BalloonLevel.ps1` builds the full project and runs all 16 `GunsAndBalloons.BalloonLevel.*` tests (close the editor first; Live Coding blocks the build). Real-content checks run in the editor: `Scripts/Editor/test_level_generation_pie.py` and `test_biome_asset_catalogs.py` (see [Scripts/Editor/README_LevelGeneration.md](../../Scripts/Editor/README_LevelGeneration.md)).
