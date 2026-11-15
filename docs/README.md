# Guns and Balloons design documentation

A modular "living" GDD: instead of one big document, a short game overview and a separate page for each feature.

## Where to start
1. [Game overview](overview.md): concept, design pillars, game loop, team. Read it first.
2. Feature pages in [features/](features/), one per mechanic.
3. [Decision log](decisions.md): what was decided and when, so settled questions don't get reopened.

## Contents
| Page | About | Owner |
| --- | --- | --- |
| [overview.md](overview.md) | Concept, pillars, loop, scope | Everyone |
| [features/balloon-controls.md](features/balloon-controls.md) | Balloon controls: panel, tilt, braking, inertia | Sokin |
| [features/engine-stove.md](features/engine-stove.md) | Stove engine, fuel and temperature zones | Rostik |
| [features/weapons.md](features/weapons.md) | Diegetic weapons, reloading, ballistics | Sokin |
| [features/grappling-hook.md](features/grappling-hook.md) | Grappling hook and resource gathering | Sokin |
| [features/fire-extinguisher.md](features/fire-extinguisher.md) | Fire extinguisher: supply on a counter, PS1-style foam | Sokin |
| [features/crosshair.md](features/crosshair.md) | Dynamic crosshair in the art style, inertia | Sokin |
| [features/wrench.md](features/wrench.md) | Wrench: take it, turn it on the stove with LMB to repair the balloon | TBD |
| [features/nemesis-airship.md](features/nemesis-airship.md) | The pursuing airship and its attacks | Vano |
| [features/biomes.md](features/biomes.md) | Biome levels and their hazards | TBD |
| [features/safe-zone.md](features/safe-zone.md) | Safe zone between levels: repairs, shop, upgrades | TBD |
| [features/ui-menus.md](features/ui-menus.md) | Main menu, navigation panel | Rostik |
| [art-style.md](art-style.md) | Visual style and post-processing | Rostik |
| [tech/weapons.md](tech/weapons.md) | Technical reference for agents: gun C++ (revolver, rifle, shotgun), viewmodel, how to add guns and hook events | Sokin |
| [tech/grappling-hook.md](tech/grappling-hook.md) | Technical reference for agents: hook C++ (pressure, flight, world-space head, rope, pickups, debug menu) | Sokin |
| [tech/fire-extinguisher.md](tech/fire-extinguisher.md) | Technical reference for agents: extinguisher C++ (contents, counter drums, spray states, foam) | Sokin |
| [tech/crosshair.md](tech/crosshair.md) | Technical reference for agents: crosshair HUD, material, spread and inertia | Sokin |
| [tech/weapon-reload.md](tech/weapon-reload.md) | Technical reference for agents: ammo supplies and the reload state machines | Sokin |
| [tech/balloon-flight.md](tech/balloon-flight.md) | Technical reference for agents: balloon flight C++ API, how to connect controls | Sokin |
| [tech/wrench.md](tech/wrench.md) | Technical reference for agents: wrench C++ (states, Fix rest poses, stove repair), FBX take split and import | TBD |
| [tech/player-architecture.md](tech/player-architecture.md) | PlayerController, the single Pawn and interaction routing | Sokin |
| [tech/level-generation.md](tech/level-generation.md) | Technical reference for agents: level generator pipeline, API, events, border proxies, interaction with the balloon | TBD |
| [tech/BalloonLevel/STATUS.md](tech/BalloonLevel/STATUS.md) | Level generation implementation status vs spec, validation results, known defects | TBD |
| [tech/BalloonLevel/HANDOFF.md](tech/BalloonLevel/HANDOFF.md) | Procedural level generation: M6 handoff, verification and implementation limits | TBD |
| [tech/stove.md](tech/stove.md) | Technical reference for agents: stove, fuel buckets, health, flight, animation and validation | Rostik |
| [tech/main-menu.md](tech/main-menu.md) | Technical reference for agents: main menu level script, in-world menu panel, mouse-only controller | Rostik |
| [tech/wing-lever.md](tech/wing-lever.md) | Technical reference for agents: wing lever, drag/grab, detents, slip API | Sokin |
| [tech/nemesis-airship.md](tech/nemesis-airship.md) | Technical reference for agents: nemesis airship, turrets, rockets, biome enemies, engine health | Vano |
| [source/directors-cut.md](source/directors-cut.md) | Original raw notes (translated to English; content not edited) | — |

## Rules
- **One feature, one page, at most ~1 page of text.** Shorter is better. A new feature = a copy of [_template.md](_template.md).
- **Write rules, not history.** Discussions belong in PRs/chat; the document gets only the outcome. Unresolved items go to "Open questions".
- **Numbers go in the "Parameters" table.** An unknown value is `TBD`; don't invent one.
- **Status in the header** (`Draft` → `Agreed` → `In progress` → `Done`) and the date of the last update.
- **`features/` pages hold only approved design.** A new feature or change is first logged in [decisions.md](decisions.md) with the status `Proposed`.
- **After approval**, change the status to `Accepted` and move it to the feature page (or create a new one from the template).
- **Changes go through git:** branch `docs/<topic>`, commit `docs: <what changed>`, PR.
- **Language: English.** Unreal class/asset names go in `code`.

## Template
The page structure is based on **One-Page Design** (Stone Librande, GDC 2010) and the **modular GDD** (Tim Ryan, "The Anatomy of a Design Document"): a short overview plus separate feature pages with the same structure (Why → How it works → Interaction → Parameters → Dependencies → Open questions).
