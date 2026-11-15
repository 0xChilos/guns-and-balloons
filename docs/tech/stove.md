# Stove and fuel - technical reference (C++)

> **For agents.** Design: [engine-stove](../features/engine-stove.md); approved decisions override the older implementation plan. Flight API: [balloon-flight](balloon-flight.md). Player-side dispatch: [player-architecture](player-architecture.md). **Updated:** 2026-11-15

## Model in one paragraph
`UStoveComponent` simulates temperature, ignition, burn time and an animated door on `BP_Balloon`. Three `UFuelSupplyComponent` buckets supply one piece of wood, coal or crystal to the pawn's `UFuelHandComponent`. Door clicks toggle the door; an open firebox accepts fuel and consumes the held piece only on success. Fuel raises the pending heat target and adds burn time; crystal heats instantly. Temperature publishes the named flight modifier `Stove`, and overheating damages the balloon's shared `UHealthComponent`. `ABP_Balloon` reads the door, gauge and burn-arrow outputs; `UStoveVfxComponent` reacts to fuel and overheat events and smokes from visible damage holes.

## Files
| File | Role |
| --- | --- |
| `Source/gunsandballoons/StoveComponent.h/.cpp` | Simulation, door/firebox interaction, flight outputs, overheat |
| `Source/gunsandballoons/StoveFuelType.h/.cpp` | Fuel DataAsset, heat/time and hand presentation |
| `Source/gunsandballoons/FuelSupplyComponent.h/.cpp` | Bucket stock, pickup, return and swap |
| `Source/gunsandballoons/FuelHandComponent.h/.cpp` | Non-firing held static-mesh prop |
| `Source/gunsandballoons/HealthComponent.h/.cpp` | Shared damage/repair pool and actor damage filtering |
| `Source/gunsandballoons/FirstPersonCameraPawn.h/.cpp` | Interaction dispatch, TakeHandItem / ConsumeHeldItem |
| `Source/gunsandballoons/StoveVfxComponent.h/.cpp` | PS1 fuel burst, sustained overheat smoke and damage-hole smoke using AGunSmoke |
| `Source/gunsandballoons/StoveEditorLibrary.h/.cpp` | Editor collision and animation helpers |
| `Source/gunsandballoons/Tests/StoveTest.cpp`, `StoveInteractionTest.cpp`, `StoveCoolingTest.cpp`, `StoveVfxTest.cpp`, `StoveDamageSmokeTest.cpp`, `HealthTest.cpp`, `HandItemTest.cpp` | Automation coverage |
| `Scripts/Editor/configure_stove.py` | Fuel assets and historical BP/ABP setup |
| `Scripts/Editor/configure_balloon_health.py` | Idempotent shared health setup without rerunning the historical stove migration |
| `/Game/Data/Stove/DA_Fuel_Wood`, `DA_Fuel_Coal`, `DA_Fuel_Crystal` | Per-type fuel assets |

## Player flow
1. Empty hand + LMB on a bucket takes one piece. Same-type bucket returns it; another type swaps it, returning the held piece to its own supply first.
2. Empty target, missing home supply or full return supply leaves hand and stocks unchanged.
3. LMB on the door toggles it. A closed-firebox click is consumed but leaves the door closed and fuel held (the later approved rule supersedes plan D2).
4. Open-firebox click feeds only held fuel. Rejected fuel stays held; accepted fuel immediately empties the hand. Weapon/empty-hand mouth clicks are consumed without shooting.
5. Fuel cannot be placed on weapon racks or exchanged for pickups; levers require an empty hand. Return fuel or feed it first.
6. With the extinguisher in hand, LMB on the stove body, door or firebox sprays (cooling) instead of toggling the door.

## Adjustable options and API
Units: seconds, degrees, degrees/second; animation translations are centimeters. **TBD** = editable prototype value, not final balance.

How to read the **Edit** column:
- **Details + BP rw** — set in the component's Details panel (BP class defaults or placed instance) and readable/writable from Blueprint at runtime. Runtime changes take effect on the next tick.
- **Details + BP ro** — set in Details; Blueprint can only read it.
- **Asset** — set on the fuel DataAsset in `/Game/Data/Stove/`.
- **C++** — not exposed to Blueprint.

### `UStoveComponent` (`Stove` on `BP_Balloon`)
**Settings**

