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
| `UMainMenuWidget` | The menu, built in code: no box, items on a soft haze from the left edge, eased hover (slide, warm colour, marker, underline), fade-in, actions, Credits/Options pages, version label, pixel arrow cursor |
| `AMainMenuScreen` | Puts the widget on an unlit, translucent panel `Distance` in front of the active camera, sized to fill its view, so the filter applies to the menu |
| `UGameOptions` | The Options switches (Music, SFX, Tutorial): `IsOn` / `SetOn`, static, saved to GameUserSettings.ini on every change. Nothing reads them yet; future music/SFX should play through Sound Classes silenced while the switch is off |
| `AMainMenuPlayerController` | Mouse only: system cursor hidden, mouse free, LMB forwarded to the screen's Widget Interaction Component |

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
- The click and hover sounds are slices of one longer sound (`FMainMenuSound`: Sound, Start, Length, FadeOut, Volume). `PlaySlice` plays from Start on a reused 2D audio component (one each for click and hover, so a hover never cuts a click) and `TickSlice` fades it out after Length. Set Length to 0 when a real one-shot sound replaces the slice. Hover plays when the hovered row button changes, checked only on the visible page.
