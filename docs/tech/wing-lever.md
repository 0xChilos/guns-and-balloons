# Wing lever and grab — technical reference (C++)

> **For agents.** How the drag-controlled wing levers work, their API, and how to hook other systems into them. Design lives in [balloon-controls](../features/balloon-controls.md) and [weapons](../features/weapons.md); if the code and the design disagree, the design wins. Flight-side API: [balloon-flight](balloon-flight.md). **Updated:** 2026-11-14

## Model in one paragraph
Each side of the basket has one `UBalloonWingLeverComponent` (Left/Right). The player (empty hand only) holds LMB on the lever handle and moves the mouse. The handle follows the hand, stops in a **detent** at every state, and has to be pulled with a firm motion to escape it. The lever angle drives the **wing target angle**; the wing catches up at `WingMoveSpeed`; the **actual wing angle** decides turn and braking, published to `UBalloonFlightComponent` as a named modifier. A released lever returns to the last detent it stood in. An open lever may randomly **slip** fully closed.

```
mouse -> Pawn (UpdateHeldGrab) -> IGrabbable::UpdateGrab -> ApplyDetents -> LeverAngle / Position
      LeverAngle -> (ABP reads GetLeverAngle)
      Position/Target -> WingAngle (constant rate) -> SampleEffectByWingAngle -> SetFlightModifier(Name, ±Turn, SpeedMult)
```

## Files
| File | Role |
| --- | --- |
| `Source/gunsandballoons/Grabbable.h` | `IGrabbable` interface (BlueprintNativeEvent) |
| `Source/gunsandballoons/BalloonWingLeverComponent.h/.cpp` | Lever state, detents, wing, slip, flight modifier |
| `Source/gunsandballoons/FirstPersonCameraPawn.h/.cpp` | Grab start/hold/end, hand state (`Armed/Holstering/Empty/Drawing`), look freeze |
| `Source/gunsandballoons/WeaponRackComponent.h/.cpp` | Rack that holsters/draws the revolver (needed to get an empty hand) |
| `Source/gunsandballoons/Tests/WingLeverSlipTest.cpp`, `PlayerGrabTest.cpp` | Automation tests (`GunsAndBalloons.WingLever.Slip`, `GunsAndBalloons.Player.GrabLifecycle`) |
| `Scripts/Editor/test_wing_api.py`, `test_wing_pie.py`, `test_phase5_interactions.py`, `configure_wing_levers.py` | Editor Python checks / setup |

## Player flow
1. Hand must be `Empty`: click the weapon rack with a gun (`HolsterWeapon`), wait `HolsterTime` (0.35 s, TBD).
2. LMB down on the lever bone (`lever_L` / `lever_R`) or `InteractShape` → pawn tries `IGrabbable` on every component of the hit actor, then the actor. First `CanGrab == true` consumes the click (no interact, no shot).
3. While LMB is held: mouse look is paused; mouse delta → `(ViewRight*DX + ViewUp*DY) * GrabDragScale` (cm) → `UpdateGrab`.
4. Release when: LMB up, hand state leaves `Empty`, camera-to-grab-point distance > `GrabBreakDistance` (0 → `InteractRange * 1.25`), `UnPossessed`, `EndPlay`, or the target reports `IsGrabbed() == false` (self-release by slip).
5. With a gun in hand LMB on the lever is a normal shot. There is no tap-to-step.

## `IGrabbable` contract
| Function | Meaning |
| --- | --- |
| `CanGrab(Pawn, Hit)` | Filter the hit + availability. Lever: bone/shape match, active, has `Flight`, ≥2 states, pawn `IsHandEmpty()`, not already grabbed |
| `BeginGrab(Pawn, Hit)` | Start holding; lever stores the grab point in bone space |
| `UpdateGrab(Pawn, WorldDragDelta)` | World-space cm displacement in the camera plane |
| `EndGrab(Pawn)` | Release (also for interruptions); must be idempotent |
| `GetGrabLocation()` | World point for break-distance check |
| `IsGrabbed()` | **Must** report true while held, otherwise the pawn drops the grab next frame |

To make a new grabbable object: implement the interface on an actor or component (C++ `_Implementation` overrides, or in Blueprint), keep a held flag, and return it from `IsGrabbed`. The pawn needs no changes.

## `UBalloonWingLeverComponent` API
Units: degrees, deg/s, seconds. State index `0` = closed, `N-1` = fully open.

