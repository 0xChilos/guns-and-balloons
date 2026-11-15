# UI, menus and the navigation panel

> **Status:** In progress (main menu: scene done, widget in progress; pause menu: built, not yet tested in game; death screen: built, checked in PIE) · **Owner:** Rostik · **Updated:** 2026-11-15

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
- **Layout:** top left, the game's name **GUNS AND BALLOONS** (large, on two lines) with **DIRECTOR'S CUT** under it, in the items' font and muted colours (the title is the items' parchment, the subtitle a dim brass), so it reads without being bright; the item list sits below it on the left, with no box: the items float on a soft dark haze from the left edge; a small `V1.0 RELEASE` sits bottom right (from the project version).
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
| Title | name: Bold 92 from (136, 72) px; subtitle: Bold 34, wide letter spacing | `UMainMenuWidget` GameTitle…, GameSubtitle…. TBD, tune by eye |
| Menu colours | `AMainMenuScreen` Brightness 0.45 | Balances the unlit menu against the scene's exposure. TBD |
| Click / hover sound | high tick: from 0.09 s, 0.34 s long; low tock: from 1.12 s, 0.24 s long; 0.04 s fade each | `UMainMenuWidget` ClickSound / HoverSound. TBD, tune by ear |
| Balloon bob | 15 cm, 8 s period | TBD |
| Balloon roll / yaw sway | 1.5° / 12 s, 3° / 24 s | TBD |

## Pause menu
Esc (or P) pauses the game; the same key, or Return to Game, resumes it.

| Item | Action |
| --- | --- |
| Return to Game | Close the menu and unpause |
| Quit to Main Menu | Reload the `MainMenu` level; the run is lost |
| Leave the Game | Quit the game |

- **Not a level:** a filtered panel in front of the player's camera, spawned in the gameplay level, so the frozen moment stays behind it and the run's state is kept.
- **Look:** the main menu's font, rows, hover, sounds and pixel cursor, with less detail: a centred **PAUSE MENU** title with a thin rule under it at the top, the three items centred below it, no haze, no version label. The crosshair is hidden while paused.
- **Background:** the whole screen is smoke, but see-through: three layers of swirling smoke drift and rise at different speeds over the frozen frame, slowly thickening and thinning. No part is ever clear, it is a little denser towards the edges, and the game stays visible through it everywhere.
- **Mouse only**, like the main menu. In the editor Esc stops Play In Editor, so use P there.
- **Hint:** during play a small key cap `[P] PAUSE` sits in the bottom-right corner, filtered like the menus. It is bright for the first 10 s, then dims but stays; hidden while the menu is open.

| Parameter | Value | Note |
| --- | --- | --- |
| Veil | dark teal, alpha 0.22 | `UPauseMenuWidget` VeilColor; only so the text reads. TBD |
| Smoke | grey (0.16, 0.17, 0.16); layers: tile 1700 / 1050 / 640 px, drift (−14, −6) / (22, −14) / (−30, −24) px/s, opacity 0.30 / 0.26 / 0.20 | SmokeColor, SmokeLayers. Together about 0.3–0.6 opaque. TBD |
| Thinnest smoke | 40 % of the densest | SmokeFloor: keeps the whole screen smoky. TBD |
| Edge smoke | up to 0.45 extra in the corners | EdgeSmoke. TBD |
| Smoke breath | −20 % over 7 s, layers out of step | SmokeBreath, SmokeBreathPeriod. TBD |
| Hint | bright 10 s, then opacity 0.4; 64 / 48 px from the corner | `UPauseHintWidget` BrightTime, DimmedOpacity, Margin. TBD |
| Layout | title at 230 px, items from 470 px (of 1080) | TitleTop, ListTop. TBD |
| Fade-in | 0.25 s | TBD |
| Keys | Esc, P | `ABalloonPlayerController` PauseKeys. TBD |

## Death screen
Shown when the engine is destroyed (`UEngineHealthComponent`: enemy hits, the airship's turrets). Team decision 2026-11-15: of three drafts (dark red smoke, blackout, silent-film title card) the title card was picked.
- **Death sequence:** the stove goes up in a big blast and the world runs at 0.3 speed for 2 s; the hands stop, the pause hint disappears, P/Esc do nothing. Then the world pauses under the screen.
- **Screen:** a silent-film title card over the frozen game, which is washed in sepia with film grain and dark corners; the grain jumps ~6 times a second (was 12); the card itself stays steady, without flicker (2026-11-15). A double line border around a dark field holds three things, all in English:
  - the title **ENGINE DESTROYED**;
  - what destroyed the engine, from the last hit: SHOT DOWN BY A TOWER CANNON, SHOT DOWN BY A CROP DUSTER, HIT BY THE AIRSHIP'S ROCKET, BLOWN UP BY A PARACHUTE MINE, HIT BY AN AA MISSILE, BROUGHT DOWN BY A FLOCK OF BIRDS, CAUGHT BY THE NEMESIS AIRSHIP (its turrets), otherwise THE ENGINE GAVE OUT;
  - one row, **TRY AGAIN**, which restarts the level. It takes a click only after 1 s, so panicked clicking from the fight can't press it.
- Same font, rows, hover, sounds and pixel cursor as the other menus; mouse only. The panel is masked and dithered (PS1 stipple instead of soft transparency).
- No run statistics (decided 2026-11-15).
- **Test level:** `/Game/Scenes/DeathScreen_Test` (built by `Scripts/Editor/create_death_screen_test_level.py` from Enemies_Test): an `ADeathScreenPreview` plays the death 6 s after Play (`Delay`, `Cause`), and TRY AGAIN restarts the level, which shows it again. In any gameplay level the console command `DeathScreen` does the same.

| Parameter | Value | Note |
| --- | --- | --- |
| Slow motion | 2 s at 0.3 speed | `ABalloonPlayerController` DeathSlowMoTime, DeathTimeDilation. TBD |
| Blast | `NemesisFx::FineExplosion` × 2.5 at the stove | TBD |
| Click delay | 1 s | `UDeathScreenWidget` InputDelay. TBD |
| Film | grain 0.18, 6 frames/s, sepia wash | GrainOpacity, FilmFrameRate, SepiaColor. TBD |

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

**Radar jamming.** Near a jammer tower the radar turns into static: flickering snow, torn rows, a stuttering sweep and ghost targets jumping around the screen. The real targets disappear, so the player can't tell where attacks come from. The state builds up as the balloon gets closer to the jammer (full from 25 % jammer strength, TBD) and clears when the balloon leaves its radius or the jammer is shot down. The warning lamp keeps working: it shows how close the threat is, not its direction. In the City biome the radar is jammed, see [biomes.md](biomes.md). The engine's gauges are on the engine itself, see [engine-stove.md](engine-stove.md).

## Open questions
- [ ] A detailed breakdown of the navigation panel: the full set of elements and how they look (TODO in the Director's Cut).
- [ ] Meta-progression between runs (the old `Legacy` item) is no longer in the menu; is it needed at all?
- [x] Pause menu: a filtered panel fixed to the camera (see above).
- [ ] Pause menu: should Options come back (Music/SFX/Tutorial while playing)? Left out at the user's request for now.
