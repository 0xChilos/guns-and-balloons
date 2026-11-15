# Levels and biomes

> **Status:** Draft · **Owner:** TBD · **Updated:** 2026-11-15

## Why
Variety between runs: each biome forces a different route and different priorities.

## How it works
- A level = a flight through one biome. After it comes the [safe zone](safe-zone.md).
- Each biome has its own obstacles and hazards.
- Every level has resources for the [grappling hook](grappling-hook.md).

## Biomes
| Biome | Obstacles | Special hazards |
| --- | --- | --- |
| Rocky | Tall, wide rocks: fly around them early | — |
| Jungle | Tall thin trees, densely packed | — |
| City | Tall buildings | Fog; radar interference (jammers) + a minigame to restore the radar |
| Desert | Tall towers with guards who shoot | Sandstorms (periodic); beacons: search for the balloon with an orange beam; if the light falls on the balloon, the beam turns red, stays on the balloon until the lamp is broken, and calls extra enemies (avoid the light or break the lamp) |
| Field | No natural obstacles | Birds: avoid or shoot them |
| Winter | Snowy hills | Random winds; the engine has to be kept warm all the time |
| Lava | — | Sudden lava pillars shooting up; high temperature |

**AA gun (all biomes).** A small flying balloon with a turret and a searchlight under its gondola. It hangs near the map edges at the player's altitude (follows it). Outside its range it shines an orange searchlight that circles around. When the balloon is within range and in line of sight, the light turns red, the turret aims (with lead) and fires bursts. It lets go once the balloon is out of range. The range is small (~50 m): AA guns along the map edge mark a line the player must not cross; they don't shoot at the player all the time. The searching beam is as long as the range. Indestructible: the player can only avoid it. Range, bursts and damage are TBD; place them so the flight corridor between AA guns stays clear.

## Parameters
| Parameter | Value | Note |
| --- | --- | --- |
| Level length (flight time) | TBD | |
| Levels per run | TBD | |
| Biome order | TBD | Random / fixed |

## Dependencies
[Balloon controls](balloon-controls.md), [Stove engine](engine-stove.md) (winter, lava), [Navigation panel](ui-menus.md) (radar in the city).

## Open questions
- [ ] How does the lava biome's high temperature affect the engine (overheats faster)?
- [ ] Which biomes are in the jam version?
