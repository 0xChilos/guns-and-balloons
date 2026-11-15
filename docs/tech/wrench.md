# Wrench — technical reference (C++)

> **For agents.** How the wrench is built. Design lives in [wrench](../features/wrench.md); if the code and the design disagree, the design wins. The gun base it shares is in [weapons](weapons.md). **Updated:** 2026-11-15

## Files
| File | Role |
| --- | --- |
| `Source/gunsandballoons/WrenchComponent.h/.cpp` | `UWrenchComponent : UGunComponent`: equip/ready/fix state machine, rest-pose search in the Fix clip, stove repair trace |
| `Source/gunsandballoons/StoveComponent.h/.cpp` | `ApplyRepair` / `AcceptsRepairHit`: the stove-only repair entry point the wrench calls |
| `Source/gunsandballoons/Tests/WrenchTest.cpp` | `GunsAndBalloons.Wrench.FixCycle` (rest times, hold/release, release during ToFix, holster/cancel); `GunsAndBalloons.Wrench.StoveRepair` (turn ends, LMB priority on the stove, repair per turn on target, in reach, up to full); `.BalloonStoveTarget` (real `BP_Balloon`: aimed from the basket at every hole, the trace hits a stove bone); `.BalloonHoles` (real `BP_Balloon` and its pawn in a playing world: damage opens holes, 3 s of turning closes two, the finished turn after release the third) |
| `Scripts/Editor/import_wrench.py` | Imports `T_Wrench` (nearest / no mips / UserInterface2D), `SK_Wrench`, `M_Wrench` (DiffuseColorMap = `T_Wrench`) and the four clips. Safe to re-run |
| `Scripts/Editor/fbx_takes.py` | Splits a binary FBX into one file per take (pure Python, byte-exact round trip) |
| `Content/Assets/Wrench` | `Wrench.fbx` (artist's source), `SK_Wrench`, `SK_Wrench_Skeleton`, `SK_Wrench_PhysicsAsset`, `M_Wrench` |
| `Content/Animations/Wrench` | `A_Wrench_<Take>.fbx` (generated from `Wrench.fbx`), clips `SK_Wrench_Equip/Idle/ToFix/Fix` |

## Rig and import
`Wrench.fbx` is one rigid mesh, no bones; its takes animate the mesh node. Interchange makes it a skeletal mesh with one bone, `wrench`, and every clip keys that bone absolutely (local to the component). Interchange imports only the **first** take of such a mesh, so `import_wrench.py` splits the takes (`fbx_takes.split_takes`) and imports each as animation only onto `SK_Wrench_Skeleton`. Two Interchange quirks are handled there:
- A channel a take keeps constant falls back to the node's rest value, not the take's: each split file's node rest transform is set to that take's first frame.
- The default range trims held frames at the end: clips import with `SET_RANGE` over the take's full length.

Re-export from the DCC → overwrite `Content/Assets/Wrench/Wrench.fbx` (not `Content/Animations/Wrench`, which holds only the generated takes) and re-run the script.

## Axes and the view pose
The artist works in Blender with a wall plane (XZ, Y = 0, not exported) that the wrench lies flat against in Fix, seen from the wall's open side, looking along +Y; the origin is the nut axis. The export is "forward −Y, up Z" (a 180° turn about Z, written into the FBX axis settings); Interchange undoes that and mirrors Y, so in the component's space UE = Blender with Y negated, and the artist's view looks along −Y with +X to the right. `ViewRotation` yaw +90° turns that view onto the camera's forward axis, so the face and the Fix swing are square to the screen. Check after a re-export: the old reference pose (Equip's first frame, Blender (10, −8, −17)) gave `SK_Wrench` bounds centred at Y = +8.

## How it works
- **Input** (pawn): taken from its rack with LMB (`UWeaponRackComponent`, see [weapons](weapons.md)). LMB held → `SetTriggerHeld(true)`; `PullTrigger` is unused.
- **State** (`EWrenchAction`): Equip → Idle (looped) → ToFix → Fix (looped) → FinishTurn → FromFix (ToFix backwards) → Idle; Unequip = Equip backwards, holds at its first frame. `OnFixStarted` when ToFix reaches the work (or LMB is pressed again during FinishTurn), `OnFixStopped` on release or holster.
- **FinishTurn:** at BeginPlay the Fix clip is sampled at 60 Hz; samples within `FixRestTolerance` (0.1° / 0.1 cm) of its first frame are rest times (here ~0, 1.25, 2.25, 3.25, 4.25 s, plus the end). On release the clip plays on to the next one.
- **Crosshair pin:** `GetCrosshairPin()` (override of `UGunComponent`'s, 0 for other items): 0 in Equip/Idle/Unequip, SmoothStep of ToFix's time through ToFix and FromFix, 1 in Fix and FinishTurn. The pawn (`UpdateWeaponSway`) scales the hold's Y/Z by `1 − pin` and the look lag and idle bob by the same, so at 1 the component's origin (the nut, where the Fix clip keeps the jaw) is on the camera's forward line, under the crosshair dot (`ACrosshairHUD` always draws it at the screen centre). The sway rotation stays; it pivots on that origin.
- **Swap:** `AnimatesOwnSwap()` = true; `PrepareToHolster` releases, the wrench returns to Idle, then plays Equip backwards; `IsReadyToHolster` at time 0. `CancelHolster` plays Equip forward from where it got.
- **Repair, one hole per turn:** `FindFixRestTimes` also finds `FixTurnEndTimes`, where a rest starts after a swing (a gap in the rest samples; here ~1.25, 2.25, 3.25, 4.25 s; the opening hold isn't one; no clip data → the clip end). Each tick in `Fix` or `FinishTurn` (up to `FinishTime`) counts the turn ends passed (`CountTurnsEnded`, wrap-safe) and for each calls `ApplyTurnRepair`: a line trace from the camera along its forward vector (the jaw is pinned there) for `RepairRange` on `RepairTraceChannel`, ignoring the owner. The first blocking hit, if the hit actor has a `UStoveComponent` whose `AcceptsRepairHit` accepts it (same rule as the extinguisher's cooling: `CoolingBones` `stove` / `furnace_door`, or `FireboxShape`), calls `Stove->ApplyRepair(RepairPerTurn, Owner)` → `UHealthComponent::Repair`; `OnTurnRepaired` fires if health came back. `OnHealthChanged` then drives BP_Balloon's `UpdateDamageHoles` (10 HP per hole, so exactly one hole closes, newest first) and `UStoveVfxComponent`'s hole smoke.
- **LMB priority:** `ShouldUseInsteadOfInteract` is true on a stove repair hit, so the pawn skips `TryInteract` (no door toggle) and passes the held LMB to the wrench.

| Property | Default | Notes |
| --- | --- | --- |
| `RepairPerTurn` | TBD 10 HP | One bullet hole per ratchet turn (BP_Balloon: a hole per 10 HP lost) |
| `RepairRange` | TBD 200 cm | From the camera; the pawn's `InteractRange` is 400 cm, so a click on the stove between 2 and 4 m turns the wrench without repairing |
| `RepairTraceChannel` | Visibility | Must be blocked by the stove bones and `StoveFirebox` |

## Known limitations
- Repair has no sound or VFX of its own yet; hook them to `OnTurnRepaired` (and `OnFixStarted` / `OnFixStopped`). Repair uses nothing up.
- View pose (`ViewOffset` (28, 8, −4), yaw +90°) is first-pass; no sound. At Equip's first frame the handle's tip is already inside the view (the clip starts with the jaw below the screen and the handle raised); it pops in and out with the swap. Lower `ViewOffset.Z`, or have the artist start Equip lower, if that shows.
- The rack shows `SK_Wrench` in its reference pose, which is the node's rest pose as exported (now a Fix pose, face-on). `place_item_pickups.py` places the pickup from the mesh bounds, so re-run it (`ITEM_PICKUP_ONLY=Wrench`) after a re-import that moves the rest pose.
