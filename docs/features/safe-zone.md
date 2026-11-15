# Safe zone

> **Status:** In progress (shop menu, run flow) · **Owner:** TBD · **Updated:** 2026-11-15

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
- Opens when a level is completed, as **its own level** (`/Game/Scenes/Shop`), with the main menu's look (haze from the left, warm hover, pixel cursor, clock sounds) and the game's filter. It has **its own background at full opacity** (nothing of the level shows through).
- Top left: title, the player's **gold**. Below: one row per item: its icon, name, what a click does and the price. A row the player can't use (gold short, already full) is dimmed; a price they can't afford shows in red.
- At the bottom: **Continue the travel**, which starts the next level.
- **Gold** is the money the hook brings in during a level plus **30 gold per completed level**; it is banked when the level is completed and carries over for the whole run, without a cap. New Run starts at 0.
- **Resources carry over:** gold, ammo, fuel, gear and the extinguisher are tracked across the run. Everything the hook brings in during a level goes straight into the basket: wood, coal and crystals into their buckets, ammo into the ammo crate, up to each item's max (anything over it is lost). Money is banked as gold at the end of the level.

## Run structure
- A run has **4 levels**. New Run (main menu) starts the first level right away (no setup page); it is always **Forest**.
- New Run fills a **biome pool**: City, Desert, Field. Each later level takes a random biome out of the pool.
- Completing a level opens the shop; after the 4th level a **victory screen** (VICTORY, MAIN MENU button) is shown instead.
- **Checkpoint:** each completed level saves the run (gold, supplies, gear, extinguisher, pool). If the engine is destroyed during a level, the death screen shows; its RETURN TO CHECKPOINT returns the run to that checkpoint and opens the shop again; the failed level's biome is played again. Dying in the first level returns to the shop with what the run started with.
- **New Run starts with:** no hook, no rifle, no shotgun, no fire extinguisher; 4 wood, 4 coal, 4 crystals, 40 rounds of ammo.
- **Fuel and ammo:** wood, coal and crystals up to 6 each (2 / 3 / 5 gold per piece), ammo up to 60 (1 gold per round).
- **Gear bought once:** hook 10 gold, rifle 30, shotgun 20; afterwards its row is greyed out with "Owned" on the icon.
- **Fire extinguisher:** a run starts without one. Buying it puts a full one (100/100) in the basket from the next level on. Once owned, its row turns into a refill (+10 units per click, up to 100). Its contents carry over between levels.

## Parameters
| Parameter | Value | Note |
| --- | --- | --- |
| Duration | TBD | Fixed, or until the player is done |
| Prices | See Shop above | `ULevelShopWidget::Items` |
| Level reward | 30 gold | `UBalloonRunSubsystem::LevelCompleteGold` |
| Levels per run | 4 | `UBalloonRunSubsystem::LevelsPerRun` |
| Biomes | Forest first, then City/Desert/Field in random order | `FirstBiome`, `BiomePoolSource` |
| New Run supplies | 4 wood, 4 coal, 4 crystals, 40 ammo | `UBalloonRunSubsystem::StartingSupplies` |
| New Run gear | None | `UBalloonRunSubsystem::StartingGear` |
| Extinguisher price | 10 gold | Sold full (100/100). TBD |
| Extinguisher refill | 1 gold per +10 units | Shown once owned; up to 100. TBD |

## Dependencies
[Weapons](weapons.md), [Stove engine](engine-stove.md), [Grappling hook and resources](grappling-hook.md) (currency), [Airship](nemesis-airship.md).

## Open questions
- [x] Currency: the money brought in by the hook (gold), banked between levels.
- [x] What ends a level: the level generator's completion zone (`ABalloonLevelGenerator::CompleteLevel`) calls the run's `CompleteLevel`.
- [ ] List of balloon upgrades.
- [ ] What the shop looks like diegetically (a trader on the airship, a delivery crate, ...)?
