# Balloon level generation handoff

Updated: 2026-11-15. Current milestone: M8 implementation, verification incomplete. Stopped at user request. Do not start M9.

## M8 ? implemented, acceptance gate pending

Implemented `UBalloonLevelRealizer` default component and generator integration: one NoCollision HISM per distinct mesh, descriptor cull distances 15000/30000/80000/0, conflicting same-mesh profiles rejected; individual colliding mesh/Blueprint Actors; decorative Actor exception restricted to flock. Actor primitive distance culling and Cull Distance Volume overrides are disabled. Generated components/Actors are transient and excluded from duplication. Cleanup tracks exact components and weak Actor references; no class/tag-based deletion. Generator deletion explicitly clears owned content.

Existing Details generation/regeneration/clear buttons now realize preview content. Regeneration stages new content, removes staged objects on failure and retains the previous successful plan/config/progress/world content; old owned collision is temporarily disabled during route generation. Events/proxy registration reset only on successful replacement. Blueprint construction/BeginPlay side effects cannot be rolled back. Existing authored border proxies remain explicit M6 registrations; no border visual/behavior Blueprint or environment asset was invented. M7 is not implemented by this work.

Added explicit debug toggles for route/corridor/borders, spline, segments, placements (biome/rule/IDs/cull and certified bounds), HISM group counts/bounds/culls and start/finish markers. Realization diagnostics record descriptor/component/Actor counts and synchronous milliseconds including loads. No generator tick or OnConstruction regeneration.

Added `BalloonLevelM8Test.cpp` and `Scripts/Verify-BalloonLevelM8.ps1` for real UWorld/HISM/mesh Actor checks, cull state, transactional conflict rejection, owned cleanup and a 10000-instance CPU workload. Final test also checks generator deletion cleanup. No uassets/maps were created or modified.

Observed verification: initial isolated compile found two test-only C++ errors, corrected. A subsequent isolated UE 5.9 build succeeded (22.27s local actions), but this predates the final deletion-cleanup/debug/duplication refinements. Automation stalled loading the default OpenWorld map before tests produced results; commandlet stopped, script changed to Entry map. Full-project UHT passed, but full C++ builds were stopped without a result. Final-source verification and Entry-map automation were queued, then stopped at the user's explicit request. No completed M8 automation results or profiling numbers exist. No rendered culling, flock Blueprint integration, editor visual inspection or PIE verification is claimed. M8 acceptance gate remains OPEN.

Next action only when authorized: rerun `powershell -ExecutionPolicy Bypass -File Scripts/Verify-BalloonLevelM8.ps1 -Incremental`, inspect the JSON report, verify source-copy hashes, complete full-project build and real-content/editor/PIE/rendering checks, then record actual profile results. Preserve pre-existing generation-mode edits and unrelated concurrent documentation changes. No commit. Stopped after recording state.

## Historical M6 handoff

## M6 timed gameplay events

Implemented `UBalloonLevelEventManager` as the generator's default ActorComponent. Blueprint APIs: RegisterEventDefinitions, StartEventTimeline, PauseEventTimeline, ResetEventTimeline, UpdateElapsedGameTime, UpdatePlayerProgress, EmitCustomSignal, GetSnapshot; delegates OnLevelEventTriggered/Rejected. Definitions support elapsed seconds, normalized progress, absolute meters and custom signals, stable threshold/priority/ID order, one-shot/cooldown/crossing policies, selectors and scalar payload. Snapshot includes elapsed/progress/watermark/fired/cooldown state and fixed preset. Explicit Start after Ready excludes all planning time; native tick uses game time and the live RoutePawn. Clear/regenerate stops/resets; EndPlay also clears subscriptions. Recursive dispatch is suppressed; callback reset cancels pending events. Details and Blueprint receiver authoring contract are in API.md.

