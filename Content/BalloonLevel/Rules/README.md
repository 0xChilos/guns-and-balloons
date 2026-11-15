# Balloon level asset tables (native M1 contract)

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

M1 validates the contract only. Placement execution begins in M4.

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
Success is `Ready`, meaning a catalog and straight logical segment plan, not a playable validated level.
StartTransform supplies world XY and yaw; flight Z is the absolute FlightHeightMeters datum.
Generator placement does not add an offset to config coordinates. Distances convert to centimetres once.
Segments use [StartS, EndS), with the final endpoint included in the last segment.
ClearGeneratedLevel/RegenerateLevel affect only the generator's spline/plan.
Length, flight height and target segment length defaults are prototype authoring values requiring tuning.
