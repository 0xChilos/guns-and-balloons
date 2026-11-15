# Stove and fuel - technical reference (C++)

> **For agents.** Design: [engine-stove](../features/engine-stove.md); approved decisions override the older implementation plan. Flight API: [balloon-flight](balloon-flight.md). **Updated:** 2026-11-15

## Model in one paragraph
`UStoveComponent` simulates temperature, ignition, burn time and an animated door on `BP_Balloon`. Three `UFuelSupplyComponent` buckets supply one piece of wood, coal or crystal to the pawn's `UFuelHandComponent`. Door clicks toggle the door; an open firebox accepts fuel and consumes the held piece only on success. Fuel raises the pending heat target and adds burn time; crystal heats instantly. Temperature publishes the named flight modifier `Stove`, and overheating damages the balloon's shared `UHealthComponent`. `ABP_Balloon` reads the door, gauge and burn-arrow outputs; `UStoveVfxComponent` reacts to fuel and overheat events.

## Files
| File | Role |
| --- | --- |
| `Source/gunsandballoons/StoveComponent.h/.cpp` | Simulation, door/firebox interaction, flight outputs, overheat |
| `Source/gunsandballoons/StoveFuelType.h/.cpp` | Fuel DataAsset, heat/time and hand presentation |
| `Source/gunsandballoons/FuelSupplyComponent.h/.cpp` | Bucket stock, pickup, return and swap |
| `Source/gunsandballoons/FuelHandComponent.h/.cpp` | Non-firing held static-mesh prop |
| `Source/gunsandballoons/HealthComponent.h/.cpp` | Shared damage/repair pool and actor damage filtering |
| `Source/gunsandballoons/FirstPersonCameraPawn.h/.cpp` | Interaction dispatch, TakeHandItem / ConsumeHeldItem |
| `Source/gunsandballoons/StoveVfxComponent.h/.cpp` | PS1 fuel burst and sustained overheat smoke using AGunSmoke |
| `Source/gunsandballoons/StoveEditorLibrary.h/.cpp` | Editor collision and animation helpers |
| `Source/gunsandballoons/Tests/StoveTest.cpp`, `StoveInteractionTest.cpp`, `StoveVfxTest.cpp`, `HealthTest.cpp`, `HandItemTest.cpp` | Automation coverage |
| `Scripts/Editor/configure_stove.py` | Fuel assets and historical BP/ABP setup |
| `Scripts/Editor/configure_balloon_health.py` | Idempotent shared health setup without rerunning the historical stove migration |
| `/Game/Data/Stove/DA_Fuel_Wood`, `DA_Fuel_Coal`, `DA_Fuel_Crystal` | Per-type fuel assets |

## Player flow
1. Empty hand + LMB on a bucket takes one piece. Same-type bucket returns it; another type swaps it, returning the held piece to its own supply first.
2. Empty target, missing home supply or full return supply leaves hand and stocks unchanged.
3. LMB on the door toggles it. A closed-firebox click is consumed but leaves the door closed and fuel held (the later approved rule supersedes plan D2).
4. Open-firebox click feeds only held fuel. Rejected fuel stays held; accepted fuel immediately empties the hand. Weapon/empty-hand mouth clicks are consumed without shooting.
5. Fuel cannot be placed on weapon racks or exchanged for pickups; levers require an empty hand. Return fuel or feed it first.

## API
Units: seconds, degrees, degrees/second; animation translations are centimeters.