Set/Add/Scale border actions use a single candidate group and M5 validation from the actual current Pawn position/heading/envelope. Native descriptors/proof commit before registered sphere components, followed by OnBorderChanged for BP visuals. Failed proposals preserve the entire previous state and consume one-shot attempts. Proxy registration is explicit (`RegisterBorderProxy`), same-world and matching/unscaled; tagged selection uses Actor tags. No automatic spawning/ownership until M8. Instant changes only; no animation API. Optional encounter/wind actions are BP notifications, not native spawn implementations. Blueprint receivers remain responsible for runtime collision safety of their spawns.

No lighting transition action. Registration rejects explicit lighting-swap requests, recognized transition commands and unknown enums, including disabled definitions. Plan lighting remains unchanged. No environment/Pawn/asset edits, PIE, or profiling.

Files: BalloonLevelEventManager.h/.cpp (new), BalloonLevelGenerator.h/.cpp, BalloonBorderThreat.h/.cpp; Tests/BalloonLevelM6Test.cpp and BalloonEventTestObserver.h (new); Scripts/Verify-BalloonLevelM6.ps1 (new); API.md, DESIGN_DECISIONS.md, TEST_REPORT.md, this handoff and docs/README.md.

Verification: clean isolated UE 5.9 compilation passed, then final source refinements compiled and all 7 M1–M6 tests passed at 10:56:04 Europe/Kyiv. JSON: succeeded=7, failed=0, notRun=0, inProcess=0. All 28 feature/test/flight-component copies match by SHA256; whitespace check passed. Commands and limits are recorded in TEST_REPORT.md. Full-project build reproduces the existing FirstPersonCameraPawn.h:335 UHT merge-conflict error; untouched. Logs: Saved/Logs/M6-ProjectBuild.log, M6-IsolatedBuild.log, M6-Automation.log. Do not claim full-project/PIE integration. No commit; stop after M6.

Next authorized milestone would be M7 fixed environment/biome atmosphere after resolving the unrelated build conflict and preparing authored preset assets. Do not start M7 in this task. Keep horizontal fixed-Z route safety, immutable Day/Night and plan → validate → realize boundaries.

## Historical M5 handoff

The following M5/M4 text describes prior milestone boundaries; M6 event/proxy contracts above supersede statements that M6 is pending.

## M5 horizontal route safety

`BalloonHorizontalRouteValidator.h/.cpp` implements a progress/lateral 2D lattice and deterministic A* at one absolute world Z. Every node/edge is checked with a full 3D enclosing box, border-sphere flight-band projection, and conservative colliding-placement AABBs. Static-mesh bounds include render bounds and simple physics geometry. NoCollision HISM scenery is excluded. Missing collision bounds fail closed; Blueprint gameplay rows currently need an external certified bounds resolver before they can pass generation. Bounds must contain all physics shapes; visual size metadata is never treated as collision proof.

Select a live balloon actor in `Generator.RoutePawn`. `InspectPawn` reads every query-enabled UShapeComponent and the actual BalloonFlightComponent authored limits, without changing movement. It encloses all shapes and all yaw orientations, including their offsets from the actor datum; hollow interiors are filled conservatively. Safety and wind inflate horizontal collision; safety also inflates height. No altitude state/neighbor exists. Tilted Pawns and nonzero initial yaw rate fail conservatively until a matching transition contract exists. For runtime validation, call InspectPawn on the current instance and pass its heading/rate plus exact current XYZ to Validate/TryBorders. A mismatched Z is rejected rather than snapped.

Actual asset inspection through a transient editor instance of `/Game/Assets/Balloon/BP_Balloon.BP_Balloon_C`: 20 active shapes (four floors, eight walls, stove, console, six envelope spheres), all BlockAllDynamic, scale 1. Conservative radius from shape AABB corners approximately 480.833cm; symmetric half-height about actor datum 1140cm. Movement: MinSpeed=50, MaxSpeed=300, InitialSpeed=100 cm/s; MaxTurnRate=30 degrees/s; turn acceleration/deceleration=60 degrees/s^2; speed acceleration/deceleration=40 cm/s^2. The loaded asset has no Rim_* shapes and differs from the latest collision setup utility; M5 does not rewrite it. Evidence: Saved/M5PawnAudit.json and Saved/M5PawnAudit.py. The temporary actor was destroyed; no asset/map was saved.

