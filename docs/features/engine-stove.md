# Stove engine

> **Status:** In progress · **Owner:** Rostik · **Updated:** 2026-11-15

## Why
A constant small task for the player's hands: keep the fire going, balancing speed against the risk of overheating.

## How it works
The engine has **2 gauges**: temperature and burn time.

**Temperature** sets the balloon's speed.
- While there is burn time, it drops by **1°/s**. Adding fuel stops the drop for **10 s**.
- **Target temperature** = the current temperature (or the current target) + the degrees of the fuel thrown in. The temperature doesn't drop while heating.
- Heating runs at a configurable rate (default **5°/s**, can be set per fuel). **Crystal** jumps straight to the target.
- If the temperature has fallen to **0**: after adding fuel, heating starts after **10 s** (burn time isn't spent meanwhile; crystal has no delay).

**Burn time** is how long the temperature is still held (≈ the amount of fuel in the stove).
- Maximum **10 min** = the stove's maximum capacity. Fuel that doesn't fit is refused.
- When burn time = 0, the temperature drops by **5°/s**.

**Fuel**: each item adds both temperature and burn time (see "Parameters"). It lies in **3 buckets**, one per type, and is taken into the hand one piece at a time.
**Buckets:** each holds up to **6 pieces**. Every piece has its own fixed spot, so how much is left reads at a glance: wood is a 3-2-1 pyramid with the log ends toward the player, coal and crystals sit in a ring of 6 spots (an empty spot shows as a dark mark / an empty collar). Each store looks like its fuel and is open or low on the player's side so the contents show: wood in a plank crate with a low front board, coal in an open sack with a rolled-down rim, crystals in an iron bucket. There are no icon signs: the store itself shows the fuel type. The stores are compact (a full one looks packed) and stand by the stove (sides as seen by the player facing it): coal right against its left side, wood right against its right side, crystals in front of the wood, by the side wall.
The same bucket takes a piece back; another bucket returns it to its own bucket and gives out its own type. If the target bucket is empty or the return isn't possible, the hand and the supplies don't change.

**Other**
- **Fire extinguisher:** a jet aimed at the body, the door or the firebox lowers the current and the target temperature, keeping the burn time. Obstacles block the jet; at 0 the stove goes out. Strength and range are TBD in the [fire extinguisher](fire-extinguisher.md).
- **Door:** fuel can be thrown in only while it is open; open, heat is lost faster; it has to be closed. In `BP_Balloon` it opens with `DoorOpenAngle = -100°`.
- **The engine body = the balloon's health.** Damage comes from overheating and from hits on **any part of the basket** (stove, door, bucket, panel, walls and floor); hits on the envelope don't reduce this pool. **The balloon can only be repaired through the stove.** Damage shows on the casing: every **10 HP lost** adds a new bullet hole: black soot, torn bright edges and a red glow in the centre, as if you see into the engine. Up to 10 holes (0 HP), all on the stove faces and lid the player sees from inside the basket, appearing in a random order. Repairs remove them again (one per 10 HP restored, newest first).
  The shared `UHealthComponent` has public `ApplyDamage` / `Repair`; point damage is taken by the `basket` bone with all its children except the envelope branch `balloon`, and by basket collisions tagged `BasketDamage`. At 0 health: the `OnDepleted` event ("balloon destroyed"), no game over yet.
- **VFX:** accepted fuel gives a short orange flash of embers in the firebox; overheating gives dark smoke above the stove; every visible bullet hole gives off a thin stream of dark smoke. After cooling down no new smoke appears and the existing smoke disperses. Style: blocky PS1 sprites; the `StoveVFX` parameters are TBD for tuning.

### Temperature and speed
| Level (°) | Effect |
| --- | --- |
| 0 | Default slow speed (×1) |
| >0 – 90 | Acceleration: speed multiplier rising linearly to ×1.6 |
| >90 | Double speed (×2) + damage to the engine |

## Interaction (diegetic)
| Element | Where | Action |
| --- | --- | --- |
| Engine body | Back of the basket | Shows health (decals); the only place to repair the balloon |
| Stove door | On the engine body | LMB: open / close (animated) |
| Firebox | Behind the door | LMB with fuel in hand: throw it in, only while the door is open; clicking the firebox doesn't open the door |
| Temperature and burn-time gauges | On the engine body | Read the state |
| 3 fuel buckets | By the stove: coal on the left, wood on the right, crystals in front of the wood by the wall | LMB with an empty hand: take a piece; with the same fuel: return it; with another: swap. The pieces in a bucket show how many are left |

With fuel in hand, clicking a weapon rack or an item does nothing, and levers can't be grabbed. The fuel has to go into the stove or back into a bucket.

## Parameters
| Fuel | Burn time | Temperature | Heating |
| --- | --- | --- | --- |
| Wood | 3 min | +20° | 5°/s |
| Coal | 2 min | +30° | 5°/s |
| Crystal | 10 s | +50° | instant |

| Parameter | Value |
| --- | --- |
| Temperature drop (burning) | 1°/s |
| Temperature drop (burn time = 0) | 5°/s |
| Drop pause after adding fuel | 10 s |
| Ignition delay from 0 / heating speed | 10 s / 5°/s (configurable) |
| Max burn time | 10 min |
| Capacity of each bucket | 6 units |
| Max temperature | TBD (120°) |
| Speed multiplier at 90° / above 90° | TBD (×1.6) / TBD (×2) |
| Overheating threshold | >90° |
| Overheating damage / balloon max health | TBD (2 units/s) / 100 units |
| Bullet holes on the casing | 1 per 10 HP lost, at most 10 |
| Heat-loss multiplier with the door open | TBD (×2) |
| Starting supply: wood / coal / crystal | TBD (4 / 4 / 2 units) |
| Burn-time overflow | Refused, the fuel stays in the hand; `bClampBurnTimeOnOverfill = false` |
| Per-fuel heating rate override | `HeatRateOverride = 0`: the stove's rate; crystal: `bInstantHeat = true` |
| Thermometer full scale | TBD (100°) |
| Door angle / speed in `BP_Balloon` | -100° / TBD (300°/s) |

## Dependencies
[Balloon controls](balloon-controls.md), [Weapons](weapons.md) (the player's hand), [Navigation panel](ui-menus.md), [Safe zone](safe-zone.md) (repairs, fuel), [Winter/Lava biome](biomes.md).

## Open questions
- [x] Is the 0 / >0–90 / >90 scale in degrees of temperature? Yes.
- [x] Models for two more buckets: the balloon model has 3 stores with 6 spots each (crate, sack, bucket).
- [x] Fuel choice (D1): 3 separate buckets; the same one takes a piece back, another swaps. If the target is empty or the return isn't possible, the hand and supplies don't change.
- [x] Door (D2): only an open firebox; a later decision removed opening the door by clicking the firebox.
- [x] Health (D3): shared by the stove and the balloon; at 0 an event, no game over.
- [x] Heating (D4): the target accumulates, the rate is configurable; crystal is instant.
- [x] Speed (D5): a `Stove` multiplier, not acceleration.
- [x] Overflow (D6): refused, the fuel stays in the hand.
- [x] Fuel and racks/items (D7): a click does nothing.
- [ ] Other proposals (bellows, changed temperature zones): see [decisions.md](../decisions.md); not approved.

Technical implementation and checks: [tech/stove.md](../tech/stove.md).
