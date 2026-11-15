# Stove engine

> **Status:** Draft · **Owner:** Rostik · **Updated:** 2026-11-13

## Why
A constant small task for the player's hands: keep the fire going, balancing speed against the risk of overheating.

## How it works
The engine has **2 gauges**: temperature and burn time.

**Temperature** sets the balloon's speed.
- While there is burn time, it drops by **1°/s**. Adding fuel stops the drop for **10 s**.
- **Target temperature = the sum of the degrees of all fuel thrown in.**
- If the temperature has fallen to **0**: after adding fuel, heating starts after **10 s** and rises at **5°/s** up to the target temperature.

**Burn time** is how long the temperature is still held (≈ the amount of fuel in the stove).
- Maximum **10 min** = the stove's maximum capacity.
- When burn time = 0, the temperature drops by **5°/s**.

**Fuel**: each item adds both temperature and burn time (see "Parameters"). Fuel is carried in a **bucket** (at most 10 units).

**Other**
- **Door:** when open, heat is lost faster; it has to be closed.
- **The engine body = the balloon's health.** Damage shows as decals on the casing.

### Temperature and speed
| Level | Effect |
| --- | --- |
| 0 | Default slow speed |
| >0 – 90 | Acceleration |
| >90 | Double acceleration + damage to the engine |

## Interaction (diegetic)
| Element | Where | Action |
| --- | --- | --- |
| Engine body | Back of the basket | Shows health (decals) |
| Stove + door | On the engine body | Throw in fuel, open / close |
| Temperature and burn-time gauges | On the engine body | Read the state |
| Fuel bucket | In the basket | Take fuel (≤ 10 units) |

## Parameters
| Fuel | Burn time | Temperature |
| --- | --- | --- |
| Wood | 3 min | +20° |
| Coal | 2 min | +30° |
| Crystal | 10 s | +50° |

| Parameter | Value |
| --- | --- |
| Temperature drop (burning) | 1°/s |
| Temperature drop (burn time = 0) | 5°/s |
| Drop pause after adding fuel | 10 s |
| Ignition delay from 0 / heating speed | 10 s / 5°/s |
| Max burn time | 10 min |
| Bucket capacity | 10 units |
| Damage above 90, effect of the open door | TBD |

## Dependencies
[Balloon controls](balloon-controls.md), [Navigation panel](ui-menus.md), [Safe zone](safe-zone.md) (repairs, fuel), [Winter/Lava biome](biomes.md).

## Open questions
- [ ] Is the 0 / >0–90 / >90 scale measured in degrees of temperature?
- [ ] Proposals about the stove: see [decisions.md](../decisions.md).
