# Main menu (technical)

> **Status:** In progress (scene and menu layout checked in PIE; hover/click not yet tested by hand) | **Updated:** 2026-11-15

Design: [features/ui-menus.md](../features/ui-menus.md).

## Pieces
| Piece | What it does |
| --- | --- |
| `/Game/Scenes/MainMenu` | Menu level. Built by `Scripts/Editor/create_main_menu_scene.py`; rerunning the script rebuilds it |
| `/Game/UI/MainMenu/BP_MenuBalloon` | `SK_Balloon` only (reference pose, no collision, no gameplay components) |
| `/Game/UI/MainMenu/LS_MenuBalloonIdle` | 24 s looping additive bob/sway, played by `LSA_MenuBalloonIdle` |
| `/Game/UI/MainMenu/MI_MenuNightSky` | Night instance of the engine sky material (`M_Sky_Panning_Clouds2`) on the copied `SM_SkySphere` |
| `/Game/UI/MainMenu/BP_MainMenuGameMode` | No pawn; `PlayerControllerClass` = `AMainMenuPlayerController` |
| `UMainMenuWidget` | The menu, built in code: no box, game title and subtitle top left, items on a soft haze from the left edge, eased hover (slide, warm colour, marker, underline), fade-in, actions, Credits/Options pages, version label, pixel arrow cursor |
| `AMainMenuScreen` | Puts the widget on an unlit, translucent panel `Distance` in front of the active camera, sized to fill its view, so the filter applies to the menu |
| `UGameOptions` | The Options switches (Music, SFX, Tutorial): `IsOn` / `SetOn`, static, saved to GameUserSettings.ini on every change. Nothing reads them yet; future music/SFX should play through Sound Classes silenced while the switch is off |
| `AMainMenuPlayerController` | Mouse only: system cursor hidden, mouse free, LMB forwarded to the screen's Widget Interaction Component |