| Member | Kind | Behaviour |
| --- | --- | --- |
| `FindStove(Actor)` | BlueprintPure static | Find the actor's stove component |
| `CanAddFuel(Fuel)` / `AddFuel(Fuel)` | BlueprintPure / Callable | Require open door, valid nonnegative finite fuel values and available burn capacity; AddFuel returns success |
| `SetDoorOpen(bool)` / `ToggleDoor()` | BlueprintCallable | Set logical door state; angle interpolates at DoorSpeed |
| `IsDoorOpen()` / `GetDoorAngle()` | BlueprintPure | Logical state / animated angle; feeding checks logical state |
| `GetTemperature()` / `GetTemperature01()` | BlueprintPure | Degrees / clamped Temperature divided by GaugeMaxTemperature |
| `GetGaugeAngle()` / `GetGaugeRotation()` | BlueprintPure | Fraction * GaugeSweepDeg / rotation around imported gauge normal in parent space |
| `GetBurnTimeLeft()` / `GetBurnTime01()` | BlueprintPure | Seconds / clamped fraction of MaxBurnTime |
| `IsLit()` / `IsOverheated()` / `GetSpeedMultiplier()` | BlueprintPure | Lit means positive temperature outside ignition delay; overheat is strictly above threshold |
| `OnFuelAdded(Fuel)` | BlueprintAssignable | Accepted fuel only, after outputs update |
| `OnIgnited` / `OnExtinguished` | BlueprintAssignable | Ignition completes / positive temperature reaches zero |
| `OnOverheatChanged(bool)` / `OnDoorChanged(bool)` | BlueprintAssignable | Overheat transition / logical door transition |
| Supply: `AddFuel(Amount)` | BlueprintCallable | Refill stock; returns accepted quantity |
| Supply: `GetCount()` / `GetCapacity()` / `OnContentsChanged(Supply, NewCount)` | Pure / event | Stock inspection and changes; StartCount initializes at BeginPlay |
| Fuel hand: `SetFuel(Type)` | BlueprintCallable | Load HandMesh and apply HandTransform; no firing behaviour |
| Health: `ApplyDamage(Amount, Causer)` / `Repair(Amount, Repairer)` | BlueprintCallable | Shared health pool; returns amount applied |
| Health: `SetHealth`, `GetHealth`, `GetHealth01`, `IsDepleted` | Callable / Pure getters | Clamped health state |
| Health: `OnHealthChanged`, `OnDepleted`, `OnRestored` | BlueprintAssignable | Health delta, depletion and recovery transitions |
| VFX: `GetFuelEffect()` / `GetOverheatEffect()` | BlueprintPure | Spawned effect actors for inspection |

### Configuration
TBD entries below are editable prototype values, not final balance.

| Field / setting | Value | Notes |
| --- | --- | --- |
| Wood: BurnTime / HeatAdd | 180 s / 20 deg | HeatRateOverride = 0, bInstantHeat = false |
| Coal: BurnTime / HeatAdd | 120 s / 30 deg | HeatRateOverride = 0, bInstantHeat = false |
| Crystal: BurnTime / HeatAdd | 10 s / 50 deg | bInstantHeat = true; skips cold ignition delay |
| HeatRate / HeatRateOverride | 5 deg/s / 0 | Per-fuel 0 uses live stove rate |
| DecayBurning / DecayEmpty | 1 / 5 deg/s | No decay while heating |
| DecayPause / IgnitionDelay | 10 / 10 s | Burn time does not drain during ignition |
| MaxBurnTime / bClampBurnTimeOnOverfill | 600 s / false | Default rejects entire piece on overflow |
| OverheatThreshold | 90 deg | Damage and overheat speed apply strictly above |
| MaxTemperature / GaugeMaxTemperature | TBD 120 deg / TBD 100 deg | Gauge can saturate before temperature cap |
| SpeedMultAt90 / SpeedMultOverheat | TBD 1.6 / TBD 2 | x1 at zero, linear to threshold, then overheat multiplier |
| OverheatDamagePerSecond / MaxHealth | TBD 2 / TBD 100 | Damage per second / shared health maximum |
| DoorOpenDecayMultiplier | TBD 2 | Open door also leaks heat during decay pause |
| DoorOpenAngle / DoorSpeed | TBD +100 deg C++ default; BP uses -100 deg / TBD 300 deg/s | Preserve approved BP direction |
| Capacity / StartCount | TBD 10 each / TBD wood 4, coal 4, crystal 2 | Three independent supplies |
| DoorBone / FireboxComponentName / InteractTraceChannel | furnace_door / StoveFirebox / Visibility | Must match collision and pawn trace |
| GaugeSweepDeg / burn-arrow travel | 240 deg / +22 cm | Rig dimensions |
| Health.DamageBones / bListenToActorDamage | stove, furnace_door / true | Phase 6 wiring; point hits filtered by bone |
| HandMesh / HandTransform / hand rest pose | TBD placeholder art and transforms | Wood prototype scale 0.12, 0.05, 0.05; tune visually |
| Extra bucket art, placement and click bounds | TBD | Manual Phase 6 uses Cylinder placeholders for wood/crystal |
| VFX FuelBurst / OverheatSmoke / colors / OverheatOffset | TBD visual tuning | Burst Amount = count; smoke Amount = puffs/s; BP offset (0, -28, 100) cm |
| FlightModifierEpsilon | 0.001 | Suppress insignificant output updates |

