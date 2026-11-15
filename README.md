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

Generator setup and editor checks: [Scripts/Editor/README_LevelGeneration.md](Scripts/Editor/README_LevelGeneration.md).

## Procedural level generation

`ABalloonLevelGenerator` plans a curved corridor, proves a fixed-height flight route, then realizes
terrain, scenery, obstacles, border balloons, loot and enemies; fixed Day/Night and fog come from the
environment controller. Menus start a level through `BalloonLevelLaunchSubsystem::StartLevel`.
- API reference: [docs/tech/BalloonLevel/API.md](docs/tech/BalloonLevel/API.md)
- Pipeline and rules: [docs/tech/level-generation.md](docs/tech/level-generation.md)
- Status and known issues: [docs/tech/BalloonLevel/STATUS.md](docs/tech/BalloonLevel/STATUS.md)
- Asset tables and placement rules: [Content/BalloonLevel/Rules/README.md](Content/BalloonLevel/Rules/README.md)
- Tests: close the editor, then `powershell -NoProfile -ExecutionPolicy Bypass -File Scripts/Verify-BalloonLevel.ps1`
