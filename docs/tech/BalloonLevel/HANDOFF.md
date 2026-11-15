# Balloon level generation handoff

Updated: 2026-11-15. Current milestone: M4. Stop before M5. M1–M4 implementation is present. Final verification PASSED: UE build and M1–M4 automation (4 tests, 0 failures), with all 19 feature/test copies byte-identical. Commands, logs and limitations are recorded in TEST_REPORT.md. Full-project integration remains blocked by existing FirstPersonCameraPawn.h merge markers (UHT line 335), reproduced during M4.

## Build and scope

Actual module/export: gunsandballoons / GUNSANDBALLOONS_API. Engine: UE 5.9.0 at G:/UE/UE_5.9. User requested specification v1.1 sections 4, 7, 11.4 and M4 only. No Pawn edits, native biome DataTable assets, actor/HISM realization, environment changes or route proof. Ready means descriptors only. No runtime radius updates until later safety transactions.

## M4 files and contracts

- BalloonLevelTypes.h keeps exactly twelve reflected asset-row columns. Adds FBalloonPlacementMetadata (config annotations, never row columns) and plain placement descriptors containing biome/row/rule/index/segment identity, soft asset path, transform, cluster/birds data, Actor/HISM kind, collision, enemy flag and cull annotation.
- BalloonLevelConfig.h adds independent SceneryEnabled/ObstaclesEnabled/EnemiesEnabled, exact category counts (-1 density, 0 none), row metadata, prototype cluster/street/ground settings requiring tuning. Borders remain independent. InteriorEnemy requires Hazard Blueprint rows; no category or Notes heuristics.
- BalloonPlacementPlanner.h/.cpp: Resolve validates and measures assets on the game thread. Build never loads or spawns UObjects. Optional ground sampler and supplied field-border polylines; generator uses flat ground and sampled corridor edges. Failure leaves no partial placement list. Per-row errors name source table, row, rule, target, planned and region. Bounded packing/geometry failures are explicit.
- Generator merges the existing shared/biome catalog, builds M3 borders, resolves rows and plans placements before committing the candidate. Read GetPlan().Placements in C++ and GetDiagnostics for preview results. Regeneration clears stale plans.
- Tests/BalloonLevelM4Test.cpp covers all six rules; Scripts/Verify-BalloonLevelM4.ps1 copies all feature sources/tests into Saved/M4Verification, compiles and runs M1–M4. The script inspects JSON test results because Unreal can exit 0 despite failed assertions.

## Rule behavior

See [catalog README](../../../Content/BalloonLevel/Rules/README.md) for the full executable authoring contract. scatter uses density per canonical sampled corridor km², clusters and spatial-hash spacing. Forest ring uses the fixed 1.5–3km central annulus, inward local X, overlapping local Y silhouettes and conservative exclusion from corridor bounds; uncovered views fail without enlarging the ring. Field line advances mesh-length chords along supplied/default borders with connected endpoints and a full-scale overlapping final remainder; insufficient/excess count budgets fail. City street uses a world-axis grid, paired rear-touching lots and separately annotated lamps; yaw is cardinal, buildings remain upright. Desert wind_aligned uses one level yaw ±10°. Field flock divides one 3–6 Actor budget across active rows, with ClusterSize passed as birds metadata.

Generic scatter supports extensible biome IDs; specialized rules skip other biomes. Uniform scale and sampled ground alignment preserve horizontal heading; slope never changes balloon flight Z. Exact gameplay counts replace total category density using relative weights and largest remainders, after reserving gameplay flock targets. Conflicting flock/category budgets fail. Disabled or exact-zero categories produce no gameplay descriptors.

Explicit FNV-1a UTF-8 seed hashing includes catalog version/biome/row/rule/item. Geometry and borders keep their independent M2/M3 streams. Spatial reservations can force later rows to retry when earlier placement changes; no global random stream is consumed. Limits: 10000 descriptors, 256 attempts/item, 500000 planning/reservation operations, 100000 grid cells. These are preview safeguards, not measured performance budgets.

Scenery meshes produce HISM descriptors; gameplay meshes produce MeshActor descriptors; Blueprint classes produce BlueprintActor descriptors, including the validated flock exception. Scenery collision is off; gameplay collision follows the validated row. Cull end annotations: clutter 15000, short trees 30000, decorative buildings/dunes 80000, mountains/gameplay Actors 0 (no distance cull). Conflicting same-mesh HISM profiles fail; no actual components or engine cull behavior are claimed before M8.

## Existing geometry and next milestone

M2 supplies canonical horizontal polyline, arc segments and snapshotted width profile. M3 supplies stationary sphere influences, fixed-envelope-Z projection, clipped union of unsafe intervals and debug/proxy APIs. See API.md and prior TEST_REPORT entries. Actual balloon envelope defaults still require tuning; no accepted route or playable-level guarantee exists.

M5 must validate the actual multi-primitive balloon with 3D sweeps along a 2D route at fixed Z, safe start/finish reservations, turning/inertia and retries. M4 placements carry no collision proof. Bind real terrain sampling and authored Blueprint bounds/catalog metadata before visual validation; BP_BirdFlock positive asset integration is not exercised by the isolated content-free harness. M8 owns world realization, one HISM per mesh, ownership/cleanup and actual culling. No PIE visual inspection or profiling claimed. Unrelated .claude/ and docs/temp/ remain untouched. No commit. Stop after M4.