| Property | Default | Edit | Notes |
| --- | --- | --- | --- |
| `HeatRate` | 5 deg/s | Details + BP rw | Default heating rate; fuels with `HeatRateOverride = 0` use the live value (biomes may change it at runtime) |
| `DecayBurning` / `DecayEmpty` | 1 / 5 deg/s | Details + BP rw | Cooling while burn time > 0 / = 0. No decay while heating |
| `DecayPause` | 10 s | Details + BP rw | Decay pause after each accepted piece |
| `IgnitionDelay` | 10 s | Details + BP rw | Delay from a cold stove; burn time does not drain meanwhile; crystal skips it |
| `MaxBurnTime` | 600 s | Details + BP rw | Burn-time capacity |
| `bClampBurnTimeOnOverfill` | false | Details + BP rw | false = reject a piece that does not fit; true = accept and clamp |
| `OverheatThreshold` | 90 deg | Details + BP rw | Overheat, damage and overheat speed apply strictly above |
| `OverheatDamagePerSecond` | TBD 2 | Details + BP rw | Health damage per second while overheated |
| `MaxTemperature` | TBD 120 deg | Details + BP rw | Temperature cap |
| `GaugeMaxTemperature` | TBD 100 deg | Details + BP rw | Temperature at full dial; the gauge can saturate before the cap |
| `SpeedMultAt90` / `SpeedMultOverheat` | TBD 1.6 / TBD 2 | Details + BP rw | Flight multiplier: x1 at 0, linear to `SpeedMultAt90` at the threshold, then `SpeedMultOverheat` |
| `DoorOpenDecayMultiplier` | TBD 2 | Details + BP rw | Faster cooling with the door open; also leaks heat during the decay pause |
| `DoorOpenAngle` / `DoorSpeed` | TBD +100 deg / TBD 300 deg/s | Details + BP rw | +100 is the approved direction in C++ and `BP_Balloon`; do not flip the sign |
| `FlightModifierEpsilon` | 0.001 | Details + BP rw | Minimum multiplier change that republishes the flight modifier |
| `DoorBone` | `furnace_door` | Details + BP rw | Hit bone that toggles the door |
| `FireboxShape` / `FireboxComponentName` | none / `StoveFirebox` | Details + BP rw | Firebox click target; BeginPlay resolves the live component by name and forces query-only collision |
| `InteractTraceChannel` | Visibility | Details + BP rw | Channel the firebox blocks; must match the pawn's trace |
| `CoolingBones` | `stove`, `furnace_door` | Details + BP rw | Bones the extinguisher can cool (plus `FireboxShape`) |
| `GaugeSweepDeg` | 240 deg | Details + BP rw | Needle sweep from `Balloon_build.py` |
| `Health` / `Flight` | auto | Details + BP rw | Leave empty: BeginPlay finds them on the owner. Set only to redirect (tests) |

**API**

| Member | Kind | Behaviour |
| --- | --- | --- |
| `FindStove(Actor)` | BlueprintPure static | The actor's stove component, or null |
| `CanAddFuel(Fuel)` / `AddFuel(Fuel)` | BlueprintPure / Callable | Require open door, valid nonnegative finite fuel values and burn capacity (or clamping); `AddFuel` returns success |
| `ApplyCooling(Degrees)` | BlueprintCallable | Finite positive degrees lower current and pending heat, clamp to zero, keep fuel. Returns heat removed; updates overheat/flight immediately; extinguishes at zero |
| `AcceptsCoolingHit(Hit)` | BlueprintPure | True for `CoolingBones` or `FireboxShape`; independent of the health damage filter. Use the same rule for the future repair tool (repair only through the stove) |
| `SetDoorOpen(bool)` / `ToggleDoor()` | BlueprintCallable | Logical door state; the angle follows at `DoorSpeed` |
| `IsDoorOpen()` / `GetDoorAngle()` | BlueprintPure | Logical state (feeding checks this) / animated angle |
| `GetTemperature()` / `GetTemperature01()` | BlueprintPure | Degrees / `Temperature / GaugeMaxTemperature` clamped to 0..1 |
| `GetGaugeAngle()` / `GetGaugeRotation()` | BlueprintPure | `Temperature01 * GaugeSweepDeg` / rotation around the imported gauge normal (parent space) |
| `GetBurnTimeLeft()` / `GetBurnTime01()` | BlueprintPure | Seconds / fraction of `MaxBurnTime` |
| `IsLit()` / `IsOverheated()` / `GetSpeedMultiplier()` | BlueprintPure | Positive temperature outside ignition / above threshold / current flight multiplier |
| `OnFuelAdded(Fuel)` | BlueprintAssignable | Accepted fuel only, after outputs update |
| `OnIgnited` / `OnExtinguished` | BlueprintAssignable | Ignition completes / positive temperature reaches zero |
| `OnOverheatChanged(bool)` / `OnDoorChanged(bool)` | BlueprintAssignable | Overheat transition / logical door transition |
| `IInteractable` | Interface | `AcceptsHit` caches door vs firebox; `Interact` toggles the door or feeds held fuel. Called by the pawn on LMB, not by gameplay code |

