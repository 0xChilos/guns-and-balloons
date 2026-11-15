# Level generation preview

## Shared and biome mesh catalogs (2026-11-15)

All nine preview configs now bind `DT_BalloonAssets_Shared` plus the matching
Forest/Field/City/Desert native table under `/Game/BalloonLevel/Rules` and enable
scenery and mesh obstacles. Shared rocks spawn in every biome; the biome supplies
its own mesh palette. Setup: `Scripts/Editor/setup_biome_asset_catalogs.py`.
Sources, tuning and remaining animated/functional actor limits are documented in
`Content/BalloonLevel/Rules/README.md`. All eight biome/preset combinations passed
the headless matrix check (`Scripts/Editor/test_biome_asset_catalogs.py`), including
shared meshes, biome meshes, absence of foreign biome scenery, and mesh obstacles.
Results: `Saved/BiomeAssetCatalogChecks.json`. Visual testing is left to the user.

## Curved route and generated borders (2026-11-15)

`MainLevel` generates eight rocket balloons on both edges of its curved
corridor with every generation, including BeginPlay. `RandomizeSeed` is enabled:
each generation picks new geometry and border positions. Disable it to reproduce
the config's seed. The fixtures use a 500 m route, 600 m corridor, 25 degree heading
limit and 100 m minimum turn radius. Flight height is 100 m to clear rolling ground.
These are preview tuning values, except the approved 600 m corridor width.

Configure the fixture and migrate away from the old saved markers with:

```text
py G:/UE/gunsandballoons/Scripts/Editor/setup_curved_level_borders.py
```

The script sets the config's `BorderActorClass` to `BP_RocketBalloon`, enables
randomized seeds, removes old authored markers, verifies generated actor counts,
and saves the settings. Borders are transient generator-owned output. Regenerate
replaces them; Clear and deletion remove them. Failed regeneration preserves the
previous level. Influence proxies are registered automatically for border events;
validated radius changes also update the rocket's engagement radius. Drift is
disabled to keep the rocket within its certified influence. The proxy supplies
collision; decorative rocket meshes do not add unplanned collision blockers.
An empty BorderActorClass creates influence proxies without rocket visuals.

Run `py G:/UE/gunsandballoons/Scripts/Editor/test_generated_level_borders.py` for
nonvisual checks of seeded layouts, randomized positions, actor/range matching,
replacement, rollback and Clear. Visual acceptance is left to the user.

Verification: full project build passed; all 12 BalloonLevel native tests passed.
The separate headless integration check passed with zero errors/warnings, covering
fixed-seed replay, fresh random layouts, actual proxy/rocket counts and ranges,
replacement, failure rollback and Clear. Build and integration logs are in
`Saved/Logs/GeneratedBorders-Build.log` and `GeneratedBorders-Integration.log`.
The PIE smoke test also passed with eight generated proxies, eight rocket balloons,
Ready status, possession and 300.6 cm travel over three seconds. No visual test was performed.
Startup correction: the map's generator was set to Manual, which the earlier
smoke test masked by calling GenerateLevel itself. The map and setup script now
explicitly select Every Level Start. The smoke test requires Ready and eight
actual border balloons from BeginPlay without invoking generation.

## M7 environment integration (2026-11-15)

`MainLevel` now contains `LevelGeneration_Environment`, bound to
`LevelGeneration_Preview`, `Ground`, the existing DirectionalLight/SkyLight,
and a persistent `LevelGeneration_Fog`. Both lights are Movable. The ground
plane covers the selected config's route envelope. The generator uses
**Every Level Start**, so each PIE session selects and applies its environment.
Post process settings are preserved. Lighting presets remain prototypes requiring tuning.

The environment gets **BiomeId** and **Preset** from the generator's config.
After editing/selecting a config, click **Preview Environment** on
`LevelGeneration_Environment` to apply it without generating a route, or click
**Generate Level** / **Regenerate Level** on the generator to update both.
Environment application is part of the shared `GenerateLevel` success path,
including Blueprint calls, regeneration and Every Level Start. No separate
environment action is needed. Failed generation preserves the previous environment;
environment application errors appear in generator diagnostics.
In Random mode generate first so the selected config is known. During play the
successful generation's config snapshot is used once; editing a source asset
does not change lighting in an active session. Fallback values apply only when
the environment has no generator assigned.

Reapply/verify the setup with `py G:/UE/gunsandballoons/Scripts/Editor/setup_level_environment.py`.
The script checks all four ground materials, fog density/color, sun rotation,
sun intensity and sky intensity for Day and Night, then saves the selected
config's preview. Results: `Saved/M7-environment-checks.json`.
It sets Every Level Start and resizes/recenters the ground for the selected config.

