# Player architecture

> **Status:** Done (C++ and headless player regressions) | **Updated:** 2026-11-15

## Responsibilities

- `ABalloonPlayerController` manages input modes and the cursor. `UseMenuInput` releases held actions and gives input to UI; `UseGameplayInput` restores gameplay mouse capture. Neither method pauses the world or changes possession.
- `AFirstPersonCameraPawn` remains the single possessed Pawn: camera, look, hand state, viewmodel, and forwarding commands to items. It has no movement component and can be attached to the balloon in the level.
- `UPlayerInteractionComponent` traces and dispatches `IInteractable` calls to components, the hit actor, or its owner. Components passing `AcceptsHit` and `CanInteract` take priority over the actor; ammunition remains on RMB. Grabbing and dragging use the existing `IGrabbable` path in the Pawn.
- Guns, the hook, the extinguisher, and held fuel keep their state in their own Pawn components. The stove, supplies, levers, and flight live in components on their respective world actors. Items do not require separate possessed Pawns.
- `UBalloonFlightComponent` is the only component that moves the balloon. The Pawn does not simulate temperature, ammunition, or flight.

## Integration

`AFirstPersonCameraGameMode` defaults to `ABalloonPlayerController` and `AFirstPersonCameraPawn`, preserving possession of an already placed Pawn. Set these classes manually when using another GameMode. Custom PlayerControllers must manage their own input modes.

In menu Blueprints, call `UseMenuInput` on the controller after opening the widget and `UseGameplayInput` after closing it. The menu owns widget focus policy. While menu input is active, the Pawn does not poll item buttons or mouse movement.

For a new world object, implement `IInteractable` on its Actor or ActorComponent, configure collision for `InteractTraceChannel`, and optionally filter bones or components through `AcceptsHit`. Keep the object's rules in that object. The Pawn functions `GetAimedInteractable`, `TryInteract`, and `TryTakeRound`, along with its trace range and channel settings, remain available to existing Blueprints.

`ReleaseGameplayInput` ends a grab, releases both triggers, and clears ammunition aiming. It runs on menu entry, `UnPossessed`, and `EndPlay`. Item components finish their own release animations.

`UGunComponent::ShouldUseInsteadOfInteract(Hit)` lets a held item prioritize its primary action on a surface. The extinguisher uses this for stove cooling surfaces, so LMB starts spraying instead of toggling the stove door. Other interactions retain their existing priority.

## Verification

`GunsAndBalloons.Player.Architecture` covers stove dispatch, hit filtering, owner fallback, ammunition on RMB, menu input without changing possession, and stopping spray. Related regressions: `Player.GrabLifecycle`, `Player.HandItems`, and `Stove.Interaction`.

2026-11-15: Editor Win64 Development built with module suffix `7150` because the open editor locked the main DLL. All three `Player.*` tests and `Stove.Interaction` passed. Full suite: 10/12; `Stove.Heat` and `Stove.TickSize` failed in unchanged thermodynamics code (empty-stove cooling and burn-time dependence on tick size). Report: `Saved/Automation/PlayerArchitecture/index.json`. Visual PIE was not checked; restart the open editor to load the new classes. Subsequent extinguisher cooling validation is recorded in [fire-extinguisher.md](fire-extinguisher.md).
