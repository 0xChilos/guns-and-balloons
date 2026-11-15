# Balloon level asset tables and M4 planners

Create DataTables using `FBalloonLevelAssetRow` (native `BalloonLevelAssetRow` in the editor):
`DT_BalloonAssets_Shared` and `DT_BalloonAssets_Forest/Field/City/Desert`.
Assign shared and exactly one selected biome table to a `BalloonLevelConfig` DataAsset.
These tables are separate from the legacy `S_LevelAsset` placement tables; M1 does not migrate those assets.
No native-schema .uasset tables have been authored yet.

CSV first column is the stable row name. Exactly twelve data columns follow:

`Asset,Category,Collision,DensityPerKm2,ScaleMin,ScaleMax,MinSpacingM,RandomYaw,AlignToSlope,ClusterSize,Rule,Notes`

- Asset: soft StaticMesh object path, e.g. `/Engine/BasicShapes/Cube.Cube`, or Blueprint-generated Actor class object path ending in `_C`. Copy the actual class reference from the editor; a Blueprint asset path is invalid. Assets resolve synchronously on the game thread during validation.
- Category: `Filler`, `Terrain`, `Background`, `Obstacle`, `Hazard`. Scenery requires meshes and Collision=false; gameplay categories require Collision=true and accept meshes or spawnable Blueprint Actor classes. Only `flock` permits scenery Actors and optional noncolliding gameplay Actors. `Creature` is not part of this schema.
- DensityPerKm2: finite nonnegative items per eligible horizontal km²; 1 km² = 1,000,000 m².
- ScaleMin/ScaleMax: finite positive uniform scale range, max >= min.
- MinSpacingM: finite nonnegative horizontal spacing in metres. ClusterSize: integer >= 1.
- RandomYaw and AlignToSlope: authored rotation flags; slope affects asset rotation only, never flight height. Rule orientation takes precedence over random yaw.
- Rule: one of the six names below. Notes: free-form documentation, not executable metadata.

Shared rows are collected first; the selected biome replaces the whole row with the same name. Every authored row is validated, including overridden rows. Results use lexical row-name ordering, with no duplicate shared instances. Errors name both source table and row. Null tables are allowed for geometry-only previews; a wrong row struct or invalid row fails the entire catalog. Names follow Unreal FName case-insensitive identity.

## Six placement rules

M4 implements deterministic descriptor planning; realization remains M8.

| Rule | Required behavior |
| --- | --- |
| `scatter` | Seeded random placement from density per horizontal km²; respect minimum spacing and cluster grouping. |
| `ring` | Background mountain meshes in a 1.5–3 km annulus about level center, outside playable space, facing inward. Overlap allowed; never distance-cull. |
| `line` | Chain fences/hedges along field borders with bounded gaps. |
| `street` | City street grid; 90° yaw increments, back-to-back buildings and lampposts along street edges. |
| `wind_aligned` | Noncolliding dune scenery uses one seeded per-level prevailing yaw plus individual deviation within ±10°. |
| `flock` | Field creates 3–6 BP_BirdFlock Actor groups, not HISM. Asset must resolve to that Blueprint-generated Actor class or subclass. Collision defaults off unless deliberately authored as gameplay hazard. |

Advanced spacing/grid/cull/count metadata belongs in separate config, never extra row columns.
Future realization: one HISM per mesh for scenery, NoCollision; separate colliding Actors for Obstacle/Hazard. Flock is the Actor exception. Clutter culls at 15000cm, short trees at 30000cm, decorative buildings/dunes at 80000cm; mountains and gameplay obstacles never distance-cull.

## M1 preview

Create a BalloonLevelConfig DataAsset, place a BalloonLevelGenerator and assign LevelConfig.
Invoke GenerateLevel in Details or Blueprint. Read GetDiagnostics and GetGenerationStatus.
Success is `Ready`, meaning geometry, border and placement descriptors. Horizontal reachability remains unvalidated until M5.
StartTransform supplies world XY and yaw; flight Z is the absolute FlightHeightMeters datum.
Generator placement does not add an offset to config coordinates. Distances convert to centimetres once.
Segments use [StartS, EndS), with the final endpoint included in the last segment.
ClearGeneratedLevel/RegenerateLevel affect only the generator's spline/plan.
Length, flight height and target segment length defaults are prototype authoring values requiring tuning.

## M4 planning contract (2026-11-15)

`BalloonPlacementPlanning::Resolve` validates assets on the game thread, measures mesh bounds, and copies technical annotations from `LevelConfig.PlacementMetadata` by stable row name. Blueprint Actor rows need positive authored `SizeMeters`; zero mesh dimensions use measured bounds. `Build` consumes resolved rows and M2 geometry without UObject loads or world mutations. It accepts an optional ground sampler (world Z cm, upward normal), plus optional field-border polylines (world cm). Without these inputs, ground is the configured flat `PlacementGroundZMeters` and field borders are the two sampled corridor edges. The generator currently uses these preview fallbacks; terrain tracing and realization remain later work.

The twelve row columns above are unchanged. Technical metadata supplies `InteriorEnemy`, `StreetLamp`, unscaled XYZ `SizeMeters` and `CullEndCm`. Enemy rows must be Hazard Blueprint Actors; other gameplay rows belong to the obstacle budget. Notes are never parsed. New biome IDs work with generic scatter; specialized rules activate only in their named biome.

