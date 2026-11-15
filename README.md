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

## Level generation sandbox

Open `/Game/Scenes/LevelGeneration_Test` and press Play. The balloon carries the
auto-possessed first-person pawn. Click the viewport to capture keyboard input:
W/S change target speed per press; hold A/D to turn; use the mouse to look.
The map has lighting and a large ground reference, with the old flight-test
obstacles removed to leave space for generation experiments. It does not include
a level generator yet. The original `BalloonFlight_Test` is unchanged.

`Scripts/Editor/create_level_generation_test.py` creates and checks this map in
Unreal Python; it refuses to overwrite an existing sandbox.

## Документація

Дизайн гри — у [docs/](docs/README.md). Інструкції для AI-агентів — [AGENTS.md](AGENTS.md).

## Native procedural planner (M4)

The C++ BalloonLevel generator now plans the biome catalog and six placement rules,
with independent optional obstacle/enemy budgets and deterministic Actor/HISM
**descriptors**. It does not yet realize content or prove a safe flight route.
Authoring details: [catalog and rules](Content/BalloonLevel/Rules/README.md).
Build/test evidence and next milestone: [handoff](docs/tech/BalloonLevel/HANDOFF.md).
Run `powershell -NoProfile -ExecutionPolicy Bypass -File Scripts/Verify-BalloonLevelM4.ps1`
for the isolated UE 5.9 build and M1–M4 automation. Full project integration is blocked
by existing merge markers in `FirstPersonCameraPawn.h`.
