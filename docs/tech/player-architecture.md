# Player architecture

> **Status:** Done (C++ and headless player regressions) | **Updated:** 2026-11-15

## Responsibilities

- `ABalloonPlayerController` manages input modes and the cursor. `UseMenuInput` releases held actions and gives input to UI; `UseGameplayInput` restores gameplay mouse capture. Neither method pauses the world or changes possession.
- `AFirstPersonCameraPawn` remains the single possessed Pawn: camera, look, hand state, viewmodel, and forwarding commands to items. `BP_Balloon` owns it through the `PlayerCamera` ChildActorComponent using `BP_BalloonPlayer` (a subclass with Player 0 auto-possession). Both test maps use this embedded player instead of a separately placed Pawn. Edit the shared seat transform on `BP_Balloon.PlayerCamera` (30, 0, 160 cm); edit player defaults on `BP_BalloonPlayer`. It has no movement component.
- `UPlayerInteractionComponent` traces and dispatches `IInteractable` calls to components, the hit actor, or its owner. Components passing `AcceptsHit` and `CanInteract` take priority over the actor; ammunition remains on RMB. Grabbing and dragging use the existing `IGrabbable` path in the Pawn.
- Guns, the hook, the extinguisher, and held fuel keep their state in their own Pawn components. The stove, supplies, levers, and flight live in components on their respective world actors. Items do not require separate possessed Pawns.
- `UBalloonFlightComponent` is the only component that moves the balloon. The Pawn does not simulate temperature, ammunition, or flight.

## Integration

All four project maps (`PostProcess`, `BalloonFlight_Test`, `LevelGeneration_Test`,
`Movement_Scene1`) use `/Game/Assets/Balloon/BP_Balloon` and its embedded
`BP_BalloonPlayer`. `Movement_Scene1`'s former bare skeletal balloon is replaced
by the shared playable actor. Edit gameplay and camera settings in these shared
Blueprints. `Scripts/Editor/share_balloon_all_levels.py` migrates/audits project
maps and writes `Saved/SharedBalloonAllLevels.json`;
`Scripts/Editor/test_shared_balloon_pie.py` checks unique possession and ownership
in the open map. World-partition balloons must be loaded with their player.

`AFirstPersonCameraGameMode` defaults to `ABalloonPlayerController` and `AFirstPersonCameraPawn`, preserving possession of an already placed Pawn. Set these classes manually when using another GameMode. Custom PlayerControllers must manage their own input modes.

In menu Blueprints, call `UseMenuInput` on the controller after opening the widget and `UseGameplayInput` after closing it. The menu owns widget focus policy. While menu input is active, the Pawn does not poll item buttons or mouse movement.

For a new world object, implement `IInteractable` on its Actor or ActorComponent, configure collision for `InteractTraceChannel`, and optionally filter bones or components through `AcceptsHit`. Keep the object's rules in that object. The Pawn functions `GetAimedInteractable`, `TryInteract`, and `TryTakeRound`, along with its trace range and channel settings, remain available to existing Blueprints.

`ReleaseGameplayInput` ends a grab, releases both triggers, and clears ammunition aiming. It runs on menu entry, `UnPossessed`, and `EndPlay`. Item components finish their own release animations.

`UGunComponent::ShouldUseInsteadOfInteract(Hit)` lets a held item prioritize its primary action on a surface. The extinguisher uses this for stove cooling surfaces, so LMB starts spraying instead of toggling the stove door. Other interactions retain their existing priority.

## Adjustable options
Set on `AFirstPersonCameraPawn` (its Blueprint class defaults or the placed pawn in the level). These properties are `protected`: the Details panel edits them, and only a Blueprint **subclass** of the pawn can read/write them at runtime. Per-item placement, kick and sway amounts live on each `UGunComponent` (see [weapons](weapons.md)). TBD = prototype value.

| Property | Default | Notes |
| --- | --- | --- |
| `InitialHandState` | Empty | Only Armed or Empty. Armed starts with the `StartingGunIndex` item; turn that rack's `bStartsWithWeapon` off |
| `StartingGunIndex` | 0 | 0 revolver, 1 rifle, 2 shotgun, 3 hook, 4 extinguisher; fuel is never a starting item |
| `HolsterTime` | TBD 0.35 s | Lowering time when putting an item on a rack |
| `LowerTime` / `RaiseTime` | 0.25 / 0.4 s | Swap lower time / raise into view (0 disables the raise) |
| `RaiseDrop` / `RaisePitch` | 20 cm / 35 deg | Start offset of the raise |
| `ReloadPoseBlendTime` | 0.25 s | Into/out of the reload pose |
| `InteractRange` | 400 cm | LMB/RMB interaction and grab trace length |
| `InteractTraceChannel` | Visibility | World objects must block this channel to be clickable |
| `bDebugInteractTrace` | false | Draw the trace for 1 s on each click |
| `GrabDragScale` | TBD 0.5 | cm of drag per mouse delta unit for levers |
| `GrabBreakDistance` | TBD 0 | cm; 0 = `InteractRange * 1.25` |
| `LookSensitivity` / `bInvertY` | 2.5 / false | Mouse look |
| `MinPitch` / `MaxPitch` | -89 / 89 deg | Pitch clamp |
| `bLimitYaw` / `MaxYawFromStart` | false / 90 deg | Optional yaw limit around the placed facing |
| `SwayAmount` / `MaxSwayAngle` / `SwayReturnSpeed` / `SwayShift` | 0.012 / 5 deg / 9 / 0.15 cm | Viewmodel look lag |
| `IdleBobAmount` / `IdleBobSpeed` | 0.25 cm / 1.6 rad/s | Idle breathing (0 disables) |
| `RecoilRecoverySpeed` / `AimKickSpeed` | 12 / 30 | Settle after a shot / aim kick sharpness |
| `BalloonSwayPitch` / `Yaw` / `Roll` / `Speed` | 1 / 0.8 / 1.5 deg / 0.12 | Placeholder basket rocking added to the aim (0 disables an axis) |
| `HandSwayBlendSpeed` | 2 | Blend to a new item's hand sway after a swap |

