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
| `UGameOptions` | The Options switches (Music, SFX, Tutorial): `IsOn` / `SetOn`, static, saved to GameUserSettings.ini on every change; `OnChanged` (native multicast) is broadcast by `SetOn`. Future music/SFX should play through Sound Classes silenced while the switch is off |
| `AMainMenuPlayerController` | Mouse only: system cursor hidden, mouse free, LMB forwarded to the screen's Widget Interaction Component. Also plays the menu music (`Music`, `MusicVolume`, `MusicFadeIn`, `MusicFadeOut`) on a 2D audio component, restarted from `OnAudioFinished` so it loops whether or not the sound wave's Looping flag is set; follows the Music switch through `UGameOptions::OnChanged` |

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
- Gameplay music: `ABalloonPlayerController` (`Music` = `/Game/Audio/Gameplay_OST`, `MusicVolume`, `MusicFadeIn`, `MusicFadeOut`) creates a 2D audio component in `BeginPlay` and starts it from 0 (`RestartMusic`, BlueprintCallable for a level that starts without a map reload). It is also a UI sound, so world pause doesn't stop it: `UpdateMusic` pauses it (`SetPaused`) whenever menu input is on, the pause menu is open or Music is off, and resumes it from `UseGameplayInput`. Looped from `OnAudioFinished`; `ShowDeath` fades it out and nothing restarts it until the level reloads.
- `AMainMenuScreen::Brightness` scales the menu's colours through `UMainMenuWidget::ScaleColors` (the pause widget also scales its veil and smoke). The pause screen keeps 1.0 until tuned in a gameplay level.

## The script
- **Look:** recreates `/Game/PostProcess`'s look actors (filter volume, sky, sun, fog, clouds) with every setting that differs from a freshly placed actor. Struct values are copied with `.copy()`; wrappers obtained by `get_editor_property` point into the source actor and break when that level closes. `/Game/PostProcess` is only read, never saved.
- **Menu-only overrides:** night (moon, sky light, teal distance fog), volumetric fog, searchlights behind the camera, the stove glow, the background meshes, chimney smoke (`NS_FactorySmoke` at the `smoke_N` sockets) and Local Fog Volumes.
- **Rebuild:** the script deletes every actor in `MainMenu` whose label starts with `Menu`, plus the copied look actors, and spawns them again. Anything placed by hand under another label stays.
- **Safety:** the script stops if the editor has unsaved packages other than its own. Don't call editor exec commands such as `EDIT COPY` from Python; that crashed the editor.

## Notes
- Widget layout uses widget pixels at `DrawHeight` 1080: 4 of them make one pixel of the filter's 270-row frame. Text under ~32 px becomes hard to read after the filter.
- `GetDefault<UGeneralProjectSettings>()->ProjectVersion` (`Config/DefaultGame.ini`, `ProjectVersion=1.0`) feeds the `V1.0 RELEASE` label.
- `UMainMenuWidget::NewRunLevel` defaults to `/Game/Scenes/Movement_Scene1` (TBD: the real first level).
- The panel is sized by deprojecting the viewport corner, not from the camera FOV: with a wide viewport the engine keeps the vertical FOV, so FOV math left the menu narrower than the screen.
- `AMainMenuScreen::Brightness` (0.45, set by the script) scales the menu's colours before the widget is built, because the scene's exposure and tonemapper brighten the unlit panel. Don't use the widget component's tint for this: the translucent widget material dims the opacity with it too, and below 1 the whole menu disappears.
- `UButton::OnClicked` has no parameters, so each row reports through a `UMainMenuClickRelay`. Hover is polled in `NativeTick` (`IsHovered`).
- The click and hover sounds are slices of one longer sound (`FMainMenuSound`: Sound, Start, Length, FadeOut, Volume). `PlaySlice` plays from Start on a reused 2D audio component (one each for click and hover, so a hover never cuts a click) and `TickSlice` fades it out after Length. `PlaySlice` also calls `StopDelayed(Length + FadeOut)`, because the fade only runs while the widget ticks: a pause-menu click hides the panel at once, and without it the rest of the 20 s clock (both the click and the hover slice) played on into the game. `NativeDestruct` frees the two audio components. Set Length to 0 when a real one-shot sound replaces the slice. Hover plays when the hovered row button changes, checked only on the visible page.
