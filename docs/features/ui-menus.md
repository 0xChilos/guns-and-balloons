# UI, menus and the navigation panel

> **Status:** In progress · **Owner:** Rostik · **Updated:** 2026-11-14

## Why
As little non-diegetic interface as possible. During play, information comes from the gauges in the basket.

## Main menu
| Item | Action |
| --- | --- |
| Continue | Continue the last run |
| New Game | New run |
| Settings | Settings |
| Credits | Authors |
| Legacy | TBD (progress between runs?) |
| Leave | Quit the game |

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
- [ ] What is `Legacy`: meta-progression, run history, unlocks?
- [ ] Pause menu: diegetic or a regular one?
