# Wrench

> **Status:** In progress · **Owner:** TBD · **Updated:** 2026-11-15

## Why
A hand-held tool for repairs: the player takes the wrench and turns it on the stove like a real one, closing the bullet holes and giving the balloon its health back.

## How it works
- The wrench is gear that sits in its own place, like the [weapons](weapons.md) and the [fire extinguisher](fire-extinguisher.md): LMB on it with an empty hand takes it, LMB on its empty place puts it back, LMB on another item swaps.
- In the hand it is drawn with its own animation (Equip) and put away with the same one played backwards (the model has no separate Unequip). While doing nothing it sways gently (Idle).
- **LMB held:** the wrench moves into working position (ToFix) and turns (Fix, four ratchet turns, looping while held).
- **Release:** the wrench finishes the turn already started back to its rest pose and moves out of working position (ToFix backwards) into Idle. Released during ToFix: it moves back right away from wherever it got to.
- From Equip to Fix the wrench is seen face-on, in front of the camera: the turn happens in the plane of the screen, with the handle reaching to the left (as the artist animated it). At rest (Idle) it is a little right of and below the centre, out of the way.
- **It works where the player aims:** as it moves into working position the jaw slides onto the crosshair dot and stays on it while turning, whatever the view does (no lag or bob; the hand only rocks it about the jaw). Off the work it slides back to the rest pose.
- **It repairs the stove.** The stove body is the balloon's health ([stove engine](engine-stove.md)); its bullet holes show the damage. **Each ratchet turn with the jaw on the stove (body, door or firebox) within reach closes one bullet hole** (the newest) and gives back 10 HP, so the holes go one by one and the more holes there are, the more turns the repair takes. A turn counts when it comes back to rest, including the turn the wrench finishes after LMB is released; moving onto the work doesn't repair, so a quick click that is released before the first turn ends does nothing. The first hole closes about 1.9 s after pressing (0.67 s onto the work + the first turn), then one per second or so. Nothing else is repaired (the stove is the only place to repair the balloon), and nothing is used up.
- With the wrench in hand, LMB on the stove turns the wrench there instead of opening or closing the door.
- The wrench doesn't shoot, has no recoil and doesn't aim (RMB does nothing).

## Interaction (diegetic)
| Action | Result |
| --- | --- |
| LMB on the wrench (empty hand) | Take the wrench (Equip) |
| Hold LMB | The wrench moves onto the crosshair and turns there |
| Hold LMB aimed at the stove | Every ratchet turn closes one bullet hole (+10 HP) |
| Release LMB | Finishes the turn and returns to Idle |

## Parameters
| Parameter | Value | Note |
| --- | --- | --- |
| Animations | Equip 0.96 s, Idle 2 s, ToFix 0.67 s, Fix 4.63 s | From `Wrench.fbx` (24 fps) |
| Repair per turn | TBD (10 HP = one bullet hole) | `RepairPerTurn`; the Fix clip has 4 turns (ending at ~1.25, 2.25, 3.25, 4.25 s), so 0 → 100 HP takes ~11 s of turning |
| Reach | TBD (2 m) | `RepairRange` from the camera (placeholder) |
| Pose in the hand | TBD | `ViewOffset` (28, 8, −4) cm, yaw +90° so it faces the camera (placeholder) |
| Place in Movement_Scene1 | TBD | Under the extinguisher, to the right of the player (`place_item_pickups.py`) |

## Dependencies
[Stove engine](engine-stove.md) (the balloon's health and bullet holes), [Weapons](weapons.md) (take/put back, shared base), [Safe zone](safe-zone.md) (balloon repairs).

## Open questions
- [x] What does the wrench repair? The stove (= the balloon's health); the player sees it as the bullet holes closing.
- [ ] Should a repair use anything up (repair materials from the hook), or be slower/limited during a fight?
- [ ] The ratchet sound, and feedback while repairing (sparks at the jaw?).
