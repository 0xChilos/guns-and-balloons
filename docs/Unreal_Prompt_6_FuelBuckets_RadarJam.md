You are connected to my Unreal Engine 5 editor (project "gunsandballoons"). **Update** the balloon with four changes: the propeller always spins, the radar gets a jammed state, the single fuel bucket becomes three buckets whose contents show how much fuel is left, and the engine shows bullet holes as the balloon loses health. Work through the steps in order and verify each one. If a step fails, stop and tell me what failed and why. Use the editor Python API where possible; where something can't be authored through your tools (AnimGraph or Blueprint graph nodes), give me exact click-by-click steps instead.

## Safety rules
The safety rules of docs/Unreal_Prompt_1_Integrate.md apply here unchanged: never delete, rename or recreate an existing asset; reimport SK_Balloon **into the existing asset**; only add components, variables and functions; don't change C++ or Scripts/; back up SK_Balloon, SK_Balloon_Skeleton and SK_Balloon_PhysicsAsset into /Game/Assets/Balloon/_Backup first; don't commit or push.

The fuel buckets use the C++ components `UFuelSupplyComponent` and `UStoveFuelType` (docs/tech/stove.md). If the project's C++ doesn't compile or those classes are missing, do steps 0–3, 6 and 7 and tell me step 4–5 is blocked.

## Source files
On disk: D:\test_git\guns-and-balloons\Content\Assets\Balloon
- SK_Balloon.fbx: 12.6k tris, 56 bones, the same 4 material slots. Bounds unchanged (about 696 × 693 × 1184 cm).
- T_Balloon_BaseColor.png, T_Balloon_ORM.png: re-baked (new buckets). T_Balloon_Radar.png is unchanged.
- **New:** SM_Fuel_Wood.fbx (one split log, 15 cm long, 7 cm thick, lies along X), SM_Fuel_Coal.fbx (one lump, about 8 cm), SM_Fuel_Crystal.fbx (one glowing shard, 14 cm tall, grows along +Z from its origin). Static meshes, authored in metres, material slots M_Balloon (+ M_Glow on the crystal).

**What changed in the model:**
- The old coal scuttle (bone `fuel_bucket`) is replaced by **three compact fuel stores** (about 30 cm wide, packed tight when full) standing by the stove (sides as seen by the player facing the stove), each open or low toward the basket centre so the contents show. They look like their fuel: wood = plank crate, coal = open burlap sack, crystal = iron scuttle bucket (no signs or icons). "Bucket" below means any of the three:

  | Bucket bone | Fuel | Where | Piece bones (filled in this order) |
  |---|---|---|---|
  | `fuel_bucket` (name kept) | wood | against the stove, on the player's right (−X in Blender) | `fuel_wood_1` … `fuel_wood_6`: end-on pyramid, 3 bottom, 2 middle, 1 top |
  | `fuel_bucket_coal` | coal | against the stove, on the player's left (+X in Blender) | `fuel_coal_1` … `fuel_coal_6`: ring of six, a dark mark under each |
  | `fuel_bucket_crystal` | crystal | in front of the wood bucket, against the side wall | `fuel_crystal_1` … `fuel_crystal_6`: ring of six iron collars |

