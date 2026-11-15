# Fire extinguisher

> **Status:** In progress · **Owner:** Sokin · **Updated:** 2026-11-14

## Why
A hand-held utility for putting out fires in the basket and on the balloon. The supply is limited and shown on the extinguisher itself, so every second of foam counts.

## How it works
- The extinguisher is gear that sits in its own place, like the [weapons](weapons.md): LMB on it with an empty hand takes it; LMB on its empty place puts it back. In the hand it sits where the weapons do; it is drawn and put away with its own animations (Equip / Unequip).
- **The supply is shown on a mechanical counter** on the cylinder: three digit drums 000–100 that roll like an odometer. The extinguisher is issued **full (100)**; the supply is kept when you put it away.
- **LMB held:** the extinguisher works: the lever is pressed (Spray_Start → Spray_Loop while held) and foam shoots from the nozzle. On release the lever returns (Spray_Stop).
- While working it uses **5 units per second** (continuously; a full one lasts 20 s). The counter counts down one unit at a time.
- **Empty (0):** if it runs out while spraying, or LMB is pressed at 0, the Empty animation plays (a dry attempt) and there is no foam.
- **Foam** (only while the supply > 0) is big, like from a real extinguisher: a dense white jet that immediately swells into ~45 cm wide puffs, surrounded by a cloud that spreads and melts away in ~1 s. The jet falls in an arc under its weight and **sticks where it lands** (on the basket it rides along with the balloon), lies in heaps for ~5 s and disappears.
- **Foam look**, per the [art style](../art-style.md): each puff is a clump of bubbles (low resolution, nearest), four flat tones (shadow between the bubbles, rim, bubble, a pixel highlight on one side), transparency via Bayer dithering (as it fades, a puff breaks into bubbles), motion at ~20 fps.

## Interaction (diegetic)
| Action | Result |
| --- | --- |
| LMB on the extinguisher (empty hand) | Take the extinguisher; the counter shows the supply |
| Hold LMB (supply > 0) | Foam from the nozzle, the counter goes down |
| LMB when empty | Dry attempt (Empty animation) |

## Parameters
| Parameter | Value | Note |
| --- | --- | --- |
| Starting supply | 100 | `InitialAmount` |
| Use rate | 5 / s | `UsePerSecond` |
| Foam jet | TBD | `FoamSpray`: 90 puffs/s, 9–12 m/s, 7° cone, puff 8→44 cm, gravity ×1, lies ~5 s (placeholders) |
| Cloud around the jet | TBD | `FoamMist`: 25 puffs/s, 4.5–8 m/s, 18° cone, puff 16→90 cm, lives ~1 s (placeholders) |
| Foam look | TBD | `AExtinguisherFoam`: bubbles (`BubblesPerPuff` 10, tones `GapShade`…`HighlightShade`), 32 px texture, up to 450 puffs, 20 Hz (placeholders) |
| Pose in the hand | TBD | `ViewOffset` (34, 16, −3), rotation −4° (placeholder) |

## Dependencies
[Weapons](weapons.md) (switching, shared base), [Stove engine](engine-stove.md) and [biomes](biomes.md) (what burns), [Safe zone](safe-zone.md) (refills).

## Open questions
- [ ] What exactly does the foam put out (fires in the basket, the stove, the envelope)? For now the foam is visual only; nothing is extinguished.
- [ ] Where is the supply refilled: the safe zone, a crate in the basket, a resource from the hook?
- [ ] Does the jet push the balloon / the player (recoil)?
