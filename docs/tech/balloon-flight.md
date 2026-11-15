# Balloon flight — technical reference (C++)

> **For agents.** How the flight code works and how to connect new systems to it. Design lives in [balloon-controls](../features/balloon-controls.md); if the code and the design disagree, the design wins. **Updated:** 2026-11-15

## Core rule
`UBalloonFlightComponent` is **the only thing that moves the balloon.** New control systems (speed panel, side levers, stove) never move the actor directly; the speed panel/keyboard call `SetTargetSpeed` / `SetTurnInput`, while persistent effects use named `SetFlightModifier` sources. The component knows nothing about input devices.

## Files
| File | Role |
| --- | --- |
| `Source/gunsandballoons/BalloonFlightComponent.h/.cpp` | Forward motion, speed and turn inertia, collision sweep |
| `Source/gunsandballoons/BalloonWingLeverComponent.h/.cpp` | Wing detents, transition angles, persistent modifiers and hit filtering |
| `Source/gunsandballoons/BalloonKeyboardControlComponent.h/.cpp` | Temporary keyboard control (W/S/A/D) for testing |
| `Source/gunsandballoons/BalloonCollisionSetupLibrary.h/.cpp` | Editor utility: generates the collision primitives in `BP_Balloon` |
| `Source/gunsandballoons/FirstPersonCameraPawn.cpp` | Player riding the balloon: tick order, view turns with the balloon |
| `Source/gunsandballoons/RevolverComponent.cpp` | Shots ignore the balloon the shooter stands in |
| `Content/Assets/Balloon/BP_Balloon`, `Content/Scenes/BalloonFlight_Test` | Balloon Blueprint and test level |

## `UBalloonFlightComponent` API
Units: cm/s, cm/s², deg/s, deg/s².

| Method | Kind | Behaviour |
| --- | --- | --- |
| `SetTargetSpeed(float)` | BlueprintCallable | Target speed, clamped to `[MinSpeed, MaxSpeed]`; `<= 0` → `MinSpeed`. The balloon can't stop or reverse. |
| `SetTurnInput(float)` | BlueprintCallable | Turn input, clamped to `[-1, 1]` (−1 left, +1 right, 0 straight). **Kept until the next call.** |
| `SetFlightModifier(FName, float, float)` | BlueprintCallable | Persistent source: turn addition clamped to [-1, 1], speed multiplier clamped to >=0. Replaces only this source. |
| `ClearFlightModifier(FName)` | BlueprintCallable | Removes one source. |
| `GetEffectiveTargetSpeed()` | BlueprintPure | `Clamp(TargetSpeed * product of multipliers, MinSpeed, MaxSpeed)`; preserves panel request. |
| `GetEffectiveTurnInput()` | BlueprintPure | `Clamp(TurnInput + sum of modifier turns, -1, 1)`. |
| `GetCurrentSpeed()` | BlueprintPure | Actual speed |
| `GetTargetSpeed()` | BlueprintPure | Last requested speed (already clamped) |
| `GetCurrentTurnRate()` | BlueprintPure | Actual turn rate |
| `GetSpeedometer01()` | BlueprintPure | `0..1` for the speedometer needle: `CurrentSpeed / SpeedometerMaxSpeed` (or `/ MaxSpeed` when 0) |

Parameters (`EditAnywhere`, protected, not changeable from BP at runtime): `MinSpeed`, `MaxSpeed`, `InitialSpeed`, `Acceleration`, `Deceleration`, `MaxTurnRate`, `TurnAcceleration`, `TurnDeceleration`, `SpeedometerMaxSpeed`. All are prototype values (`// TBD`); the design still lists them as `TBD`. Debug: `bDebugLog`, `bAutoTest` — keep `false` in normal play.

## What happens each tick (`TG_PrePhysics`)
1. Speed and turn rate move toward their targets at a **constant** rate (`FInterpConstantTo`, not exponential).
2. Forward move along the **horizontal** heading (yaw only). Every `UShapeComponent` on the actor with query collision enabled is swept separately; the move is cut at the earliest blocking hit. The owner and all attached actors (the player) are ignored. A primitive that starts inside an obstacle may still move *away* from it. No shapes at all → plain root sweep.
3. Yaw rotation around Z — **no collision check** (as per design).

## Tick order
`BalloonKeyboardControlComponent` and both `BalloonWingLeverComponent`s → `BalloonFlightComponent` → `AFirstPersonCameraPawn`. Input applies in the same frame, and the player sees the balloon after it has moved. Any new ticking control source should also become a prerequisite of flight: `Flight->AddTickPrerequisiteComponent(this)`.

