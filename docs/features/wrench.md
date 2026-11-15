# Wrench

> **Status:** In progress · **Owner:** TBD · **Updated:** 2026-11-15

## Why
A hand-held utility for repairs: the player takes the wrench and turns it like a real one. What exactly it repairs isn't decided yet (see open questions).

## How it works
- The wrench is gear that sits in its own place, like the [weapons](weapons.md) and the [fire extinguisher](fire-extinguisher.md): LMB on it with an empty hand takes it, LMB on its empty place puts it back, LMB on another item swaps.
- In the hand it is drawn with its own animation (Equip) and put away with the same one played backwards (the model has no separate Unequip). While doing nothing it sways gently (Idle).
- **LMB held:** the wrench moves into working position (ToFix) and turns (Fix, four ratchet turns, looping while held).
- **Release:** the wrench finishes the turn already started back to its rest pose and moves out of working position (ToFix backwards) into Idle. Released during ToFix: it moves back right away from wherever it got to.
- The wrench doesn't shoot, has no recoil and doesn't aim (RMB does nothing).
- For now the wrench only animates: there is no repair.

## Interaction (diegetic)
| Action | Result |
| --- | --- |
| LMB on the wrench (empty hand) | Take the wrench (Equip) |
| Hold LMB | The wrench moves to work and turns |
| Release LMB | Finishes the turn and returns to Idle |

## Parameters
| Parameter | Value | Note |
| --- | --- | --- |
| Animations | Equip 0.96 s, Idle 2 s, ToFix 0.67 s, Fix 4.63 s | From `Wrench.fbx` (24 fps) |
| Pose in the hand | TBD | `ViewOffset` (30, 14, −12), rotation −4° (placeholder) |
| Place in Movement_Scene1 | TBD | Under the extinguisher, to the right of the player (`place_item_pickups.py`) |

## Dependencies
[Weapons](weapons.md) (take/put back, shared base), [Safe zone](safe-zone.md) (balloon repairs), [Stove engine](engine-stove.md).

## Open questions
- [ ] What does the wrench repair (the envelope, the stove, the basket's mechanisms), and how does the player see the repair happening?
- [ ] How long does a repair take, and does it use anything up (repair materials from the hook)?
- [ ] The ratchet sound.
