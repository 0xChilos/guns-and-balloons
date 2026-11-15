# Random level generation: rules for DT_<Level>_Placement

Data: `/Game/Assets/Locations/<Level>/DT_<Level>_Placement` (row struct `/Game/Assets/Locations/Blueprints/S_LevelAsset`).
Imported from `<Level>_Placement.csv`; the first CSV column is the row name.

## S_LevelAsset columns
| Column | Meaning |
|---|---|
| Asset | Soft object path. A StaticMesh (`/Game/Assets/Locations/<Level>/<Mesh>`), or a Blueprint class (`..._C`) for the rows below |
| Category | Filler, Terrain, Background, Obstacle, Hazard, Creature |
| Collision | Whether the placed thing collides with the balloon |
| DensityPerKm2 | Instances per km² (scatter rules) |
| ScaleMin / ScaleMax | Uniform random scale range |
| MinSpacingM | Minimum distance between instances, in metres |
| RandomYaw | Random yaw (0-360) |
| AlignToSlope | Align the up axis to the ground normal |
| ClusterSize | Instances per cluster |
| Rule | Placement rule (below) |
| Notes | Free text from the CSV |

## Rows that spawn Blueprints instead of meshes
| Row | Asset |
|---|---|
| SM_Factory_A / SM_Factory_B | BP_Factory_A / BP_Factory_B (smoke from every `smoke*` socket) |
| SK_Windmill | BP_Windmill |
| SM_JammerTower | BP_JammerTower (includes the dish; skip the SM_JammerDish row) |
| SM_TurretTower | BP_TurretTower (includes head, guns, engine; skip the SM_TurretTower_Head/_Guns/_Engine rows) |
| SK_Lighthouse | BP_Lighthouse |
| SK_Bird | BP_BirdFlock |

These rows stay meshes but must be spawned as **Static Mesh actors with the AC_LootSpots component added**:
SM_Pine_Tall_A/B, SM_RangerTower, SM_Church, SM_ClockTower, SM_Tower_Office_A/B, SM_Mesa, SM_Hoodoo.

## Rules
- `scatter`: random positions at DensityPerKm2, at least MinSpacingM apart, in clusters of ClusterSize.
- `ring`: Forest mountains. Only outside the playable area, in a ring 1.5–3 km from the centre, facing inward, scaled ScaleMin–ScaleMax, overlapping so the horizon is closed.
- `line`: fences and hedges, chained end to end along field borders (SM_Fence = 4 m section, SM_Hedge = 6 m).
- `street`: city blocks. Snap to a street grid, yaw in 90° steps, buildings back to back, lampposts along the streets.
- `wind_aligned`: dunes. All share one yaw ±10° per level. Downwind is Blender +Y = **Unreal −Y** after import (same conversion as SM_LighthouseBeam, whose cone runs along −Y).
- `flock`: spawn BP_BirdFlock, 3–6 flocks per Field level (each flock spawns 8–24 birds; flock origin = ground point).

## Categories
- Filler, Terrain, Background: Hierarchical Instanced Static Mesh components, one per mesh, NoCollision.
- Obstacle, Hazard: separate actors with collision.

## Cull distances (HISM end cull distance)
- Small clutter: 15000 cm
- Short trees: 30000 cm
- Buildings and dunes: 80000 cm
- Mountains and obstacles: never cull (0)

## Loot
- AC_LootSpots spawns loot on every socket starting with `loot_` (SpawnChance 0.6 by default).
- Loot table = `LootClasses` + `LootWeights` (same index). Defaults: Coal 30, AmmoBox 25, Log 20, MoneyPouch 18, Crystal 7.
  Per level, change the Crystal weight on the component (for example Desert 12, City 4).
- Set `Seed` on every AC_LootSpots from the generator's seed before BeginPlay; the stream also mixes in the structure's position, so a seed always gives the same loot.
- For actors you add AC_LootSpots to at runtime, set its variables first, then register the component.

## Ground and atmosphere per level
- Ground: a large plane (or the generator's landscape) with `MI_<Level>_Ground` (/Game/Assets/Locations/Materials).
- Exponential Height Fog:

| Level | Fog colour | Density |
|---|---|---|
| Forest | (0.35, 0.45, 0.45) | 0.03 |
| Field | (0.75, 0.70, 0.55) | 0.015 |
| City | (0.40, 0.35, 0.28) | 0.05 |
| Desert | (0.85, 0.75, 0.60) | 0.02 |
