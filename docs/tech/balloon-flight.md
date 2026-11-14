# Balloon flight — technical reference (C++)

> **For agents.** How the flight code works and how to connect new systems to it. Design lives in [balloon-controls](../features/balloon-controls.md); if the code and the design disagree, the design wins. **Updated:** 2026-11-14

## Core rule
`UBalloonFlightComponent` is **the only thing that moves the balloon.** New control systems (speed panel, side levers, stove) never move the actor directly; they call `SetTargetSpeed` / `SetTurnInput`. The component knows nothing about input devices.

## Files
| File | Role |
| --- | --- |
| `Source/gunsandballoons/BalloonFlightComponent.h/.cpp` | Forward motion, speed and turn inertia, collision sweep |
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
`BalloonKeyboardControlComponent` → `BalloonFlightComponent` → `AFirstPersonCameraPawn`. Input applies in the same frame, and the player sees the balloon after it has moved. Any new ticking control source should also become a prerequisite of flight: `Flight->AddTickPrerequisiteComponent(this)`.

## Connecting a new system
- **Find the component:** `GetOwner()->FindComponentByClass<UBalloonFlightComponent>()` (C++) or `Get Component by Class` (BP) on the balloon actor. From the player, go through `GetAttachParentActor()`.
- **Speed panel:** `SetTargetSpeed(value)` or `SetTargetSpeed(GetTargetSpeed() ± step)`.
- **Side levers:** `SetTurnInput(±1)` on press, `SetTurnInput(0)` on release. Write only on state change, not every frame.
- **Stove (speed limit):** there is no runtime setter for `MaxSpeed` yet. Either clamp on the stove side before calling `SetTargetSpeed`, or add a public setter (e.g. `SetSpeedLimit`) — confirm with a human first.
- **UI / speedometer:** read `GetSpeedometer01()` / `GetCurrentSpeed()` / `GetCurrentTurnRate()`. There are no events/delegates — polling only.
- **Collision:** add/remove a `UShapeComponent` on the balloon actor — the sweep picks it up automatically, no code change needed. The root `BalloonCollision` is disabled (`NoCollision`). Regenerate primitives with `UBalloonCollisionSetupLibrary::AddBalloonCollisionPrimitives("/Game/Assets/Balloon/BP_Balloon.BP_Balloon")`, then compile and save the BP. Dimensions are in the Parameters table of [balloon-controls](../features/balloon-controls.md).
- **Things carried by the balloon** (player, cargo): attach them — attached actors are ignored by the sweep and don't block flight.

## Known limitations and pitfalls
- `BalloonKeyboardControlComponent` writes `SetTurnInput(0)` every frame when no key is held **and** when `bEnabled = false` — it will override the levers. Fix it (write only on state change) or remove the component from `BP_Balloon` before adding levers.
- In the keyboard component, `TurnDuration`, `TurnTimer`, `bPrevTurnLeft/Right` are unused; the header comment "while held" for W/S is wrong — speed changes per press.
- The player adds the flight tick prerequisite only in `BeginPlay`, and only if already attached. If it gets attached later, add the prerequisite there too, otherwise the camera lags a frame.
- After a head-on collision the balloon stays pressed against the obstacle at full `CurrentSpeed`; only turning frees it. Collision behaviour is not defined in the design.
- No replication, no altitude control (open design question).
- `UBalloonCollisionSetupLibrary` is an editor utility in the runtime module; in packaged builds it returns `false`.
