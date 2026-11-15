# Grappling hook — technical reference (C++)

> **For agents.** How the hook is built and how to extend it. Design lives in [grappling-hook](../features/grappling-hook.md); if the code and the design disagree, the design wins. The gun base it shares is in [weapons](weapons.md). **Updated:** 2026-11-15

## Files
| File | Role |
| --- | --- |
| `Source/gunsandballoons/HookComponent.h/.cpp` | `UHookComponent : UGunComponent`: pressure, throw, flight, grab, reel-in, pump, gauge, world head and rope |
| `Source/gunsandballoons/HookPickup.h/.cpp` | `AHookPickup`: resource the hook grabs. Default: tinted `/Engine/BasicShapes/Cube`, 30 cm; any static mesh with simple collision can go on `Mesh` (e.g. `SM_Loot_*`). `Color` tints the shape (`Color`), loot materials (`HighlightColor`) and the sparkle (`SparkColor`). Optional `SparkleSystem` (Niagara, e.g. `NS_LootSparkle`) on the `Sparkle` component, stopped while carried. `bConsumeOnDelivery`: destroyed on delivery instead of dropped (off by default). `Contents` (array of `FResourceYield`; empty = not counted), `GrabSound`; `MarkCollected()` is true only once. Floating (no physics) or physics |
| `Source/gunsandballoons/ResourceType.h` | `EResourceType` None/Ammo/Money/Wood/Coal/Crystal (mirrors Blueprint `E_LootType`); `FResourceYield` (`ResourceType`, `MinAmount`, `MaxAmount`, `RollAmount()` inclusive, Max below Min = Min); `ResourceTypes::All`, `ToIndex` |
| `Source/gunsandballoons/ResourceLedger.h/.cpp` | `UResourceLedger : UWorldSubsystem`: count per resource, `Add`, `GetCount`, `CollectPickup` (once per pickup; adds `RollAmount()` of every entry in its `Contents`), `OnCountChanged(Type, NewCount, Delta)`. Per world: a new level starts at 0 |
| `Source/gunsandballoons/ResourceCounterBoard.h/.cpp` | `AResourceCounterBoard`: in-world counter (plate, `LOOT`, per resource an icon mesh and a `%03d` `UTextRenderComponent`), faces +X, no collision. Rolls each number to the ledger's count (`RollSpeed`, `MaxRollTime`), `TickSound` per step (at most every `MinTickInterval`), lights the row (`FlashTime`), spins/swells its icon |
| `Source/gunsandballoons/HookDebugMenu.h/.cpp` | `FHookDebugMenu`: Slate overlay, `H` |
| `Source/gunsandballoons/Tests/HookChargeTest.cpp` | `GunsAndBalloons.Hook.ChargeAndPump` |
| `Source/gunsandballoons/Tests/ResourceLedgerTest.cpp` | `GunsAndBalloons.Resources.Ledger` (per-resource counts, events, None and non-positive amounts ignored); `GunsAndBalloons.Resources.YieldRoll` (rolls stay in range and reach both ends: ammo 1–3, money 1–10) |
| `Scripts/Audio/generate_loot_sounds.py` | Synthesizes `Content/Audio/Sounds/SFX_Loot_{Ammo,Money,Wood,Coal,Crystal}.wav` and `SFX_Counter_Tick.wav` (stdlib only, 22.05 kHz mono, fixed seeds); run with UE's bundled `python.exe -I` |
| `Scripts/Editor/import_loot_sounds.py` | Imports those WAVs as SoundWaves into `/Game/Audio/Sounds` (re-import in place) |
| `Scripts/Editor/place_hook_pickups.py` | Places `HookTest_*` pickups in `Movement_Scene1` (re-run replaces them) |
| `Scripts/Editor/create_hook_loot_grab_level.py` | Creates `/Game/Scenes/Hook_Loot_Grab` (see below); re-run replaces what it placed |
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
- **Flight** (`EHookState` Stowed → Out → Grabbing → Returning): each tick a sphere sweep (`HookRadius`) ignoring the player and its whole attachment tree (the balloon). An `AHookPickup` hit → `OnHooked` (no physics/collision), attached to the head, `GrabAnim`, `OnPickupGrabbed`, return after the clip. Anything else or `MaxRange` → return empty (`GrabAnim` closes the claws). Returning at `ReturnSpeed` to the gun's `hook_head`; at `CatchDistance` the pickup is let go (`OnDelivered`: physics on, small push toward the player) → `OnPickupDelivered`; a pickup with `bConsumeOnDelivery` is destroyed right after that broadcast instead (not during world teardown); arriving → `OnHookReturned`.
- **Grab feedback** (`PlayGrabFeedback`): `GrabEffect` (`NS_BurstSpark`) at the head and the pickup's `GrabSound` via `PlaySoundAtLocation` (no attenuation, so it is heard at any range), if SFX is on (`UGameOptions`).
- **Arrived vs dropped:** `DeliverPickup(bArrived)`. Only the catch point (`CatchDistance`) is an arrival: `OnPickupDelivered`, then for `bConsumeOnDelivery` an `ArrivalEffect` (`NS_BurstPuff`) and `Destroy`. `StowHead` (the hook put away mid-reel) and `EndPlay` pass false: the pickup just falls, no event, not counted.
- **Counting:** the pawn binds `Hook->OnPickupDelivered` → `UResourceLedger::CollectPickup`; `AResourceCounterBoard` listens to `OnCountChanged`. The hook knows nothing about counting.
- **World head:** the gun is a first-person primitive (own FOV, squashed to the eye), so a head flying out on it would draw over the world and miss its target's pixels. Instead `HeadMesh` is a second, world-space `SK_Hook` (`UGunAnimInstance`, no collision, fixed skel bounds) whose pose scales `body` by 1e-3 and `hook_head` by 1e3 with zero translation: only the head and claws show, at full size, on the component origin. Meanwhile the gun's `hook_head` is scaled to 0. `Rope` is a world `UStaticMeshComponent` from the gun's `muzzle` (converted) to the head.
- **Holster:** `IsReadyToHolster` only with the head stowed and no pump stroke running, so a swap waits for the hook to come back.
- **Tick order:** the hook ticks after the pawn (`AddPrerequisite(Owner)`), so head and rope start from this frame's gun position; `HeadMesh` ticks after the hook.
- **Textures:** `WeaponTexture` = `T_Hook_256` on every slot at BeginPlay (as the guns do), then the `M_Rope` slot gets `RopeTexture` = `T_Hook_Rope_64`. `M_Lamp` reads the atlas too (UV ≈ 0.64–0.69, 0.23–0.24).