Candidate turns use C2 quintic Bezier corners. Recursive Bernstein derivative hulls bound continuous curvature and angular acceleration, including acceleration/deceleration of forward speed. Convex-hull deviation inflates each engine sweep so the complete curve is covered between proof points. Straight initial tangent is required; A* reserves the first edge. Minimum turning radius is the larger of config radius and inspected speed/yaw-rate ratio. Sampled proof points remain at fixed Z; they are a feasibility/debug result, never an autopilot.

Width uses the global rich-curve minimum, clamped by MinWidthCm, and a Lipschitz distance guard between checks. This deliberately rejects some locally wide detours. The square sweep encloses a yaw-invariant circular envelope, and corridor/threat clearance includes its diagonal. World sweeps conservatively treat all query-enabled object types as blockers, ignoring only the balloon and its attached riders. Ground/terrain with query collision is checked at full height. These policies may reject valid routes, but do not accept a vertical bypass.

Generator Ready now requires proof; failure clears the old plan/spline. Initial generation resolves rows once, then retries placement with stable per-attempt seed salt, preserving exact requested gameplay counts and geometry/border streams. No density relaxation. Config RouteAttempts is [1,12]; lattice limit 100000 states per attempt, combined occupancy/curve operations 500000 per validation, curve subdivision depth 10. Public pure validation may retry finer lattices; generator uses one lattice per placement attempt. Diagnostics include seed, attempt, cells visited and failure reason. `GetPlan().RoutePoints`, `PlacementAttempt`, and the green path from DrawBorderDebug expose the proof. TryBorders commits descriptors/proof only after validating from the current XY; failed proposals leave both unchanged. M6 still owns events and world-proxy transactions; M8 owns realization.

Final verification: clean isolated UE 5.9 build PASSED; M1-M5 automation PASSED (5 tests, zero failures), 2026-11-15 10:44:52 Europe/Kyiv. All 24 source/test/flight-component copies match by SHA256. Verification evidence and commands are recorded in TEST_REPORT.md. Full-project integration remains blocked by the pre-existing FirstPersonCameraPawn.h merge markers at UHT line 335. No Pawn edits, event system, environment work, realization, PIE playtest, or profiling. Stop after M5.

## Historical M4 handoff

The following M1-M4 details describe the prior milestone; their references to absent route safety are superseded by M5 above.

## Build and scope

Actual module/export: gunsandballoons / GUNSANDBALLOONS_API. Engine: UE 5.9.0 at G:/UE/UE_5.9. User requested specification v1.1 sections 4, 7, 11.4 and M4 only. No Pawn edits, native biome DataTable assets, actor/HISM realization, environment changes or route proof. Ready means descriptors only. No runtime radius updates until later safety transactions.

## M4 files and contracts

- BalloonLevelTypes.h keeps exactly twelve reflected asset-row columns. Adds FBalloonPlacementMetadata (config annotations, never row columns) and plain placement descriptors containing biome/row/rule/index/segment identity, soft asset path, transform, cluster/birds data, Actor/HISM kind, collision, enemy flag and cull annotation.
- BalloonLevelConfig.h adds independent SceneryEnabled/ObstaclesEnabled/EnemiesEnabled, exact category counts (-1 density, 0 none), row metadata, prototype cluster/street/ground settings requiring tuning. Borders remain independent. InteriorEnemy requires Hazard Blueprint rows; no category or Notes heuristics.
- BalloonPlacementPlanner.h/.cpp: Resolve validates and measures assets on the game thread. Build never loads or spawns UObjects. Optional ground sampler and supplied field-border polylines; generator uses flat ground and sampled corridor edges. Failure leaves no partial placement list. Per-row errors name source table, row, rule, target, planned and region. Bounded packing/geometry failures are explicit.
- Generator merges the existing shared/biome catalog, builds M3 borders, resolves rows and plans placements before committing the candidate. Read GetPlan().Placements in C++ and GetDiagnostics for preview results. Regeneration clears stale plans.
- Source/gunsandballoons/Tests/BalloonLevelM4Test.cpp covers all six rules; Scripts/Verify-BalloonLevelM4.ps1 copies all feature sources/tests into Saved/M4Verification, compiles and runs M1–M4. The script inspects JSON test results because Unreal can exit 0 despite failed assertions.

