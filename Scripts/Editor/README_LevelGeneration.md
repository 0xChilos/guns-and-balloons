# Level generation: editor scripts

Reference: [API](../../docs/tech/BalloonLevel/API.md), [pipeline](../../docs/tech/level-generation.md), [status and known issues](../../docs/tech/BalloonLevel/STATUS.md).

`/Game/Scenes/MainLevel` is the gameplay and test level. Its `ABalloonLevelGenerator` references the shared `BP_Balloon`, uses **Every Level Start** and **Randomize Seed**, and lists the eight configs `DA_Environment_<Forest|Field|City|Desert>_<Day|Night>` under `/Game/Blueprints/Tests/Environment`. Plans are transient: generate again after opening the map, or press Play.

## Scripts (run in the editor: `py G:/UE/gunsandballoons/Scripts/Editor/<script>`)
| Script | Purpose |
| --- | --- |
| `setup_biome_asset_catalogs.py` | Reimports `Content/BalloonLevel/Rules/BalloonAssets_*.json` into the native `DT_BalloonAssets_*` tables and binds them to the eight configs. |
| `setup_level_environment.py` | Binds environment, ground, fog and lights for the generator and checks all biome/preset combinations. |
| `setup_biome_skies.py` | Creates the sky materials under `Content/BalloonLevel/Skies`. |
| `test_biome_asset_catalogs.py` | Headless matrix: every biome and preset generates shared and biome meshes and obstacles. |
| `test_generated_level_borders.py` | Seeded border layouts, rocket balloon ranges, replacement, rollback and Clear. |
| `test_level_generation_pie.py` | PIE smoke test: Ready from `BeginPlay`, eight borders, possession, flight. Logs `LEVEL_GENERATION_PIE_PASS`. |
| `verify_level_enemies.py` | Generated enemies per biome. |

Native automation tests: `powershell -ExecutionPolicy Bypass -File Scripts/Verify-BalloonLevel.ps1` (close the editor first).

## Manual acceptance checklist (visual checks are not automated)
1. For each of the eight configs, start a fresh PIE session (a runtime biome/preset change is rejected by the environment controller).
2. Confirm the generator reports `Ready` and the environment controller reports the expected biome and preset.
3. Ground uses `MI_<Biome>_Ground`, with no gaps under the route.
4. Fog: Forest blue-green (0.03), Field warm pale (0.015), City brown smog (0.05), Desert sand (0.02). The Start Options fog density overrides these.
5. Fixed Day/Night sun, color and brightness stay stable during flight. Night stays readable through the player camera.

All lighting, fog and distance values are prototype tuning (TBD).