## Pause menu
| Piece | What it does |
| --- | --- |
| `ABalloonPlayerController` | `PauseKeys` (Esc, P) call `TogglePause`. `PauseGame`: `SetPause(true)`, `UseMenuInput` (releases held gameplay input), then Game-and-UI input with the mouse free, centred and the system cursor hidden; hides the HUD (crosshair); spawns `APauseMenuScreen`. `ResumeGame` undoes it. LMB is forwarded to the panel while paused (bindings run when paused and don't consume, since the pawn polls the mouse itself) |
| `APauseMenuScreen` | `AMainMenuScreen` that ticks while paused (actor, panel, pointer), sits 15 cm from the camera and sets `PanelMaterial` (an `AMainMenuScreen` property) to `/Game/UI/PauseMenu/M_PauseMenuPanel`, so the gun and the basket are never drawn over it. Its pointer is `Custom`: the mouse ray is intersected with the panel's plane, no scene trace |
| `UPauseMenuWidget` | `UMainMenuWidget` child with its own `BuildTree`: veil, `SmokeLayers` (each a 256² tiling texture of domain-warped periodic value noise made at runtime, with a density floor so nothing is clear), a radial edge-smoke image, centred title and rows. Actions `Resume`, `QuitToMenu` (`MainMenuLevel`), `Quit` |
| `APauseHintScreen` + `UPauseHintWidget` | The `[P] PAUSE` corner hint during play. Spawned by the controller in `BeginPlay` (only with a local player), hidden while paused. No collision, pointer off, widget redrawn at 15 Hz |
| `/Game/UI/PauseMenu/M_PauseMenuPanel` | Copy of `/Engine/EngineMaterials/Widget3DPassThrough` (translucent, unlit, one-sided) with depth test off. Made by `Scripts/Editor/create_pause_menu_material.py`. The engine's `_Translucent(_OneSided)` widget materials are instances and can't turn depth test off |

- Resume can come from the panel's own click, so the panel is hidden at once and destroyed next tick.
- Menu click sounds play while paused because `CreateSound2D` makes UI sounds.
- `AMainMenuScreen::Brightness` scales the menu's colours through `UMainMenuWidget::ScaleColors` (the pause widget also scales its veil and smoke). The pause screen keeps 1.0 until tuned in a gameplay level.

## The script
- **Look:** recreates `/Game/PostProcess`'s look actors (filter volume, sky, sun, fog, clouds) with every setting that differs from a freshly placed actor. Struct values are copied with `.copy()`; wrappers obtained by `get_editor_property` point into the source actor and break when that level closes. `/Game/PostProcess` is only read, never saved.
- **Menu-only overrides:** night (moon, sky light, teal distance fog), volumetric fog, searchlights behind the camera, the stove glow, the background meshes, chimney smoke (`NS_FactorySmoke` at the `smoke_N` sockets) and Local Fog Volumes.
- **Rebuild:** the script deletes every actor in `MainMenu` whose label starts with `Menu`, plus the copied look actors, and spawns them again. Anything placed by hand under another label stays.
- **Safety:** the script stops if the editor has unsaved packages other than its own. Don't call editor exec commands such as `EDIT COPY` from Python; that crashed the editor.

## Notes
- Reusable launch-method verification (2026-11-15): build succeeded; all 17 level/menu tests passed (9 with fixture warnings), including a standalone game-instance test confirming default-map travel, settings handoff, missing-map rejection and preservation of a queued launch after rejection. Restart an already open editor to load the new reflected method.
- Reusable launch API: `UBalloonLevelLaunchSubsystem::StartLevel(InOptions, Level, OutError)` handles validation, handoff and map travel independently of the widget. In another menu, get this game-instance subsystem and call **Start Level** with the shared options struct; an empty map input defaults to MainLevel. The run calls it from `UBalloonRunSubsystem::StartLevel`.
- Level-start verification (2026-11-15): full editor build succeeded; all 16 level-generation/menu setup automation tests passed (9 with fixture warnings). Tests cover menu field collection and Back, one-shot launch handoff, invalid values, overrides, enemy/obstacle scaling and failed-regeneration retention. A headless, unsaved `MainLevel` smoke test generated all four biomes with Day/Night and the selected height, width and fog. The setup page's filtered appearance and mouse dragging have not been checked by hand.
- New Run (`UMainMenuWidget::StartNewRun`, 2026-11-15) starts the run directly: `UBalloonRunSubsystem::StartNewRun` then `StartLevel` (Forest). The New Run setup page (`MainMenuLevelSetup.cpp`) was removed; level settings now come from `UBalloonRunSubsystem::LevelOptions`. `UBalloonLevelLaunchSubsystem` still carries the options across map travel and is consumed once in generator BeginPlay, forcing start generation.
- Widget layout uses widget pixels at `DrawHeight` 1080: 4 of them make one pixel of the filter's 270-row frame. Text under ~32 px becomes hard to read after the filter.
- `GetDefault<UGeneralProjectSettings>()->ProjectVersion` (`Config/DefaultGame.ini`, `ProjectVersion=1.0`) feeds the `V1.0 RELEASE` label.
- `UBalloonRunSubsystem::RunLevel` defaults to `/Game/Scenes/MainLevel`, which supplies the level generator and player balloon.
- The panel is sized by deprojecting the viewport corner, not from the camera FOV: with a wide viewport the engine keeps the vertical FOV, so FOV math left the menu narrower than the screen.
- `AMainMenuScreen::Brightness` (0.45, set by the script) scales the menu's colours before the widget is built, because the scene's exposure and tonemapper brighten the unlit panel. Don't use the widget component's tint for this: the translucent widget material dims the opacity with it too, and below 1 the whole menu disappears.
- `UButton::OnClicked` has no parameters, so each row reports through a `UMainMenuClickRelay`. Hover is polled in `NativeTick` (`IsHovered`).
- The click and hover sounds are slices of one longer sound (`FMainMenuSound`: Sound, Start, Length, FadeOut, Volume). `PlaySlice` plays from Start on a reused 2D audio component (one each for click and hover, so a hover never cuts a click) and `TickSlice` fades it out after Length. `PlaySlice` also calls `StopDelayed(Length + FadeOut)`, because the fade only runs while the widget ticks: a pause-menu click hides the panel at once, and without it the rest of the 20 s clock (both the click and the hover slice) played on into the game. `NativeDestruct` frees the two audio components. Set Length to 0 when a real one-shot sound replaces the slice. Hover plays when the hovered row button changes, checked only on the visible page.
