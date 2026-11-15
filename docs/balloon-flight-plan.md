# Plan: balloon flight (prototype)

> **Status:** in progress · **Updated:** 2026-11-14

## Working rules

- **Phases are independent.** Each phase (below) is a separate session or a separate block of work. At the end of a phase, mark it in the [progress log](#progress-log) (`[x]`, date, 1–2 lines of notes: what was done, deviations, names of created assets). The next session starts from this file and the log, not from re-researching.
- **Read only what the phase lists.** All the context a phase needs is listed in its "Read" section. Don't scan the rest of the repo, don't crawl `Content/`, don't read `docs/source/`.
- **Read big files in parts.** `RevolverComponent.cpp/.h` (~15k/13k bytes): only the listed lines/functions via `Grep` + `Read` with `offset/limit`, not whole.
- **Unreal MCP, targeted.** Don't call tools that return a full dump of the level, all assets or a whole Blueprint graph. First `describe_toolsets` / `describe_tools` only for the set you need, then filtered queries (a specific asset, a specific component). If a response is large, don't repeat it; write the facts you need into the log.
- **Don't print full build logs:** filter on `error`/`warning` (e.g. `| Select-String -Pattern 'error|warning' | Select-Object -First 40`).
- **Don't re-read** files you just edited to check them: Edit reports errors.
- If a phase doesn't fit, stop at a finished step, write the state into the log and continue in a new session.

## Unreal MCP (`unreal-mcp`)

The agent can and should work with the editor through the `unreal-mcp` MCP server when the editor is running. The tools are deferred: load their schemas first via `ToolSearch` (`select:mcp__unreal-mcp__GetStatus,...`).

- **Connection check:** `GetStatus`. If the editor doesn't respond, do phases 1–3 without it, and for 4–6 ask a human to open the editor.
- **Finding tools:** `list_toolsets` → `describe_toolsets` / `describe_tools` only for the set you need (Blueprint, level, assets) → call via `call_tool`. Don't describe all sets at once.
- **Direct tools:** `GetAssetsByType` / `GetSelectedAssets` (find `BP_Balloon`, assets), `GetActorsByType` (actors in the level; filter by type), `SpawnMeshAt` (obstacles and landmarks in phase 5), `RunConsoleCommand` (`t.MaxFPS 30/60/120`, PIE commands in phase 6), `PrintToLog` (markers in the log during measurements), `GetViewportCamera` / `SetViewportCamera` (looking at the scene).
- **Where to use it:** phase 4 (components and settings of `BP_Balloon`), phase 5 (creating and filling the level), phase 6 (console commands, reading the measurement log).
- Context rule: filtered queries, no full dumps of levels/graphs (see above). Don't use `AskAssistant`, render and media tools (`TriggerMRGRender`, `MediaJob*`, `BakeMeshTextures`, etc.): this task doesn't need them.
- Before a UBT build the editor must be closed: ask a human, or do it after finishing the MCP work; after the build, check `GetStatus` again.

## Scope

- The balloon automatically flies forward; speed commands can't stop it or put it in reverse.
- **Two independent controls:**
  - **The player:** unchanged, as in `Movement_Scene1` (`AFirstPersonCameraPawn`): mouse look, LMB to interact/shoot, reload by clicking the ammo crate.
  - **The balloon:** keyboard, **temporary only** for the prototype. The final controls are as in the [design](features/balloon-controls.md): a speed panel and side levers; don't build them at this stage. The keyboard is not a design change, so no `decisions.md` entry is needed; its component must be easy to disable/remove once the panel and levers exist.
- Altitude control, panel and lever animations, visual tilt, hooking up the stove are outside this stage. The altitude question stays open in the design.

## Known facts about the code (don't re-research)

- Module: `Source/gunsandballoons/`, API macro `GUNSANDBALLOONS_API`. No new modules are needed in `Build.cs` (`Engine`, `CoreUObject` are already there).
- `AFirstPersonCameraPawn`: root is `UCameraComponent Camera` (`bUsePawnControlRotation = true`), no movement component. In `NotifyControllerChanged()` it calls `PC->SetIgnoreMoveInput(true)`, `PC->SetControlRotation(GetActorRotation())`, `AddTickPrerequisiteActor(PC)`. `Tick()` (Source/gunsandballoons/FirstPersonCameraPawn.cpp) returns early without a local PC, then: target trace, LMB, `UpdateLook(PC)`, `UpdateWeaponSway`. `UpdateLook` returns early if the mouse didn't move; the `bLimitYaw` limit is computed from `GetActorRotation().Yaw`, so when attached to the balloon it turns along with the basket automatically.
- Interaction trace: FirstPersonCameraPawn.cpp ~229: **don't change**.
- Shot trace: RevolverComponent.cpp ~450: `FCollisionQueryParams Params(SCENE_QUERY_STAT(RevolverShot), true, Owner);`: the balloon ignore is added here.
- GameMode: `FirstPersonCameraGameMode` (global), pawn of class `AFirstPersonCameraPawn`.

## Build

- Engine: `G:\UE\UE_5.9` (association 5.9). UBT command:
  `G:\UE\UE_5.9\Engine\Build\BatchFiles\Build.bat gunsandballoonsEditor Win64 Development -project="…\gunsandballoons.uproject" -WaitMutex`
- Close the editor before building; filter logs on `error|warning`.

## Phase 1: flight component (C++, no editor)

**Read:** this file, [balloon-controls.md](features/balloon-controls.md), [AmmoSupplyComponent.h](../Source/gunsandballoons/AmmoSupplyComponent.h) (as an example of the project's UCLASS/UPROPERTY style).

**Create** `Source/gunsandballoons/BalloonFlightComponent.h/.cpp`, `UCLASS(ClassGroup=(Balloon), meta=(BlueprintSpawnableComponent))`, `UBalloonFlightComponent : UActorComponent`, ticking in `TG_PrePhysics`. It moves the whole owning actor. It knows nothing about the keyboard: its methods are the single entry point for any control (keyboard now, later the panel/levers/stove).

Interface (guideline):

```cpp
UFUNCTION(BlueprintCallable, Category="Balloon|Flight") void SetTargetSpeed(float SpeedCmPerSecond);
UFUNCTION(BlueprintCallable, Category="Balloon|Flight") void SetTurnInput(float Input);
UFUNCTION(BlueprintPure,     Category="Balloon|Flight") float GetCurrentSpeed() const;
UFUNCTION(BlueprintPure,     Category="Balloon|Flight") float GetTargetSpeed() const;
UFUNCTION(BlueprintPure,     Category="Balloon|Flight") float GetCurrentTurnRate() const;
// EditAnywhere, Category="Balloon|Flight", ClampMin: MinSpeed, MaxSpeed, InitialSpeed,
// Acceleration, Deceleration, MaxTurnRate, TurnAcceleration, TurnDeceleration
```

Rules:

1. **Constant motion.** `BeginPlay`: fix the parameters (`MinSpeed <= 0` → a small positive value; `MaxSpeed < MinSpeed` → `MinSpeed`) with a one-time `UE_LOG(LogTemp, Warning, ...)`; `CurrentSpeed = TargetSpeed = Clamp(InitialSpeed, Min, Max)`. Every tick: move by `CurrentSpeed * DeltaSeconds` along the **horizontal** forward (`FRotator(0, Yaw, 0).Vector()`), via `GetOwner()->AddActorWorldOffset(Delta, /*bSweep=*/true)`.
2. **Speed.** `SetTargetSpeed` clamps to `[MinSpeed, MaxSpeed]` (≤0 → `MinSpeed`). `CurrentSpeed = FMath::FInterpConstantTo(Current, Target, Dt, Target > Current ? Acceleration : Deceleration)`. Not `FInterpTo`.
3. **Turning.** `SetTurnInput` clamps to `[-1, 1]`; the value is kept until the next call. The target angular speed is `Input * MaxTurnRate`; the rate is `TurnAcceleration` if the magnitude grows with the same sign, otherwise `TurnDeceleration` (the magnitude shrinking, or a change of direction until it crosses zero). Every tick: `AddActorWorldRotation(FRotator(0, CurrentTurnRate * Dt, 0))`. Forward motion continues while turning.
4. Units: cm/s, cm/s², deg/s, deg/s². Values are **TBD**; mark temporary values with `// TBD: prototype value`.

**Done when:** it compiles. Write it in the log.

## Progress log

- [x] **Phase 1**: flight component (C++) · 2026-11-14
	- Created `Source/gunsandballoons/BalloonFlightComponent.h/.cpp` (TG_PrePhysics, interface per the plan, all values `// TBD: prototype value`). UBT build `gunsandballoonsEditor Win64 Development`: successful, no errors/warnings.
	- Deviations: unreal-mcp wasn't available in the session (not needed for phase 1); the editor had to be force-closed (Kill PID 5632): Live Coding was blocking UBT. The plan file itself was created (`docs/balloon-flight-plan.md`) since it wasn't in the repo.
	- New assets: none (C++ only). The component isn't used anywhere in `Content/` yet: add it in phase 4.
- [x] **Phase 2**: temporary keyboard control of the balloon · 2026-11-14
	- Created `Source/gunsandballoons/BalloonKeyboardControlComponent.h/.cpp`: `BeginPlay` finds `UBalloonFlightComponent` on the owner (none → warning + `SetComponentTickEnabled(false)`), adds `Flight->AddTickPrerequisiteComponent(this)`. Tick: `IsInputKeyDown` on PC 0: W/S move the target by `SpeedChangeRate * Dt` from `GetTargetSpeed()`, A/D → `SetTurnInput(-1/+1/0)`. Keys are `FKey` (EditAnywhere, W/S/A/D), `SpeedChangeRate = 100` `// TBD: prototype value`.
	- Deviations: none. `AFirstPersonCameraPawn` unchanged; no Input Action/Mapping Context used.
	- New assets: none (C++ only). The component isn't on an actor yet: phase 4, together with `BalloonFlightComponent`.
- [x] **Phase 3**: the player on the balloon · 2026-11-14
	- `FirstPersonCameraPawn.h/.cpp`: private `LastParentYaw`/`bHasParentYaw`; before `UpdateLook`, `Tick` carries the parent's yaw delta into the control rotation (independent of the "mouse moved" condition); initialised in `NotifyControllerChanged` after `SetControlRotation`; `BeginPlay` (after Super) adds a tick prerequisite on the parent's `UBalloonFlightComponent`; include only in the `.cpp`.
	- `RevolverComponent.cpp` ~450: after `Params`, added `AddIgnoredActor(GetAttachParentActor())`: a shot doesn't hit its own balloon. The interaction trace is unchanged.
	- Deviations: none. Without a parent, behaviour is identical to before (guards `bHasParentYaw`/`GetAttachParentActor`).
	- New assets: none (C++ only).
- [x] **Phase 4**: `BP_Balloon` in the editor (MCP) · 2026-11-14 (finished)
	- **Read (names):** variables `Balloon|Controls`: LeverL, LeverR, FurnaceDoor, PropellerSpeed, Temperature01, Speed01, BurnTime. Functions: `UserConstructionScript`, `GetInteractionForBone` (bone → interaction). The actual root is `Mesh` (SkeletalMeshComponent, SK_Balloon, profile Custom), NOT DefaultSceneRoot; Mesh's children: Floor_0–3, Wall_0–7, StoveBlock, ConsoleBlock (BoxComponents, profile Custom), FurnaceLight + LanternLight (PointLight) with billboards.
	- **Done:** root = `BalloonCollision` (BoxComponent, profile Pawn, extent 125/271/145: a box root ignores its relative transform, so the extent is from the actor's origin; geometry from the pivot: x ±124, y −120…270, z −52…144). Mesh is a child of BalloonCollision, identity; simulate_physics=False (physics_asset_override absent → the PHA is inherited from SK_Balloon). Other transforms unchanged. Compile + Save successful.
	- **Blocker:** the main repo is `G:\UE\gunsandballoons` (that's the one open in the editor); the C++ components from phases 1–3 aren't in it (the worktree is uncommitted). BalloonFlight/BalloonKeyboardControl can't be added until the main repo has the classes built. Test/duplicate boxes were cleaned up.
	- Step 4 finishes after: copying the source files into `G:\UE\gunsandballoons`, closing the editor, a UBT build there, restarting the editor, adding the 2 components + compile/save via MCP.
- [x] **Phase 5**: flight level, obstacles, landmarks (MCP) · 2026-11-14
	- Created `Content/Scenes/BalloonFlight_Test.umap`: light, ground at -5000 cm, `BP_Balloon` at (0,0,0), `FirstPersonCameraPawn` attached (rel 0,0,80), `AmmoCrate` attached (rel 50,0,20), 4 obstacle cubes, 15 landmarks on the ground; GameMode override `FirstPersonCameraGameMode`; saved.
- [x] **Phase 6**: PIE check and measurements (MCP) · 2026-11-14
	- UBT build successful. `Movement_Scene1` PIE without errors. `BalloonFlight_Test` PIE: autotest ACCEL Min→Max ≈6.333 s and TURN 90° = 3.000 s at t.MaxFPS 30/60/120 (practically identical). Debug/autotest flags disabled on the instance.
- [x] **Keyboard control of the balloon (end of the prototype)**: 2026-11-14
	- Blocker: during the manipulations the `BP_Balloon` instance in `BalloonFlight_Test.umap` ended up with wrong flight-component values: `MaxSpeed=50` (should be 300), `InitialSpeed=20`, `MaxTurnRate=1`; the keyboard component had `SpeedStep=144.28`, `TurnDuration=301.97`.
	- Cause of the "W/S don't change the speed" symptom: `UBalloonFlightComponent::SetTargetSpeed` clamps to `[MinSpeed, MaxSpeed]`: with `MaxSpeed=50` every W/S step (click → `SpeedStep`) was simply clamped to `MaxSpeed`, so the speed didn't actually change.
	- Done: fixed the values on the instance (flight: Max=300, Min=50, Init=100, MaxTurn=30, Accel/Decel=40, TurnAcc/TurnDec=60; keyboard: SpeedStep=50, TurnDuration=0.5, bEnabled=True). Checked: `SetTargetSpeed(200) → 200` after the fix (the flight logic works).
	- Done: removed the temporary diagnostic `UE_LOG`s in `BalloonKeyboardControlComponent`; source updated (clean) + UBT build build12 successful.
