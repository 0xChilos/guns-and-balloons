# Shop between levels (technical)

> **Status:** Done (checked in PIE on Movement_Scene1) | **Updated:** 2026-11-15

## Items
| Item | Icon (from model) | Count | Price per click |
| --- | --- | --- | --- |
| Wood | SM_Loot_Log | x/6 | 2 gold, +1 |
| Coal | SM_Loot_Coal | x/6 | 3 gold, +1 |
| Crystals | SM_Loot_Crystal | x/6 | 5 gold, +1 |
| Ammo | SM_Loot_AmmoBox | x/60 | 1 gold, +1 round |
| Fire extinguisher | SM_Extinguisher | x/100 | 10 gold (comes full); once owned 1 gold, +10 |
| Hook | SK_Hook | — | 10 gold, once ("Owned") |
| Rifle | SK_MartiniHenry | — | 30 gold, once |
| Shotgun | SK_Homestead78 | — | 20 gold, once |

Icons are textures in `/Game/UI/Shop/Icons/T_ShopIcon_*`. Fuel counts are the pieces in the basket's buckets (`UFuelSupplyComponent`), ammo the rounds in the crate (`UAmmoSupplyComponent`); the run reads them at `CompleteLevel` and puts them back in the next level (`ApplyToWorld`). Owned gear fills its rack in the next level; gear not owned leaves its rack empty.

Design: [features/safe-zone.md](../features/safe-zone.md).

## Pieces
| Piece | What it does |
| --- | --- |
| `UBalloonRunSubsystem` | Game instance subsystem: the run's gold, supplies, gear and extinguisher, kept across `OpenLevel`. `StartNewRun` (main menu New Run; fills the biome pool and saves the first checkpoint), `StartLevel` (opens `RunLevel` = MainLevel through `UBalloonLevelLaunchSubsystem::StartLevel` with `LevelOptions` and the next biome), `CompleteLevel`, `ContinueTravel`, `FailLevel` (engine destroyed: restore the checkpoint, open the shop), `IsRunWon`, `ShopLevel` (default `/Game/Scenes/Shop`), `SpendGold`, `BuySupply`, `BuyGear`, `BuyExtinguisher`, `RefillExtinguisher`, `OnChanged` |
| `UBalloonRunWorldHook` | World subsystem: after every actor of a game level has begun play, calls `ApplyToWorld` (racks, buckets, ammo crate, the pawn's extinguisher contents) |
| `/Game/Scenes/Shop` | The shop level: `BP_MainMenuGameMode` (its `AMainMenuPlayerController` forwards LMB to every menu screen), `ShopCamera` (auto-activates for player 0, FOV as the main menu), `ShopPostProcess` (unbound, settings copied from MainMenu: `MI_OilyDepths`, manual exposure, bias 1), `LevelShopScreen` |
| `ALevelShopScreen` | Child of `AMainMenuScreen`: the same panel in front of the camera (60 cm, Brightness 0.45 as the main menu), showing `ULevelShopWidget` |
| `ULevelShopWidget` | The shop, built in code with the main menu's look: its own background over the whole view at full opacity (`BackgroundTexture`; empty = a built-in painted backdrop), gold, title, item rows (icon, name, count, price), "Continue the travel" at the bottom. Items and prices are in `Items` (`FLevelShopItem`). Only the content fades in; the background is opaque from the first frame |

## Flow
1. `ABalloonLevelGenerator::CompleteLevel` (the completion zone) calls the run's `CompleteLevel` (also BlueprintCallable). It reads the basket (extinguisher, buckets, ammo crate; gear too when no run is active), adds the level's `UResourceLedger` Money to the gold (hooked ammo and fuel are already in the basket: `UBalloonRunWorldHook` listens to `UResourceLedger::OnCountChanged` and calls `StoreCollected`, which fills the buckets / crate up to 6 per fuel and 60 rounds), adds `LevelCompleteGold` (30), counts the level, saves the checkpoint, lets go of anything held (`ABalloonPlayerController::UseMenuInput`) and opens `ShopLevel`. After `LevelsPerRun` (4) levels, `ALevelShopScreen` shows `UVictoryWidget` (main menu look, MAIN MENU row) instead of the shop.
2. Item clicks call the subsystem; `OnChanged` makes the widget redraw.
3. "Continue the travel" calls `StartLevel`: Forest for the first level, then a random biome from the pool (removed from it; after a death, the failed biome again). If the shop level is played on its own (no run), it logs a warning and stays.
4. When the engine is destroyed (`UEngineHealthComponent`), `ABalloonPlayerController::ShowDeath` plays the death screen (ENGINE DESTROYED, the cause, TRY AGAIN). TRY AGAIN (`EMainMenuAction::Retry` in `UMainMenuWidget::ActivateItem`) calls the run's `FailLevel`, which restores the checkpoint and opens `ShopLevel`; without a run it reloads the level. With another player controller (no death screen), `UEngineHealthComponent` calls `FailLevel` itself after `RestartDelay`.
4. In the new level, `ApplyToWorld` sets up the basket from the run.

## Notes
- New Run (`StartNewRun`): 0 gold, no gear (`StartingGear` empty), no extinguisher, supplies from `StartingSupplies` (4 wood, 4 coal, 4 crystals, 40 ammo); the first level's buckets and crate are set to these.
- A run is active only after New Run or the first `CompleteLevel`. A level played straight from the editor has no run, so the basket stays as placed; the first `CompleteLevel` then starts a run from what the basket holds (the extinguisher counts as owned if it is on its rack or in hand).
- Test without a finish trigger: in PIE, call `CompleteLevel` from a Blueprint, or from Python on the PIE world: find the subsystem with `unreal.ObjectIterator(unreal.BalloonRunSubsystem)` whose outer is `unreal.GameplayStatics.get_game_instance(world)` (UE 5.9 Python has no `SubsystemBlueprintLibrary`), then `complete_level(world)`.
- Automation test `GunsAndBalloons.Run.Shop` (`Tests/BalloonRunTest.cpp`) covers the gold, extinguisher, gear and supply rules (New Run, buy, refill limits, bought once, failed purchases cost nothing).
- Between `CompleteLevel` and the shop level loading, Esc/P do not open the pause menu (`ABalloonPlayerController::PauseGame` checks `IsShopOpen`).
- Click/hover audio is a UI sound (`bIsUISound`).