- `scatter`: target `round(eligible horizontal km² * DensityPerKm2)`. Corridor area uses trapezoidal integration of canonical M2 width samples; a constant-width 1500×600m corridor is 0.9km². Positions use width rejection sampling, exact snapshotted width eligibility, seeded cluster anchors, uniform-disc offsets within `ClusterRadiusMeters`, and horizontal spacing reservations. Last cluster may be partial.
- `ring`: Forest Background meshes only. Density uses the 1.5–3km annulus area. A seeded common radius and equal angular stations make a closed ring about the arc midpoint. Local X faces inward; local Y is silhouette width. The angular-coverage requirement 2*radius*tan(pi/count) must be less than minimum-scaled silhouette width, otherwise fail with uncovered-horizon diagnostics. Conservative mesh bounding circles must remain outside M2 corridor bounds. Ring deliberately ignores spacing, never collides and never culls; it never expands the annulus to fit a large level.
- `line`: Field chains use measured/authored local X mesh length at each seeded uniform scale. Endpoints advance along supplied polylines by the first forward circle intersection, giving connected straight mesh segments across sampled bends. Final short remainder overlaps a full-size segment without changing the scale range. The density/count budget must cover every supplied border exactly; insufficient or excess budgets fail explicitly. Default borders are both corridor edges; a supplied field region still uses the corridor area as its density budget. Mesh origins must be centered; extreme bends may need smaller authored segments.
- `street`: City grid aligns to world XY, anchored at world zero. `StreetCellMeters`, `StreetRoadMeters`, `StreetLotDepthMeters` and `StreetLampIntervalMeters` configure it. Buildings use paired lots at block midlines, local X facing the street; actual scaled mesh rear faces meet back-to-back. Oversized footprints fail. Lots are reserved across catalog rows, so building footprints cannot reuse a grid lot. Pairs are shuffled together; an odd target can leave one final unpaired lot. `StreetLamp` rows use street edges at interval stations instead. Yaw is 0/90/180/270 degrees; `RandomYaw` and slope tilt do not override this rule. Density chooses the number of eligible grid lots/edge stations.
- `wind_aligned`: Desert density/spacing/clusters, one seeded level yaw and per-item jitter within ±10°, regardless of RandomYaw.
- `flock`: Field shares one seeded 3–6 Actor budget among all active flock rows in lexical order, overriding density. Other biomes skip it. ClusterSize is passed as `BirdsPerFlock` metadata, never an Actor multiplier. Resolve requires BP_BirdFlock or a subclass. Decorative flocks use the scenery switch. Gameplay flock rows count against their category override; conflicting positive budgets fail rather than violate the flock target. Exact zero or disabled gameplay categories produce none.

All rules sample uniform ScaleMin–ScaleMax. Scatter/flock use RandomYaw; directional rules override it. Ground Z is independent of balloon flight Z. AlignToSlope follows the sampled normal while preserving horizontal heading; street buildings remain upright. Sampling failure or invalid normals fail the plan.

SceneryEnabled, ObstaclesEnabled and EnemiesEnabled are independent of BordersEnabled. ObstacleExactCount/EnemyExactCount=-1 uses row density; zero means none; positive values replace the total category target. After reserving any gameplay flock budget, counts use relative positive row densities, largest remainders and lexical ties. Missing eligible weights fail. Decorative flock Actors are not enemies by default.

Descriptors contain biome/row/rule/index/segment IDs, asset path, transform, cluster and flock data, collision, representation and cull end. Scenery meshes produce HISM descriptors, mesh gameplay rows produce MeshActor descriptors, Blueprint classes produce BlueprintActor descriptors. No HISM is created in M4. Cull annotations are 15000 (clutter), 30000 (short trees), 80000 (decorative buildings/dunes), or 0 (no distance cull for mountains/gameplay Actors). Same-mesh HISM profile conflicts fail, preserving one future component per mesh. Actual engine culling requires M8 verification.

Random seeds use explicit lowercase UTF-8 FNV-1a with seed, catalog version, biome, Placement/v1, row, rule and item index. Wind/flock budgets have independent streams. Rows are sorted lexically. Spacing reservations use a spatial hash and the larger spacing of two rows; adding a spatial conflict can change later rows through rejection without consuming their streams. Ring is exempt. Failure clears all placements. Limits: 10000 descriptors, 256 attempts per item, 500000 planning/reservation operations and 100000 street grid cells. Diagnostics identify table, row, rule, target/planned count and region for row failures. These are preview safeguards, not profiled gameplay budgets. Cluster/street/ground settings remain editable prototype tuning values.

Verification: `powershell -NoProfile -ExecutionPolicy Bypass -File Scripts/Verify-BalloonLevelM4.ps1`. See [handoff](../../../docs/tech/BalloonLevel/HANDOFF.md). Native-schema biome DataTables and visual PIE results are not supplied by this milestone.

Re-verification on 2026-11-15 passed a clean UE 5.9 harness build and all four
M1–M4 automation tests, including the six-rule and row-error checks. Current report:
`Saved/M4Verification/TestReport/index.json`. Full-project compilation remains
blocked by existing Pawn header merge markers; M5 has not started.
