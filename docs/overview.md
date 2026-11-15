# Game overview

> **Status:** Draft · **Updated:** 2026-11-13

## Concept
A first-person steampunk roguelite. The player flies a hot-air balloon, fleeing an invincible airship through a chain of biome levels. All controls are diegetic: levers, the stove and the weapons are in the balloon's basket; the HUD is minimal or absent.

A game-jam prototype; mechanics are built the way they will work in a future full game.

| | |
| --- | --- |
| **Genre** | Roguelike / roguelite |
| **Setting** | Steampunk |
| **Camera** | First person, inside the basket |
| **Engine** | Unreal Engine (C++ + Blueprints) |
| **Style** | Low-poly PS1 ([art-style.md](art-style.md)) |

## Design pillars
1. **Diegesis.** Every action is a physical interaction with an object in the basket. Information comes from gauges, not a HUD.
2. **Inertia and weight.** The balloon speeds up and slows down slowly; the route has to be planned ahead.
3. **Awkward weapons.** Few rounds, manual reloading, bullet drop: every shot matters.
4. **Constant escape.** The player always moves forward; stopping = death.

## Game loop
```
Level (flying, dodging, fighting, gathering resources)
   → Safe zone (the airship tows the balloon: repairs, ammo, upgrades, shop)
      → Next level (another biome) → ...
```
- **Minute-to-minute loop:** manage speed/tilt → feed the stove → shoot back → grab resources with the hook.
- **Level loop:** fly through the level, survive, gather resources.
- **Meta loop:** upgrades for the balloon and gear between levels; progress between runs: see `Legacy` in [ui-menus.md](features/ui-menus.md).

## Features
See the contents in [README.md](README.md).

## Team
| Who | Area |
| --- | --- |
| Sokin | Core 3C (camera, controls, interaction), balloon, weapons |
| Vano | AI (airship, enemies) |
| Rostik | Stove engine, documentation, menus, art |

## Out of scope (for now)
Multiplayer, story, voice-over.