### `UStoveFuelType` (DataAsset)
| Property | Wood / Coal / Crystal | Edit | Notes |
| --- | --- | --- | --- |
| `DisplayName` | — | Asset | UI name |
| `BurnTime` | 180 / 120 / 10 s | Asset | Burn time added per piece |
| `HeatAdd` | 20 / 30 / 50 deg | Asset | Added to the pending heat target |
| `bInstantHeat` | false / false / true | Asset | Jump to the target and skip the cold ignition delay |
| `HeatRateOverride` | 0 | Asset | deg/s for this fuel; 0 = stove `HeatRate` |
| `HandMesh` / `HandTransform` | Cube / Cylinder / Cone, TBD | Asset | Held prop and its offset/rotation/scale; wood prototype scale 0.12, 0.05, 0.05 |

### `UFuelSupplyComponent` (one bucket)
| Property | Default | Edit | Notes |
| --- | --- | --- | --- |
| `Fuel` | none | Details + BP ro | The bucket's only fuel type |
| `Capacity` | TBD 10 | Details + BP ro | Maximum stock |
| `StartCount` | TBD 4 | Details + BP ro | Stock at BeginPlay; design 4 wood / 4 coal / 2 crystal |
| `InteractTraceChannel` | Visibility | Details + BP ro | Channel `InteractShape` blocks |
| `InteractShape` / `BucketMesh` | box 20 cm / empty | Details (nested) + BP ro | Click box and optional placeholder look; size and place per bucket (TBD) |

| Member | Kind | Behaviour |
| --- | --- | --- |
| `AddFuel(Amount)` | BlueprintCallable | Refill (hook, safe zone); returns the number accepted under `Capacity` |
| `GetCount()` / `GetCapacity()` | BlueprintPure | Current stock / capacity |
| `OnContentsChanged(Supply, NewCount)` | BlueprintAssignable | Any stock change, including BeginPlay |
| `FindSupplyFor(Owner, Type)` | C++ static | First bucket on the actor holding `Type`; for mixed refills |
| `IInteractable` | Interface | Take / return / swap on LMB (see Player flow) |

### `UFuelHandComponent` (pawn's held fuel)
| Member | Kind | Behaviour |
| --- | --- | --- |
| `Fuel` / `FuelProp` | BP ro | Held type and its static-mesh prop |
| `SetFuel(Type)` | BlueprintCallable | Load `HandMesh`, apply `HandTransform`; null clears. Never fires |
Take or drop it through the pawn: `TakeHandItem(GetFuelHand())` / `ConsumeHeldItem()` (see [player-architecture](player-architecture.md)).

### `UHealthComponent` (`Health` on `BP_Balloon`)
| Property | Default (BP after script) | Edit | Notes |
| --- | --- | --- | --- |
| `MaxHealth` | TBD 100 | Details + BP ro | Health starts here at BeginPlay |
| `bListenToActorDamage` | true | Details + BP ro | Bind owner point/any damage |
| `DamageBones` | `basket` | Details + BP ro | Empty = every point hit counts (except ignored) |
| `bIncludeChildBones` | true | Details + BP ro | Listed bones cover their children (whole basket: stove, door, bucket, display, ...) |
| `IgnoredBones` | `balloon` | Details + BP ro | Envelope branch takes no damage from this pool |
| `DamageComponentTag` | `BasketDamage` | Details + BP ro | Tag on bone-less basket boxes `Wall_*`, `Floor_*`, `StoveBlock`, `ConsoleBlock`; `Envelope_*` untagged |

| Member | Kind | Behaviour |
| --- | --- | --- |
| `ApplyDamage(Amount, Causer)` | BlueprintCallable | Returns damage applied; no-op for non-finite/≤0 or when depleted |
| `Repair(Amount, Repairer)` | BlueprintCallable | Returns health restored; works from 0. Generic API: callers must enforce the **stove-only** repair rule |
| `SetHealth(Value)` | BlueprintCallable | Clamped; non-finite ignored |
| `GetHealth()` / `GetHealth01()` / `IsDepleted()` | BlueprintPure | Current / normalized / at zero |
| `AcceptsPointHit(Component, Bone)` | BlueprintPure | Damage filter: tagged component, else walk from the hit bone to the root; first `IgnoredBones`/`DamageBones` match decides |
| `OnHealthChanged(Health, NewHealth, Delta)` | BlueprintAssignable | Every change (damage decals later) |
| `OnDepleted` / `OnRestored` | BlueprintAssignable | Reaching 0 ("balloon destroyed", no game over yet) / leaving 0 |

