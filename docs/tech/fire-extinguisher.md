# Fire extinguisher — technical reference (C++)

> **For agents.** How the extinguisher is built. Design lives in [fire-extinguisher](../features/fire-extinguisher.md); if the code and the design disagree, the design wins. The gun base it shares is in [weapons](weapons.md). **Updated:** 2026-11-15

## Files
| File | Role |
| --- | --- |
| `Source/gunsandballoons/ExtinguisherComponent.h/.cpp` | `UExtinguisherComponent : UGunComponent`: contents, spray state machine, counter drums, foam emission |
| `Source/gunsandballoons/ExtinguisherFoam.h/.cpp` | `AExtinguisherFoam : AGunSmoke` (its `SmokeClass`): the foam's air, and the bubble-cluster sprite textures (`DrawPuffShape` override) |
| `Source/gunsandballoons/Tests/ExtinguisherTest.cpp` | `GunsAndBalloons.Extinguisher.SprayAndCounter` (drain, empty, holster, odometer, drum axis vs the Ref clips) |
| `Scripts/Editor/configure_extinguisher_texture.py` | Sets `T_Extinguisher_256` to nearest / no mips / UserInterface2D, like `T_Hook_256` (the import left it bilinear) |
| `Content/Assets/Fire_extinguisher` | `SK_Extinguisher`, clips `SK_ExtinguisherSK_Extinguisher_*`, `M_Extinguisher` (`T_Extinguisher_256`) |

`Content/Animations/Fire_extinguisher` holds duplicate imports with their own skeletons (`A_Ext_*_Skeleton`); they don't play on `SK_Extinguisher`.

## Rig (`SK_Extinguisher_Skeleton`)
`root → body → {lever, nozzle → nozzle_tip, digit_100, digit_10, digit_1}`. The root is the carry handle on top; the tank hangs 44 cm below, the nozzle tip is at (+28, 0, −12.7) and sprays along mesh +X. The counter is on the tank's left side (−Y).
- **Clips:** `Equip` 0.5 s (comes up 30 cm from below), `Unequip` 0.37 s (goes down), `Idle` (1 frame), `Spray_Start` 0.17 s (lever down), `Spray_Loop` 0.4 s (shake, lever held), `Spray_Stop` 0.23 s, `Empty` 0.5 s (dry sputter). All clips key the drums at 0.
- **Counter:** each drum is `Counter_Ref_000` + **36° per digit about its own +Y** (FBX: Y −50° + 36°·d; `Ref_073` = tens 7, ones 3; `Ref_100` = hundreds 1). The test checks this against the Ref clips.

## How it works
- **Input** (pawn): taken from its rack with LMB (`UWeaponRackComponent`, see [weapons](weapons.md)). LMB click → `PullTrigger` (only acts when empty: `Empty` clip, `OnDryFire`), then while LMB stays down → `SetTriggerHeld(true)` (new virtual on `UGunComponent`, unused by the guns).
- **State** (`EExtinguisherAction`): Equip → Idle → SprayStart → Spray (looped) → SprayStop → Idle; Empty → Idle; Unequip holds its last frame. Foam flows in SprayStart and Spray while `Amount > 0`; `Amount -= UsePerSecond·dt`. Reaching 0 → `Empty`, `OnEmptied`. `OnSprayStarted`/`OnSprayStopped` on flow changes.
- **Counter:** `DisplayedCount` rolls (`CounterRollSpeed`, 12/s) to `ceil(Amount)`; `GetDrumDigits` turns it into odometer drum positions (a drum rolls on only while the ones below pass 9→0).
- **Foam:** two layers, `FoamSpray` (the jet: 90/s, swells 4→22 cm radius, falls, sticks 5 s) and `FoamMist` (the cloud around it: 25/s, 18° cone, 8→45 cm, thins out, no sticking). Each tick `Amount` puffs/s of each are emitted into the foam actor at `nozzle_tip`, direction `nozzle → nozzle_tip` (through `FirstPersonToWorld`), spread along the tip's path this frame, carrying `InheritVelocity` of its motion (clamped 1500 cm/s).
- **Swap:** `AnimatesOwnSwap()` = true, so the pawn skips its own lower/raise; `PrepareToHolster` releases the lever, then plays `Unequip`; `IsReadyToHolster` once it's done. `CancelHolster` (the pawn calls it when putting the item away is called off) plays `Equip` from as far down as it got. Put on its rack, it also takes itself out of view with `Unequip` first.

## `AGunSmoke` additions (used by the foam)
`DrawPuffShape` (protected virtual: fills one shape's Density/Shade grid; base = the smoke blob; the foam draws one big bubble plus `BubblesPerPuff` smaller ones on a lumpy base, four flat tones, a highlight texel up-left), `bTurnPuffs` (random quarter turns; the foam turns them off so the highlights agree), `MaxPuffs` editor cap 512. `FGunSmokePuffs::Gravity` (cm/s², constant downward pull) and `StickTime` (s: a puff that hits a surface stops there, attached to the hit component, and lives that long more; 0 = slides like smoke). Both default 0, so gun smoke is unchanged.

## Cooling verification
2026-11-15: Editor Win64 Development build passed (module suffix 7151; restart the open editor to load new C++ classes). `GunsAndBalloons.Extinguisher.StoveCooling` passed: overheat clears, fuel time is retained, old heat target does not undo cooling, nozzle hit cools, misses/obstacles/range/empty tank block cooling, stove use takes priority over door interaction, basket hits are ignored, temperature clamps at zero. Existing spray, player and stove interaction tests passed. Full suite: 11/13; the previously observed `Stove.Heat` and `Stove.TickSize` timing failures remain. Report: `Saved/Automation/StoveCooling/index.json`. Visual PIE remains unverified.

## Known limitations
- `UGunComponent::ShouldUseInsteadOfInteract(Hit)` defaults false; the extinguisher overrides it for cooling surfaces. The armed Pawn checks it before dispatching an interaction, so LMB on the stove starts spray instead of toggling the door. Rack interactions still work.
- Stove cooling uses `ApplySprayCooling` from the world-space nozzle along its direction during active emission. First blocking Visibility hit only, within `StoveCoolingRange` (400 cm, TBD); `AcceptsCoolingHit` accepts `CoolingBones` (stove/furnace_door) or the stove's FireboxShape. `StoveCoolingPerSecond` (20 deg/s, TBD) is independent of visual puff count. The last partial contents frame caps spray duration at Amount/UsePerSecond. Other fires are not extinguished yet.
- No refill source; `SetAmount` exists for one.
- On possession with the extinguisher in hand from the start (`InitialHandState` Armed, `StartingGunIndex` 4) the pawn's raise still applies (no `Equip` clip then).
- View pose, foam and counter speeds are first-pass values; no sound.
- Up to 450 foam sprites (one static mesh component each, masked) — watch the cost on weaker machines; lower `MaxPuffs` / `Amount` if needed.
- Which side the bubble highlight lands on screen depends on the plane mesh's UV layout; if it reads as lit from below, move `Shine` in `DrawPuffShape`.