| Member | Kind | Behaviour |
| --- | --- | --- |
| `SetState(int)` | BlueprintCallable | Clamp, set `TargetPosition = LastDetent`; lever and wing then move at their speeds. Clears detent/strain. Intended for BP/debug; while grabbed the lever visually stays under the hand until release |
| `StepState()` | BlueprintCallable | Next state per `StepMode` (`PingPong` / `Cycle`); not used by player input |
| `TrySlip(Chance)` | BlueprintCallable | Roll `Chance` (≤0 never, ≥1 always, non-finite never); on success `Slip()`. Returns whether it slipped. For wind, hits, shaking |
| `Slip()` | BlueprintCallable | Close fully, bypassing intermediate detents at `SlipMoveSpeed`. `false` if state is 0, or grabbed and `!bSlipWhileGrabbed`. With `bSlipWhileGrabbed` it ends the grab first |
| `OnSlipped(From, To)` | BlueprintAssignable | Once per slip, after the target changes. For sound/shake/UI |
| `GetState()` | BlueprintPure | `LastDetent`: last detent the lever stood in (not the continuous position) |
| `GetNumStates()` | BlueprintPure | `States.Num()` (authoritative) |
| `GetPosition()` | BlueprintPure | Continuous `[0, N-1]`, derived from the real lever angle |
| `GetLeverAngle()` | BlueprintPure | For the ABP: real angle + `Clamp(strain, ±break) * DetentGive` (visual spring) |
| `GetWingAngle()` / `GetWingOpen01()` | BlueprintPure | Actual wing angle / normalised 0..1 |
| `IsGrabbed()` | C++ / interface | Held now |
| `FindWingLever(Actor, Side)` | static BlueprintPure | Resolve a lever by side; independent of component order or names |
| `RegenerateStates()` | CallInEditor | Rebuild `States` from the generator params (overwrites manual edits) |

### Configuration (all `// TBD: prototype value` unless noted)
| Group | Property | Default | Notes |
| --- | --- | --- | --- |
| Setup | `Side`, `LeverBone` (`lever_L`), `WingBone` (`stopper_L`), `InteractShape` | Left | Set per instance. Hit accepted by bone name or by `InteractShape` |
| Generator | `NumStates` | 4 | Clamp 2..256 |
| Generator | `LeverClosedAngle` / `LeverOpenAngle` | −20 / 70 | Must be strictly monotonic across states |
| Generator | `MaxWingAngle` | 90 | Wing 0→max, linear |
| Generator | `MaxTurnInput` / `OpenSpeedMultiplier` | 1 / 0.8 | Linear 0→max turn; speed ×1→×0.8 |
| Generator | `DetentBreakAngle` | 5 | Detent stiffness, copied into each state |
| Generator | `MaxSlipChance` | 0.05 | State i gets `i/(N-1) * Max`; state 0 = 0 |
| Per state | `States[i]` | generated | `LeverAngle, WingAngle, TurnInput, SpeedMultiplier, DetentBreakAngle, SlipChance`. **Source of truth**; editing any generator property regenerates and **overwrites manual edits** |
| Motion | `LeverMoveSpeed` / `WingMoveSpeed` | 400 / 60 °/s | Lever when released; wing always |
| Grab | `ReleaseMode` | `ReturnToLastDetent` | Or `SnapToNearestDetent`, `Free` |
| Grab | `LeverAxis` / `DragSign` | `(0,1,0)` / +1 | Bone-local rotation axis, drag direction. Flip `DragSign` if the handle moves opposite to the hand |
| Grab | `HandleLength` / `MinGrabRadius` | 20 / 2 cm | Radius fallback when grab point is near the pivot |
| Grab | `LeverDragMaxSpeed` | 400 °/s | Caps handle speed (limits "ripping" through detents) |
| Grab | `DetentStrainDecay` / `DetentGive` | 10 °/s / 0.2 | Strain decays while the mouse is still; visual spring |
| Slip | `SlipCheckInterval` / `SlipMoveSpeed` | 1 s / 600 °/s | |
| Slip | `bSlipWhileGrabbed` / `SlipSeed` | false / 0 | 0 = random seed, otherwise reproducible |
| Pawn | `GrabDragScale` / `GrabBreakDistance` | 0.5 cm per mouse unit / 0 (= `InteractRange*1.25`) | On the pawn |

## Detent rules (`ApplyDetents`)
- The handle moving across a state's angle in one frame **stops on the first** one (a flick over two detents stops on the first).
- Held in a detent: mouse movement accumulates in `DetentStrain`. Escape when `|strain| >= States[i].DetentBreakAngle`; the excess carries over. Strain decays at `DetentStrainDecay` while held, so slow creeping does not escape.
- End states are hard stops (no escape outward).
- A detent just escaped in a direction is skipped while continuing that way, so the handle is not recaptured.
- `LastDetent` changes only when a detent actually catches the handle. Released between detents → returns to `LastDetent`, so `GetState()` is stable.
- Wing angle follows `SampleByPosition(TargetPosition)`; while held in a detent the wing stays still (strain only affects the visual lever).