### `UStoveVfxComponent` (`StoveVFX_0`)
| Property | Default | Edit | Notes |
| --- | --- | --- | --- |
| `EffectClass` | `AGunSmoke` | Details + BP rw | Sprite effect actor class |
| `FuelBurst` / `OverheatSmoke` | TBD | Details + BP rw | Burst: Amount = puff count; smoke: Amount = puffs/s |
| `FuelColor` / `OverheatColor` | orange / dark grey, TBD | Details + BP rw | Tint |
| `OverheatOffset` / `StoveBone` | (0, 0, 100) cm, BP (0, -28, 100) / `stove` | Details + BP rw | Smoke origin relative to the stove bone, in mesh cm |
| `DamageSmoke` / `DamageColor` | 3 puffs/s per hole, small (1→6 cm), 0.6–1.2 s, TBD / dark grey, TBD | Details + BP rw | Thin stream from every visible damage hole |
| `DamageBonePrefix` / `MaxDamageHoles` | `stove_damage_` / 10 | Details + BP rw | Hole bones `stove_damage_1..10`; a hole smokes while its bone isn't hidden |

| Member | Kind | Behaviour |
| --- | --- | --- |
| `GetFuelEffect()` / `GetOverheatEffect()` / `GetDamageEffect()` | BlueprintPure | Spawned effect actors (inspection/tests) |

### Rig and animation constants (not properties)
| Value | Where | Notes |
| --- | --- | --- |
| Burn-arrow travel +22 cm | `ABP_Balloon` | `burn_arrow` Z = `GetBurnTime01() * 22` |
| `StoveFirebox` ~15 x 3 x 14 cm | `BP_Balloon` | Accepted as is (planned 24 x 5 x 20 cm) |
| Extra bucket art, placement and click bounds | `BP_Balloon` | TBD; Cylinder placeholders for wood/crystal |

## Flight integration
The stove ticks in `TG_PrePhysics`, adds itself as a flight tick prerequisite, and publishes `SetFlightModifier("Stove", 0, multiplier)` when changed. Other flight modifiers multiply with it; effective target speed remains clamped to flight MinSpeed/MaxSpeed. EndPlay clears the modifier and prerequisite. Use TargetSpeed 100 when checking x2 so MaxSpeed does not mask it.

## How to use it
Add Stove and Health to the balloon owner; the stove discovers both Health and Flight in BeginPlay if unassigned. Overheat calls Health.ApplyDamage; other systems use ApplyDamage/Repair directly. OnDepleted is the balloon-destroyed integration event; game-over flow remains external. Run `configure_balloon_health.py` after the C++ build: it sets DamageBones = basket (with child bones), IgnoredBones = balloon and tags the basket collision boxes. Point damage counts once despite the matching actor any-damage callback. Repair is allowed only through the stove; Health.Repair stays a generic API, so the caller (repair tool, safe zone) enforces the stove target, e.g. with `AcceptsCoolingHit`.

Typical Blueprint calls: get the stove with `FindStove(BalloonActor)`, then read `GetTemperature01` / `GetBurnTime01` for UI, bind `OnOverheatChanged` for warnings, call `AddFuel` from scripted events (door must be open), `ApplyCooling` for water/foam effects, and set `HeatRate` from a biome. Refill buckets with `AddFuel(Amount)` on each `UFuelSupplyComponent`.

VFX binds OnFuelAdded, OnOverheatChanged and the health component's OnHealthChanged: accepted fuel emits an orange burst, overheat emits dark rising smoke, cooling stops emission while existing puffs fade. While the balloon is damaged it ticks and emits a thin dark stream from every damage-hole bone that isn't hidden (BP_Balloon shows/hides the holes from the health), out of the stove's side and rising; at full health the tick stops (unless overheated). Source motion follows flight speed. EndPlay unbinds and destroys effects; dedicated servers skip VFX.

## Setup in `BP_Balloon`
**Fuel buckets are not placed yet.** The fuel DataAssets `DA_Fuel_Wood`, `DA_Fuel_Coal` and `DA_Fuel_Crystal` are prepared with the design values, but `BP_Balloon` has only the legacy `WoodFuelSupply` (wood, on the modeled bucket); `FuelBucket_Coal`, `FuelBucket_Wood` and `FuelBucket_Crystal` are absent (checked in the editor 2026-11-15), so only wood can be picked up in game.

