# Crosshair

> **Status:** In progress · **Owner:** Sokin · **Updated:** 2026-11-16

## Why
A simple dynamic crosshair: it shows where the shot will go and how accurate it is right now, and feels "alive": it reacts to the balloon, sway, recoil, weapon switches and camera turns.

## How it works
- Look: in the [art style](../art-style.md): big low-res pixels (2× upscale), a dark outline, colour quantisation with Bayer dithering, dithered translucency, grain. Bone-white colour.
- Shape: four ticks and a dot in the centre.
- **The dot is always where the shot goes** (recoil, sway and the balloon turn the view itself).
- **Aiming down sights (ADS, RMB):** the crosshair, dot included, fades out smoothly while the weapon rises to the eye (you aim with the front sight); when the weapon is lowered, it fades back in.
- The ticks spread apart (the gap):
  - by the weapon's spread at true scale (revolver wider, rifle almost closed, shotgun widest with the pellet fan);
  - from the recoil of each shot;
  - from camera turns (mouse, balloon turning, sway);
  - from the balloon's speed and jolts;
  - on a weapon switch: they spread and fade while the weapon is lowered, and close in with the new one.
- **Inertia:** the ticks hang on a spring. When the camera turns they lag behind; when the turn stops they overshoot and settle. The balloon's acceleration (speeding up, braking, climbing, descending) throws them the opposite way.
- Empty hand: just the dot. Over something you can interact with or grab (a balloon lever, an item on the basket's racks), and while dragging a lever, the dot opens smoothly into **four corner brackets**: it swells into a small square, the sides open in the middle as it grows, and the brackets snap a little past their size and settle. The dot stays in the middle. Looking away closes them back into the dot.
- Item in hand over a rack where a click would put it back or swap it: the same brackets open around the ticks. A rack shows them only when the click would do something (an empty rack with an empty hand doesn't).
- A weapon with no round loaded (or the hook below 100): the ticks are translucent; also while reloading.

## Parameters
| Parameter | Value | Note |
| --- | --- | --- |
| Crosshair pixel size | screen height / 540 | `VirtualHeight`, 2× upscale from the art style |
| Colours, tick length/thickness, grain | bone white / dark olive outline, 5 / 1, 6 % | `MI_Crosshair` |
| Spread from recoil, turns, balloon, weapon switch | TBD | `ACrosshairHUD`, starting values in [decisions](../decisions.md) |
| Interact brackets: size, arm length, thickness, time, snap | TBD | `InteractBracketHalfSize` 6 px (13×13), `InteractBracketLength` 3 px, `InteractBracketThickness` 1 px, `InteractMorphTime` 0.15 s, `InteractOvershoot` 1.7 |
| Inertia: lag, spring frequency, damping, maximum | TBD | `TurnInertia` 0.05 px/(°/s), `InertiaFrequency` 1.8 Hz, `InertiaDamping` 0.35, `MaxInertia` 16 px |

## Dependencies
[Weapons](weapons.md), [Grappling hook](grappling-hook.md), [Balloon controls](balloon-controls.md), [Art style](../art-style.md).

## Open questions
- [ ] Should the dot also sway with inertia (then it no longer shows exactly where the shot goes)? Right now only the ticks do (`bInertiaMovesDot`).
- [ ] A separate crosshair shape for the hook / shotgun?
- [ ] An interaction prompt next to the brackets (key + action + item name, e.g. "LMB  Take  Revolver")? Interactables don't provide this text yet.
