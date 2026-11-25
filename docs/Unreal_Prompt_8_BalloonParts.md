You are connected to my Unreal Engine 5 editor (project "gunsandballoons"). The player balloon used to be one skeletal mesh, SK_Balloon. It is now split into the balloon itself plus 6 part meshes, so different people can change one part without touching the others. Bring the split into the project **without breaking anything my teammates built on the balloon**. Work through the steps in order and verify each one. If a step fails, stop and tell me what failed and why. Use the editor Python API (`unreal` module) where possible. If something can't be authored through your tools (Blueprint graph nodes), give me exact click-by-click steps instead.

## How the split works (read first)
- **SK_Balloon** stays the main mesh and keeps everything that code relies on:
  - the full skeleton SK_Balloon_Skeleton (all 56 bones and sockets)
  - the physics asset SK_Balloon_PhysicsAsset (every click and hit body: lever_L, furnace_door, ammo_box and so on)
  - the AnimBP ABP_Balloon, the hidden bones (fuel pieces, stove bullet holes) and the `Mesh` component in BP_Balloon

  Its geometry is now only the basket, envelope, ropes and propeller (plus sandbags and lantern).
- **6 part meshes** use the **same skeleton**. Every FBX contains the full skeleton, but each mesh is skinned only to its own bones:

  | Asset | Contains | Material slots |
  |---|---|---|
  | SK_Balloon_Stove | stove, chimney pipe, thermometer, furnace + door, burn-time scale, 10 bullet holes | M_Balloon, M_Glow |
  | SK_Balloon_NavConsole | radar, speedometer, warning lamp | M_Balloon, M_Display, M_WarningLamp |
  | SK_Balloon_Stoppers | both wings (stoppers) with hinge brackets, both levers with quadrants | M_Balloon |
  | SK_Balloon_FuelStorage | wood crate, coal sack, crystal bucket and their 6+6+6 pieces | M_Balloon, M_Glow |
  | SK_Balloon_ToolStorage | tool board with extinguisher cup, hook brackets, wrench peg | M_Balloon |
  | SK_Balloon_WeaponWall | gun rack, revolver hook, ammo box with lid | M_Balloon |

- In BP_Balloon each part is a SkeletalMeshComponent attached to `Mesh` that **follows it with Leader Pose**. ABP_Balloon keeps animating the bones on `Mesh` (furnace door, needles, wings, levers, ammo lid), and the parts move with it.
- The parts have **no collision**. All traces still hit `Mesh`'s physics bodies and return the same bone names, so the C++ components need no changes:
  - BalloonWingLeverComponent, StoveComponent, StoveVfxComponent, AmmoSupplyComponent, HealthComponent
  - WeaponRackComponent (socket_small_weapon on `Mesh`)

## Safety rules (they override everything below)
1. **Never delete, rename or recreate** BP_Balloon, BP_BalloonPlayer, ABP_Balloon, BP_TestComp, SK_Balloon, SK_Balloon_Skeleton, SK_Balloon_PhysicsAsset, any material or material instance, or a level.
2. SK_Balloon is **reimported into the existing asset** (Reimport with the new file). Never delete it and import a new one.
3. Do **not** regenerate or edit SK_Balloon_PhysicsAsset. Do not create physics assets for the part meshes.
4. Don't change C++ code or Scripts/. Don't change existing Blueprint logic, except where step 6 tells you to redirect a material call. Report anything else that no longer fits, with file, Blueprint and node.
5. Before step 2, back up into /Game/Assets/Balloon/_Backup_Split: duplicate SK_Balloon, SK_Balloon_Skeleton, SK_Balloon_PhysicsAsset, BP_Balloon, BP_BalloonPlayer, ABP_Balloon and the 3 textures (EditorAssetLibrary.duplicate_asset). If that folder already exists, use _Backup_Split_2. Delete the backup only when I say so.
6. Work on my current git branch. Don't commit or push.

