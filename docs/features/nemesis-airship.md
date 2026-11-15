# Nemesis airship and enemies

> **Status:** In progress (airship, turrets, rockets, bomber, mines, crop duster, biome enemies, AA gun: C++ prototype; some enemies still use placeholder cubes) · **Owner:** Vano · **Updated:** 2026-11-15

## Why
Constant pressure that drives the player forward. The distance to the airship is the main measure of danger.

## How it works
- A big airship **always pursues** the player. It **cannot be destroyed**.
- It flies **along the balloon's trail** at the **balloon's speed without fuel** (the minimum). It catches up only when the balloon actually flies slower: stuck against an obstacle, braking with the wings, etc.
- **No ramming.** If the balloon is closer than the turret distance, **the turrets kill the player**: the run ends. Closer than the warning distance, the turrets turn towards the balloon.
- **The attack type depends on the distance** to the balloon. The attacks are the same on every level.

| Band | Attack |
| --- | --- |
| Turrets (closest) | Death |
| D1 | Rockets |
| D2 | Bomber (one) drops mines on parachutes |
| D3 (farthest) | Crop dusters with guns |

- **Damage** hits the basket and is taken from the engine's health (see [engine-stove.md](engine-stove.md)).

## Enemies
| Enemy | Behaviour | Counter |
| --- | --- | --- |
| Rocket | Fixed trajectory (a curve) to an intercept point; not guided, doesn't self-detonate. Damages only on a direct hit on the basket + slows the balloon | Dodge (change course/speed) or shoot it down with one shot |
| Bomber | One; takes a position ahead of and above the balloon and never changes it until shot down. Drops parachute mines from the left wing, from under the fuselage, then from the right wing, around the balloon's course | Shoot it down (the wing engines are visible from behind) or fly around the mines |
| Parachute mine | Descends slowly | Fly around it or shoot it |
| Crop duster | Launches from the airship, takes a position to the right or left of the balloon and never changes side or distance until shot down; fires at the basket | Hit the engine or the fuselage |

Planes don't change altitude. Enemies shoot straight at the basket, with no correction for bullet drop. Biome enemies (tower guard, beacon, birds, patrols, AA gun) are described in [biomes.md](biomes.md).

## Damage to the engine
The engine has **100 HP**. All values are prototype values (TBD).

| Enemy | Damage per hit | How often | Max per cycle | Hits to kill |
| --- | --- | --- | --- | --- |
| Airship turrets | Instant death | When the basket is within 30 m of the airship | — | — |
| Rocket | 25 + the balloon slows by 20% for 3 s | One every 7 s, at most 2 in the air | 25 | 4 |
| Parachute mine (bomber) | 30, the same anywhere within a 5 m radius | One every 7–10 s | 30 | 4 |
| Bomber | No direct damage, only through mines | — | — | — |
| Crop duster (also lava patrols) | 3 per bullet | A burst of 6 every 4 s (0.5 s warning before each burst) | 18 per burst | 34 bullets, ≈ 6 full bursts |
| Tower guard (desert) | 12 | One shot every 6 s (aims 2 s, reloads 4 s) | 12 | 9 |
| AA gun (all biomes) | 5 per bullet | A burst of 5 every ≈ 3 s (1 s warm-up before the first), 2.5° spread, only within its 50 m radius | 25 per burst | 4 full bursts (20 bullets) |
| Bird flock (field) | 3 per bird | A bird is gone after it hits; one active flock at a time | 15 per flock | ≈ 7 flocks |
| Beacon (desert) | None | Calls a crop duster every 15 s while locked on | — | — |

Bullets (crop duster, guard, AA gun) count only if they pass within 1.3 m of the basket's centre. A rocket needs a direct hit; a mine hits everything within its radius.

## Parameters
| Parameter | Value | Note |
| --- | --- | --- |
| Airship speed | = the balloon's minimum speed | `MinSpeed` from `UBalloonFlightComponent` |
| Turret / warning distance | 30 m / 50 m | Prototype, from the airship's nose to the basket |
| D1 / D2 / D3 | 30–100 m / 100–220 m / > 220 m | Prototype |
| Rocket: speed, damage, slowdown | 25 m/s, 25, −20% for 3 s | Prototype |
| Rockets: cooldown / limit | 7 s / 2 | Prototype |
| Hits to shoot down a plane | Engine: 1 from any weapon. Fuselage: revolver 4, shotgun 3 (one shot = one hit), rifle 2 | Decision 2026-11-14 |
| Mines in the air at once | 3 | Prototype |
| Mine damage radius | 5 m | Prototype (was 8 m) |
| Engine health | 100 | TBD, prototype |
| Airship height above the balloon's trail | 15 m | Prototype |

## Dependencies
[Balloon controls](balloon-controls.md), [Stove engine](engine-stove.md), [Weapons](weapons.md), [Safe zone](safe-zone.md) (the airship tows the balloon), [Biomes](biomes.md) (biome enemies). Technical: [tech/nemesis-airship.md](../tech/nemesis-airship.md).

## Open questions
- [ ] The rocket's slowdown doesn't take the balloon below `MinSpeed`, so at minimum speed it has no effect and the airship doesn't catch up. Let the slowdown go below the minimum?
- [ ] How does the story explain the enemy airship towing the balloon in the safe zone?
