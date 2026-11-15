# Balloon Flight — Procedural Level Generation

**Implementation specification and coding-LLM execution plan**  
**Target:** Unreal Engine 5.8, C++ gameplay systems with Blueprint/Data Asset authoring  
**LLM context window:** 100,000 tokens  
**Document status:** implementation-ready plan; no code is claimed to exist yet  
**Version:** 1.1 — 2026-11-15 (horizontal flight, fixed lighting presets, biome DataTables)

> **Instructions to the coding LLM:** You are the implementation engineer for this feature. Read the project/repository before editing. Work in milestone-sized changes, compile/test after each milestone, and do not claim any tool, engine build, asset, or test result that you have not observed. Treat this document as the functional specification; existing project conventions override example paths and naming only when the same behavior is preserved. If there is a contradiction, report it and take the smallest safe approach. Do not implement the entire specification in a single unverified edit.

---

## 0. Objective and nonnegotiable gameplay behavior

Build a **soft-linear / indirectly linear balloon-flight level generator**. The player advances from **Start** to **Finish**, choosing alternate **horizontal routes** around obstacles and hostile corridor borders. **The player cannot maneuver vertically**: do not implement ascent/descent steering, alternate high/low routes, or vertical neighbors in navigation. The balloon's flight height is a **fixed design datum per level** (or follows an already-existing Pawn height controller, if the repository explicitly provides one); this generator must not introduce vertical maneuver controls. World assets may have height and terrain slope, but they are evaluated as obstacles intersecting the balloon's fixed-height swept collision envelope.

**Mandatory capabilities**

1. **Editable horizontal dimensions:** spline arc length, base width, local width profile, curvature, flight-height datum, and segment length are designer-configurable; units entered as meters, converted to UE centimeters internally.
2. **Biome support:** Forest, Field, City, Desert, and extensible biome IDs. Assets can be **shared across biomes** or **biome-specific**, with a deterministic catalog merge/override policy.
3. **DataTable asset catalog:** each row has exactly these designer-facing fields: `Asset`, `Category`, `Collision`, `DensityPerKm2`, `ScaleMin`, `ScaleMax`, `MinSpacingM`, `RandomYaw`, `AlignToSlope`, `ClusterSize`, `Rule`, `Notes`. Detailed schema and validation in section 4.
4. **Placement rules:** `scatter`, `ring`, `line`, `street`, `wind_aligned`, `flock`; document each in adjacent `README.md` and, where possible, reflected code metadata/tooltip comments.
5. **Optional enemies and obstacles:** independent enable switches, configurable counts/densities, easily replaceable types; biome-specific and shared entries work for both.
6. **Enemy-shaped borders:** enemy/hazard influence radii intrude into the corridor, dynamically reshaping the *horizontal* safe area; expansion is permitted only if a valid horizontal route remains.
7. **Horizontal route validation:** check actual balloon collision envelope and turning capability at the fixed flight height; never accept a level merely because it could be bypassed by rising or descending.
8. **Timed events:** elapsed gameplay time, distance/progress, and custom signals with Blueprint handlers. Events may update threats or spawn encounters, **but cannot switch day/night at runtime**.
9. **Fixed lighting:** each level chooses **Day OR Night** before play; no continuous cycles, runtime sky transitions, or event-driven phase changes. Biome fog/ground settings apply to both presets.
10. **Ground, sky and fog:** ground plane or existing landscape with `MI_<Level>_Ground`; one `ExponentialHeightFog` configuration per level according to the required biome table in section 10; match existing balloon post-process aesthetic.
11. **Rendering policy:** `Filler`, `Terrain`, `Background` use **HISM, one component per mesh, NoCollision**; `Obstacle` and `Hazard` use **individual actors with collision**. Cull rules exactly as specified in section 11.
12. **Reproducibility:** same seed, config, biome and catalog version yield the same abstract layout; generation is plan → validate → realize, independently testable.

**Example configuration (editable; not universal defaults):**

| Property | Example | Meaning |
|---|---:|---|
| Length | 1,500 m | Spline arc length |
| Base width | 150 m | Initial playable corridor width |
| Flight height | 100 m | Fixed balloon flight-height datum for generation |
| Target segment length | 250 m | Logical generation chunks |
| Border enemy count | 24 | Separate from interior enemies |
| Border radius | 12–35 m | Horizontal influence footprint |
| Obstacle count override | 40 | Optional exact gameplay count; scenery uses density |
| Interior enemies | 8 | Can be disabled or overridden by events |
| Minimum horizontal route clearance | 40 m | Configurable; inflate by balloon size |
| Biome | `Forest` | Per-level visual and spawn catalog |
| Light preset | `Day` | Alternatively `Night`, fixed for session |
| Seed | 12345 | Deterministic layout |

**Out of scope in MVP:** vertical flight controls, multi-altitude routing, automatic day/night cycles or transitions, fully procedural landscapes, large-scale combat AI, multiplayer, and entirely time-dependent moving-hazard navigation.

### 0.1 Horizontal coordinate and collision model

The spline describes progression, not a compulsory trajectory. `C(s)` is the center point at arc-length `s`. `R(s)` is a **horizontal** normalized right vector (based on the tangent projected onto XY). The flight route is represented in **2D**:

```text
Pxy(s, lateral) = Cxy(s) + Rxy(s) * lateral
where s in [0, LevelLength]
      lateral in [-W(s)/2, +W(s)/2]
BalloonZ = LevelFlightAltitudeZCm   // fixed; do not add a maneuver axis
```

World objects remain fully 3D. For route occupancy, test the swept balloon collision shape at `BalloonZ` against their 3D collision volumes and project **only intersecting** threats/objects to horizontal blocking footprints. Tall structures can obstruct the flight plane; scenery strictly below it may not. Horizontal disc-like border threat regions occupy the allowed balloon flight-height band according to their configured threat policy.

**Invariants:** no level outcome may depend on passing above or below an obstacle; the generated path is only a feasibility proof, not an autopilot. The generator must not alter the existing Pawn's movement component merely to satisfy a generated route.

## 1. Definition of done

The completed feature is accepted only when these statements are true and supported by tests:

- Designers change length, width, biome, fixed flight height and seeded geometry without C++ edits.
- A fixed seed + config + catalog version produces identical plan descriptors; independent subsystem streams prevent irrelevant layout changes.
- A balloon cannot be guided vertically by this system and **every accepted generated level has a passable horizontal start-to-finish route** at its configured flight height.
- Boundary enemy radius changes deform the horizontal safe corridor; runtime changes are prevalidated against the player's current location.
- Obstacles and enemies can be enabled independently; biome-specific and shared catalog entries both participate.
- Every asset DataTable row validates the exact field schema, soft mesh/Blueprint-class reference, category, and rule compatibility.
- `scatter`, `ring`, `line`, `street`, `wind_aligned`, and `flock` rules operate according to the adjacent README and deterministic tests.
- `Filler`, `Terrain` and `Background` instances are allocated to one `UHierarchicalInstancedStaticMeshComponent` per mesh, collision disabled; `Obstacle` and `Hazard` use individual collision-enabled actors; `flock` uses its explicitly named Blueprint-actor exception.
- HISM cull distances are correct by asset type; mountains and gameplay obstacles do not distance-cull.
- The correct ground material `MI_<Level>_Ground` and biome-specific fog color/density are applied without destroying designer-owned level actors.
- Each level runs in one **unchanging** Day or Night preset; no animation/timeline can transition it to the other.
- Time/progress/custom events are configurable via Blueprint; no day/night switching action is offered.
- Clean editor regeneration never duplicates generated instances or destroys unrelated content.
- Automated tests cover geometry, 2D route safety, collision categories, placement rules, biome merges, culling, fixed presets, and event behavior.

Do not mark features complete merely because source compiles. UE editor/BP asset creation, PIE results and profiling need actual evidence or must be labeled unverified.

## 2. Technical decisions and constraints

### 2.1 Engine architecture

- C++ owns deterministic planning, DataTable validation, placement rules, border influence geometry, fixed-height **2D** route verification, events, HISM/Actor realization, ownership, cleanup, and diagnostics.
- Blueprint and editor assets own biome catalogs, actor classes, lighting presets, visual effects, and gameplay event receivers.
- Separate **Plan** (pure descriptors and tests) from **Validate** (clearance, rules, reachability) from **Realize** (resolve assets, add HISM transforms / spawn actors).
- `USplineComponent` is a horizontal progression reference; it does not enforce motion or construct a vertical track.
- Use `FRandomStream` and stable hashed per-system/per-row/per-instance seeds; never global random for layout.
- Use `UDataTable` with a reflected row `UStruct` descended from `FTableRowBase`. Shared and per-biome tables merge by stable row name with clear precedence.
- HISM components are generator-owned and keyed by mesh asset, with one component per distinct static mesh; `SetCollisionEnabled(ECollisionEnabled::NoCollision)` on every such component.
- `Obstacle`/`Hazard` placement is realized via individual Actors even if the soft asset points at a Static Mesh (wrap it in a spawnable mesh Actor). Dynamic collision/hazard behavior remains separate from scenery.
- `flock` explicitly produces `BP_BirdFlock` Actors rather than HISM instances despite its decorative/ecosystem purpose; default their collision off unless they are deliberately authored as gameplay hazards.
- The PCG plugin and World Partition are optional; the generator's plan is the source of truth.

### 2.2 Units, axes, slope

- Designer-facing metric distances are meters, Unreal positions/distances are centimeters; convert exactly once at API boundaries.
- `X/Y` describe all free horizontal maneuvers; `Z` is computed from the **fixed level flight datum** for balloon routing, not a player-controlled degree of freedom.
- Terrain may slope and objects may be ground-anchored. `AlignToSlope` uses sampled ground surface normals for **asset rotation only**, never to change the player's path into a climb/descent route.
- `DensityPerKm2` is items per square kilometer of eligible *horizontal* area, `1 km² = 1,000,000 m²`.
- `ScaleMin/ScaleMax` are uniform scale factors, seeded per item; spacing is measured in meters and uses horizontal footprint distance.

### 2.3 Runtime policy

- Explicit editor generation/preview; gameplay generation only when catalogs/necessary assets are ready.
- Expensive validation cannot run every frame. Dynamic border events propose and validate before commit.
- Avoid unnecessary AI Controllers on boundary threats; procedural hazard footprints can be cheap proxies.
- Day/Night is a **construction/level-load choice**. Applying a preset at initialization is allowed; changing it during gameplay is out of scope.
- No worker-thread UObject loading or spawning. Pure plan computation may later run off-thread if profiling justifies it.

## 3. Modules, contracts, and proposed project layout

> **Important:** The names here are proposed class names. They are **not existing Unreal Engine APIs**. The implementation LLM must check the project's actual module name, source folder layout, engine version, existing balloon Pawn, collision profiles, and coding style before committing to paths.

Suggested feature folder under your actual game module:

```text
Source/<GameModule>/
  BalloonLevel/
    Public/
      BalloonLevelTypes.h
      BalloonLevelConfig.h
      BalloonLevelGenerator.h
      BalloonLevelEventTypes.h
      BalloonLevelEventManager.h
      BalloonEnvironmentPresetController.h
      BalloonAssetCatalog.h
      BalloonAssetRuleTypes.h
      BalloonBorderThreat.h
      BalloonLevelDebug.h
    Private/
      BalloonLevelConfig.cpp
      BalloonLevelGenerator.cpp
      BalloonLevelEventManager.cpp
      BalloonEnvironmentPresetController.cpp
      BalloonAssetCatalog.cpp
      BalloonAssetRuleTypes.cpp
      BalloonBorderThreat.cpp
      BalloonLevelDebug.cpp
    Generation/
      Public/
        BalloonSplinePlanner.h
        BalloonBorderPlanner.h
        BalloonPlacementPlanner.h
        BalloonHorizontalRouteValidator.h
        BalloonBiomePlacementRules.h
        BalloonLevelPlan.h
      Private/
        BalloonSplinePlanner.cpp
        BalloonBorderPlanner.cpp
        BalloonPlacementPlanner.cpp
        BalloonHorizontalRouteValidator.cpp
        BalloonBiomePlacementRules.cpp
        BalloonLevelPlan.cpp
    Tests/
      BalloonLevelGenerationTests.cpp
      BalloonLevelEventTests.cpp
      BalloonEnvironmentPresetTests.cpp
      BalloonHorizontalRouteValidationTests.cpp
      BalloonBiomeCatalogTests.cpp
      BalloonPlacementRuleTests.cpp
Content/
  BalloonLevel/
    Configs/        DA_BalloonLevel_Default
    Rules/          DA_BorderThreatRules, DT_BalloonAssets_Shared,
                    DT_BalloonAssets_Forest, DT_BalloonAssets_Field,
                    DT_BalloonAssets_City, DT_BalloonAssets_Desert, README.md
    Blueprints/     BP_BalloonLevelGenerator, BP_BorderThreat_*, BP_Obstacle_*, BP_Enemy_*
    Curves/         CF_WidthProfile, CF_Difficulty
    Materials/      MI_Forest_Ground, MI_Field_Ground,
                    MI_City_Ground, MI_Desert_Ground
    Presets/        DA_Forest_Day, DA_Forest_Night, etc.
    Levels/         L_BalloonGenerationTest
    Debug/          Optional editor visualizers
Docs/
  BalloonLevel/
    IMPLEMENTATION.md
    DESIGN_DECISIONS.md
    API.md
    TEST_REPORT.md
    HANDOFF.md
```

Keep code private/public based on intended consumers; do not create empty placeholder files just to match this tree. Tests can be in a separate test module when project conventions require it.

### 3.1 Types (design-level contract)

Use strong enums and explicit structures, with Blueprint reflection where required. Suggested:

```cpp
// Pseudocode contracts; reconcile signatures with installed UE headers.
UENUM(BlueprintType)
enum class EBalloonLevelGenerationStatus : uint8
{
    NotStarted, Planning, Validating, Ready, Realizing,
    Complete, InvalidConfig, InsufficientSpace, Unreachable, Failed
};

UENUM(BlueprintType)
enum class EBalloonBorderInfluencePolicy : uint8
{
    Blocked, Damaging, Pushback, VisualOnly
};

UENUM(BlueprintType)
enum class EBalloonLevelTriggerType : uint8
{
    ElapsedGameTime, NormalizedProgress, AbsoluteProgress, CustomSignal
};

UENUM(BlueprintType)
enum class EBalloonLightingPreset : uint8
{
    Day, Night // Chosen at load; immutable throughout gameplay.
};

USTRUCT(BlueprintType)
struct FBalloonSpawnRule
{
    // Stable ID, actor class or soft class reference, weight, min clearance,
    // allowed segment/progress/biome filters, exact count or density.
    // NO altitude maneuver or multi-altitude route logic.
};

struct FBalloonLevelPlan
{
    // Seed/version, spline samples, segment descriptors, border descriptors,
    // obstacle descriptors, enemy descriptors, biome scenery descriptors,
    // fixed-height 2D route proof, ground/fog preset,
    // diagnostics; NO UObject ownership or spawned actors.
};
```

**Design recommendation:** Keep the abstract plan mostly plain C++ structs that can be hashed/tested. Use `USTRUCT` for portions consumed by Blueprint and editor. Generated plan descriptors should contain **stable IDs** (`SegmentId`, `SpawnRuleId`, `InstanceIndex`) rather than actor pointers.

### 3.2 Public generator API

The coordinating Actor should expose approximately:

```cpp
UCLASS(Blueprintable)
class <MODULE_API> ABalloonLevelGenerator : public AActor
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Balloon Level")
    TObjectPtr<UBalloonLevelConfig> LevelConfig;

    UFUNCTION(BlueprintCallable, Category="Balloon Level")
    bool GenerateLevel();

    UFUNCTION(BlueprintCallable, Category="Balloon Level")
    void ClearGeneratedLevel();

    UFUNCTION(BlueprintCallable, Category="Balloon Level")
    bool RegenerateLevel();

    UFUNCTION(BlueprintPure, Category="Balloon Level")
    float GetNormalizedProgress(const FVector& WorldPosition) const;

    UFUNCTION(BlueprintPure, Category="Balloon Level")
    EBalloonLevelGenerationStatus GetGenerationStatus() const;

    // Implement event delegates and validated config getters as appropriate.
};
```

The LLM must replace `<MODULE_API>` with the real export macro and must not paste this conceptual pseudocode unmodified. Do not mark methods BlueprintPure if they can spawn, mutate, or perform expensive nontrivial searches. Provide a `GetDiagnostics()` API for UI/logging, and optionally `GeneratePreview()` / `ClearPreview()` as editor-only commands.

### 3.3 Subsystem ownership model

```text
ABalloonLevelGenerator
  ├─ Config + Biome DataTables (read-only during one generation)
  ├─ Spline (USplineComponent)
  ├─ Plan builder (pure C++ service)
  │    ├─ Spline planner
  │    ├─ Border planner
  │    ├─ Biome catalog merger + rule planner
  │    ├─ Placement planner
  │    └─ Horizontal route validator
  ├─ Realizer (HISM per mesh; separate obstacle/hazard Actors; cleanup)
  ├─ Runtime border manager (radius changes and pre-apply validation)
  ├─ Event manager (owned component or UObject)
  └─ Fixed environment preset controller (Day OR Night + biome fog/ground)
```

Dependencies flow one way: `Config -> Plan -> Validation -> Realization -> Runtime`. A subsystem should not reach into another subsystem's internals; use typed descriptors and explicit function arguments. Runtime changes update a **runtime hazard snapshot**, not the immutable initial plan.

---

## 4. Configuration data model

Implement `UBalloonLevelConfig` (`UDataAsset` or `UPrimaryDataAsset`) that points to `DT_BalloonAssets_Shared` plus the selected biome's table. Preserve the authored row fields **exactly** as requested; put advanced technical metadata (precise collision footprints, cull class, street-grid spacing, group IDs, counts) into optional *separate* configuration assets or deterministic inferred defaults, not extra required DataTable columns.

### 4.1 Level configuration

| Group | Required fields | Behavior |
|---|---|---|
| `Geometry` | `Seed`, `LengthMeters`, `BaseWidthMeters`, `SegmentTargetMeters`, `FlightHeightMeters`, `Curvature`, `WidthProfile`, `StartTransform` | Horizontal route only; fixed flight Z |
| `Biome` | `BiomeId`, `SharedAssetTable`, `BiomeAssetTable`, optional catalog version | Forest, Field, City, Desert or new names; shared + local |
| `Ground` | `GroundMaterial`, `GroundPlaneMesh` or existing `Landscape` reference, ground bounds/offset | `MI_<Level>_Ground` applied on plane or landscape; do not generate new terrain engine unless already present |
| `Borders` | enable, rules, count, min/max radius, intrusion, min safe width, dynamic radius options | All influence zones evaluated at flight height |
| `Obstacles` | enable, exact-count override (optional), spacing, placement attempts | `Obstacle`/`Hazard` rows; individual colliding Actors |
| `InteriorEnemies` | enable, exact-count override, encounter rules | Independent from border threats; shared + biome-specific actor classes |
| `RouteSafety` | horizontal balloon radius, balloon Z extent, safety margin, minimum turning radius, wind drift margin, 2D grid resolution, retries | Sweep collision shape at fixed flight height; **no climb variables** |
| `Events` | timed, distance, custom signal definitions | Cannot change lighting preset |
| `Lighting` | `Preset=Day|Night`, per-biome preset asset or references to existing lights/sky | Fixed for duration of gameplay |
| `Fog` | per-biome `FLinearColor`, density, fog actor reference | Use table in section 10 |
| `Generation` | max attempts, retry policy, preview toggles | Fail explicitly on impossible authored rules |

### 4.2 Required asset DataTable row schema (exact field names)

```cpp
// DESIGN CONTRACT / EXAMPLE, not compile-verified UE5.8 source.
USTRUCT(BlueprintType)
struct FBalloonLevelAssetRow : public FTableRowBase
{
    GENERATED_BODY()

    // ONE soft path: points to a UStaticMesh asset OR to an Actor Blueprint
    // generated UClass (usually ...BP_Name.BP_Name_C). Validate after load.
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FSoftObjectPath Asset;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName Category; // Filler, Terrain, Background, Obstacle, Hazard

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    bool Collision = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    float DensityPerKm2 = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    float ScaleMin = 1.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    float ScaleMax = 1.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    float MinSpacingM = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    bool RandomYaw = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    bool AlignToSlope = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 ClusterSize = 1;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName Rule; // scatter, ring, line, street, wind_aligned, flock

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FString Notes;
};
```

`Asset` is intentionally a **single field** as requested. The resolver first examines the resolved object type. A `UStaticMesh` can be used for a HISM or wrapped in a separate `AStaticMeshActor` for colliding categories; a `UClass` must derive from `AActor`, can be spawned as a Blueprint Actor, and must not be treated as a mesh. The Blueprint soft reference in CSV/JSON must use the correct *generated class object path*; validate it in editor rather than guessing from a `.uasset` path. If the existing project requires a different soft-reference representation, keep the exact serialized `Asset` column and implement a narrow adapter.

**Row validation (errors, not silent corrections):** invalid/empty soft path; missing asset; invalid `Category` or `Rule`; `DensityPerKm2 < 0`; nonfinite numbers; `ScaleMin <= 0`; `ScaleMax < ScaleMin`; `MinSpacingM < 0`; `ClusterSize < 1`; incompatible asset and category; unsupported class for a renderer; invalid rule-specific requirements. `Collision` must be `false` for Filler/Terrain/Background; it must be `true` for Obstacle/Hazard (except explicitly permitted *actor-only flock*). Report row name and source DataTable in every error.