## `UGunAnimInstance` additions
`SetBoneTranslation(Bone, TOptional<FVector>)` replaces a bone's local translation; `SetBoneRotationOffset(Bone, FQuat)` turns it in its own frame (`Rotation = Sampled × Offset`). Applied after `SetBonePose`, before `SetBoneScale`.

## Debug menu (`H`)
Overlay in the top-left, no mouse capture, so play continues: Up/Down select, Left/Right change, Shift ×10. Rows: pumps 0→100, pressure (±10), range, throw speed, reel-in speed, heavy-lever pressure (±5), last-stroke speed (±5%); below them a preview of every stroke's speed. Values last for the play session; copy good ones into `UHookComponent`'s constructor/defaults.

## Known limitations
- Delivery drops the pickup, or with `bConsumeOnDelivery` removes it. No inventory or resource types yet (design open question).
- The flying head doesn't inherit the balloon's velocity; flight is in world space.
- Sound/VFX only on grab and arrival (above); none yet for the shot, dry click, return or pumping: bind to `OnFired`, `OnDryFire`, `OnHookReturned`, `OnPumped`, `OnFullyCharged`.
- The view pose (`ViewOffset` 35/18/−9, as the rifle and shotgun) and the speeds are first-pass values.

## Test scene `Hook_Loot_Grab`
`/Game/Scenes/Hook_Loot_Grab` is a copy of `Movement_Scene1` (World Partition; landscape, racks, ammo crate, balloon) made by `Scripts/Editor/create_hook_loot_grab_level.py`:
- A placed `FirstPersonCameraPawn` (`Player_HookLoot`, auto-possess Player 0, `InitialHandState` Armed, `StartingGunIndex` 3) starts with the hook in hand; `HandItem_Hook` starts empty so the hook can be put back. The pawn faces away from the hand-item racks: their invisible `InteractShape` boxes block the hook head, which leaves from the gun below and right of the eye.
- The five `SM_Loot_*` models stand at 5.5–22.7 m as `AHookPickup` (`HookLoot_*`, folder `HookLoot`): `MI_Loot` materials, `Color` = their `BP_Loot_*` `HighlightColor`, `NS_LootSparkle`, `bConsumeOnDelivery`. All are non-physics (`bFloating`) even the two on the ground, because in PIE the streamed landscape arrives after BeginPlay and simulated pickups would fall through it. Each has its `Contents` (the ammo box: ammo 1–3 and money 1–10; the others: 1 of their own resource) and `GrabSound` (`/Game/Audio/Sounds/SFX_Loot_*`).
- `HookLoot_CounterBoard` (`AResourceCounterBoard`) stands 1.7 m ahead and 0.95 m left of the player, 15 cm below the eye, turned to face them: in view, clear of every line to the loot.
- The `SM_Loot_*` meshes had no simple collision after import (the manifest lists one hull each); the script gave each a box, which the hook's sweep and physics need.
- Copying a World Partition map from Python: use `EditorLoadingAndSavingUtils.save_map` (what File > Save Current Level As does), then load the copy. `EditorAssetLibrary.duplicate_asset` + `load_level` kills the editor (fatal "World Memory Leaks": the copy is a newly created, initialized world that `Map_Load` cannot unload).
- `unreal.Rotator(a, b, c)` in Python is (roll, pitch, yaw); pass keywords.
