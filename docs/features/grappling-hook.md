# Grappling hook and resources

> **Status:** In progress · **Owner:** Sokin · **Updated:** 2026-11-16

## Why
A way to gather resources on the fly without stopping, and a risk/reward choice: getting distracted by loot during the chase.

## How it works
- The hook is pneumatic (like the flamethrower in Hunt: Showdown): after a shot **the pressure has to be pumped up** before it can fire again.
- The hook is gear that sits in its own place, like the [weapons](weapons.md): LMB on it with an empty hand takes it; LMB on its empty place puts it back. In the hand it sits where the weapons do.
- Pressure is shown on a **gauge on the hook itself**: a needle 0–100, a green zone near 100. The hook is issued **fully charged** (100).
- **The hook only works at 100.** LMB: the hook flies forward along the crosshair, pressure drops to 0. Below 100, LMB just clicks.
- **The rope has no physics:** a straight line from the muzzle to the hook's head.
- Hit a resource with the hook → the claws close (Grab animation) and the hook pulls the resource back to the player. The resource is released in front of the player and falls (into the basket).
- **Feedback on the grab:** the moment the claws close, the resource makes its own sound (an ammo box rattles, coins jingle, a log knocks, coal crunches, a crystal chimes) and sparks fly at the hook head, however far out it is.
- **What an item holds:** one or more resources, each a random amount in its own range, rolled when it arrives. The ammo box holds ammo (1–3) and some money (1–10).
- **Counting:** each resource has its own count. A resource counts when the hook brings it to the player (not when it is grabbed), once: one dropped and hooked again doesn't count twice, and one the hook drops on the way (put away mid-reel) doesn't count.
- **The counts are shown in the world:** a board with a small model of each resource and a three-digit number. When a count goes up, the number rolls up with a mechanical tick and the row lights up for a moment. For now the board stands near the player; in the game it belongs in the basket.
- Missed a resource (a wall or the end of the range) → the hook comes back empty.
- After it returns, pump it up: each pump stroke (Pump animation) adds `100 / number of pumps`. At 100 the Charged animation plays and the hook is ready again.
- **Pumping is easy at first, harder after the middle:** the lever goes stiff, each stroke is slower than the last, the final one is the hardest. The number of strokes to 100 doesn't change.
- Resources:
  - spawn at authored `loot_*` sockets/bones on generated scenery, at most one hookable pickup per spot (including instanced meshes); the level config's Item Density sets the percentage filled (0-100%, default 100%, prototype tuning), repeatably for the level seed; regeneration removes the previous pickups;
  - lie on the level's surroundings;
  - hang from small balloons flying in the air.

## Interaction (diegetic)
| Action | Result |
| --- | --- |
| LMB on the hook (empty hand) | Take the hook |
| Fire the hook (LMB, pressure 100) | The hook flies, grabs a resource and pulls it to the player |
| Pump the pressure (pump) | The gauge needle rises; at 100 the hook is ready again |
| The hook grabs a resource | The resource's own sound, sparks at the hook head |
| The resource reaches the player | Its count on the board rolls up and ticks, the row lights up |

## Parameters
| Parameter | Value | Note |
| --- | --- | --- |
| Range | TBD | `MaxRange`, currently 2500 cm (placeholder) |
| Pumps per shot | TBD | `PumpsToFullCharge`, currently 5; adjustable in the debug menu (`H`) |
| Pressure at which pumping gets harder | TBD | `HardPumpFrom`, currently 50; debug menu |
| Speed of the last (hardest) stroke | TBD | `HardPumpPlayRate`, currently 40% of an easy one; debug menu |
| Hook flight speed | TBD | `ThrowSpeed`, currently 2500 cm/s |
| Reel-in speed | TBD | `ReturnSpeed`, currently 1500 cm/s |
| Starting pressure | 100 | `InitialCharge` |
| What a resource item brings in | TBD | `AHookPickup::Contents`: a list of resource ranges rolled on arrival. Ammo box: ammo 1–3 **and** money 1–10; money pouch, log, coal, crystal: 1 of their own. `BP_Loot` lists ammo 30, money 25, coal 30 |
| Counter roll speed | TBD | `AResourceCounterBoard::RollSpeed` 12 steps/s, a big jump at most `MaxRollTime` 1 s |
| Row light-up time | TBD | `FlashTime`, currently 0.8 s |

## Dependencies
[Weapons](weapons.md), [Biomes](biomes.md), [Safe zone](safe-zone.md) (what resources are spent on).

## Open questions
- [ ] Which resource types exist (fuel, ammo, currency, repair materials)? Counted for now: ammo, money, wood, coal, crystal (the `SM_Loot_*` models).
- [ ] Where does the resource counter live in the game (a board in the basket, on the hook, at the safe zone)? Does the count carry over between levels?
- [ ] Can the hook grab objects for manoeuvring?
- [ ] Which action pumps the hook? Currently: hold RMB (see [decisions](../decisions.md)).
- [ ] Where exactly does the resource go: currently it falls in front of the player; in the game, into the basket / inventory?