- The stove's chimney pipe is rerouted and thinner (10 cm): a short rise from the stove, a horizontal run to the back-right wall (behind the coal sack), straight up about 5 cm off that wall, then a bend inward through the rope load ring and up the skirt, ending just inside the balloon's mouth ring. It is braced to the rope load ring and stayed to the rim. It is part of the stove (bone `stove`) and has no collision of its own; if BP_Balloon has a collision shape for the old pipe through the middle of the basket, report it.
- **Tool board** on the wall to the left of the navigation console (replaces the old board): a plain tool board in the balloon's colours (plank panel, iron frame, brass corners). It holds the wrench (brass peg, top left, in plain view from the middle of the basket), the grappling hook (two forked brackets, hung diagonally: grip low on the left, claw up to the right) and the extinguisher (floor cup + strap, bottom right, hose toward the middle). The three tool sockets keep their names but moved:

  | Socket | Tool | Head / long axis |
  |---|---|---|
  | `socket_tool_vertical_1` | extinguisher | the carry handle (the extinguisher's origin) / up |
  | `socket_tool_horizontal` | grappling hook | the grip end (the hook's origin) / along the barrel, 30° up toward the claw (to the right) |
  | `socket_tool_vertical_2` | wrench | the top of the wrench (its origin) / down |

  Nothing in C++ uses these sockets yet (the tools are pickups placed by `Scripts/Editor/place_item_pickups.py`). Report where the tools are placed now; don't move them onto the stand unless I ask.
- The stopper levers (`lever_L`, `lever_R`) moved 5 cm toward the back, to the centre of their side walls. Nothing to change: BalloonWingLeverComponent reads the bone transforms. Just check in PIE that both levers can still be grabbed and drive their stoppers.
- Every piece is a separate part rigidly skinned to its own bone (child of its bucket bone). **Hiding a piece bone hides that piece.** Showing pieces 1..N and hiding the rest makes the bucket show exactly N pieces, and empty slots stay visible (gap in the pyramid, dark mark, empty collar).

## 0. Look before touching anything
Report:
- On BP_Balloon: every `FuelSupplyComponent` (name, Fuel, Capacity, StartCount, attach parent/socket, relative location), and what drives the propeller (`PropellerSpeed` and the propeller bone node in ABP_Balloon, and who sets it).
- MPC_Radar: does the scalar `RadarJam` exist? M_Display: its current radar Custom node inputs and code. Compare the code with docs/Unreal_Prompt_3_RadarEnemies.md step 2 and tell me the differences before changing anything.
- BP_JammerTower: does it write `RadarJam` into MPC_Radar, and back to 0 when the balloon leaves its radius or the dish is shot down?
Then make the backups.

## 1. Textures
Re-import T_Balloon_BaseColor.png and T_Balloon_ORM.png in place. Keep their settings (Nearest, TEXTUREGROUP_Pixels2D, NoMipmaps, NeverStream; BaseColor sRGB + TC_EditorIcon, ORM linear + TC_Masks).

## 2. Skeletal mesh
Reimport SK_Balloon into the existing asset with the same settings as Prompt 1 step 2 (no materials, textures or animations, scale 1.0, convert_scene, Import Normals). Let the reimport merge the new bones into SK_Balloon_Skeleton. Don't regenerate the physics asset.

Verify, and report:
- Bounds still about 696 × 693 × 1184 cm (if 100× off: undo and stop).
- New bones present: `fuel_bucket_coal`, `fuel_bucket_crystal`, `fuel_wood_1..6` (children of `fuel_bucket`), `fuel_coal_1..6`, `fuel_crystal_1..6`, `stove_damage_1..10` (children of `stove`). All earlier bones still exist (`lever_L`, `stopper_L`, `furnace_door`, `propeller`, the sockets …).
- The 4 material slots still have MI_Balloon, MI_Glow, MI_Display, MI_WarningLamp.

## 3. Fuel piece meshes
Import SM_Fuel_Wood, SM_Fuel_Coal and SM_Fuel_Crystal into /Game/Assets/Balloon as static meshes: import_materials false, import_textures false, uniform scale 1.0, convert_scene true, Auto Generate Collision off, normal import method Import Normals. Assign MI_Balloon to slot M_Balloon and MI_Glow to slot M_Glow. Report each one's bounds (wood ≈ 15 × 7 × 7 cm, coal ≈ 8 × 8 × 6 cm, crystal ≈ 8 × 5 × 14 cm).

Then set them as the hand props: `HandMesh` on /Game/Data/Stove/DA_Fuel_Wood, DA_Fuel_Coal, DA_Fuel_Crystal = SM_Fuel_Wood, SM_Fuel_Coal, SM_Fuel_Crystal (they currently hold engine placeholder shapes). Leave `HandTransform` as it is and tell me how the pieces look in the hand in PIE, so I can tune it.

## 4. Three buckets on BP_Balloon
One `FuelSupplyComponent` per bucket. Keep the existing `WoodFuelSupply`; add `CoalFuelSupply` and `CrystalFuelSupply` only if no supply for that fuel exists yet.

| Component | Fuel | Attach to Mesh, socket (bone) | Relative location | InteractShape extent | Capacity | StartCount |
|---|---|---|---|---|---|---|
| WoodFuelSupply | DA_Fuel_Wood | `fuel_bucket` | (0, 0, 22) | (17, 17, 16) | **6** | TBD 4 |
| CoalFuelSupply | DA_Fuel_Coal | `fuel_bucket_coal` | (0, 0, 22) | (17, 17, 16) | **6** | TBD 4 |
| CrystalFuelSupply | DA_Fuel_Crystal | `fuel_bucket_crystal` | (0, 0, 22) | (17, 17, 16) | **6** | TBD 2 |

- Leave `BucketMesh` empty: the buckets are part of SK_Balloon.
- WoodFuelSupply currently sits at the old scuttle (relative (47, 27, 18)). Moving it to its new bucket is expected; tell me its old and new values.
- **Bucket collision**, so the player can't walk through them: one BoxComponent per bucket, attached to the same bone, extent (16, 16, 15), relative location (0, 0, 15). Collision: block Pawn only, **ignore Visibility** (and whatever channel the supplies' `InteractTraceChannel` is), so clicks still reach the InteractShape.

## 5. Show how full each bucket is
In BP_Balloon add a function **UpdateFuelBucket(Supply: FuelSupplyComponent, Count: int)**:
1. Prefix from the supply: WoodFuelSupply → `fuel_wood_`, CoalFuelSupply → `fuel_coal_`, CrystalFuelSupply → `fuel_crystal_` (or switch on Supply.Fuel).
2. For i = 1 … 6: bone = Prefix + i. If i ≤ Count → Mesh.UnHideBoneByName(bone), else Mesh.HideBoneByName(bone, PBO_None).

Add a function **InitFuelBuckets**, called at the end of the existing BeginPlay (append the call, don't change what BeginPlay does):
- Bind each supply's `OnContentsChanged(Supply, NewCount)` to UpdateFuelBucket.
- Then call UpdateFuelBucket(Supply, Supply.GetCount()) once for each supply (StartCount may already have been applied before the binding).

Pieces fill in slot order (1 = first in, the highest number leaves first), so a bucket always shows its count in the same positions.

## 5b. Engine damage holes
The stove body is the balloon's health (shared `UHealthComponent`, MaxHealth 100). The model has **10 bullet holes** on the stove, bones `stove_damage_1` … `stove_damage_10` (children of `stove`), each one rigidly skinned like the fuel pieces: a black soot blot, torn bright-metal petals and a glowing red core (on slot M_Glow, so it glows and flickers like the furnace coals). All 10 are on the stove faces the player sees from inside the basket (the two front-diagonal faces and the lid). They appear in a **random order**; the bone numbers are only slot names.
- One hole per **10 HP missing**: `N = clamp(floor((MaxHealth − Health) / 10 + 0.001), 0, 10)` (the 0.001 keeps float drift such as 80.0000001 HP from dropping a hole). 100 HP = no holes, 90 = 1, … 0 = all 10.
- In BP_Balloon add an int array variable **DamageOrder**. In InitDamageHoles (below) fill it with 1 … 10 and shuffle it (a new random order every play session).
- Add a function **UpdateDamageHoles(Health: float)**: compute N, then for k = 0 … 9: bone = `stove_damage_` + DamageOrder[k]; k < N → Mesh.UnHideBoneByName(bone), else Mesh.HideBoneByName(bone, PBO_None). So holes open in the shuffled order and close in reverse (the newest hole closes first).
- Add a function **InitDamageHoles**, called from the end of the existing BeginPlay (append the call only): shuffle DamageOrder, bind the health component's `OnHealthChanged(Health, NewHealth, Delta)` to UpdateDamageHoles(NewHealth), then call UpdateDamageHoles(GetHealth()) once.
- Repairs go through `UHealthComponent::Repair` (or SetHealth), which broadcasts OnHealthChanged, so the holes close again (newest first) whatever repairs the engine. Don't add a repair interaction here: how the player repairs (the wrench?) is still an open design question.
- The hole bones have no physics bodies; check that hits on the stove still reach the health component the same way as before.
- **Smoke from the holes** is C++ (`UStoveVfxComponent`, the existing `StoveVFX` component on BP_Balloon): while the balloon is damaged it emits a thin dark smoke stream from every `stove_damage_*` bone that isn't hidden. Nothing to build: just compile the C++, check that StoveVFX's new `DamageSmoke` / `DamageColor` settings show up, and leave their defaults.

## 6. Propeller always spins
The propeller must spin all the time, also at the minimum flight speed and while the stove is cold.
- In ABP_Balloon add floats `PropellerIdleSpeed` (default 360 deg/s, TBD tuning) and `PropellerAngle`.
- In the Event Graph update: `Spin = max(PropellerIdleSpeed, PropellerSpeed)`; `PropellerAngle = fmod(PropellerAngle + Spin × DeltaTime, 360)`.
- Drive the existing `propeller` bone node (roll) from `PropellerAngle` instead of accumulating there. If the bone is driven anywhere else, don't add a second driver: change only its input and tell me where it was.
- If `PropellerSpeed` is never set (always 0), the idle speed alone keeps it spinning. If it is set from flight speed, keep that: faster flight still spins it faster.
- Check that nothing gates the update (flight state, stove on/off, Mesh "Visibility Based Anim Tick Option" while visible).

## 7. Radar jammed state
Near a jammer tower the radar turns into static: sparkling noise, torn rows, a stuttering sweep and flickering **ghost blips** that jump around. The real targets fade out completely, so the player can't tell where attackers are. It is driven by `RadarJam` (0 = clear, 1 = next to a jammer); full static from 25 % jammer strength (`J = saturate(RadarJam × 4)`, TBD).
1. MPC_Radar: add the scalar `RadarJam` (default 0) if it's missing.
2. M_Display: add the Custom node input **Jam** ← CollectionParameter MPC_Radar `RadarJam`, and replace the node's code with the code in docs/Unreal_Prompt_3_RadarEnemies.md step 2 (it now contains the jammed state; everything else is unchanged). If step 0 found differences from that prompt, apply only the jam additions on top of the existing code instead (blocks marked "jammed", "ghost targets", the `blip *=` line and the `noise` lines). Compile, save M_Display and MI_Display.
3. BP_JammerTower must write the **strongest** jammer's value into `RadarJam`, and 0 when the balloon is out of every jammer's range or the dish is destroyed. If it doesn't, add that (or set it from BP_Balloon's `OnRadarJam(Strength)` handler) and tell me what you changed.
4. The warning lamp keeps working while jammed: it shows how close the worst threat is, not where it is.

## 8. Test and report
In BalloonFlight_Test (PIE):
- The propeller spins from the first frame, also at minimum speed and with a cold stove, and spins faster at higher speed if PropellerSpeed is driven.
- The buckets start with 4 logs, 4 lumps and 2 crystals in their slots. Take a piece: the highest-numbered one disappears. Return it: it reappears. A full bucket (6) accepts nothing more. Swap between buckets updates both.
- Each fuel shows its own mesh in the hand.
- Damage the balloon in steps (ApplyDamage 10 from a test key or the console): one more hole appears every 10 HP, all 10 at 0 HP, and their cores glow. Repair 25: the two newest holes close (the third stays until 10 more HP). Restart PIE: the holes appear in a different order. Full repair removes all of them. Each visible hole gives off a thin stream of dark smoke; a closed hole stops smoking.
- Place a BP_JammerTower near the balloon's path: the radar fades into static with ghost blips and no real blips inside the radius, and recovers when you leave or shoot the dish down.
- Screenshots: the three buckets from the player's standing position (one partly empty), and the radar clear / jammed.
- A list of everything you changed, added and skipped (and why).
