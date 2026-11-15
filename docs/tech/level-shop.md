# Shop between levels (technical)

> **Status:** Done (checked in PIE on Movement_Scene1; prices TBD) | **Updated:** 2026-11-15

Design: [features/safe-zone.md](../features/safe-zone.md).

## Pieces
| Piece | What it does |
| --- | --- |
| `UBalloonRunSubsystem` | Game instance subsystem: the run's gold and extinguisher (owned, contents), kept across `OpenLevel`. `StartNewRun` (main menu New Run), `CompleteLevel(NextLevel)`, `ContinueTravel`, `SpendGold`, `BuyExtinguisher(Price)`, `RefillExtinguisher(Price, Units)`, `OnChanged` |
| `UBalloonRunWorldHook` | World subsystem: after every actor of a game level has begun play, calls `ApplyToWorld` (extinguisher racks empty or full, the pawn's extinguisher contents) |
| `ALevelShopScreen` | Child of `AMainMenuScreen`: the same panel in front of the camera, but at 15 cm (in front of the item in hand), ticking while paused, and taking LMB itself (`bExecuteWhenPaused`) |
| `ULevelShopWidget` | The shop, built in code with the main menu's look: title, gold, item rows (icon, name, detail, price), "Continue the travel" at the bottom. Items and prices are in `Items` (`FLevelShopItem`); empty `Icon` = built-in pixel icon |

## Flow
1. Whatever ends a level calls `CompleteLevel(NextLevel)` (BlueprintCallable). It reads the extinguisher's contents from the pawn, adds the level's `UResourceLedger` Money to the gold, gives the mouse to the menu (`ABalloonPlayerController::UseMenuInput`, then Game+UI mode with a hidden system cursor), spawns `ShopScreenClass` and pauses the game.
2. Item clicks call the subsystem; `OnChanged` makes the widget redraw.
3. "Continue the travel" unpauses and opens `NextLevel`, or the current map when it is empty.
4. In the new level, `ApplyToWorld` sets up the basket from the run.

## Notes
- A run is active only after New Run or the first `CompleteLevel`. A level played straight from the editor has no run, so the basket stays as placed; the first `CompleteLevel` then starts a run from what the basket holds (the extinguisher counts as owned if it is on its rack or in hand).
- Test without a finish trigger: in PIE, call `CompleteLevel` from a Blueprint, or from Python on the PIE world: find the subsystem with `unreal.ObjectIterator(unreal.BalloonRunSubsystem)` whose outer is `unreal.GameplayStatics.get_game_instance(world)` (UE 5.9 Python has no `SubsystemBlueprintLibrary`), then `complete_level(world, None)`.
- Automation test `GunsAndBalloons.Run.Shop` (`Tests/BalloonRunTest.cpp`) covers the gold and extinguisher rules (New Run, buy, refill limits, failed purchases cost nothing).
- While the shop is open, Esc/P do not open the pause menu (`ABalloonPlayerController::PauseGame` checks `IsShopOpen`), so the pause menu cannot unpause the game under the shop. The `[P] PAUSE` hint still shows, dimmed, in the corner (TBD: hide it during the shop).
- Click/hover audio is a UI sound (`bIsUISound`), so it plays while paused.