Follow the [Phase 6 wiring and PIE checklist](stove-phase6-checklist.md), whose migration and acceptance remain pending. It reuses the modeled bucket for coal and adds wood/crystal placeholders. The historical script still creates WoodFuelSupply on the modeled bucket: **do not rerun its Blueprint setup after manual migration**, or it will recreate the old supply. Fuel assets can be created separately with configure_fuel_assets().

Before wiring, check BP_Balloon and its owner chain for AmmoSupplyComponent: pawn LMB dispatch rejects that chain. Keep ammo in separate crates. Use one query-only StoveFirebox behind the door, Visibility Block (current size accepted). The existing collision for furnace_door is kept; no extra door body needs to be fitted. BeginPlay resolves FireboxComponentName if FireboxShape references a BP template. Account for imported bone scale when fitting boxes; check placed-instance overrides.

ABP_Balloon caches StoveRef and reads StoveDoorAngle, StoveTemp01 and StoveBurn01 during animation update. Preserve wing controls; append bone controls before Component To Local: furnace_door additive component-space yaw, gauge_needle additive parent-space GetGaugeRotation(), burn_arrow additive component-space Z = GetBurnTime01() * 22 cm. Keep alpha 1. The script preserves manually fitted firebox placement via the StoveFitted tag; clearing it requests a refit. It refits the door body and refuses ambiguous pose chains or saving failed Blueprint compiles.

## Validation (2026-11-15)
API/settings tables were checked against the current headers (`StoveComponent.h`, `StoveFuelType.h`, `FuelSupplyComponent.h`, `FuelHandComponent.h`, `HealthComponent.h`, `StoveVfxComponent.h`) and `BP_Balloon` was inspected in the editor. The new health filter (`bIncludeChildBones`, `IgnoredBones`, `DamageComponentTag`) is not built or tested yet.

Earlier records on this page reported a successful Development Editor build, Heat/Interaction/Overheat automation, two successful commandlet setup runs, and all nine then-existing GunsAndBalloons tests after Phase 5. A recorded PIE check took wood, opened the door and fed 180 s: the arrow moved +6.6 cm and the gauge rose after the 10 s cold-start delay. A later PIE check on the placed `BalloonFlight_Test` actor confirmed the stove resolves the `BP_Balloon` Health pool and two crystals heat to 100 degrees and reduce health. These historical checks predate the pending three-bucket migration and do not establish Phase 6 acceptance.

Coverage exists in `GunsAndBalloons.Stove.Heat`, `.Overheat`, `.Interaction`, `.VFX`, `.DamageSmoke`, the cooling test, `GunsAndBalloons.Health` and `GunsAndBalloons.Player.HandItems`. Full three-bucket, health, visual/prop-placement and lever-regression acceptance remains the unchecked Phase 6 record.

## Known issues and pitfalls
- Until `configure_balloon_health.py` runs on the new build, `BP_Balloon` Health filters on the exact bone `basket` only: hits on its child bones (stove, door, bucket) and on the bone-less wall/floor boxes deal no damage.
- Health and supply settings are Blueprint read-only: change them in Details, not at runtime from Blueprint.
- Phase 6 manual component migration and all its PIE acceptance items are pending; final bucket models and hand presentation need visual tuning.
- Do not follow the older plan's closed-mouth auto-open instruction; door clicks alone toggle it.
- Open-door leakage continues during decay pause after heating ends; closing the door preserves the intended pause.
- Logical door state changes before its animated angle reaches the endpoint.
- Imported bone scale and stale instance overrides can hide the firebox or enlarge collision; verify world-space dimensions and camera traces.
- Health depletion only emits an event. Game-over flow, damage decals and external repair integration are not validated by the stove checks.

2026-11-15: damage-hole smoke added to `UStoveVfxComponent`. Editor Win64 Development build passed. `Stove.DamageSmoke` (new: tick starts on damage, stops at full health, no crash without hole bones), `Stove.VFX`, `.Overheat`, `.Interaction` and `GunsAndBalloons.Health` passed; `Stove.Heat` and `Stove.TickSize` still fail with the known timing mismatches. Emission from real hole bones is not tested: the project's `SK_Balloon` asset doesn't have the `stove_damage_*` bones until it is reimported (docs/Unreal_Prompt_6_FuelBuckets_RadarJam.md).
