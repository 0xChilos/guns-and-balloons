# Balloon Level Asset DataTables — Authoring Rules

**Recommended location:** `Content/BalloonLevel/Rules/README.md` (or your project's source-controlled documentation folder beside imported DataTable source files).  
**Compatible specification:** `Balloon_Level_Generation_LLM_Implementation_Plan.md` v1.1.  
**Goal:** Make shared and biome-specific asset placement reproducible, understandable, and editable without changing generator code.

## 1. Tables and biome merge

- `DT_BalloonAssets_Shared` — assets eligible for every biome.
- `DT_BalloonAssets_Forest` — Forest-specific trees, background mountains, threats and props.
- `DT_BalloonAssets_Field` — Field vegetation, border fences/hedges and flocks.
- `DT_BalloonAssets_City` — street buildings, obstacles, lampposts, hazards.
- `DT_BalloonAssets_Desert` — dunes, desert props and hazards.

At level generation, select the Shared table and **one** biome table. Merge by stable DataTable row name; a biome row of the same name **replaces the entire shared row**. Sort rows deterministically by row name before making seeded placement decisions. Assets can be shared or unique to a biome. A new biome requires its own table and config, not generator source modifications.

## 2. DataTable columns (exact required names)

| Field | Value type | Authoring contract |
|---|---|---|
| `Asset` | Soft object path | Soft reference to a `UStaticMesh` **or** soft reference to the generated `UClass` of an Actor Blueprint. Resolve and type-check before spawning. |
| `Category` | `Name` | One of `Filler`, `Terrain`, `Background`, `Obstacle`, `Hazard`. |
| `Collision` | `bool` | `false` for decorative HISM categories; `true` for blocking/damaging actor categories, subject to the noncolliding decorative flock exception. |
| `DensityPerKm2` | `float` | Expected instances per square kilometer of eligible horizontal area. Do not apply to the entire world plane unless it is the specified eligible area. |
| `ScaleMin` | `float` | Minimum uniform instance scale, strictly positive. |
| `ScaleMax` | `float` | Maximum uniform instance scale, at least `ScaleMin`. |
| `MinSpacingM` | `float` | Minimum allowed horizontal center/footprint separation in meters. Nonnegative. |
| `RandomYaw` | `bool` | May randomize yaw unless rule-specific yaw takes precedence. |
| `AlignToSlope` | `bool` | Align ground-placed mesh orientation to sampled terrain surface normal where appropriate. Does **not** create vertical balloon maneuvers. |
| `ClusterSize` | `int` | Instances per scatter cluster, at least 1. For `flock` can mean birds-per-flock if the Actor supports it; does not multiply flock Actor count. |
| `Rule` | `Name` | `scatter`, `ring`, `line`, `street`, `wind_aligned`, `flock`. |
| `Notes` | `String` | Human-readable asset/rule authoring comments. Not executable logic. |

**Validation:** reject bad soft paths, unsupported asset types/categories/rules, invalid scales/density/spacing/cluster size, a Blueprint actor class entered as a Static Mesh, or collision/category mismatches. Log the table name and row name for every invalid entry. A Blueprint class reference must point to its generated Actor class (`_C` path), not merely the `.uasset` Blueprint object.

## 3. Placement rule behavior

### `scatter` — generic density and clusters

Generate random positions inside the eligible biome/level region at `DensityPerKm2`:

```text
TargetCount = round(EligibleAreaM2 / 1,000,000 * DensityPerKm2)
```

Scatter items in clusters of `ClusterSize` (`1` = unclustered). Every individual instance must be at least `MinSpacingM` apart from other relevant instances; use deterministic seeded cluster anchors and bounded placement retries. Report a placement failure if requested counts violate the available area/spacing constraint. Apply randomized scale uniformly in `[ScaleMin, ScaleMax]`. `RandomYaw` allows full yaw randomization when the selected rule does not override it.

### `ring` — Forest horizon mountains

Only eligible for **Forest mountain Background assets**. Place mesh instances **outside the playable corridor** in a ring/annulus **1.5–3 km from the level's horizontal centre** (1500–3000 meters radius), and orient the fronts inward toward the playable space. Use scale `[ScaleMin, ScaleMax]`. Intentional overlapping projected mountain silhouettes should close the horizon with no clear gaps. Overlap is allowed for this backdrop even if generic `MinSpacingM` would normally forbid it; document any special overlap policy in row `Notes` or a separate metadata asset. Use HISM, **NoCollision**, **never distance-cull**. Use Forest fog to hide the mountain bases. If the authored region is too large for the 1.5–3 km annulus to enclose correctly, emit an explicit horizon-coverage warning instead of secretly changing the ring distances.

### `line` — fences and hedges

For Field borders. Use field-border polylines as the anchor source. Chain fence or hedge segments **end to end**, aligning their long axis to each border tangent and advancing by the usable mesh length, avoiding gaps and excessive overlap. Resolve short final segment remnants deterministically (small stretch if allowable, or a warning/termination rule). Treat nonblocking decorative fence segments as HISM NoCollision; fence/hedge segments that are actually blocking must be `Obstacle` Actors with collision.

### `street` — City block grid

Snap building lots and street objects to a shared configurable street grid. Yaw is quantized to **0°, 90°, 180°, 270°**; `RandomYaw` does not permit intermediate angles. Place buildings **back-to-back** on corresponding lots using deterministic dimensions/spacing. Place lampposts **along street edges**, not randomly inside buildings/streets. A separate City grid config should define block/lot widths, road width, edge offset, and lamp spacing; these do not need to be added to the 12-field DataTable. `DensityPerKm2` controls desired population over eligible grid lots. Buildings that physically block the balloon use `Obstacle` actors with collision and no culling; decorative buildings can use HISM with 80,000 cm end cull and NoCollision.

### `wind_aligned` — Desert dunes

Choose **one prevailing yaw per generated level**, using a stable stream seeded for the biome/level. Each dune takes the same yaw plus an independent seeded jitter in **[-10°, +10°]**. This overrides `RandomYaw`. All dune instances obey `[ScaleMin, ScaleMax]`, eligible terrain, slope alignment if requested, and density/spacing, but they must never randomly face unrelated directions. Decorative dunes belong in NoCollision HISM and cull at **80,000 cm**.

### `flock` — Field birds

Generate **3–6 distinct `BP_BirdFlock` Actor instances per Field level**, using a stable level seed and legal spatial positions. `DensityPerKm2` does not control the number of flock Actors for this rule. The Blueprint may use `ClusterSize` for birds within a flock when supported; otherwise document the field as unused for this row. Flock requires a Blueprint Actor class, not a static mesh. It is an explicit **Actor exception** to decorative HISM instancing. By default, decorative flocks do not physically collide with the balloon and do not count toward enemy totals. If deliberately made damaging, configure them as gameplay `Hazard` Actors and include their footprints in route validation.

## 4. Category routing and collisions

| `Category` | Runtime representation | Collision |
|---|---|---|
| `Filler` | HISM, one component per distinct `UStaticMesh` | `NoCollision` |
| `Terrain` | HISM, one component per distinct `UStaticMesh` | `NoCollision` |
| `Background` | HISM, one component per distinct `UStaticMesh` | `NoCollision` |
| `Obstacle` | Separate Actor (`AStaticMeshActor` wrapper or Blueprint) | Enabled |
| `Hazard` | Separate Actor with a collision/overlap profile | Enabled |

`flock` is the actor exception described above. Actual ground plane/landscape is configured separately; it is **not** one of the Terrain decoration HISMs.

**One HISM per mesh:** If one mesh is assigned incompatible cull profiles in different rows, issue a validation error or apply a documented common cull policy, rather than silently spawning two HISM components for the same mesh.

## 5. Required cull distances

| Group | End cull distance in Unreal centimeters |
|---|---:|
| Small clutter | `15000` |
| Short trees | `30000` |
| Decorative buildings and dunes | `80000` |
| Mountain/background ring | **Never cull** |
| Gameplay `Obstacle` Actors | **Never cull** |
| Gameplay `Hazard` Actors | Prefer no distance cull when safety-relevant |

These limits apply to visual distance culling, **not** physics collision range. Categorize meshes into cull groups using stable metadata or asset naming conventions validated by the importer. Do not add a mandatory `CullProfile` column to the requested row schema.

## 6. Ground, lighting and fog

- Ground: one plane or existing landscape with `MI_<Level>_Ground` (e.g. `MI_Forest_Ground`).
- Lighting: **Day** or **Night**, chosen before gameplay and **fixed** throughout the level. No time-of-day cycles or event-driven switching.
- Use Exponential Height Fog matched to the game's balloon post-process configuration.

| Biome | Fog color (linear RGB) | Density | Note |
|---|---|---:|---|
| Forest | `(0.35, 0.45, 0.45)` | `0.03` | Blue-green; hide mountain bases |
| Field | `(0.75, 0.70, 0.55)` | `0.015` | Warm haze |
| City | `(0.40, 0.35, 0.28)` | `0.05` | Brown smog |
| Desert | `(0.85, 0.75, 0.60)` | `0.02` | Hot pale haze |

## 7. Invariants for generator authors

- The player may maneuver **horizontally only**. Obstacle route proofs are calculated in XY, with full balloon 3D collider tested at fixed flight height. No up/down escape routing.
- Plan → Validate → Realize. Placement-rule planning does not spawn Actors or load arbitrary world objects per iteration.
- Reserve a viable horizontal route before accepting colliding obstacles/hazards/border expansions.
- Scene HISM components cannot acquire collision accidentally through asset defaults.
- Row order, catalog merge, flock count, dune yaw, and procedural seeds must be deterministic.
- The `Notes` field is for descriptions/authoring guidance, not a script language.
- Do not silently weaken spacing, counts, ring radius or lighting invariants; report actionable validation errors.
