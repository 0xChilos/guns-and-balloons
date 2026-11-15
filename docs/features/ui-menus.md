# UI, menus and the navigation panel

> **Status:** In progress (main menu: scene done, widget in progress) · **Owner:** Rostik · **Updated:** 2026-11-15

## Why
As little non-diegetic interface as possible. During play, information comes from the gauges in the basket.

## Main menu
| Item | Action |
| --- | --- |
| Continue | Continue the last run (disabled until there are saves) |
| New Run | Start a new run |
| Credits | Authors: Oleksii Nahorniak, Ivan Luzhanskiy, Ivan Petrenko, Rostyslav Ivlev |
| Options | On/off switches (below) |
| Quit | Quit the game |

- **Background:** its own level `MainMenu` with the in-game balloon (`SK_Balloon`) slowly bobbing and swaying, seen through the same filter as the game (`MI_OilyDepths`).
- **The menu is filtered too:** it is a panel in the 3D scene in front of the camera, not an overlay, so it gets the same low resolution, dither and grain.
- **Layout:** top left is left empty for the game's title and subtitle; the item list sits below it on the left, with no box: the items float on a soft dark haze from the left edge; a small `V1.0 RELEASE` sits bottom right (from the project version).
- **Feel:** a hovered item eases right, warms to the stove/searchlight colour, and gets a small marker and an underline that draws itself; nothing snaps. The menu fades in when the level opens.
- **Sounds:** from one steampunk-clock recording (`Main_Menu_Button_Click`): a click plays its high tick, moving the cursor onto a row plays its lower, quieter tock (once per row entered; not on the disabled Continue). Silent while SFX is off.
- **Mouse only:** hovering an item selects it, LMB confirms. No keyboard navigation. The cursor is drawn inside the menu so it is filtered too.
- **Options:** three switches, each flipped by a click and saved for the player at once: **Music** ON/OFF, **SFX** ON/OFF, **Tutorial** YES/NO (all on by default). SFX already mutes the menu click; there is no music or tutorial yet, and when they are added they must honour these switches.
- **Returning to the menu** (Quit to Menu, end of run) reloads the `MainMenu` level; the run's state is lost unless it was saved.

| Parameter | Value | Note |
| --- | --- | --- |
| Camera framing | location (-3000, -750, 520), pitch 2°, FOV 50° | TBD, tune by eye |
| Balloon facing | yaw −76° | The propeller faces away from the camera, hidden behind the basket |
| Time of day | night: moon 0.65 lux (cool), sky light 0.15, teal distance fog; night sky with stars and a moon disc (`MI_MenuNightSky`) | Menu only. TBD |
| Menu atmosphere | volumetric fog on (scattering 0.6, extinction ×2, from 15 m to 200 m) | Menu only; gameplay levels keep volumetric fog off. TBD |
| Searchlights | 2 narrow beams (4° cone, 3000 cd) from behind the camera, out of frame, grazing the balloon | Soft light on part of the balloon, never full. TBD |
| Background | `SM_Factory_B` ×2 (chimney smoke), `SM_JammerTower`, 11 × `SM_Pine_Tall_B`, 150–250 m away | Hidden in the smoke: only the upper parts (chimneys, pine tops, mast) show. TBD |
| Smoke | 12 Local Fog Volumes, faintly glowing: a low band along the horizon and banks around the background | Swallows the bases; static. TBD |
| Menu colours | `AMainMenuScreen` Brightness 0.45 | Balances the unlit menu against the scene's exposure. TBD |
| Click / hover sound | high tick: from 0.09 s, 0.34 s long; low tock: from 1.12 s, 0.24 s long; 0.04 s fade each | `UMainMenuWidget` ClickSound / HoverSound. TBD, tune by ear |
| Balloon bob | 15 cm, 8 s period | TBD |
| Balloon roll / yaw sway | 1.5° / 12 s, 3° / 24 s | TBD |

## Pause menu
Esc pauses the game and shows Resume · Options · Quit to Menu on the same kind of filtered panel, attached to the player's camera. Options is the same widget as in the main menu.

## Navigation panel (in the basket)
The panel in front of the player, through which the [balloon is controlled](balloon-controls.md).

| Gauge | Shows | Look |
| --- | --- | --- |
| Radar | Enemy objects on the map; distance marks to the airship | Retro two-colour monitor |
| Speedometer | Current speed | Minimalist, like on old trains |

The speedometer reads the current speed from the flight component, which already includes inertia; no extra smoothing is needed. Needle position: `Clamp(CurrentSpeed / Scale, 0, 1)`. Zero on the scale is 0 cm/s, so at minimum speed the needle stays right of zero.

| Speedometer parameter | Value | Note |
| --- | --- | --- |
| `SpeedometerMaxSpeed` | 0 | 0 = use `MaxSpeed`; a positive value sets the full scale in cm/s. Needs tuning. |

In the City biome the radar is jammed, see [biomes.md](biomes.md). The engine's gauges are on the engine itself, see [engine-stove.md](engine-stove.md).

## Open questions
- [ ] A detailed breakdown of the navigation panel: the full set of elements and how they look (TODO in the Director's Cut).
- [ ] Meta-progression between runs (the old `Legacy` item) is no longer in the menu; is it needed at all?
- [x] Pause menu: a filtered panel fixed to the camera (see above).