## Connecting a new system
- **Find the component:** `GetOwner()->FindComponentByClass<UBalloonFlightComponent>()` (C++) or `Get Component by Class` (BP) on the balloon actor. From the player, go through `GetAttachParentActor()`.
- **Speed panel:** `SetTargetSpeed(value)` or `SetTargetSpeed(GetTargetSpeed() ± step)`.
- **Side levers:** `UBalloonWingLeverComponent` implements `IInteractable`. `SetState` / `StepState` choose a detent; `GetState`, `GetNumStates`, `GetLeverAngle`, `GetWingAngle`, `GetWingOpen01` expose it. `States.Num()` is authoritative. `RegenerateStates` and editor generator changes overwrite manual states. Defaults: 4, PingPong, lever -20…70°, wing 0…90°, turn 0…1, speed multiplier 1…0.8; prototype tuning values.
- **Wing transitions:** angles move at constant rates (400 / 60 °/s). Effects interpolate by wing angle from the actual current effect, keeping mid-transition clicks continuous. Equal wing angles apply the target effect immediately. Modifiers publish only on change, keyed by component name; EndPlay clears them. Left is negative, right positive. BeginPlay regenerates fewer than two states and warns on non-monotonic wing angles.
- **Component interaction:** pawn traces once, tries hit-actor components with `AcceptsHit(Hit) && CanInteract`, then retains actor/owner and ammo-supply behaviour. Default `AcceptsHit` is true; wing levers match the configured bone or optional `InteractShape`. `TraceAimedTarget` still supports ammo aiming.
- **Animation:** resolve cached components with `FindWingLever(Actor, Side)` and read their angles. Do not keep independent BP control values. The physics asset already contains lever_L/R bodies.
- **Imported axes:** ABP uses component degrees directly: lever local Roll = negative LeverAngle on both sides; wing local Pitch = positive WingAngle on Left, negative on Right. Legacy BP-to-ABP lever writers are disconnected. Components auto-activate so `CanInteract` accepts clicks.
- **Stove (speed limit):** there is no runtime setter for `MaxSpeed` yet. Either clamp on the stove side before calling `SetTargetSpeed`, or add a public setter (e.g. `SetSpeedLimit`) — confirm with a human first.
- **UI / speedometer:** read `GetSpeedometer01()` / `GetCurrentSpeed()` / `GetCurrentTurnRate()`. There are no events/delegates — polling only.
- **Collision:** add/remove a `UShapeComponent` on the balloon actor — the sweep picks it up automatically, no code change needed. The root `BalloonCollision` is disabled (`NoCollision`). Regenerate primitives with `UBalloonCollisionSetupLibrary::AddBalloonCollisionPrimitives("/Game/Assets/Balloon/BP_Balloon.BP_Balloon")`, then compile and save the BP. Dimensions are in the Parameters table of [balloon-controls](../features/balloon-controls.md).
- **Current model fit (2026-11-15):** basket circumradius 97.5 cm, 7 cm wicker walls, eight separate 15 cm rim boxes at Z 96–109 cm. Floor/wall extents derive from the octagon apothem. Stove Y = +54.36 cm, console Y = −70.58 cm; the envelope is unchanged. Generated boxes carry `BasketDamage`. `Scripts/setup_balloon_collision.py` applies the setup and checks alignment against the imported stove/display bones; rerun it after model changes.
- **Things carried by the balloon** (player, cargo): attach them — attached actors are ignored by the sweep and don't block flight.

## Known limitations and pitfalls
- **Pawn grab (phase 5):** LMB first checks `IGrabbable` components, then the hit actor, only with an empty hand. A successful `CanGrab` consumes the click before interaction/fire. `IsGrabbed` is a BlueprintNativeEvent; targets must report their held state. While held, mouse delta becomes `(ViewRight * DX + ViewUp * DY) * GrabDragScale`, without look sensitivity or Y inversion. Mouse look pauses; balloon yaw still carries the view. Release, distance, hand-state change, `UnPossessed` and `EndPlay` end the grab; destroyed targets are cleared, and self-released targets are cleared without a second `EndGrab`. `GrabDragScale = 0.5 cm/unit` is TBD; `GrabBreakDistance = 0` selects `InteractRange * 1.25` (TBD), positive values override it in cm. Flight runs before pawn, so input affects flight next frame; lever `UpdateGrab` already updates its angle immediately.
- **Wing slip API:** `TrySlip(Chance)` (clamped probability; 0 never, 1 always) and `Slip()` return whether the remembered open detent changed to 0. `OnSlipped(FromState, ToState)` fires once after the target changes. Both are blocked while grabbed unless `bSlipWhileGrabbed` is enabled, which ends the grab first. The lever bypasses intermediate detents at `SlipMoveSpeed` (600 deg/s); the wing and flight effects still follow `WingMoveSpeed`. Every `SlipCheckInterval` (1 s), only an ungrabbed lever settled at its remembered detent rolls `States[LastDetent].SlipChance`. `SlipSeed != 0` seeds an `FRandomStream` at BeginPlay; 0 generates a random seed. Generator `MaxSlipChance` defaults to 0.05; all new defaults need prototype tuning. Existing Blueprint overrides are preserved.
- Keyboard base input and wing modifiers are independent. Keyboard release/disable can reset base `TurnInput` without clearing either wing.
- In the keyboard component, `TurnDuration`, `TurnTimer`, `bPrevTurnLeft/Right` are unused; the header comment "while held" for W/S is wrong — speed changes per press.
- The player adds the flight tick prerequisite only in `BeginPlay`, and only if already attached. If it gets attached later, add the prerequisite there too, otherwise the camera lags a frame.
- After a head-on collision the balloon stays pressed against the obstacle at full `CurrentSpeed`; only turning frees it. Collision behaviour is not defined in the design.
- No replication, no altitude control (open design question).
- `UBalloonCollisionSetupLibrary` is an editor utility in the runtime module; in packaged builds it returns `false`.

## Verification

2026-11-15 collision model update: Editor Win64 Development build passed. `Scripts/setup_balloon_collision.py` regenerated and saved the Blueprint and cleared stale shape overrides in `BalloonFlight_Test`. `Scripts/Editor/validate_balloon_collision.py` scanned all three project maps and passed for the single placed balloon, checking primitive counts, wall/floor dimensions, damage tags and alignment with the current imported stove/display bones.
`Scripts/Editor/test_wing_api.py` checks source composition, clamps, state generation (2/4/6), PingPong and Cycle on transient components. `Scripts/Editor/test_wing_pie.py` runs 12 checks in `BalloonFlight_Test`, including actual lever traces/clicks, ABP angles, flight inertia/braking, actor/owner interaction regressions and EndPlay cleanup. It removes its temporary actors without saving the level. Last run: 2026-11-14, passed; Editor Win64 Development build passed.