## Source files
D:\test_git\guns-and-balloons\Content\Assets\Balloon (metres, Z up, import at uniform scale 1.0):
- SK_Balloon.fbx, SK_Balloon_Stove.fbx, SK_Balloon_NavConsole.fbx, SK_Balloon_Stoppers.fbx, SK_Balloon_FuelStorage.fbx, SK_Balloon_ToolStorage.fbx, SK_Balloon_WeaponWall.fbx
- T_Balloon_BaseColor.png, T_Balloon_ORM.png, T_Balloon_Radar.png
- SM_Fuel_Wood.fbx, SM_Fuel_Coal.fbx, SM_Fuel_Crystal.fbx

**The texture atlas was re-baked with a new UV layout.** The textures, every SK_Balloon* mesh and the 3 SM_Fuel_* meshes must all be (re)imported in this same session. Old meshes with the new textures (or the reverse) look scrambled.

## 0. Look before touching anything
Report:
- **SK_Balloon:** bone count, Skeleton sockets, material slots and bounds.
- **BP_Balloon and BP_BalloonPlayer:** their component trees, and every node in their graphs that uses the `Mesh` component's materials. Search for:
  - Create Dynamic Material Instance, Set Material
  - Set Scalar/Vector Parameter Value on a material
  - Set Custom Primitive Data
  - Hide/Unhide Bone by Name
  - Set Visibility on Mesh
- **Other references:** every asset or level that references SK_Balloon (Reference Viewer). For example the main menu scene, BalloonFlight_Test, and any test levels.

Then make the backups (rule 5).

## 1. Textures
Reimport the 3 PNGs into the existing textures and keep their settings:
- All three: Filter Nearest, Pixels2D, no mipmaps, NeverStream.
- T_Balloon_BaseColor and T_Balloon_Radar: sRGB on.
- T_Balloon_ORM: sRGB off, TC_Masks.

Then reimport SM_Fuel_Wood, SM_Fuel_Coal and SM_Fuel_Crystal into their existing assets.

## 2. SK_Balloon: reimport in place
- Reimport into the existing asset. Settings:
  - import_materials false, import_textures false, import_animations false
  - scale 1.0, convert_scene true, force_front_x_axis false
  - Import Normals
  - keep the skeleton SK_Balloon_Skeleton
- Verify:
  - still 56 bones, Skeleton sockets unchanged, socket_small_weapon still there
  - bounds about 696 × 693 × 1184 cm
  - slots M_Balloon → MI_Balloon and M_Glow → MI_Glow

  M_Display and M_WarningLamp are gone from SK_Balloon; they now live on the NavConsole part. If a slot was cleared, reassign the existing MI.
- The physics asset must still have all its bodies (compare with the backup), including the ones on bones that now have no geometry on SK_Balloon (lever_L, furnace_door, ammo_box, ...).