## Slip
Every `SlipCheckInterval`, an **ungrabbed lever settled exactly on its remembered detent** rolls `States[LastDetent].SlipChance`. Free-release rests between detents never roll. Slip is independent of balloon speed. The lever falls at `SlipMoveSpeed`; the wing and flight effects fall at `WingMoveSpeed`, so turn/braking decay smoothly.

## Flight integration
- Publishes `Flight->SetFlightModifier(GetFName(), ±Turn, SpeedMult)` only when values change. Left is negative turn, Right positive. `EndPlay` clears it.
- Effect is interpolated by the **actual wing angle**, so mid-transition changes stay continuous and the effect is constant while angles rest.
- Lever ticks in `TG_PrePhysics` and is a tick prerequisite of the flight component. The pawn ticks after flight, so a drag applied in frame N reaches flight in N+1 (the visual lever angle updates immediately).
- Both wings open = air brake (multipliers multiply, `MinSpeed` floor). A and D keys add independently to base `TurnInput`.
- ABP reads `GetLeverAngle()` / `GetWingAngle()`: lever local Roll = −LeverAngle (both sides); wing local Pitch = +WingAngle (Left) / −WingAngle (Right). Do not add independent BP control values.

## How to use it
**Blueprint (listen to slips):**
`Find Wing Lever (Actor = Balloon, Side = Left)` → `Bind Event to OnSlipped` → play sound / shake.

**C++ (wind gust that may rip levers loose):**
```cpp
for (EBalloonSide S : { EBalloonSide::Left, EBalloonSide::Right })
    if (auto* L = UBalloonWingLeverComponent::FindWingLever(Balloon, S))
        L->TrySlip(GustStrength01);   // 0..1, e.g. 0.3
```
**Read for UI/animation:** `GetWingOpen01()`, `GetState()/GetNumStates()`.
**Force a state in a test or cutscene:** `SetState(n)`.
**Change feel without code:** edit `States[i]` per state (e.g. stiffer or slippier detent `n`) *after* setting generator values; remember any later generator edit regenerates all states.

## Setup in `BP_Balloon`
Two `BalloonWingLeverComponent`s (`Side` Left/Right, bones `lever_L/R`, `stopper_L/R`), a `BalloonFlightComponent`, and a `WeaponRack` on `socket_small_weapon`. The physics asset exposes `lever_L/R` bodies so traces return `BoneName`. `Scripts/Editor/configure_wing_levers.py` sets the lever bones but does not touch newer fields — set them in the editor.

## Validation (2026-11-14)
- **Static review of the C++:** the paths below were read through and look consistent: detent capture/escape, release/return, slip (including slip while grabbed), publish-on-change, lifecycle. Float equality used for "settled at detent" (`Position == TargetPosition`, `LeverAngle == detent angle`) is exact by construction: `FInterpConstantTo` lands on the target and `PositionFromLeverAngle` returns an exact integer at detent angles.
- **Automation tests** `GunsAndBalloons.WingLever.Slip` and `GunsAndBalloons.Player.GrabLifecycle`: **Success** in the editor at 15:25 UTC, 2026-11-14. A fresh run queued from this session did not complete (the editor waits for ≥10 FPS while unfocused), so it is not counted.
- **Not covered by any automated test:** detent capture/escape and strain, release-to-last-detent, drag→angle conversion (`LeverAxis`, `DragSign`), flight modifier values, `SnapToNearestDetent`/`Free`. These are the phase 7 PIE checklist in [wing-lever-drag-plan.md](../wing-lever-drag-plan.md), **not yet run**.
- A gap in tests: no automation test uses a real skeletal mesh, so grab-point/tangent math is only verified in PIE.

## Known issues and pitfalls
- **Doc/code drift:** the drag plan and `balloon-controls.md` list `DetentBreakAngle` 12°, `DetentStrainDecay` 30 °/s, `DetentGive` 0.25, `LeverAxis (1,0,0)`, `HandleLength 42`, `LeverDragMaxSpeed 360`; the code has 5°, 10 °/s, 0.2, `(0,1,0)`, 20 cm, 400 °/s. All are TBD prototype values — pick one set and update the docs (design wins), or ask a human.
- `docs/decisions.md` has a corrupted last row (question marks from an encoding error) for the hand-state / weapon-rack decision; it needs rewriting.
- `SetState` while grabbed changes the target but the lever stays under the hand until release; `UpdateGrab` then overwrites `TargetPosition`.
- `AcceptsHit_Implementation` on the lever is a plain method, not an interface override (the lever no longer implements `IInteractable`).
- `BeginPlay` only **warns** about non-monotonic lever/wing angles; behaviour with such data is undefined.
- Drag delta latency: one frame to flight (see above).
- No replication.