### 4.3 Shared + biome-specific catalog merging

```text
DT_BalloonAssets_Shared      // may include common rocks, towers, birds, etc.
DT_BalloonAssets_Forest      // unique trees, ring mountains, etc.
DT_BalloonAssets_Field       // hay, fences/hedges, bird flocks, etc.
DT_BalloonAssets_City        // street blocks, lampposts, etc.
DT_BalloonAssets_Desert      // dunes, sparse hazards, etc.
```

- Select exactly one `BiomeId` per generated level (future multi-biome level sections are an extension, not required MVP).
- Collect shared rows, then overlay biome-specific rows keyed by stable **DataTable row name**. A biome row with the same name overrides the shared row entirely; do not merge individual columns implicitly.
- Stable lexicographic row ordering and stable per-row seeds remove dependence on DataTable iteration order.
- A shared row must not spawn twice merely because it exists in the shared table and a biome table.
- Maintain separate enable/count controls for interior enemies, border threats, obstacles, and noninteractive scenery even when they share a catalog.
- Newly created biome table follows the same row schema without core generator modifications.

### 4.4 Configuration example (documentation only)

```yaml
level:
  seed: 12345
  biome: Forest
  length_m: 1500
  base_width_m: 150
  segment_target_m: 250
  fixed_flight_height_m: 100
  ground_material: MI_Forest_Ground
  lighting_preset: Day # OR Night, fixed for the entire run
catalogs:
  shared: DT_BalloonAssets_Shared
  biome: DT_BalloonAssets_Forest
borders:
  enabled: true
  count: 24
  radius_min_m: 12
  radius_max_m: 35
  intrusion_max_m: 18
  minimum_safe_width_m: 40
  allow_dynamic_radius: true
obstacles:
  enabled: true
  exact_count_override: 40 # optional override; otherwise density rule
interior_enemies:
  enabled: true
  count: 8
route_safety:
  balloon_horizontal_radius_m: 4
  balloon_vertical_extent_m: 6 # collision footprint ONLY, not maneuvering
  safety_margin_m: 2
  minimum_turn_radius_m: 18
  horizontal_cell_size_m: 5
```

Use a separate config value for authorial count overrides so `DensityPerKm2` retains its native density semantics. If a count override is active, document whether it replaces or caps the per-row density output (recommended: replace the *total* category target while preserving relative density weights among eligible rows).

## 5. Geometry and segments

### 5.1 Spline generation

**MVP:** Generate a centerline of `N+1` control points across the desired distance, with bounded sideways offsets and smooth tangents. Rescale or iteratively solve to produce requested **arc length** rather than assuming X displacement equals level length. Do not allow loops, crossings, or turns too tight for the balloon. For generated curved paths, use curvature/noise filtered at a scale significantly larger than obstacle size.

Recommended algorithm:

1. Start at `StartTransform` heading.
2. Initialize a stable random stream for `Geometry` using `(Seed, "Geometry", Version)`.
3. Propose smoothly changing headings/curvature within `MaxCurvature` and `MinimumTurningRadius` limits.
4. Create spline samples and calculate cumulative arc length.
5. Trim/interpolate the final sample to `LengthMeters`, ensuring `abs(actual - requested) < tolerance`.
6. Check non-self-intersection / non-adjacent segment proximity; on invalid path, retry with bounded reduced curvature.
7. If retries are exhausted, generate a documented **straight corridor fallback** when allowed, or return `InvalidGeometry`.

For an authored centerline mode, read the placed `USplineComponent` and interpret `LengthMeters` either as an explicit trim limit or as a derived read-only length. **Do not silently rescale an artist-authored spline**.

### 5.2 Segment scheme

Partition by arc length, not by number of spline points:

```text
Segment 0: [0, 250 m)
Segment 1: [250, 500 m)
...
Segment 5: [1250, 1500 m]
```

`SegmentId` is zero-based, stable for the same config, and ownership is assigned deterministically. Boundary points have an explicit tie-breaking rule, e.g. segment `[start, end)` except last. Each segment stores `StartS`, `EndS`, local samples, base bounds, and placement/route diagnostics. Segments are **logical planning units**; streaming is a separate policy.

### 5.3 Width profile and fixed flight height

`WidthAt(s) = max(MinWidth, BaseWidth * WidthCurve(s/Length))`.

Two independent mechanisms affect horizontal usable width: authored width profile and intruding enemy-shaped border hazard regions. The route exists in XY at one `FlightHeightMeters` datum; this parameter sets the height of collision queries in the 3D world but **is not a controllable altitude range**. Objects are queried against the balloon's actual vertical collision extent at this Z. Terrain/slope grounding affects scenery placement only.

Warn when the fixed flight datum or ground intersects the balloon collider anywhere on the proposed path. Do not create a high-altitude bypass to fix this; reject or adjust the *level design* with explicit diagnostics.

### 5.4 Progress tracking

Track the nearest spline distance, but use a monotonic `MaxReachedS` (or validated checkpoint advancement) to trigger one-shot progress events. Maintain a separate **current** position for UI/backtracking. The progress calculator should avoid sudden jumps to another section of a curved spline; use nearest-point search constrained to the previously known segment plus neighbors and a fallback full search after teleport.

---

## 6. Borders as enemy/hazard influence regions

### 6.1 Model

Border enemies/hazards create a **horizontal irregular corridor**. Treat each threat as a circle/disc in XY with a typed gameplay influence policy (blocked, damaging, pushback, visual) at the balloon's fixed flight plane. Their visual and world-space collision geometry may be 3D; for path planning, project only threat influence that overlaps the balloon's fixed-height collision envelope.

For a center offset `d` outside the boundary and radius `r`, intrusion depth is approximately `max(0, r-d)`; compute exact overlap against the spline-based border region. Optional `VerticalCylinder` or `Sphere` influence shapes can exist for 3D visual/physics intersection, but must be projected onto the fixed balloon flight plane. **Never accept an overflight or underflight path** as a safety proof.

### 6.2 Border planning steps

1. Build left/right boundaries as sampled offset curves from the center spline.
2. Distribute `ExactCount` across sides by stable deterministic partitioning (configurable balance is optional).
3. For each border threat, choose a rule, longitudinal position, radius, side, desired intrusion, altitude range, and influence policy at fixed flight height using its own per-item stream.
4. Compute world-space influence shape and store descriptor without spawning.
5. Apply minimum threat spacing if configured; overlapping influence zones are allowed only under explicit rules.
6. Sample corridor cross sections to detect severe closures; run full route validation against all obstacles afterward.
7. Save debug diagnostics: radius, intrusion, side, overlap, and any **horizontal** path closure.

Do not fill the entire exterior world with actors. Use a clearly defined corridor-envelope/out-of-bounds policy and threat zones near the playable edges. When a player crosses the envelope, handle that as a gameplay consequence (warning/damage/teleport/failure) separate from each individual threat actor.

### 6.3 Runtime radius modifications

A border event may ask for `SetRadius`, `AddRadius`, `ScaleRadius`, or `AnimateRadius` on one threat, a tag group, a segment, or the whole level.

**Transaction protocol (required):**

```text
Event requests change
  -> prepare candidate radius snapshot
  -> update shape descriptions in plan-space
  -> validate at least one safe route / permitted escape policy
  -> if safe: commit and animate visual/collision representation
  -> if unsafe: clamp to safe maximum OR reject/defer with explicit result
  -> publish BorderChanged event and update debug view
```

Never apply unsafe full-size changes first and validate afterward. For MVP, radius animations can follow a set of prevalidated discrete safe states; intermediate values must be safe too (or be verified by conservative bounds). Runtime route checking must use the player's current region and reachable finish, not only a fresh spawn-to-finish route if the player might be trapped behind an expanding hazard.

### 6.4 AI and border hazards are different concerns

- `ABalloonBorderThreat` exposes collision/influence/visual state and optional damage/pushback behavior.
- Boundary visuals may be simple Actors or instances, but their influence descriptors remain owned by the runtime border manager.
- Interior flying enemies may pursue/attack; **do not** make every border threat a full tick-driven pursuing enemy.
- An interface such as `IBalloonInfluenceProvider` can expose the occupied/hazard shape to the validator.
- An interface such as `IBalloonTimedEventReceiver` can receive event payloads in Blueprint.

For a first implementation, keep border threats stationary. Add moving threats only after route validation supports time-dependent occupancy or applies conservative swept-volume checks.

---

## 7. Biome assets, placement rules, obstacles and enemies

### 7.1 Asset categories and representation

| Category | Realization | Collision | Typical items |
|---|---|---|---|
| `Filler` | HISM, one per Static Mesh | `NoCollision` | grass, small clutter, decorative shrubs |
| `Terrain` | HISM, one per Static Mesh | `NoCollision` | non-gameplay terrain props and dunes |
| `Background` | HISM, one per Static Mesh | `NoCollision` | distant mountains, horizon silhouettes |
| `Obstacle` | Separate Actor (Blueprint Actor or mesh-wrapping Actor) | **Enabled** | towers, buildings when gameplay-blocking, trees if blocking |
| `Hazard` | Separate Actor with gameplay effect | **Enabled** | dangerous objects, border threats where authored as row assets |

`flock` is an explicit **rule-specific Actor exception**: spawn `BP_BirdFlock` (not a HISM) because the flock needs group behavior. Default to no physical balloon collision for decorative flocks; if birds cause damage, classify the row as `Hazard` and include its collision footprint in route safety. Avoid trying to make a Blueprint Actor itself a HISM mesh.

Do not confuse `Terrain` category scatter meshes with the actual ground surface: ground is separately built/assigned in section 10.

### 7.2 Rule registry and documentation

Implement `IBalloonPlacementRule` or a central deterministic dispatcher keyed by `FName Rule` (small fixed set initially), with documented input/output contracts. Each rule receives resolved row, biome, eligible region, fixed seed stream, terrain sampler, and collision reservation index; it returns *descriptors* (position, yaw, uniform scale, asset ID, instance ID), **not directly spawned objects**. The validator checks every result before realization.

**Mandatory adjacent file:** `Content/BalloonLevel/Rules/README.md` (or repository mirror if content folders cannot store a readable text README). Include these six rule descriptions verbatim or more precisely:

- **`scatter`** — Random eligible horizontal positions at `DensityPerKm2`; all instances at least `MinSpacingM` apart, organized into clusters of `ClusterSize`. Count target is `round(EligibleAreaKm2 * DensityPerKm2)` unless an explicit gameplay count override applies. If `ClusterSize>1`, generate cluster anchors then members with deterministic offsets and spacing enforcement; report impossible packing instead of ignoring separation.
- **`ring`** — Forest mountains only **outside the playable area** in an annulus **1.5–3 km from the level's horizontal center**. Face meshes inward, sample `ScaleMin–ScaleMax`, and intentionally overlap projected silhouettes to close the horizon. Use terrain height for placement where available. The ring must not affect corridor collision, and mountain instances **never cull**. If large/curved geometry needs wider backdrop, preserve the stated central annulus constraint and report uncovered views instead of silently changing radius.
- **`line`** — Fences and hedges are chained end-to-end along field borders. Align segment tangents to the supplied field-border polylines, use mesh-length metadata or measured bounds to advance along the line, avoid accidental gaps, and handle final short remainders deterministically. For collision-enabled rows use individual Actors; decorative nonblocking rows can be HISM.
- **`street`** — City-block rows snap to a shared street grid. Quantize yaw to **0°, 90°, 180°, 270°**, place building back-to-back using lot depth and exclusion footprints, and place lampposts along street edges on configured intervals. A documented grid/lot generator config supplies road widths and cell/lot dimensions; `DensityPerKm2` controls eligible density/budget, not arbitrary off-grid jitter.
- **`wind_aligned`** — Desert dunes choose **one per-level prevailing yaw**, seeded once, then every dune uses that yaw with per-instance jitter limited to **±10°**. This overrides `RandomYaw`; dunes honor `ScaleMin–ScaleMax`, ground slope, and density rules.
- **`flock`** — Spawn **`BP_BirdFlock`**, with **3–6 separate flock Actors per Field level**, deterministic per level seed and eligible spawn region. This explicit biome/rule count takes precedence over `DensityPerKm2` for flock actor count; `ClusterSize` may be used by the Blueprint for birds-per-flock if supported, not to multiply actor count. For other biomes, flock rows are disabled unless explicitly extended by a biome rule.

**Rule precedence:** `Rule`-specific yaw/placement overrides `RandomYaw` and generic scatter. `AlignToSlope` applies only where surface normals are meaningful; does not tilt buildings away from their street orientation unless explicitly configured. `Collision` is enforced by category and actor/rule exception. `ScaleMin–ScaleMax` is always applied unless rule documentation declares an authoring-scale exception. `Notes` is for human documentation, not executable conditional logic.

### 7.3 Density, spacing, bounds and determinism

- Compute density from *eligible area* (corridor, field polygon, grid lots, or annulus) in km². Do not treat entire ground plane area as gameplay-scatter area.
- Deterministically allocate counts by row. With many rows, use stable row-name ordering, independent per-rule seeds, and a spatial hash for `MinSpacingM` checks.
- Separate gameplay exact-count budgets (optional Overrides) from decorative density counts. Disabled enemies/obstacles produce zero gameplay descriptors even if the table contains rows.
- Resolve static mesh visual bounds and gameplay collision metadata up front; before world spawn, conservatively check billboard/footprint overlap at fixed flight height.
- Start/Finish safe areas must not contain hazardous blocking actors. Visual `Filler/Terrain/Background` do not block navigation regardless of depicted height.
- If placement is impossible, fail with rule-specific diagnostics (`RowName`, target count, planned count, rejected reason, spatial region); never loop indefinitely or silently violate spacing.
- Store stable `(Biome, TableRowName, Rule, InstanceIndex, SegmentId)` IDs and independent streams. Changing dune count should not move border threats.

### 7.4 Obstacles and hazards

- `Obstacle` and `Hazard` always become individually managed Actors with collision enabled, never HISM. For raw mesh rows spawn a mesh-wrapping Actor (`AStaticMeshActor` or project-specific class) with correct collision profile.
- Determine collision interaction with the balloon at its **fixed Z**. World geometry may be tall; validate the envelope at flight height and swept clearance in XY.
- Hazards with damage/no collision blocking can use an overlapping collision volume but remain Actors. Threat occupancy policy (blocked / unsafe / warning) is explicit in route safety.
- The generator owns created actors, tracks weak refs, and never destroys authored actors that happen to share class/tags.

### 7.5 Interior enemies and shared/biome entries

- Enemy spawn management is independent from border threat placement and scenery. It selects allowed shared/biome class entries and applies the level's `EnemiesEnabled` and total count settings.
- Support initial, on-segment-entry, and event-triggered encounters. Count 0 and disabled both produce zero initial enemies.
- A flying enemy may move in 3D under its own AI, but **the generator cannot assume the player can maneuver vertically to evade it**. Define conservative encounter clearance on the fixed player flight plane.
- Prevent double-spawning the same row when shared and biome tables overlap (biome-specific row wins).
- `BP_BirdFlock` is a decorative ecosystem group unless explicitly configured as an enemy/hazard class; avoid counting decorative flocks as hostile enemies by default.

### 7.6 Independent randomness

```text
StreamSeed = StableHash32(BaseSeed, GenerationVersion, BiomeId,
                          SystemTag, TableRowName, RuleId,
                          SegmentId, InstanceIndex)
```

Use explicitly stable integer/string hashing, not UObject pointers or unordered iteration. Catalog version belongs in the plan signature; reproducibility is guaranteed within documented engine/content/config versions.

## 8. Horizontal route safety validation — required

### 8.1 Flight and world collision model

The balloon can maneuver laterally/forward/backward on a **single fixed flight plane**, not rise/dive to bypass obstacles. A 2D route point is only valid when the balloon's *full 3D collision envelope* at that XY and fixed Z does not hit 3D world obstacles. The validator therefore searches **2D** but uses **3D swept collision tests**.

Define `FBalloonFlightEnvelope`:

```text
CollisionRadiusMeters          horizontal effective radius
CollisionHalfHeightMeters      3D collider Z extent at fixed flight datum
AdditionalSafetyMarginMeters  inflation for gameplay/latency
MinimumTurnRadiusMeters       inertia/turnability
ConservativeWindMarginMeters  optional lateral drift margin
FixedFlightHeightMeters       level-configured center Z
```

There are **no** `MaxClimbSlope`, `MinAltitudeMeters`, `MaxAltitudeMeters`, altitude nodes, or vertical steering heuristics in this generator. If existing Pawn movement still changes Z autonomously, inspect it in M0 and either (a) normalize the game's movement contract with its owner, or (b) validate a narrow conservative Z envelope that encloses the passive bobbing; **do not add vertical maneuver paths**.

### 8.2 Validation pipeline

1. Validate corridor base width, inflated balloon size, terrain/ground intersection at fixed flight height, and start/finish clearance.
2. Rasterize or lattice-sample candidate cells in `(s, lateral)` only; mark cells blocked by projected 3D obstacle/hazard collision at the fixed flight plane, plus hostile border radii.
3. Use A* (or equivalent) over horizontal neighbors, with costs for turning, clearance, and forward progress; do not add vertical neighbors.
4. Smooth the candidate path, then check each edge using swept 3D collision (balloon radius + half-height + margin) **at fixed Z**.
5. Enforce horizontal turn-radius/inertia constraints and a buffer for wind lateral drift.
6. For live radius updates, validate a route from the balloon's **current XY** to the finish and reject/clamp the unsafe update before committing visuals/collision.

A collision-opaque mountain rendered in HISM with NoCollision is decoration and must not be treated as a physical path blocker; an `Obstacle` actor with collision is.

### 8.3 Output and debug

```text
FHorizontalRouteValidationResult
  bReachable
  FailureReason
  RoutePointsXYWorldCm[]
  FixedFlightHeightWorldCm
  MinimumClearanceMeters
  EstimatedMaximumTurnCurvature
  CellsVisited
  AttemptCount
  Warnings[]
```

The route proof exists for safe generation only. The player is **never forced** to follow the exact computed route.

### 8.4 Retries/fallback

Bound total retries (e.g. 12). Preserve requested obstacle/hazard counts unless a designer has opted into a documented relaxed-density fallback. Retry placement/layout using stable per-attempt seeds, then fail with a useful diagnostic if no safe horizontal route exists. A vertically passable gap does **not** count as success.

---

## 9. Event system

### 9.1 Event definitions

Implement `UBalloonLevelEventManager` (an Actor Component on generator or a `UObject` with explicit lifecycle). It owns definitions and runtime state, but does not contain hardcoded gameplay actions.

Suggested event fields:

```text
EventId            Unique stable FName / Gameplay Tag
bEnabled           Default true
TriggerType        ElapsedGameTime / Progress / CustomSignal
Threshold          Seconds, meters, or normalized 0..1 progress
TargetTag          Optional named receiver/group
ActionType         Optional built-in action category for common actions
Payload            Typed parameters or tagged payload
Priority           Deterministic tie-breaking at equal thresholds
RepeatPolicy       Once / RepeatAfterCooldown / OnEveryCrossing
CooldownSeconds    If repeatable
bPauseWithGame     Elapsed gameplay time excludes pauses by default
```

The manager should support an **event interface** (`IBalloonLevelEventReceiver`) or multicast delegates that can be implemented/bound in Blueprints. To avoid a fragile generic event enum with innumerable cases, prefer a small number of core actions plus a Blueprint-extensible dispatch (`EventId`, `TargetTag`, typed payload). Native convenience actions may include:

- `SpawnEnemyGroup` (via the enemy manager)
- `SetBorderRadius` / `ScaleBorderRadius` (validated transaction)
- `ChangeWind` (signal to existing flight/wind system; integration stub until wind exists)
- `ApplyBiomeEffect` (optional atmosphere gameplay effect that does **not** change fixed day/night preset)
- `ToggleObstacleGroup` (only if runtime safety permits)
- `BroadcastGameplayTag` / custom Blueprint action

### 9.2 Runtime semantics

- Start timer when level transitions to gameplay-ready; do not count planning/spawning as player time.
- Default elapsed time uses **game time**, not wall clock; pausing freezes it. Explicitly support opt-in real-time behavior only if needed.
- Handle threshold crossing by evaluating `previous < threshold <= current` (with appropriate equality behavior at initial trigger). A large tick or teleport must not skip due events.
- Deterministic firing order: trigger time/progress, then priority, then stable `EventId`.
- One-shot events fire once, even after player backtracking. Repeatable events track next eligible threshold/time and cooldown.
- Changes triggered in the same frame are batched into candidate state where feasible. If order matters, document the transaction ordering and make it stable.
- Clear/restart must reset event timers, subscriptions, and fired-event state.
- Integrate save/load later by serializing elapsed time, progress watermark, fired IDs, cooldown state, and current fixed preset ID; MVP can expose a serializable snapshot structure without wiring save slots.

### 9.3 Example configured event timeline

| Trigger | Action | Target behavior |
|---|---|---|
| 30 s elapsed | `IncreaseWind` | Call a Blueprint receiver for wind intensity change |
| 40% progress | `SpawnEnemyGroup` | Spawn 3 flying enemies in valid ahead-of-player volume |
| 65% progress | `ScaleBorderRadius` | Try +15% radius within runtime route-safety constraints |
| 85% progress | `IncreaseFogHazard` | Optional gameplay visibility/hazard signal; the fixed Day/Night lighting choice does not change |
| Custom signal `BossDefeated` | `OpenFinish` | Unlock finish condition via Blueprint |

