# Stove: Phase 4 integration

Phase 6 manual wiring and acceptance: [BP_Balloon checklist](stove-phase6-checklist.md).
It migrates the existing wood supply to coal and adds the other two buckets.
The historical Phase 5 setup below describes the pre-migration state.

`UStoveComponent` implements `IInteractable`. `AcceptsHit` caches door versus firebox;
`Interact` consumes that cache once. Only door clicks toggle the door; a closed
firebox click leaves the door closed without consuming fuel. Open firebox clicks call `AddFuel` only for an active
`UFuelHandComponent` in a nonempty hand, then `ConsumeHeldItem` only on success.
Clicks remain consumed when fuel is absent or rejected.

`FireboxShape` is assigned in BP; BeginPlay configures query-only collision, ignoring
all channels except `InteractTraceChannel` (Visibility). If the reference still points
to a Blueprint template, BeginPlay resolves the owner's `FireboxComponentName`
(default `StoveFirebox`) instead. Door-local imported box dimensions are scaled by
the reference bone transform when fitting the mouth in mesh centimeters.
Refit any placed instance that overrides the old small box as well as the Blueprint.
The pawn must use the same channel. Do not add an AmmoSupplyComponent to BP_Balloon:
it routes clicks to RMB.

Bucket pickup and hand presentation are implemented; see Phase 5 below.
Verified in PIE on 2026-11-15: camera click takes wood, door click opens, mouth
click consumes wood and adds 180 s. The burn arrow moves +6.6 cm at 180/600 s;
after the 10 s cold-start delay, heat rises and the model's gauge needle rotates.

## Editor setup

### Stove VFX

`configure_stove.py` adds `StoveVFX` (`UStoveVfxComponent`) to `BP_Balloon`.
It binds `OnFuelAdded` and `OnOverheatChanged`, reusing `AGunSmoke`'s generated
PS1-style masked sprites; no external particle assets are required. Fuel acceptance
emits one orange burst from the live firebox. Overheat starts dark rising smoke above
the stove and stops emission on cooling; existing puffs fade naturally. EndPlay
unbinds both events and destroys both effect actors.