Manual acceptance checks (visual testing deferred at the user's request):

1. Select `LevelGeneration_Preview` and use **Selected Config**. Set **Level
   Config** to one of the eight saved assets under
   `/Game/Blueprints/Tests/Environment`: `DA_Environment_<Biome>_<Day|Night>`.
   Forest Day is assigned by default. All eight are also listed in Random Biome
   Configs for convenient reference, but keep Selected Config for ordered testing.
2. Start a fresh PIE session for each of the eight combinations. Confirm the
   generator is Ready and the environment controller reports the expected
   applied biome/preset. Stop PIE between combinations: runtime preset changes
   are intentionally rejected with BLG-064.
3. Check that ground uses `MI_<Biome>_Ground`, has no gaps beneath the route,
   and remains readable through the balloon's existing post process.
4. Check fog: Forest blue-green (density .03), Field warm pale (.015),
   City brown smog (.05), Desert sand (.02). Check horizons and obstacle visibility.
5. Check fixed Day/Night sun direction, color and brightness; sky illumination
   should match the preset and remain stable during flight, with no cycle.
   Evaluate Night visibility and automatic exposure through the player camera.

These eight environment fixtures inherit geometry from `DA_LevelGenerationPreview`
and disable scenery, obstacles and enemies to isolate M7 ground/fog/lighting.
They do not provide full biome scenery or hazard catalogs. Recreate missing
fixtures with `py G:/UE/gunsandballoons/Scripts/Editor/create_environment_test_configs.py`.
The script saves the assets, assigns Forest Day, and selects the generator.

Editor property checks passed for all eight combinations. The attempted PIE
matrix stopped before play due to a test-script enum error; no PIE acceptance
or visual result is claimed. A fresh build attempt hit the open editor's
hot-reload DLL lock; verification used the existing compiled controller.

Open `/Game/Scenes/MainLevel`. Both test maps use instances of the
shared `/Game/Assets/Balloon/BP_Balloon`. Edit that Blueprint's components and
compile/save it to update both levels. Keep gameplay tuning in the Blueprint;
only placement and rider attachments belong to each map. The old
`BP_LevelGenerationBalloon` asset is unused by this setup.

The camera/player is embedded in `BP_Balloon` as the `PlayerCamera` child actor
(`BP_BalloonPlayer`). Adjust its seat position in the balloon Blueprint. Do not
place another player Pawn in these maps.

The placed `LevelGeneration_Preview` actor references that balloon and
`/Game/Blueprints/Tests/DA_LevelGenerationPreview`. Click **Generate Level** or
**Regenerate Level** in its Details, then inspect Status and Diagnostics.
Plans are transient, so generate again after reopening the map.

To rebuild the fixture and draw its boundaries, border spheres and green route
for ten minutes, run this in the editor console:

```text
py G:/UE/gunsandballoons/Scripts/Editor/setup_level_generation_preview.py
```

The script resets the preview config: seed 12345, straight 500 m route, 200 m
width, absolute flight Z 100 m, eight borders, no placement catalog. These are
test fixture values requiring tuning, not approved gameplay parameters.
No scenery or obstacle actors are spawned by this planning milestone.

To verify generation and actual flight in PIE (the script ends PIE afterward):

```text
py G:/UE/gunsandballoons/Scripts/Editor/test_level_generation_pie.py
```

Success logs `LEVEL_GENERATION_PIE_PASS`: Ready, eight borders, attached player
possession, original balloon controls, and more than 100 cm travel in three seconds.

Generation controls (after rebuilding the project): select `LevelGeneration_Preview`
and open **Balloon Level > Generation** in Details.
- **Generation Mode**: **Manual (Button)** uses Generate Level / Regenerate Level;
  **Every Level Start** generates automatically on each BeginPlay (including PIE).
- **Biome Selection**: **Selected Config** uses Level Config; **Random** picks a
  valid entry from **Random Biome Configs**. Assign one DataAsset per biome with
  its matching BiomeId, asset tables and placement metadata. An empty list fails
  with a diagnostic; null entries are ignored and duplicate configs count once.
- Enable **Randomize Seed** for fresh layouts; leave disabled to use the config seed.
  Active Config shows the selected biome and effective seed without editing assets.

Manual plans are transient and are not baked into maps or copied into PIE.
Runtime/manual Blueprint calls use the same Generate Level API. These controls
plan and validate routes; they do not add world realization or scenery spawning.


M8 source implementation now realizes transient HISM/Actor preview content through the same Details buttons. Clear Generated Level removes tracked generated content; failed regeneration retains the prior successful level. Draw Generation Debug exposes route, spline, segment, placement and HISM toggles. Final-source build, automation and visual checks are pending after the user-requested stop; see docs/tech/BalloonLevel/HANDOFF.md. Earlier descriptor-only statements above describe the pre-M8 fixture.