## 3. Import the 6 part meshes
Into /Game/Assets/Balloon/Parts. For each SK_Balloon_<Part>.fbx:
- Import as Skeletal Mesh. **Skeleton = /Game/Assets/Balloon/SK_Balloon_Skeleton** (do not create a new skeleton), create_physics_asset false, import_materials false, import_animations false, scale 1.0, Import Normals.
- Assign the existing material instances by slot name: M_Balloon → MI_Balloon, M_Glow → MI_Glow, M_Display → MI_Display, M_WarningLamp → MI_WarningLamp.
- Verify:
  - no new skeleton or physics asset was created
  - the mesh uses SK_Balloon_Skeleton
  - SK_Balloon plus the 6 parts together are 12,852 triangles
  - each part sits in the right place when previewed (it carries the skeleton's reference pose)

## 4. BP_Balloon: add the part components
Open BP_Balloon (and BP_BalloonPlayer, if it has its own balloon mesh rather than inheriting `Mesh` from BP_Balloon). Keep every existing component. Add, only if missing, 6 SkeletalMeshComponents as children of `Mesh`:
- Names: `Stove`, `NavConsole`, `Stoppers`, `FuelStorage`, `ToolStorage`, `WeaponWall`.
- Settings for each:
  - Skeletal Mesh Asset = the matching part
  - relative transform zero
  - Collision Preset NoCollision, Generate Overlap Events off, Can Character Step Up On No
  - Cast Shadow on
  - **Use Bounds from Leader Pose Component** on
  - Anim Class none
- **Leader pose:** in the Construction Script (append, don't change existing nodes), call `Set Leader Pose Component` (Target = each part, New Leader Bone Component = `Mesh`). Do it in the Construction Script so it also works in the editor viewport and in levels before BeginPlay.

## 5. Check that hidden bones still hide on the parts
The game hides bones on `Mesh`:
- fuel pieces fuel_wood_N, fuel_coal_N and fuel_crystal_N when used
- stove bullet holes stove_damage_N until the balloon takes damage

With Leader Pose the followers use the leader's bone transforms, so a bone hidden on `Mesh` should disappear on the part too. **Test it:**
- In PIE, hide fuel_wood_1 and stove_damage_1 on `Mesh` (and unhide the bullet hole).
- Check that the log and the hole on the FuelStorage and Stove components follow.

If they don't follow, don't change the C++. Add a small BP function `SyncHiddenBones` on BP_Balloon that copies the hidden state of every fuel_* and stove_damage_* bone from `Mesh` to the Stove and FuelStorage components. Call it on a 0.1 s timer from BeginPlay, without changing what BeginPlay already does. Then tell me, so the owner of StoveVfxComponent and the fuel code can do it in C++ later.

## 6. Redirect material calls to the right part
Every graph node found in step 0 that changed materials on `Mesh` for a part that moved must now target that part's component. Keep the same logic and only change the target component:
- **Warning lamp:** `SetWarningLamp` (DMI on slot M_WarningLamp) → **NavConsole**.
- **Radar:** anything on M_Display → **NavConsole**. The radar itself is driven by MPC_Radar, which needs no change.
- **Stove and furnace glow** (M_Glow parameters for the firebox or coals) → **Stove**. Crystal or coal glow in the bucket → **FuelStorage**. If one node changes M_Glow for both, run it on both components.
- Anything on M_Balloon that should affect the whole balloon (tint, damage, flash) → run it on `Mesh` and on all 6 parts (a ForEach over the parts is fine).

List every node you changed: Blueprint, function, old target and new target.

## 7. Other places that use SK_Balloon
For every asset or level from step 0 (other than BP_Balloon and BP_BalloonPlayer) that shows SK_Balloon on its own, for example a static decoration in the main menu scene:
- If it is a placed BP_Balloon, nothing to do.
- If it is a bare SkeletalMeshActor with SK_Balloon, **report it**. It now shows only the basket, envelope and propeller. Suggest replacing it with BP_Balloon, but don't change the level unless I say so.

## 8. Test and report
- **BalloonFlight_Test plays:**
  - The balloon looks complete: stove, console, wings, levers, fuel buckets, tool board, weapon wall.
  - Nothing floats or is offset, and nothing is scrambled (textures match the UVs).
- **Interactions still work:**
  - pulling the levers moves the wings
  - the furnace door opens
  - the thermometer, speedometer and burn arrow move
  - the ammo lid opens on look-at and the ammo box gives ammo
  - fuel pieces disappear when taken
  - bullet holes appear when damaged
  - the revolver sits on its hook via socket_small_weapon
  - the warning lamp and radar work
- Run the existing automation tests for the balloon (Tests: Health, Wrench, PlayerArchitecture, the wing and stove tests) and report the results.
- A screenshot from inside the basket and one from outside.
- A list of everything you changed, added and skipped (and why).

## Who owns what after this
| Part asset | Edit in Blender | Then in Unreal |
|---|---|---|
| any SK_Balloon_<Part> | the part's code in Balloon_build.py (its objects sit under PART_<Part> in Balloon.blend), then run Balloon_export.py | reimport that part |
| SK_Balloon | objects under GRP_Basket / GRP_Envelope / GRP_Propeller | reimport SK_Balloon in place |

- All parts share one texture atlas. Any geometry change re-packs it, so after such a change also reimport the 3 textures and every SK_Balloon* and SM_Fuel_* mesh, in one go.
- The export is deterministic: unchanged geometry gives identical textures.
- Changing a material colour only re-bakes the textures; reimport the textures alone.
- New bones or sockets are added to SK_Balloon_Skeleton (through SK_Balloon), so all parts see them.