Blueprint tuning: `EffectClass`, `FuelBurst` (Amount = count per accepted piece),
`OverheatSmoke` (Amount = puffs per second), both colors, `StoveBone`, and
`OverheatOffset` (mesh centimeters, unaffected by the imported bone's 100x scale).
All visual numbers are TBD prototypes. `GetFuelEffect` / `GetOverheatEffect` expose
the spawned actors for inspection or further Blueprint tuning. VFX do not run on
dedicated servers. The balloon Blueprint places vent smoke at mesh offset
(0, -28, 100) cm to clear the dome. Source motion and local wind follow the flight
component's current speed (the balloon moves directly, so actor velocity can be zero).
`GunsAndBalloons.Stove.VFX` verifies rejection, bursts, continuous
overheat emission, cooling and cleanup.

### Blueprint and model setup

Close the editor, build `gunsandballoonsEditor Win64 Development`, then reopen.
In Tools → Execute Python Script, select `Scripts/Editor/configure_stove.py`.
It preserves fuel assets, adds Stove and StoveFirebox if absent, connects update
variables, appends three bone controls to the existing component-space chain, and
configures a query-only door body via `UStoveEditorLibrary` (creating it if absent).
Door bounds come from imported vertices in bone space, without a one-unit minimum
(the component scales the mesh). The firebox is fitted behind the door once.
Subsequent runs preserve manually tuned firebox placement; clear its
`StoveFitted` component tag to refit. Rerunning setup refits the door body.
Compile failures stop saving the affected Blueprint.
The helper refuses ambiguous pose chains; use the manual steps below in that case.
Check the generated boxes visually before PIE and tune the firebox as needed.

## Manual fallback / visual checks

1. Open BP_Balloon → Components. Add StoveComponent named Stove and a Box Collision
   named StoveFirebox attached to Mesh. Fit the box inside the mouth, behind the closed
   door. Set Query Only, ignore all channels, block Visibility. Assign Stove.FireboxShape.
   Ensure no stove/basket collision approximation hides the mouth from the camera trace.
2. Open SK_Balloon_PhysicsAsset. Select furnace_door → Add Body → Box. Fit it tightly
   to the door; the hinge is at its edge. Set Kinematic, Query Only, ignore all channels
   except Visibility = Block. Ensure Mesh enables query collision and blocks Visibility.
3. Open ABP_Balloon → Event Graph. Add StoveRef (StoveComponent object), StoveDoorAngle,
   StoveTemp01, StoveBurn01 (floats). On Blueprint Update Animation, get the owning actor,
   find StoveComponent and store StoveRef. If valid, read GetDoorAngle, GetTemperature01
   and GetBurnTime01 into the floats, then continue existing wing updates.
4. In AnimGraph, append Transform (Modify) Bone controls before Component To Local.
   Use additive rotation, component space for furnace_door: yaw = StoveDoorAngle.
   For gauge_needle use additive rotation in parent space: GetGaugeRotation derives
   the imported normal and applies StoveTemp01 × StoveRef.GaugeSweepDeg
   (240°, from Balloon_build.py GSW).
   For burn_arrow use additive component-space translation Z = StoveBurn01 × 22 cm.
   Keep all control alpha values at 1 and preserve the wing chain.
5. Compile and save BP, ABP and PhysicsAsset. In PIE click the door twice: it should
   smoothly reach DoorOpenAngle then 0. If it swings inward, negate DoorOpenAngle.
   Click the closed mouth: it stays closed and retains fuel. With Phase 3 equipped, an open
   mouth accepts fuel; a full stove retains the held item. Check a normal weapon click
   on the open mouth does not shoot or add fuel.
6. Select the live ABP debug instance and watch StoveDoorAngle/StoveTemp01/StoveBurn01.
   Compare with the live stove GetDoorAngle/GetTemperature01/GetBurnTime01 values.
   At temperature 0/full gauge the needle should reach the two ends of its 240° arc;
   adjust the imported gauge axis/sign if needed. At burn fraction 1 the arrow moves
   exactly +22 cm. PIE/art acceptance requires a human visual pass.

Automation: `GunsAndBalloons.Stove.Interaction` covers filtering, cache consumption,
door toggles, closed-mouth rejection, empty-mouth safety, rejected fuel retention and
successful fuel consumption. Existing stove tests
cover interpolation and normalized outputs.

Validated 2026-11-15: Development Editor build passed; Heat, Interaction and Overheat
automation passed. Commandlet setup compiled/saved BP_Balloon, ABP_Balloon and the
PhysicsAsset successfully on two consecutive runs. Visual PIE acceptance remains open.


## Phase 5: fuel in hand (2026-11-15)

`UFuelHandComponent` is the sixth and last pawn inventory item. It has no skeletal
asset: a `FuelProp` static mesh attaches to `FuelHandMesh`. `SetFuel` synchronously
loads the fuel asset's HandMesh and applies HandTransform. The prop uses first-person
rendering; fuel never fires or updates smoke. The inherited rest pose fields remain
editable; the prototype offset and fuel transforms need visual tuning.

`TakeHandItem` requires an empty hand and an item in the pawn inventory. It shares
DrawWeapon's raise helper and clears the home rack. `ConsumeHeldItem` hides the item
immediately and clears pending rack transitions before setting Empty. Fuel cannot
be holstered or swapped onto a weapon rack; the rack declines its click.
StartingGunIndex remains limited to 0..4, including at runtime.

`UFuelSupplyComponent` stores one fuel type. Add three instances to the balloon,
assign wood/coal/crystal, and set StartCount to 4/4/2 (TBD prototype values), with
Capacity 10 (TBD). InteractShape is the clickable box; BucketMesh is optional and
should stay unset for the bucket already modeled in the balloon rig. `configure_stove.py`
now connects that bucket to wood (4 pieces, capacity 10; TBD prototype values),
with a mesh-space click box centered at (47, 27, 18) cm and extents (17, 17, 17) cm.
Its top clears the existing bucket physics body so camera clicks reach the supply.
The wood hand prop uses a TBD placeholder scale (0.12, 0.05, 0.05).
The two other
bucket meshes and shape bounds need editor placement. No bucket assets are changed
by this phase.

Empty-hand clicks take one piece. Same-type clicks return it if space remains.
Other-type clicks return to FindSupplyFor(Owner, held type) before taking the target
fuel, provided both operations fit. An empty target, missing home, or full home
leaves stocks and hand unchanged. AddFuel returns the accepted refill quantity;
OnContentsChanged reports each new stock count. Runtime stock initializes from
StartCount in BeginPlay.

Automation: GunsAndBalloons.Player.HandItems covers fuel pickup, presentation,
return, three-bucket swaps, rack rejection, capacity, empty buckets, closed/open
firebox feeding and updates after consumption. Development Editor build and all
9 GunsAndBalloons automation tests passed on 2026-11-15. Visual PIE validation of
prop placement and bucket art remains a manual check.
