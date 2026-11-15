# Balloon controls

> **Status:** In progress (wing brakes: Done; slipping C++: Done; pawn grab C++: Done; lever control by dragging: In progress, [plan](../wing-lever-drag-plan.md)) · **Owner:** Sokin · **Updated:** 2026-11-15

## Why
Flying should feel like piloting a heavy craft: speeding up and braking with inertia, the route planned ahead.

## How it works
- The balloon **always moves forward**; it can't stop completely.
- **Speed** is set on the control panel and depends on the state of the [stove engine](engine-stove.md).
- **Turning/tilt:** the player pushes the lever on the chosen side of the basket → an iron panel extends on that side and brakes against the air (like a kayak paddle) → the balloon tilts and turns that way.
- All changes of speed and direction have **inertia** (acceleration, not an instant change).
- Levers have a configurable array of states (4 by default). The lever and wing angles, effect, detent stiffness and slip chance are set per state.
- **The lever is controlled by dragging, with an empty hand only** (put the weapon away first, see [weapons](weapons.md)): aim at the handle, hold LMB and move the mouse. The lever follows the hand from any angle; the view is frozen meanwhile. There is no tap-to-step.
- **A detent at every state.** The handle stops in it; to go further you have to "pull" decisively with the mouse (slowly creeping doesn't break through). The end states are hard stops.
- The left lever opens its wing when the mouse is dragged to the right.
- **Didn't reach the next detent** and let go: the lever returns to the previous one (the last one it rested in).
- **Slipping:** a released open lever can slip on its own and close completely at once, even while resting in a detent. The chance depends only on the state (0 in "closed"), not on speed. Other systems (wind, hits) can trigger a slip with their own chance. While the player holds the lever, it doesn't slip.
- The effect changes smoothly with the wing's actual angle (the wing follows the lever with inertia) and is stable while the angle doesn't change. Turning force doesn't depend on speed. Both wings cancel turning and multiply braking, keeping the minimum speed. The panel's speed request is kept.
- The basket has **8 corners (octagonal)**.
- Collision follows the model with primitives: envelope spheres, separate walls, rim and floor of the hollow basket, blocks for the stove and the console. Translation checks all active primitives; the solid root `BalloonCollision` is disabled. Rotation is not collision-checked yet.

## Interaction (diegetic)
| Object | Action | Result |
| --- | --- | --- |
| Control panel in front of the player | Change speed | Smooth change of speed |
| Lever on the side of the basket | Hold LMB with an empty hand and drag | The handle moves from detent to detent, the brake panel extends, the balloon turns and brakes |

## Parameters
| Parameter | Value | Note |
| --- | --- | --- |
| Minimum speed | TBD | When fuel = 0 |
| Drag scale / break-off distance | TBD: 0.5 cm/mouse unit / InteractRange × 1.25 | Prototype in the pawn; `GrabDragScale`, `GrabBreakDistance` (0 = automatic from InteractRange), needs tuning |
| Panel extension speed | 60 °/s | Prototype, needs tuning; 0→90° in 1.5 s |
| States / wing angles | 4 / 0…90° | The array is editable; the generator is uniform |
| Lever angles from rest / speed | −20…+70° / 400 °/s | Prototype, needs tuning |
| Turn / wing speed multiplier | 0…1 / 1…0.8 | Prototype; both wings give ×0.64, not below MinSpeed |
| Detent stiffness | 12° of pull | Prototype, per state |
| Slip chance | 0…0.05 per check, once per 1 s | Prototype; 0 in "closed" |
| Handle slip speed | 600 °/s | Prototype; all the way to state 0, no stops at detents; the wing closes at its own speed |
| Slipping while held / seed | false / 0 | Prototype; if allowed, the grab is released first; 0 = random seed, other values reproducible |
| Turn speed | TBD | |
| Inertia (acceleration/braking) | TBD | |
| Envelope spheres: centre Z / radius | 430/180, 550/290, 680/340, 810/300, 940/200, 1040/95 cm | Approximates the model's shape in Mesh space; needs visual tuning. |
| Basket | 8 box walls, 8 box rim sections, 4 box floor sections | Model as of 2026-11-15: octagon radius 97.5 cm, wall thickness 7 cm; inner apothem ≈83.08 cm. Rim: Z 96…109 cm, 15 cm thick. Empty interior. Sizes are set by `BalloonCollisionSetupLibrary`. |
| Stove / console: centre Y | +54.36 / −70.58 cm | Mesh space; stove block Z 7…121 cm. Checked against the current imported skeleton. |

## Dependencies
[Stove engine](engine-stove.md), [Navigation panel](ui-menus.md), [Biomes](biomes.md).

## Open questions
- [ ] What do "8 corners" mean: just the basket's shape, or 8 interaction points (levers)?
- [ ] Is there altitude control (up/down)?
- [ ] Does weight (cargo, resources) affect inertia? The notes mention "Weight".