All events are optional and data-driven; none of the example event names should be hardcoded into the generator.

### 9.4 API expectations

```cpp
// Conceptual signatures, not verbatim engine headers:
void StartEventTimeline();
void PauseEventTimeline(bool bPaused);
void ResetEventTimeline();
void UpdateElapsedGameTime(float DeltaSeconds);
void UpdatePlayerProgress(float NormalizedProgress, float AbsoluteMeters);
void EmitCustomSignal(FName SignalId, FName TargetTag);
bool RegisterEventDefinitions(const TArray<FBalloonLevelEventDefinition>& Definitions);
```

Use one authoritative update loop with no per-event `Tick` requirement. Provide Blueprint-accessible delegates for `OnLevelEventTriggered` and `OnLevelEventRejected` (e.g., an invalid border expansion).

---

## 10. Fixed Day/Night, ground and biome atmosphere

### 10.1 Fixed Day/Night only

Every generated level chooses **one** lighting preset at initialization: `Day` **or** `Night`. It remains constant during the complete level play session. **Do not implement** a clock, normalized time of day, changing sun angle, continuous cycle, dusk/dawn transition, `TransitionToNight` event, or transition Tick.

The controller (`ABalloonEnvironmentPresetController`, or an adapted existing level environment manager) applies an authored preset once after the map's lighting actors are available. Use existing DirectionalLight/SkyLight/SkyAtmosphere/PostProcess actors when present; do not duplicate project-owned environment systems.

```cpp
// Conceptual; confirm actual project API and Unreal version in M0.
UENUM(BlueprintType)
enum class EBalloonLightingPreset : uint8 { Day, Night };

// Initial configuration only; should not be called by timed gameplay events.
void ApplyInitialLightingPreset(EBalloonLightingPreset Preset);
EBalloonLightingPreset GetSelectedLightingPreset() const;
```

For design previews, permit changing the selected preset in editor and explicitly **Regenerating/Reloading** the environment; this is an authoring action, not gameplay day/night transition.

### 10.2 Ground per level

1. Use one sufficiently large ground plane with a material instance named `MI_<Level>_Ground`, **or** use the generator's/existing landscape if available.
2. Forest → `MI_Forest_Ground`; Field → `MI_Field_Ground`; City → `MI_City_Ground`; Desert → `MI_Desert_Ground` by default. Allow per-level override while retaining naming convention.
3. Plane/landscape must extend under generated playable corridor and any intended distant background objects. Sample its surface for `AlignToSlope` placement if relevant; the balloon stays on its fixed flight plane.
4. Ground is a single owned mesh/landscape reference, **not** an asset-row `Terrain` HISM by default. Do not duplicate an existing landscape actor on regeneration.
5. Ground needs normal world collision only where gameplay/physics demands it; this is distinct from scenery-HISM **NoCollision** rule.

### 10.3 Exponential Height Fog per biome (required defaults)