## Flight integration
The stove ticks in `TG_PrePhysics`, adds itself as a flight tick prerequisite, and publishes `SetFlightModifier("Stove", 0, multiplier)` when changed. Other flight modifiers multiply with it; effective target speed remains clamped to flight MinSpeed/MaxSpeed. EndPlay clears the modifier and prerequisite. Use TargetSpeed 100 when checking x2 so MaxSpeed does not mask it.

## How to use it
Add Stove and Health to the balloon owner; the stove discovers both Health and Flight in BeginPlay if unassigned. Overheat calls Health.ApplyDamage; other systems use ApplyDamage/Repair directly. OnDepleted is the balloon-destroyed integration event; game-over flow remains external. Configure DamageBones to stove and furnace_door; point damage must count once despite the matching actor any-damage callback.

VFX binds OnFuelAdded and OnOverheatChanged: accepted fuel emits an orange burst, overheat emits dark rising smoke, cooling stops emission while existing puffs fade. Source motion follows flight speed. EndPlay unbinds and destroys effects; dedicated servers skip VFX.

## Setup in `BP_Balloon`
Follow the [Phase 6 wiring and PIE checklist](stove-phase6-checklist.md), whose migration and acceptance remain pending. It reuses the modeled bucket for coal and adds wood/crystal placeholders. The historical script still creates WoodFuelSupply on the modeled bucket: **do not rerun its Blueprint setup after manual migration**, or it will recreate the old supply. Fuel assets can be created separately with configure_fuel_assets().

Before wiring, check BP_Balloon and its owner chain for AmmoSupplyComponent: pawn LMB dispatch rejects that chain. Keep ammo in separate crates. Use one query-only StoveFirebox behind the door, Visibility Block, and a fitted query-only furnace_door physics body. BeginPlay resolves FireboxComponentName if FireboxShape references a BP template. Account for imported bone scale when fitting boxes; check placed-instance overrides.

ABP_Balloon caches StoveRef and reads StoveDoorAngle, StoveTemp01 and StoveBurn01 during animation update. Preserve wing controls; append bone controls before Component To Local: furnace_door additive component-space yaw, gauge_needle additive parent-space GetGaugeRotation(), burn_arrow additive component-space Z = GetBurnTime01() * 22 cm. Keep alpha 1. The script preserves manually fitted firebox placement via the StoveFitted tag; clearing it requests a refit. It refits the door body and refuses ambiguous pose chains or saving failed Blueprint compiles.

## Validation (2026-11-15)
Documentation/API review in Phase 7 checked the source headers, simulation, approved decisions and Phase 6 checklist. No build, automation or PIE was rerun for this documentation-only change.

Earlier records on this page reported a successful Development Editor build, Heat/Interaction/Overheat automation, two successful commandlet setup runs, and all nine then-existing GunsAndBalloons tests after Phase 5. A recorded PIE check took wood, opened the door and fed 180 s: the arrow moved +6.6 cm and the gauge rose after the 10 s cold-start delay. These historical checks predate the pending three-bucket migration and do not establish Phase 6 acceptance.

Coverage exists in `GunsAndBalloons.Stove.Heat`, `.Overheat`, `.Interaction`, `.VFX`, `GunsAndBalloons.Health` and `GunsAndBalloons.Player.HandItems`. Full three-bucket, health, visual/prop-placement and lever-regression acceptance remains the unchecked Phase 6 record.

## Known issues and pitfalls
- `BP_Balloon` now includes `Health` with actor damage enabled and point-hit bones `stove` / `furnace_door`. Verified on the placed `BalloonFlight_Test` actor in PIE: stove resolves this pool, and two crystals heat to 100 degrees and reduce health. MaxHealth remains the TBD prototype 100.
- Phase 6 manual component migration and all its PIE acceptance items are pending; final bucket models and hand presentation need visual tuning.
- Do not follow the older plan's closed-mouth auto-open instruction; door clicks alone toggle it.
- Open-door leakage continues during decay pause after heating ends; closing the door preserves the intended pause.
- Logical door state changes before its animated angle reaches the endpoint.
- Imported bone scale and stale instance overrides can hide the firebox or enlarge collision; verify world-space dimensions and camera traces.
- Health depletion only emits an event. Game-over flow, damage decals and external repair integration are not validated by the stove checks.
