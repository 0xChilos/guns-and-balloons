# Levels and biomes

> **Status:** Draft · **Owner:** TBD · **Updated:** 2026-11-15

## Why
Variety between runs: each biome forces a different route and different priorities.

## How it works
- Generation: `Manual (Button)` from a button, or `Every Level Start` automatically on every BeginPlay. The plan is temporary and isn't saved in the map.
- Biome: `Selected Config`, or `Random` from the `RandomBiomeConfigs` list with their asset tables. `RandomizeSeed` gives a new seed on every generation; otherwise the config's seed is used. The source DataAsset isn't changed.
- A level = a flight through one biome. After it comes the [safe zone](safe-zone.md).
- A level is a **corridor** from start to finish. Rocket balloons (`BP_RocketBalloon`) patrol both edges of the corridor: flying over the edge = getting shot at.
- Each biome has its own obstacles and hazards.
- Every level has resources for the [grappling hook](grappling-hook.md) (loot spots on tall structures).
- The biome set matches the existing assets (`/Game/Assets/Locations/<Level>`).

## Biomes
| Biome | Obstacles | Special hazards |
| --- | --- | --- |
| Forest (`Forest`) | Tall pines (they took over the jungle's tall thin trees), a fire lookout tower; a ring of mountains on the horizon | — |
| Field (`Field`) | Almost no natural obstacles; windmills | Birds (flocks): avoid them, shoot them, or scare the flock with a shot in its direction (a scared flock flies off for good and does no damage) |
| City (`City`) | Tall buildings, a church, a clock tower, factories | Fog; radar jammers (shoot down the dish) + a minigame to restore the radar |
| Desert (`Desert`) | Mesas, rock pillars; tall towers with a cannon: within range it fires single shells with a reload; a fuel tank on the platform: one shot at it destroys the cannon and the whole tower | Sandstorms (periodic); beacons: search for the balloon with an orange beam up to a set distance; the player can avoid the light or shoot the lamp before being spotted; once spotted, the beam turns red, follows the balloon until the lamp is broken or the balloon gets far enough from the beacon, and calls a pair of crop dusters (at least one always comes) |

**AA gun (all biomes).** A small flying balloon with a turret and a searchlight under its gondola. It hangs near the map edges at the player's altitude (follows it). Outside its range it shines an orange searchlight that circles around. When the balloon is within range and in line of sight, the light turns red and it launches homing missiles, one at a time: a missile chases the balloon while it stays in range and blows up harmlessly the moment the balloon leaves; a hit deals 50; one shot destroys a missile. The missile is slow enough (~9 m/s) to give the player a few seconds to get out of range, but faster than the balloon. It lets go once the balloon is out of range. The range is small (~50 m): AA guns along the map edge mark a line the player must not cross; they don't shoot at the player all the time. The searching beam is as long as the range. Indestructible: the player can only avoid it. Range, missile speed and damage are TBD; place them so the flight corridor between AA guns stays clear.

Patrolling crop dusters fly a route; one that spots the balloon (within ~200 m) brings its group into the attack.

## Parameters
| Parameter | Value | Note |
| --- | --- | --- |
| Corridor width | 600 m | Default; ≥ 2 × the rocket balloons' `AggroRangeM` (300 m), so in the middle of the corridor the player is out of their range |
| Level length (flight time) | TBD | |
| Flight height | TBD | The balloon's spawn height; there is no height control |
| Levels per run | TBD | |
| Biome order | TBD | Random / fixed |

## Dependencies
[Balloon controls](balloon-controls.md), [Stove engine](engine-stove.md), [Navigation panel](ui-menus.md) (radar in the city), [Airship and enemies](nemesis-airship.md).

## Open questions
- [ ] Which biomes are in the jam version?
- [ ] What happens when the balloon flies over the corridor's edge (only getting shot at, or a hard boundary)?