`ABalloonPlayerController` and `UPlayerInteractionComponent` have no editable settings. `AFirstPersonCameraGameMode` takes a placed auto-possess pawn, otherwise spawns one at the PlayerStart.

## API
### `ABalloonPlayerController`
| Member | Kind | Behaviour |
| --- | --- | --- |
| `UseMenuInput()` | BlueprintCallable | Call after opening a menu: releases held gameplay actions, gives input to UI. No pause, no possession change |
| `UseGameplayInput()` | BlueprintCallable | Call after closing a menu: restores mouse capture |
| `IsGameplayInputEnabled()` | BlueprintPure | Current mode |

### `AFirstPersonCameraPawn`
| Member | Kind | Behaviour |
| --- | --- | --- |
| `GetHandState()` / `IsHandEmpty()` | BlueprintPure | `EPlayerHandState`: Armed, Holstering, Empty, Drawing |
| `OnHandStateChanged(State)` | BlueprintAssignable | Every hand state transition |
| `GetActiveGun()` | BlueprintPure | Item in hand (the outgoing one during a swap); with an empty hand, the last held |
| `FindGun(Class)` | BlueprintPure | This pawn's item of that class (one of each), or null |
| `DrawWeapon(Rack)` / `HolsterWeapon(Rack)` / `SwapWeapon(Rack)` | BlueprintCallable | Take from / put on / swap with a `UWeaponRackComponent`; return false when not allowed (wrong rack, reloading, mid-swap, fuel in hand) |
| `TakeHandItem(Item)` | BlueprintCallable | Raise one of the pawn's own items into an empty hand without a rack (fuel from buckets) |
| `ConsumeHeldItem()` | BlueprintCallable | Hide the held item at once and empty the hand; no rack is touched (fuel fed or returned) |
| `GetAimedInteractable()` | BlueprintCallable | The `IInteractable` actor or ammo supply under the crosshair within `InteractRange`, or null |
| `TryInteract()` | BlueprintCallable | What LMB does: dispatch to the aimed `IInteractable`; true = click consumed (no shot). Ammo supplies are skipped |
| `TryTakeRound()` | BlueprintCallable | What RMB does on an ammo supply: one round (or eject a case first) |
| `ReleaseGameplayInput()` | BlueprintCallable | End grab, release triggers, clear ammo aiming (menus, unpossess, EndPlay) |
| `GetCamera()`, `GetRevolver()`, `GetRifle()`, `GetShotgun()`, `GetHook()`, `GetExtinguisher()`, `GetFuelHand()`, `Get*Mesh()`, `GetGunMesh(Gun)` | C++ | Component accessors; in Blueprint use the read-only component variables or `FindGun` |
| `GetGunLowered()` / `GetRecoilAlpha()` | C++ | 0..1 values read by the crosshair |

### Interfaces for world objects
| Interface | Member | Behaviour |
| --- | --- | --- |
| `IInteractable` | `AcceptsHit(Hit)` | Filter by `Hit.BoneName` / `Hit.GetComponent()`; default true |
| | `CanInteract(Pawn)` | False ignores the click (falls through to firing); default true |
| | `Interact(Pawn)` | The action; on an ammo-supply owner it runs on RMB instead |
| `IGrabbable` (empty hand only) | `CanGrab(Pawn, Hit)` / `BeginGrab` / `UpdateGrab(Pawn, WorldDragDelta)` / `EndGrab` | Lever-style drag; delta in cm in the camera plane |
| | `IsGrabbed()` / `GetGrabLocation()` | Target ended the grab itself (slip) / point used for `GrabBreakDistance` |
| `UGunComponent` (C++ virtual) | `ShouldUseInsteadOfInteract(Hit)` | Held item takes LMB on that surface (extinguisher on stove cooling parts) |

All interface functions are BlueprintNativeEvent: implement in C++ via `_Implementation` or in Blueprint (Class Settings > Interfaces). Dispatch order: components of the hit actor first (each `AcceptsHit` + `CanInteract`), then the actor, then its owner chain.

## Verification

`GunsAndBalloons.Player.Architecture` covers stove dispatch, hit filtering, owner fallback, ammunition on RMB, menu input without changing possession, and stopping spray. Related regressions: `Player.GrabLifecycle`, `Player.HandItems`, and `Stove.Interaction`.

2026-11-15: Editor Win64 Development built with module suffix `7150` because the open editor locked the main DLL. All three `Player.*` tests and `Stove.Interaction` passed. Full suite: 10/12; `Stove.Heat` and `Stove.TickSize` failed in unchanged thermodynamics code (empty-stove cooling and burn-time dependence on tick size). Report: `Saved/Automation/PlayerArchitecture/index.json`. Visual PIE was not checked; restart the open editor to load the new classes. Subsequent extinguisher cooling validation is recorded in [fire-extinguisher.md](fire-extinguisher.md).