## Rule behavior

See [catalog README](../../../Content/BalloonLevel/Rules/README.md) for the full executable authoring contract. scatter uses density per canonical sampled corridor km², clusters and spatial-hash spacing. Forest ring uses the fixed 1.5–3km central annulus, inward local X, overlapping local Y silhouettes and conservative exclusion from corridor bounds; uncovered views fail without enlarging the ring. Field line advances mesh-length chords along supplied/default borders with connected endpoints and a full-scale overlapping final remainder; insufficient/excess count budgets fail. City street uses a world-axis grid, paired rear-touching lots and separately annotated lamps; yaw is cardinal, buildings remain upright. Desert wind_aligned uses one level yaw ±10°. Field flock divides one 3–6 Actor budget across active rows, with ClusterSize passed as birds metadata.

Generic scatter supports extensible biome IDs; specialized rules skip other biomes. Uniform scale and sampled ground alignment preserve horizontal heading; slope never changes balloon flight Z. Exact gameplay counts replace total category density using relative weights and largest remainders, after reserving gameplay flock targets. Conflicting flock/category budgets fail. Disabled or exact-zero categories produce no gameplay descriptors.

Explicit FNV-1a UTF-8 seed hashing includes catalog version/biome/row/rule/item. Geometry and borders keep their independent M2/M3 streams. Spatial reservations can force later rows to retry when earlier placement changes; no global random stream is consumed. Limits: 10000 descriptors, 256 attempts/item, 500000 planning/reservation operations, 100000 grid cells. These are preview safeguards, not measured performance budgets.

Scenery meshes produce HISM descriptors; gameplay meshes produce MeshActor descriptors; Blueprint classes produce BlueprintActor descriptors, including the validated flock exception. Scenery collision is off; gameplay collision follows the validated row. Cull end annotations: clutter 15000, short trees 30000, decorative buildings/dunes 80000, mountains/gameplay Actors 0 (no distance cull). Conflicting same-mesh HISM profiles fail; no actual components or engine cull behavior are claimed before M8.

## Existing geometry and next milestone

M4 re-verification: the requested implementation was already present in the checkout; review required no source changes. A fresh clean harness build and all four automation tests passed on 2026-11-15 at 10:22:57 Europe/Kyiv (08:22:57 UTC). All 19 source/test copies match by SHA256. Current report: Saved/M4Verification/TestReport/index.json; logs: Saved/Logs/M4-IsolatedBuild.log and M4-Automation.log. Full-project build again failed at FirstPersonCameraPawn.h:335; log: Saved/Logs/M4-RecheckProjectBuild.log. See TEST_REPORT.md for commands and limits. Stop before M5.

M2 supplies canonical horizontal polyline, arc segments and snapshotted width profile. M3 supplies stationary sphere influences, fixed-envelope-Z projection, clipped union of unsafe intervals and debug/proxy APIs. See API.md and prior TEST_REPORT entries. Actual balloon envelope defaults still require tuning; no accepted route or playable-level guarantee exists.

M5 must validate the actual multi-primitive balloon with 3D sweeps along a 2D route at fixed Z, safe start/finish reservations, turning/inertia and retries. M4 placements carry no collision proof. Bind real terrain sampling and authored Blueprint bounds/catalog metadata before visual validation; BP_BirdFlock positive asset integration is not exercised by the isolated content-free harness. M8 owns world realization, one HISM per mesh, ownership/cleanup and actual culling. No PIE visual inspection or profiling claimed. Unrelated .claude/ and docs/temp/ remain untouched. No commit. Stop after M4.
