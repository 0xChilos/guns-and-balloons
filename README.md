| Folder | Content |
| ------ | ------- |
| `Animations` | Animation sequences, montages, blend spaces, animation Blueprints |
| `Assets` | Static meshes, skeletal meshes, 3D models |
| `Audio` | Sound effects, music, dialogue, MetaSounds |
| `Blueprints` | Actor Blueprints, components, interfaces, gameplay logic |
| `Data` | Data assets, tables, curves, structs and enums |
| `Input` | Enhanced Input assets |
| `Materials` | Materials, material instances and functions |
| `Scenes` | Maps, levels, level sequences, lighting setups |
| `Textures` | Surface textures, masks, normal maps |
| `UI` | Widgets, menus, HUDs, UI graphics |
| `VFX` | Niagara effects, particles, visual-effect materials |

## Documentation

Game design is in [docs/](docs/README.md). Instructions for AI agents are in [AGENTS.md](AGENTS.md).

## Main gameplay level

Open `/Game/Scenes/MainLevel` and press Play. It is the main gameplay level (formerly
`LevelGeneration_Test`): the shared `BP_Balloon` carries the auto-possessed first-person
pawn, and the level generator builds the route, borders, scenery and environment at every
level start. Click the viewport to capture keyboard input: W/S change target speed per
press; hold A/D to turn; use the mouse to look. The old PlayerStart and the test AmmoCrate
were removed. The original `BalloonFlight_Test` is unchanged.

`Scripts/Editor/create_level_generation_test.py` is the historical script that created the
sandbox (now pointing at `MainLevel`); it refuses to overwrite an existing map.

## Native procedural planner (M4)

The C++ BalloonLevel generator now plans the biome catalog and six placement rules,
with independent optional obstacle/enemy budgets and deterministic Actor/HISM
**descriptors**. It does not yet realize content or prove a safe flight route.
Authoring details: [catalog and rules](Content/BalloonLevel/Rules/README.md).
Build/test evidence and next milestone: [handoff](docs/tech/BalloonLevel/HANDOFF.md).
Run `powershell -NoProfile -ExecutionPolicy Bypass -File Scripts/Verify-BalloonLevelM4.ps1`
for the isolated UE 5.9 build and M1–M4 automation. Full project integration is blocked
by existing merge markers in `FirstPersonCameraPawn.h`.

M4 was re-verified on 2026-11-15: clean isolated compilation and all four M1–M4
automation tests passed. The full-project build reproduced the Pawn header blocker.
See the handoff for current report and log paths. Stop before M5.