Use one existing or generator-owned `AExponentialHeightFog` component/actor per level. Apply biome values in `FLinearColor` terms (verify visual appearance with the project's post-processing and color pipeline) and set fog density. Do **not** assume a 0–255 sRGB tuple. Match the balloon's post-process look. The values below are **authoring defaults**; designers may override individual levels explicitly.

| Level / biome | Fog color (`R, G, B`, linear float) | Density | Artistic notes |
|---|---|---:|---|
| **Forest** | `(0.35, 0.45, 0.45)` blue-green | `0.03` | Hide mountain bases |
| **Field** | `(0.75, 0.70, 0.55)` warm haze | `0.015` | Soft distant contrast |
| **City** | `(0.40, 0.35, 0.28)` brown smog | `0.05` | Atmospheric pollution |
| **Desert** | `(0.85, 0.75, 0.60)` hot pale haze | `0.02` | Hot washed-out distance |

Apply to Exponential Height Fog's actual UE component properties, avoiding duplicate fog layers. The **Day/Night** lighting preset may modify exposure/light intensity but must retain that biome's authored fog color and density by default. Changes to environmental haze driven by gameplay events (if implemented) are *separate* effects and cannot secretly switch lighting preset.

### 10.4 Validation

- For each biome: check ground material name, existing landscape vs plane ownership, fog color/density, fog actor count, and NoCollision policy on decorative scenery.
- For each biome + Day/Night combination: test obstacle silhouettes, finish visibility, sky, fog and existing balloon post-process consistency.
- Verify environment properties do not vary between two gameplay timestamps when no explicit non-lighting weather effect occurs.
- Do not require or test mid-transition lighting; it does not exist in this design.

## 11. World realization, lifecycle, and scalability

### 11.1 Spawn ownership and cleanup

Every generated Actor or component must be tracked by the generator or a segment record. Prefer weak references for objects that can be destroyed externally; unsubscribe from all delegates on cleanup. Mark generated entities with a consistent owner/tag where useful, but **do not destroy arbitrary designer-placed actors merely because they share a tag**.

Lifecycle:

```text
Uninitialized
 -> ConfigValidated
 -> PlanBuilt
 -> RouteValidated
 -> AssetsReady
 -> ActorsRealized
 -> RuntimeActive
 -> Unloading/Clearing
 -> Uninitialized
```

On generation failure, preserve the last good level if `Regenerate` was called and the new candidate failed. Realization should be transactional where feasible: allocate new plan/spawns, confirm completion, then swap state and remove old instance. If full transaction is impractical, document the temporary visibility state and avoid exposing half-generated collisions to a live player.

### 11.2 Segment activation

**MVP:** realize the entire level for small sizes and track segment references. **P1 optimization:** add a segment lifecycle:

```text
Unplanned -> Planned -> LoadingAssets -> Active -> Dormant -> Unloaded
```

Maintain N segments ahead and M behind as a configurable budget. Start/finish content and global event state must remain valid even if their segments are not active. Do not tie logical generation to actor existence: plan descriptors persist independently and are authoritative for reactivation.

When a segment becomes active, spawn its actors using stored plan descriptors. Re-activation must reproduce the **same instance IDs**, not resample random positions. For actors that move/die during gameplay, store runtime deltas or explicitly designate them ephemeral; do not inadvertently respawn defeated enemies when revisiting a segment.

### 11.3 Engine partitioning and PCG integration

- For a **small to medium corridor**, ordinary generator-owned segments plus **HISM per static mesh, NoCollision** for Filler/Terrain/Background are required.
- For a **large authored world**, World Partition can stream authored landscape and set dressing; the BalloonLevel generator still owns its procedural gameplay state.
- For scenery art, UE PCG graphs may accept the generated spline/volume as inputs, **but must preserve DataTable category, HISM and rule invariants**. They are an optional visual layer and must respect protected route areas/critical collision volumes.
- Avoid generating significant collision-heavy Actors every frame. Batch operations or show a temporary generating state when necessary.

### 11.4 Required instancing and cull distances

For **Filler**, **Terrain**, **Background**, create or reuse one `UHierarchicalInstancedStaticMeshComponent` **per distinct Static Mesh** in the generated level. All its instances have `NoCollision`. A Blueprint Actor cannot be placed into a HISM; if the asset row is a Blueprint class, either reject it for these categories or provide an explicitly validated static-mesh extraction adapter—do not silently spawn N decorative actors.

| Scene group | HISM end cull distance (cm) | Requirements |
|---|---:|---|
| Small clutter | `15000` | 150 m; HISM + NoCollision |
| Short trees | `30000` | 300 m; HISM + NoCollision |
| Buildings and dunes **when decorative** | `80000` | 800 m; HISM + NoCollision |
| Mountains / horizon backdrop | **Never cull** | HISM + NoCollision; avoid distance culling |
| Gameplay `Obstacle` Actors | **Never cull** | Separate colliding actors; don't use HISM |
| Gameplay `Hazard` Actors | Prefer **never cull** if gameplay-significant | Separate colliding actors |

`CullProfile` is a derived authoring annotation from stable row name/tags or a separate metadata asset; it is **not an extra mandatory DataTable column**. Because the user requested **one HISM component per mesh**, resolve potential conflicts (same mesh used for two different cull profiles) through a catalog validation error or a documented unifying cull policy; do not silently create multiple HISMs for the same mesh. Set the HISM component's start/end cull distances using the actual engine API after confirming version behavior. For no distance culling, use the engine's documented disabled-distance-cull configuration and verify at runtime; don't assume one magic value without checking. Never make collision or gameplay reliability depend on the renderer's cull state.

`ring` mountains are intentionally overlapping background silhouettes; the overlap exemption does **not** waive NoCollision. `street` buildings that are actual blocking `Obstacle` rows are Actors and **never cull**—the 80,000-cm building limit applies only to decorative buildings.

### 11.5 Performance budgets (initial hypotheses; measure and revise)

| Metric | Initial target | Measurement |
|---|---|---|
| Full 1.5 km planning + validation | <= 500 ms on developer PC, excluding asset load | Scoped C++ timing / Unreal Insights |
| Initial interactive realization | No visible multi-second stall | PIE profiling, spawn counts |
| Event dispatch | No measurable recurring frame spikes | Timers and event profiler |
| Border runtime update | No frame hitch for normal single-group change | Capture worst-case update |
| Idle generation-system cost | Near-zero when no events/updates | `stat game`, Unreal Insights |
| Owned actor/component count | Within an agreed project budget | Per-segment debug counters |

These are **design targets, not achieved measurements**. Record actual hardware, level seed, counts, timings, and baseline. Prioritize correctness before optimization; only introduce parallelism after deterministic serial planning is proven.

### 11.6 Thread safety

Pure geometry/validation calculations may later run in worker tasks. Do not access/modify UObjects or spawn actors off the game thread. For MVP, keep execution on the game thread unless profiling demonstrates a need; structure pure plan computations so they can later be moved safely.

---

## 12. Editor and designer experience

The minimum tool needs:

1. A placed `BP_BalloonLevelGenerator` with `LevelConfig` editable in Details.
2. `Generate Preview`, `Clear Preview`, `Regenerate` functions callable via editor utilities or `CallInEditor` where valid.
3. Debug rendering toggles for spline, horizontal corridor, width, start/finish, segment boundaries, 2D obstacle footprints at fixed flight height, border radii, horizontal occupancy and validated route, plus HISM groups, cull ranges and biome placement-rule overlays.
4. Clear generation status and error diagnostics; errors include which rule, which segment, and how to fix the constraint.
5. Per-biome config, shared+biome DataTables, rule README, ground material mapping, fixed Day/Night preset, and a sample PIE level.
6. Optional `RandomizeSeed` as an intentional action (never automatically change an explicit seed when regenerating).

**Do not rebuild the entire world in `OnConstruction` on every property edit**. This can be expensive and dangerous for designer-owned assets. Provide an explicit button and (optionally) an opt-in debounced editor preview with cleanup.

### 12.1 Debug categories

```text
BalloonLevel.Spline
BalloonLevel.Segments
BalloonLevel.Bounds
BalloonLevel.BorderInfluence
BalloonLevel.Obstacles
BalloonLevel.Enemies
BalloonLevel.RouteSafety
BalloonLevel.BiomeCatalog
BalloonLevel.PlacementRules
BalloonLevel.HISM
BalloonLevel.Fog
BalloonLevel.Events
BalloonLevel.Performance
```

Color suggestions: base bounds cyan, border threats red/orange, solid obstacles gray, validated safe route green, start/finish gold. Colors must not be required for accessibility; use labels or distinct line styles.

### 12.2 Error examples

```text
BLG-001 InvalidConfiguration: FlightHeightMeters must be finite.
BLG-011 NoValidSpawnRule: Obstacles.Count=30 but all rules disabled/weight=0.
BLG-021 InsufficientSpace: 40 obstacles requested, only 32 valid placements
        after 8,000 bounded attempts; segment=4; rule=Tower.
BLG-030 Unreachable: No horizontal route from segment 2 to 3 at fixed Z.
BLG-041 BorderChangeRejected: Proposed radius increase would trap balloon.
BLG-051 MissingAsset: Soft class BP_FlyingEnemy_Bird failed to load.
BLG-061 InvalidAssetRow: DT_BalloonAssets_City, Row=Tower_01, Obstacle requires collision.
BLG-062 InvalidRule: Rule=ring is reserved for Forest mountain Background rows.
BLG-063 HISMProfileConflict: identical mesh assigned incompatible cull profiles.
BLG-064 InvalidLightingEvent: Day/Night is fixed and cannot be changed by events.
```

These error IDs are project-specific proposals, not engine errors. Keep numeric identifiers stable after introduction.

---

## 13. Implementation milestones and dependencies

**Rule:** Stop after each milestone, build/test, record actual results and a handoff. The coding LLM must not invent `.uasset` creation or UE MCP tool availability.

### M0 — Repository, Pawn and asset audit

- Locate the actual `.uproject`, module, UE version, Pawn/collision/movement systems, AI, renderer, fog/sky, ground materials, and editor/MCP/test tools.
- Verify what **horizontal-only maneuvers** means in current Pawn code. Identify any Z bobbing; record a conservative fixed-height collision envelope without inventing climb controls.
- Discover current assets and possible forest/field/city/desert catalogs, imported DataTables, Ground Materials and `BP_BirdFlock` if present.
- Write `DESIGN_DECISIONS.md` with true asset names/paths, project constraints and reproducible build command.

**Gate:** no fabricated dependencies; unknowns and blockers identified.

### M1 — Base types, DataTables, config, straight geometry

- Add exact `FBalloonLevelAssetRow` schema, shared/biome table pointers, category and rule validation, deterministic catalog merging.
- Add config and basic level generator, horizontal straight spline, segments, fixed flight-height datum, no vertical maneuver logic.
- Document table format and rule names in `Content/BalloonLevel/Rules/README.md` or adjacent source mirror.

**Gate:** duplicate row resolution, invalid fields, length/width, segment tests pass.

### M2 — Curvature, variable width, horizontal player progress

- Implement horizontal curved spline planner, width curve, deterministic seed, start/end, progression and self-cross checks.
- Test the absence of a vertical path axis; changing fixed Z is an authoring choice, not maneuvering.

**Gate:** geometry and progress tests pass.

### M3 — Enemy-shaped corridor boundaries

- Implement border influence descriptors and radius intrusion in XY, separate enemy/border counts, Blueprint hazard proxy and debug geometry.
- Integrate fixed-height overlap with the balloon collider.

**Gate:** radii change horizontal traversable area deterministically.

### M4 — Biome catalog and six placement rules

- Build `scatter`, `ring`, `line`, `street`, `wind_aligned`, `flock` planning functions with validated rule inputs and stable descriptors.
- Implement density per km², cluster sizes, spacing, scale, slope alignment and yaw rule precedence.
- Validate Forest mountains 1.5–3 km outside center, Field 3–6 flocks, City 90° grid buildings/lampposts, Desert shared dune yaw±10°.
- Implement optional interior enemy and obstacle planning with shared/biome catalog selection and exact-count override behavior.

**Gate:** all six rules pass deterministic geometry tests; optional gameplay actors can be turned off independently.

### M5 — Horizontal reachability, retries and runtime safety proof

- Build **2D** route lattice `(progress, lateral)` with full balloon 3D collision/sweeps at fixed Z; turning/inertia/drift checks.
- Test no high/low bypass, complete wall closure, narrow gap, failure diagnostics, fixed seed replay and retries.

**Gate:** no generated playable level is accepted without a horizontally reachable route.

### M6 — Time/progress/custom Blueprint events

- Implement event definitions, exact-once/order/cooldown/pausing, BP dispatch, optional enemy spawns and prevalidated border radius updates.
- Reject day/night transition event types; preserve immutable lighting preset during gameplay.

**Gate:** event tests and safety transaction pass.

### M7 — Fixed environment and biome atmosphere

- Bind ground plane or landscape and `MI_<Level>_Ground`.
- Apply **Day OR Night** preset once at load. Add no clocks or transition APIs.
- Configure forest/field/city/desert Exponential Height Fog values (section 10), respect existing post process.

**Gate:** 8 biome×preset combinations render as authored with stable lighting and expected fog.

### M8 — Realization, instancing, cull policy, editor workflow

- Implement HISM **one per mesh**, NoCollision for Filler/Terrain/Background; instantiate Obstacle/Hazard as separate colliding Actors; `flock` Blueprint Actor exception.
- Apply cull distances: clutter 15000cm, short trees 30000cm, decorative buildings/dunes 80000cm, mountains/obstacles never cull.
- Implement safe regeneration/cleanup, previews, biome/rule debug toggles and measured profiling.

**Gate:** actual UE checks confirm HISM counts, collision and culling; no duplicate regeneration artifacts.

### M9 — QA, API guide, sample maps and handoff

- Complete automated tests in section 14 and PIE scenarios for all four biomes, both fixed presets, all six rules, events and collision safety.
- Produce `API.md`, `TEST_REPORT.md`, `HANDOFF.md`, and DataTable-adjacent `README.md` with exact field/rule behavior; report unavailable editor assets honestly.

**Gate:** another developer can regenerate a level and reproduce tests from documentation.

### 13.1 Dependency graph

```text
M0 → M1 → M2 → M3 → M4 → M5 → M6 → M7 → M8 → M9
                   └─ Catalog/rule testing can be developed independently
                      once M1 types and config contracts are stable.
```

Keep edits to common config/types owned by one integrator; parallel agents may investigate rules, write isolated tests, or inspect environment assets.

## 14. Automated test matrix

Use UE Automation/Functional/PIE tests appropriate to existing project conventions. Test plan calculations without loading assets where possible; test actor/HISM realization separately.

| ID | Layer | Scenario | Expected |
|---|---|---|---|
| CFG-01 | Unit | Zero or negative length/width | Rejected |
| CFG-02 | Unit | Nonfinite fixed flight height, invalid turn radius | Rejected |
| CFG-03 | Unit | Missing valid rows for positive gameplay count | Named row/table diagnostic |
| CFG-04 | Unit | Invalid category/rule, scale interval, negative density, spacing, cluster size | Row rejected |
| GEO-01 | Unit | 1500m length, 250m segments | 6 segments |
| GEO-02 | Unit | 1620m length, 250m segments | 7 segments; last 120m |
| GEO-03 | Unit | Width variations and curved spline | Correct horizontal widths, no self-intersection |
| GEO-04 | Functional | Player attempts vertical maneuver supplied by generator | No vertical maneuver input or navigation option |
| RNG-01 | Unit | Same seed, catalog version and biome | Same plan hash |
| RNG-02 | Unit | Change obstacle count | Geometry/border streams remain stable |
| CAT-01 | Unit | Shared rows only | Present in Forest/Field/City/Desert |
| CAT-02 | Unit | Same row name in Shared and City | City row overrides; exactly one row |
| CAT-03 | Unit | Unsupported soft Blueprint-class path | Explicit resolution error |
| CAT-04 | Unit | Nonmatching asset type/category | Validation error |
| RULE-01 | Unit | scatter density and eligible km² | Deterministic target and spacing |
| RULE-02 | Unit | scatter ClusterSize>1 | Cluster grouping and MinSpacingM honored |
| RULE-03 | Unit | ring Forest backdrop | All mountain centers in annulus 1.5–3km from level center; outside playable; inward yaw; overlap allowed |
| RULE-04 | Unit | line fences/hedges | Chained with bounded gap error along field borders |
| RULE-05 | Unit | street grid | Snapped lots and yaw multiple of 90°; back-to-back buildings; lamppost edge placement |
| RULE-06 | Unit | wind_aligned dunes | One seed yaw, each within ±10° |
| RULE-07 | Unit/PIE | flock on Field | 3–6 `BP_BirdFlock` Actors, no decorative HISM substitute |
| BOR-01 | Unit | Border disabled | Zero threats |
| BOR-02 | Unit | Radius expansion into corridor | Safe XY space shrinks |
| BOR-03 | Unit | Fixed-height plane intersects sphere hazard | Horizontal projected footprint blocks |
| BOR-04 | Unit | Sphere sits entirely above/below fixed flight collision envelope | Does not block, **no high/low escape path** |
| OBS-01 | Unit | Zero obstacle count or disabled | Zero gameplay obstacle Actors |
| OBS-02 | Unit | Enough room for exact count | Exact number of actor descriptors |
| OBS-03 | Unit | Excessive density | Bounded `InsufficientSpace` failure |
| ENM-01 | Unit | Enemy system independently disabled | No interior enemy descriptors |
| RTE-01 | Unit | Horizontal open corridor | Reachable |
| RTE-02 | Unit | Collision wall cuts corridor at fixed Z | Unreachable, even if overflight would be open |
| RTE-03 | Unit | Winding XY corridor and insufficient turn radius | Unreachable |
| RTE-04 | Unit | Dynamic border expansion would trap current player XY | Update rejected/clamped |
| ENV-01 | Editor/PIE | Four biomes × Day/Night | Fixed presets only, correct material/fog |
| ENV-02 | Unit/PIE | Fixed lighting at two gameplay times | Identical preset; no cycle or transition |
| ENV-03 | Editor | Forest/Field/City/Desert fog | Exact colors and densities in section 10 |
| ENV-04 | Editor | Ground plane or existing landscape | Material `MI_<Level>_Ground`, no duplicate ground |
| INS-01 | Editor/PIE | Filler/Terrain/Background per mesh | One HISM per mesh, NoCollision |
| INS-02 | Editor/PIE | Obstacle/Hazard rows | Individual Actors with collision |
| INS-03 | Editor/PIE | Small clutter/short trees/buildings+dunes | End cull 15000/30000/80000cm |
| INS-04 | Editor/PIE | Ring mountains and Obstacle actors | Never distance-cull |
| EVT-01 | Unit | Large progress/time step | Due events triggered, not skipped |
| EVT-02 | Unit | Backtracking / restart / pause | Stable one-shot state and timer semantics |
| EVT-03 | Unit | Event requests Day/Night change | Rejected as invalid action |
| LIF-01 | PIE | Regenerate twice | No duplicate actors/components or leaked refs |
| PERF-01 | Profile | 1500m sample + distant ring | Measured CPU/memory/HISM/Actor counts; no unverified budget claim |

### 14.1 Seed regression suite

Record 3–5 pinned seeds per biome plus shared/biome DataTable version checksums. Hash *sorted, quantized plan descriptors* including selected mesh/class path, placement transform, category, rule and lighting preset; exclude pointer addresses and unstable UObject enumeration. An intentional layout change requires review, not automatic update of expected hashes.

### 14.2 Manual QA scenarios

- Fly a Forest level with mountains forming a closed visual horizon (1.5–3 km annulus); verify their bases are softened by blue-green fog and they never cull.
- Fly a Field level with chained hedges/fences and 3–6 live `BP_BirdFlock` Actors; verify decoration isn't counted as hostile enemies.
- Fly a City level: back-to-back blocks, lampposts beside roads, 90° yaw, collision on actual obstacle buildings only.
- Fly a Desert level: dunes align with one prevailing yaw ±10°, pale fog, NoCollision decorative dune HISM.
- Try to turn around a tall colliding obstacle; **never** climb/fall to bypass it through generator-created controls.
- Expand border radii near the balloon, verify safety rejection/commit before visual update.
- Test all four biomes in Day and Night separately; neither preset changes after event or elapsed time.
- Regenerate after editing Width/Length/biome; old instances, fog ownership and events clean up correctly.

## 15. Coding LLM orchestration — designed for 100k context

A 100k context window is enough to read this entire specification plus selected project files, but **not** a reason to ingest the whole repository or implement all milestones in one response. The recommended workflow is **one responsible coding agent**, optional narrow read-only helper agents, and a written handoff after every milestone.

### 15.1 Context budget

Treat 100,000 tokens as a **hard shared window for instructions, inputs, tool responses, edits, and generated output**. Use the following **working budget**, adjusting to the actual model/tool accounting:

| Context allocation | Suggested tokens | Purpose |
|---|---:|---|
| Task/specification and persistent instructions | 12,000 | This implementation document, coding policies, current milestone |
| Repo architecture and targeted source files | 28,000 | Module/build definitions, existing Pawn/AI/lighting interfaces, nearby code patterns |
| Current milestone design/code/test context | 25,000 | Files being edited, exact interfaces, current failures |
| Tool responses/compiler output | 12,000 | Truncated relevant errors, asset inspection, verification |
| Model working/output budget | 13,000 | Code edits, decisions, short rationale, status |
| Safety reserve | 10,000 | Unexpected context requirements, corrections, new compiler errors |
| **Total** | **100,000** | |

**Practical rules:**

- Keep approximately 10k tokens unused until finishing the milestone. A 100k model may have separate output-token limits; inspect model settings and do not assume the entire window is available for new output.
- **Load by dependency, not directory:** Read `.uproject`, module config, generator files, relevant Pawn collision code, and current tests. Pull in Blueprint asset metadata using available Unreal tooling, not binary dumps.
- Do not paste full generated code repeatedly into chat; use file-edit tools, then view diffs and bounded compiler/test output.
- Summarize long tool logs. Keep first error, preceding context, and root cause; stop re-reading hundreds of duplicate errors.
- Compact after every major milestone: update `HANDOFF.md`, reopen only the necessary source files and the next milestone.
- **Never rely only on a context summary to know current source code.** Re-open exact files before editing after a context reset.
- For a very small local model, subdivide M3–M5 further. For 100k context, one milestone per working session is sensible.

### 15.2 Persistent working files

Maintain these short documents during implementation (do not create until M0 determines the real repo layout):

**`Docs/BalloonLevel/HANDOFF.md`** — at most ~1–3 pages:

```markdown
# Handoff
Current milestone: M3
State: M0–M2 complete; M3 in progress
Engine/project build: <observed version and command>
Changed files: <paths>
Working APIs: <exact names/signatures>
Commands executed: <commands and observed results>
Test status: <pass/fail/not run>
Known issues: <actionable, highest severity first>
Next action: <single concrete task>
Do not change: <architecture decisions and constraints>
```

**`DESIGN_DECISIONS.md`** — append short decisions when the spec permits alternatives:

```text
Decision ID, question, chosen approach, why, tradeoff, date, affected files.
```

**`TEST_REPORT.md`** — append reproducible verification records:

```text
Milestone, command, toolchain/environment, result, failure logs (short), status.
```

### 15.3 Tools policy

The coding LLM may use local shell, Git, Unreal Editor automation, a UE MCP server (if connected), build tools, and test runners **only if those capabilities are actually available**.

- Tool availability must be discovered first; do not hallucinate Unreal MCP endpoints, Blueprint node names, editor menus, source files, or successful engine commands.
- For `.uasset` creation/modification, prefer Unreal Editor, an available supported MCP integration, or documented editor scripting. Do not attempt to write binary `.uasset` files directly in a text editor.
- If the engine is unavailable, implement pure source files/tests as far as possible, clearly mark `UNVERIFIED_WITH_ENGINE`, and give precise manual steps to finish. Do not equate static syntax checks with UE compilation.
- Commit/source-control changes only when user instructions or repository workflow allow it. Do not overwrite user assets, remove unrelated code, or perform sweeping refactors without necessity.
- Inspect `git status`/diff before and after each milestone to isolate modifications.

### 15.4 Output contract at the end of every milestone

Every implementation milestone response should include these sections concisely:

```text
1. Implemented: exact feature subset, not broad claims.
2. Files: changed/created paths (no nonexistent paths).
3. Public API/Blueprint exposure: key classes and how to use them.
4. Verification: exact commands run and observed pass/fail outputs.
5. Remaining issues/limitations: prioritized, including missing editor assets.
6. Handoff: what was recorded in HANDOFF.md.
7. Next milestone: ONE next step with prerequisites.
```

Do not silently skip a failed test to proceed. If blocked, make the smallest source-level advance possible and write an actionable blocker report.

### 15.5 Master implementation prompt — copy into the coding LLM

```text
ROLE
You are a senior Unreal Engine gameplay/tools engineer working with a
100,000-token context window. Implement the Balloon Level procedural
system in small compile-verified milestones.

SOURCE OF TRUTH
Read Balloon_Level_Generation_LLM_Implementation_Plan.md in full, then
audit the actual project/source/tools. This specification is version 1.1.
Engine target UE5.8 unless project evidence differs. Example signatures
and paths are contracts, not proof of existing classes/assets.

NONNEGOTIABLE GAMEPLAY
- Level is indirectly linear, with alternate HORIZONTAL flight paths.
- The balloon CANNOT maneuver vertically. No altitude steering, 3D A*,
  climb/descent pathfinding, high/low obstacle bypass, or ascent events.
- Level length and width are editable. Borders are enemies/hazards whose
  radii deform horizontal playable space. Radius events are prevalidated.
- Enemies and obstacles are optional; both can be shared across biomes
  or selected specifically for Forest, Field, City, Desert.
- Lighting is a FIXED Day OR Night preset per level. NEVER implement
  a day/night cycle, transition, or event that swaps presets.
- The level has ground plane/landscape with MI_<Level>_Ground and
  Exponential Height Fog per section 10's exact biome table.

ASSET DATATABLE
Use EXACT designer-facing fields: Asset, Category, Collision,
DensityPerKm2, ScaleMin, ScaleMax, MinSpacingM, RandomYaw,
AlignToSlope, ClusterSize, Rule, Notes. Asset is a soft static-mesh
reference or Blueprint-generated Actor-class soft path.
Merge shared and biome-specific rows by stable row name; biome overrides.

PLACEMENT RULES
scatter: density per km², spacing, clusters.
ring: Forest mountain silhouettes, radius 1.5–3km from level center,
  outside playable space, face inward, overlap, never cull.
line: Field fences/hedges chain along field borders.
street: City street grid, yaw 90° increments, back-to-back buildings,
  lampposts along streets.
wind_aligned: Desert dunes use one per-level yaw ±10°.
flock: Field spawns 3–6 BP_BirdFlock Actor groups.
Document these rules in README beside the DataTables (or matching
source mirror) and with relevant code comments/tooltips.

REPRESENTATION & CULLING
Filler/Terrain/Background => one HISM per StaticMesh, NoCollision.
Obstacle/Hazard => separate colliding Actors.
flock => explicit BP_BirdFlock Actor exception.
HISM cull end distances: clutter 15000cm; short trees 30000cm;
decorative buildings/dunes 80000cm; mountains NEVER cull.
Obstacle Actors NEVER cull. Keep gameplay and visual culling separate.

EXECUTION
1. Complete M0 audit first. STOP before M1.
2. Then follow M1→M9 one milestone at a time: plan, implement,
   compile, test, inspect diff, update HANDOFF and TEST_REPORT.
3. Inspect the actual balloon Pawn, collision envelope, materials,
   biome assets, BP_BirdFlock, Unreal/MCP/editor tool availability.
4. NEVER claim tests, builds or UE asset creation without tool evidence.
5. Use deterministic plan→validate→realize with independent seed streams.
6. Maintain an explicit horizontal reachability proof at fixed flight Z.
7. Keep unrelated game files intact, avoid modifying source outside scope.
8. Resolve ambiguities with minimal reversible design and log in
   DESIGN_DECISIONS.md; do not silently weaken requirements.

FIRST TASK
Perform ONLY M0 repository audit. Report actual UE module, target
version, Pawn movement/collision, catalog/asset paths, available editor
and test tools, lighting/ground setup, potential blockers, real commands,
and the file plan for M1. Do not write gameplay implementation yet.
```

### 15.6 Milestone prompts — copy one at a time

#### M1 — Config, DataTable and minimal generator

```text
Read current HANDOFF and specification sections 3–5 + M1. Implement
exact FBalloonLevelAssetRow fields, soft mesh/Blueprint-class path
validation, shared+biome table merge, config, straight horizontal
spline, segment plan and statuses. Add README beside tables documenting
all six rules. Compile/test/diff, update HANDOFF; stop at M1.
```

#### M2 — Curved horizontal spline and progress

```text
Read HANDOFF and sections 5, 8, M2. Implement deterministic curved
XY geometry, width profile, progression and start/finish. Balloon has
fixed flight Z: no vertical control/path axis. Add tests, compile,
report results and hand off. Stop.
```

#### M3 — Border enemies

```text
Read HANDOFF and section 6 + M3. Implement enemy-shaped border XY
influence regions, exact optional count, variable radii, collision
policy at fixed flight Z, Blueprint extension points and debug draws.
Test determinism/intrusion; compile, hand off and stop.
```

#### M4 — Biome catalog and six placement rules

```text
Read HANDOFF and sections 4, 7, 11.4, M4. Implement deterministic
scatter, ring, line, street, wind_aligned, flock planners with exact
row schema, biome override, density/spacing/cluster/scale/yaw/slope,
actor vs HISM descriptors, rule-specific diagnostics. Ring: Forest
1.5–3km, inward, overlapping. Flock: Field 3–6 BP_BirdFlock Actors.
Independent optional obstacle/enemy counts. Add tests for all six
rules and row errors. Compile/test, update README and hand off. Stop.
```

#### M5 — 2D route safety

```text
Read HANDOFF and section 8 + M5. Check actual Pawn movement envelope.
Implement ONLY horizontal 2D lattice at fixed Z, but collide against
full 3D balloon shape/obstacles. A* in XY/progress-lateral, turning,
wind buffer, sweeps, deterministic bounded retries and diagnostics.
Reject routes requiring ascent/descent. Test wall closure, narrow gap,
sharp turn and border changes. Compile/test, hand off. Stop.
```

#### M6 — Timed gameplay events

```text
Read HANDOFF and sections 6.3, 9, M6. Implement elapsed time,
progress and custom signals with Blueprint receivers, pause/restart,
exact-once state and validated border-radius changes. No lighting
preset transition action. Reject any event requesting Day/Night swap.
Run event and runtime-safety tests, compile and hand off. Stop.
```

#### M7 — Fixed Day/Night, ground and fog

```text
Read HANDOFF and section 10 + M7. Audit existing environment Actors.
Apply exactly one fixed Day or Night lighting preset at level load,
not a transition. Add ground plane or existing landscape with
MI_<Level>_Ground; apply Forest/Field/City/Desert ExponentialHeightFog
color/density exactly as table. Verify post-process compatibility,
cleanup/ownership and 8 biome×preset combinations if editor access
exists. Compile/test, hand off. Stop.
```

#### M8 — HISM/Actors, culls and editor integration

```text
Read HANDOFF and sections 7, 11–12, M8. Realize categories:
Filler/Terrain/Background = one HISM per mesh + NoCollision;
Obstacle/Hazard = independent colliding Actors; flock = BP actor.
Set HISM end cull: clutter 15000, short trees 30000, decorative
buildings/dunes 80000, mountains no cull; obstacles no cull.
Add owned cleanup, debug views, editor preview and profile actual
results; no invented .uassets. Compile/test, hand off. Stop.
```

#### M9 — Tests and documentation

```text
Read HANDOFF and sections 1, 14, M9. Run available tests/PIE checks
for horizontal routes, biome merging, six placement rules, HISM and
actor collision, cull policy, fog/ground, fixed Day/Night, events,
regeneration and deterministic seeds. Deliver API.md, adjacent table
README.md, TEST_REPORT.md and HANDOFF.md. Mark unavailable engine
verification explicitly and do not claim unrun tests. Stop.
```

### 15.7 Multi-agent policy (optional)

For an orchestrator that can spawn subagents, assign tasks by **ownership boundaries**:

| Agent | Role | Allowed changes |
|---|---|---|
| `Integrator` | Main responsible coding agent, architecture, merge, builds | All feature files; exclusive edit authority for shared interfaces |
| `GeometryResearch` | Read-only math/Unreal spline API investigation | Notes/test proposals; no central code edits |
| `TestEngineer` | Build targeted test scenarios after interfaces are fixed | `Tests/` only unless Integrator approves |
| `BiomeEnvironmentResearch` | Inspect fixed day/night presets, materials, fog, DataTables | Notes/configuration suggestions |
| `PlacementRulesEngineer` | Implement only isolated rule planners after M1 contracts freeze | Separate rule files/tests, no central types |
| `PerformanceReviewer` | Inspect profiles and suggest optimization | Review notes; avoid unmeasured rewrites |

Rules: do not have two agents edit `BalloonLevelTypes.h`, config, generator, or shared contracts concurrently. Use distinct branches/worktrees if separate agents must write in parallel; merge only after compile/tests. With limited local compute, a single sequential agent is preferable to simultaneous expensive model calls.

### 15.8 How to recover after context compaction/restart

The new working session should load **only**:

1. This spec's **objective, invariants, and current milestone**.
2. `HANDOFF.md` and `DESIGN_DECISIONS.md`.
3. Relevant `.h`/`.cpp` files in the current dependency chain.
4. Relevant tests and the shortest current compiler/test failure logs.
5. `git diff`/status to identify unfinished changes.

Then **verify** the summary against actual files and re-run the relevant tests before editing. The LLM should not infer stale API signatures from memory or continue a partially applied patch blindly.

---

## 16. Risk register and mitigation

| Risk | Why it matters | Mitigation | Priority |
|---|---|---|---|
| Generator assumes altitude escape | Directly violates horizontal-only maneuvering | 2D graph and 3D collision queries at fixed Z; explicit no-vertical tests | P0 |
| Border radius traps the balloon | Reshapes path at runtime | Validate candidate from current XY before commit | P0 |
| Overly curved/self-crossing spline | Progress mapping ambiguous | Bounded horizontal curvature and local projection | P0 |
| Density/count impossible to satisfy | Infinite loops, missing assets | Bounded attempts and row-specific diagnostics | P0 |
| Shared/biome double spawn | Duplicate scenery/enemies | Row-name override, stable ordering | P0 |
| Wrong type in single soft `Asset` field | Blueprint class path interpreted as mesh | Type resolver with explicit failures and editor validation | P0 |
| HISM collision enables unintended blocking | Visual content blocks gameplay | Mandatory NoCollision for non-gameplay categories; tests | P0 |
| Collider accidentally omitted from Obstacle/Hazard | Balloon clips through threat | Always individual Actors + collision profile validation | P0 |
| Ring horizon has large gaps | Forest scene lacks enclosing backdrop | Annulus/inward yaw/overlap validation and screenshot checks | P1 |
| Same mesh needs conflicting cull profiles | One HISM per mesh invariant broken | Catalog validation/explicit unified policy | P1 |
| Fog conflicts with existing post process | Washed-out/too dark render | Inspect existing environment and compare all eight biome/preset combos | P1 |
| LLM implements day/night transitions | Feature bloat and contradiction | Fixed enum/preset only; reject lighting events | P0 |
| Flock counted as enemy | Wrong gameplay population | Distinct ecosystem actor category/semantics | P1 |
| Editor regeneration destroys authored landscape/fog | Destructive cleanup | Generator-owned refs, preserve external actors | P0 |
| UE APIs/paths differ from docs | Compile/runtime errors | M0 audit and compile/test each milestone | P0 |
| 100k context window overfilled | Lost constraints, wasted turns | Handoff, targeted source reads, output/verification reserve | P0 |
| Made-up build or asset verification | False completion report | Evidence-based TEST_REPORT / `UNVERIFIED_WITH_ENGINE` | P0 |

## 17. Deliverable checklist

### C++ architecture

- [ ] Data Asset with biome, dimensions, fixed flight height, fixed light preset and seed
- [ ] `FBalloonLevelAssetRow` with exact 12 fields requested
- [ ] Shared/biome DataTable loader with deterministic override
- [ ] Horizontal spline and variable width/segments
- [ ] Enemy border influence radii and prevalidated runtime changes
- [ ] Independent optional enemy and obstacle count controls
- [ ] Six placement rules (`scatter`, `ring`, `line`, `street`, `wind_aligned`, `flock`)
- [ ] Horizontal route validator using 3D balloon collision at fixed Z
- [ ] Timed/progress/custom Blueprint events, no lighting-transition event
- [ ] Fixed Day / Fixed Night lighting selection
- [ ] Ground plane/landscape with `MI_<Level>_Ground`
- [ ] Per-biome Exponential Height Fog presets
- [ ] HISM per static mesh and mandatory NoCollision on Filler/Terrain/Background
- [ ] Separate colliding Actors for Obstacle and Hazard, `BP_BirdFlock` actor exception
- [ ] Required cull policy (15000/30000/80000cm; mountain/obstacle never)
- [ ] Deterministic planning, diagnostics, owned cleanup and regeneration

### Asset authoring and editor

- [ ] `DT_BalloonAssets_Shared`
- [ ] `DT_BalloonAssets_Forest`, `DT_BalloonAssets_Field`, `DT_BalloonAssets_City`, `DT_BalloonAssets_Desert`
- [ ] Adjacent `README.md` documenting all six rule semantics and category policy
- [ ] `MI_Forest_Ground`, `MI_Field_Ground`, `MI_City_Ground`, `MI_Desert_Ground` or mapped project equivalents
- [ ] Day/Night fixed lighting preset assets or existing actor references
- [ ] Environment fog authoring per biome
- [ ] `BP_BirdFlock` integration or explicitly documented missing asset
- [ ] Editor generation, preview, clear and debug tools
- [ ] Sample map for each biome, or a single switchable demonstration map

### QA and handoff

- [ ] Automated tests from section 14
- [ ] PIE smoke test for all four biomes and both lighting presets
- [ ] Verified no vertical flight maneuvers introduced by generator
- [ ] HISM collision/cull/component count tests
- [ ] Terrain alignment, City snapping, Forest horizon and Field flocks visual QA
- [ ] Timed event and border safety tests
- [ ] Reproducible seed/layout report
- [ ] Performance measurements with observed hardware
- [ ] Updated `API.md`, `DESIGN_DECISIONS.md`, `TEST_REPORT.md`, `HANDOFF.md`

## 18. Implementation notes from current Unreal documentation

The following official documentation pages can help the coding LLM confirm integration options **against the actual installed engine version**. They are **references**, not permission to assume that optional plugins are enabled:

1. **Unreal Engine 5.8 — Random Streams**: repeatable seeded random generation in Blueprints.  
   <https://dev.epicgames.com/documentation/unreal-engine/random-streams-in-unreal-engine>
2. **Unreal Engine 5.8 — World Partition**: distance-based streaming of authored world cells via streaming sources; distinguish this from generator-owned runtime actors.  
   <https://dev.epicgames.com/documentation/en-us/unreal-engine/world-partition-in-unreal-engine>
3. **Unreal Engine 5.8 — Procedural Content Generation Framework**: optional PCG system for artist-facing environment population.  
   <https://dev.epicgames.com/documentation/unreal-engine/procedural-content-generation-framework-in-unreal-engine>
4. **Unreal Engine 5.8 — PCG generation modes**: non-partitioned, partitioned, and runtime generation approaches.  
   <https://dev.epicgames.com/documentation/en-us/unreal-engine/using-pcg-generation-modes-in-unreal-engine>

**Final implementation rule:** Prioritize a correct, deterministic, validated **horizontal** flight corridor and biome-authored scenery over procedural spectacle. A level that looks interesting but traps the balloon is a failed generation. A clean straight corridor that proves the architecture is a valid first milestone.
