# Grappling hook — technical reference (C++)

> **For agents.** How the hook is built and how to extend it. Design lives in [grappling-hook](../features/grappling-hook.md); if the code and the design disagree, the design wins. The gun base it shares is in [weapons](weapons.md). **Updated:** 2026-11-14

## Files
| File | Role |
| --- | --- |
| `Source/gunsandballoons/HookComponent.h/.cpp` | `UHookComponent : UGunComponent`: pressure, throw, flight, grab, reel-in, pump, gauge, world head and rope |
| `Source/gunsandballoons/HookPickup.h/.cpp` | `AHookPickup`: placeholder resource (tinted `/Engine/BasicShapes/Cube`, 30 cm); floating or physics |
| `Source/gunsandballoons/HookDebugMenu.h/.cpp` | `FHookDebugMenu`: Slate overlay, `H` |
| `Source/gunsandballoons/Tests/HookChargeTest.cpp` | `GunsAndBalloons.Hook.ChargeAndPump` |
| `Scripts/Editor/place_hook_pickups.py` | Places `HookTest_*` pickups in `Movement_Scene1` (re-run replaces them) |
| `Content/Assets/Hook` | `SK_Hook`, clips `SK_HookSK_Hook_*`, `SM_HK_Rope`, materials `M_Hook`/`M_Lamp` (`T_Hook_256`), `M_Rope` (`T_Hook_Rope_64`) |

`Content/Animations/Hook` holds duplicate imports with their own skeletons (`A_Hook_*_Skeleton`); they don't play on `SK_Hook`.

## Rig (`SK_Hook_Skeleton`)
`SK_Hook → root → body → {needle, pump, hook_head → {hook_rope, claw_L, claw_R}, muzzle, rope_attach}`. Mesh +X is the barrel; `body` is the back of the gun, `hook_head` sits 41.1 cm forward.
- **Clips:** `Idle` (1 frame), `Throw` (body kick, claws open), `Grab` (claws close), `Pump` (pump lever stroke), `Charged` (ready kick); each 0.367 s. `Demo_ThrowGrabReturn` is the animator's reference only.
- **Gauge:** `Needle_Ref_0` → `Needle_Ref_100` is **240° about the needle's own +Y** (the Idle pose is 50). A Slerp between the two would take the 120° short way under the dial, so the code poses the needle from `Needle_Ref_0` and adds `FQuat(+Y, 240° × charge)`.
- `HookHead_Ref_0/_100cm`: the head slides along body −Y. Not used (the flying head is a separate world copy, below).
- No vertex is skinned to `hook_rope`/`rope_attach`; the flying rope is `SM_HK_Rope` (1 cm along +X, stretched).

## How it works
- **Input** (pawn): taken from its rack with LMB (`UWeaponRackComponent`, see [weapons](weapons.md)); LMB → `PullTrigger`; RMB held, not on an ammo supply → `SetSecondaryHeld(true)` (new virtual on `UGunComponent`, unused by the guns); `H` toggles the debug menu.
- **Pressure:** `Charge` 0..100, starts at `InitialCharge` (100). `PullTrigger` below 100 → `OnDryFire` only. A pump stroke (`PumpAnim` at `GetPumpStrokeRate`) runs while RMB is held, the head is stowed and the charge is below 100; at the end of each stroke `Charge += 100 / PumpsToFullCharge` → `OnPumped`; reaching 100 plays `ChargedAnim` → `OnFullyCharged`. **Heavy lever:** a stroke plays at `PumpPlayRate` while the pressure it reaches is at or below `HardPumpFrom` (50), then slower, linearly down to `PumpPlayRate × HardPumpPlayRate` (0.4) for the stroke that reaches 100; the number of strokes is unchanged. The needle follows at `NeedleSpeed`.
- **Throw:** charge → 0, `ThrowAnim` on the gun, `OnFired` (pawn recoil). A line trace from the eye (`MaxRange`, `TraceChannel`) picks the target; the head starts at the gun's `hook_head` (through `FirstPersonToWorld`) and flies at `ThrowSpeed` toward it.
- **Flight** (`EHookState` Stowed → Out → Grabbing → Returning): each tick a sphere sweep (`HookRadius`) ignoring the player and its whole attachment tree (the balloon). An `AHookPickup` hit → `OnHooked` (no physics/collision), attached to the head, `GrabAnim`, `OnPickupGrabbed`, return after the clip. Anything else or `MaxRange` → return empty (`GrabAnim` closes the claws). Returning at `ReturnSpeed` to the gun's `hook_head`; at `CatchDistance` the pickup is let go (`OnDelivered`: physics on, small push toward the player) → `OnPickupDelivered`; arriving → `OnHookReturned`.
- **World head:** the gun is a first-person primitive (own FOV, squashed to the eye), so a head flying out on it would draw over the world and miss its target's pixels. Instead `HeadMesh` is a second, world-space `SK_Hook` (`UGunAnimInstance`, no collision, fixed skel bounds) whose pose scales `body` by 1e-3 and `hook_head` by 1e3 with zero translation: only the head and claws show, at full size, on the component origin. Meanwhile the gun's `hook_head` is scaled to 0. `Rope` is a world `UStaticMeshComponent` from the gun's `muzzle` (converted) to the head.
- **Holster:** `IsReadyToHolster` only with the head stowed and no pump stroke running, so a swap waits for the hook to come back.
- **Tick order:** the hook ticks after the pawn (`AddPrerequisite(Owner)`), so head and rope start from this frame's gun position; `HeadMesh` ticks after the hook.
- **Textures:** `WeaponTexture` = `T_Hook_256` on every slot at BeginPlay (as the guns do), then the `M_Rope` slot gets `RopeTexture` = `T_Hook_Rope_64`. `M_Lamp` reads the atlas too (UV ≈ 0.64–0.69, 0.23–0.24).

## `UGunAnimInstance` additions
`SetBoneTranslation(Bone, TOptional<FVector>)` replaces a bone's local translation; `SetBoneRotationOffset(Bone, FQuat)` turns it in its own frame (`Rotation = Sampled × Offset`). Applied after `SetBonePose`, before `SetBoneScale`.

## Debug menu (`H`)
Overlay in the top-left, no mouse capture, so play continues: Up/Down select, Left/Right change, Shift ×10. Rows: pumps 0→100, pressure (±10), range, throw speed, reel-in speed, heavy-lever pressure (±5), last-stroke speed (±5%); below them a preview of every stroke's speed. Values last for the play session; copy good ones into `UHookComponent`'s constructor/defaults.

## Known limitations
- Pickups are placeholders; delivery only drops them. No inventory or resource types yet (design open question).
- The flying head doesn't inherit the balloon's velocity; flight is in world space.
- No sound/VFX: bind to `OnFired`, `OnDryFire`, `OnPickupGrabbed`, `OnPickupDelivered`, `OnHookReturned`, `OnPumped`, `OnFullyCharged`.
- The view pose (`ViewOffset` 35/18/−9, as the rifle and shotgun) and the speeds are first-pass values.
