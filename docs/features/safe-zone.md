# Safe zone

> **Status:** In progress (shop menu) · **Owner:** TBD · **Updated:** 2026-11-15

## Why
A break between levels: the player rests from the pressure and makes decisions about upgrading the balloon.

## How it works
- After a level is cleared, the airship **tows the balloon** to the next level. There are no attacks.
- What's available:
  - restocking ammo;
  - repairing the balloon (the engine body);
  - balloon upgrades;
  - buying new gear and weapons.

## Shop (implemented first)
- Opens when a level is completed: the game pauses and the shop appears on a filtered panel in front of the camera, with the main menu's look (haze from the left, warm hover, pixel cursor, clock sounds).
- Top left: title, the player's **gold**. Below: one row per item: its icon, name, what a click does and the price. A row the player can't use (gold short, already full) is dimmed; a price they can't afford shows in red.
- At the bottom: **Continue the travel**, which opens the next level.
- **Gold** is the money the hook brings in during a level; it is banked when the level is completed and carries over for the whole run. New Run starts at 0.
- **Fire extinguisher:** a run starts without one. Buying it puts a full one (100/100) in the basket from the next level on. Once owned, its row turns into a refill (+10 units per click, up to 100). Its contents carry over between levels.

## Parameters
| Parameter | Value | Note |
| --- | --- | --- |
| Duration | TBD | Fixed, or until the player is done |
| Prices | TBD | `ULevelShopWidget::Items` |
| Extinguisher price | 10 gold | Sold full (100/100). TBD |
| Extinguisher refill | 1 gold per +10 units | Shown once owned; up to 100. TBD |

## Dependencies
[Weapons](weapons.md), [Stove engine](engine-stove.md), [Grappling hook and resources](grappling-hook.md) (currency), [Airship](nemesis-airship.md).

## Open questions
- [x] Currency: the money brought in by the hook (gold), banked between levels.
- [ ] What ends a level (a finish trigger, the generator's finish point, ...) and calls `CompleteLevel`.
- [ ] List of balloon upgrades.
- [ ] What the shop looks like diegetically (a trader on the airship, a delivery crate, ...)?
