You are connected to my Unreal Engine 5 editor (project "gunsandballoons"). **Update** my steampunk air balloon to the latest model, without breaking work my teammates already did on it. Work through the steps in order and verify each one. If a step fails, stop and tell me what failed and why. Where possible use the editor Python API (`unreal` module: AssetImportTask, AssetToolsHelpers, MaterialEditingLibrary, EditorAssetLibrary). If something can't be authored through your tools (for example AnimGraph or Blueprint graph nodes), give me exact click-by-click steps instead.

## Safety rules: read first, they override everything below
The project already has a working balloon made by my teammates. In /Game/Assets/Balloon that is:
- BP_Balloon, ABP_Balloon, BP_TestComp
- the materials M_Balloon, M_Glow, M_Display and their instances MI_Balloon, MI_Glow, MI_Display
- SK_Balloon, SK_Balloon_Skeleton, SK_Balloon_PhysicsAsset

C++ code in Source/gunsandballoons relies on it:
- BalloonFlightComponent, BalloonWingLeverComponent (uses bones `lever_L`, `stopper_L`)
- WeaponRackComponent (attaches the revolver to socket `socket_small_weapon`)
- BalloonCollisionSetupLibrary (adds the basket collision boxes to BP_Balloon)
- the level Content/Scenes/BalloonFlight_Test

Rules:
1. **Never delete, rename or recreate an existing asset.** Never replace BP_Balloon, ABP_Balloon, BP_TestComp, any material or material instance, or the physics asset.
2. Only these may be overwritten: the 3 textures, by re-importing them in place, and the mesh **SK_Balloon**, by **reimporting into the existing asset** (Reimport / ReimportWithNewFile). Do not delete it and import a new one: that would break every reference.
3. Only **add**: new assets (MPC_Radar, M_WarningLamp, MI_WarningLamp), new Skeleton sockets, and new variables, components or functions on BP_Balloon and ABP_Balloon. Before adding anything, check whether it already exists. If it does, leave it and tell me.
4. Don't change C++ code, Scripts/, or existing Blueprint graph logic. If something there no longer fits the new model, report it with the file and line. Don't fix it.
5. Before step 2, make a safety copy: duplicate SK_Balloon, SK_Balloon_Skeleton and SK_Balloon_PhysicsAsset into /Game/Assets/Balloon/_Backup (EditorAssetLibrary.duplicate_asset). Delete that folder only when I say so.
6. Work on my current git branch. Don't commit or push anything; I'll do that.

## Source files
On disk: D:\test_git\guns-and-balloons\Content\Assets\Balloon
- SK_Balloon.fbx: one skeletal mesh, 9.8k tris, 26 bones, 4 material slots (M_Balloon, M_Glow, M_Display, M_WarningLamp). Authored in metres, Z up.
- T_Balloon_BaseColor.png (1024, colour atlas), T_Balloon_ORM.png (1024: R = AO, G = roughness, B = metallic), T_Balloon_Radar.png (256, radar CRT background).

**What changed since the version in the project:**
- The basket is 25% narrower: 166 cm inside instead of 226 cm, same 109 cm wall height.
- New weapon wall: the revolver hook, a gun rack for a shotgun and a rifle, and an ammo box with a lid (bones ammo_box > ammo_lid). The old weapon sockets were renamed:

  | Old | New |
  |---|---|
  | socket_long_gun_1 | socket_shotgun |
  | socket_long_gun_2 | socket_rifle |
  | socket_small_weapon | socket_revolver |

- New warning lamp on the navigation console, with a 4th material slot M_WarningLamp and socket_warning_lamp.
- Radar screen with enemy blips: docs/Unreal_Prompt_3_RadarEnemies.md.

## 0. Look before touching anything
List what exists in /Game/Assets/Balloon. For SK_Balloon, report its current:
- bones and Skeleton sockets
- material slots
- bounds

For BP_Balloon, report its components and variables, and for ABP_Balloon its variables. Then make the backup copies (rule 5).

## 1. Textures
Re-import the 3 PNGs in place (same asset names, same paths). Make sure they keep, or get:
- all three: Filter Nearest, LODGroup TEXTUREGROUP_Pixels2D, MipGenSettings NoMipmaps, NeverStream true
- T_Balloon_BaseColor and T_Balloon_Radar: sRGB on, CompressionSettings TC_EditorIcon
- T_Balloon_ORM: sRGB off, TC_Masks

Save.

## 2. Skeletal mesh: reimport into the existing SK_Balloon
- Reimport SK_Balloon from D:\test_git\guns-and-balloons\Content\Assets\Balloon\SK_Balloon.fbx into the **existing** asset.
- Settings: import_materials false, import_textures false, import_animations false, uniform scale 1.0, convert_scene true, force_front_x_axis false, normal_import_method Import Normals.
- Keep using the existing SK_Balloon_Skeleton. The skeleton gets new bones (ammo_box, ammo_lid, renamed socket bones); let the reimport merge them into it.
- Do **not** regenerate the physics asset.

Verify, and report:
- Bounds about 696 × 693 × 1184 cm. If they are 100× off, undo and stop.
- Bones: root > basket. Basket's children: ropes, balloon, stove, fuel_bucket, propeller, stopper_L, stopper_R, lever_L, lever_R, display, ammo_box, plus non-deforming socket bones. Also stove > gauge > gauge_needle, stove > furnace_door, stove > burn_arrow, display > speed_needle, ammo_box > ammo_lid. `lever_L` and `stopper_L` must still exist (BalloonWingLeverComponent uses them).
- Material slots: the existing 3 keep their materials, and a 4th slot M_WarningLamp appears. If the reimport cleared any slot, re-assign the existing MI_Balloon / MI_Glow / MI_Display.
- Open BalloonFlight_Test and check that BP_Balloon still loads and shows the mesh.

## 3. Sockets: add, never remove
Add a Skeleton socket (zero offset) on each of these bones, unless a socket with that name already exists:

| Socket | Where |
|---|---|
| socket_revolver | on the revolver hook; the long axis = the barrel, pointing down |
| socket_shotgun, socket_rifle | the gun rack slots; head = butt, long axis up along the gun |
| socket_warning_lamp | at the lamp glass, long axis out of the console face |
| socket_tool_horizontal, socket_tool_vertical_1, socket_tool_vertical_2 | tool rack |

**Compatibility socket:**
- WeaponRackComponent attaches the revolver to `socket_small_weapon`. If that socket no longer exists after the reimport, add a Skeleton socket named **socket_small_weapon** on bone `socket_revolver`, with zero offset, so the existing code keeps working.
- If the old `socket_long_gun_1` / `socket_long_gun_2` are used anywhere in Blueprints, add them the same way on `socket_shotgun` / `socket_rifle`, and tell me where they are used.

## 4. Materials: add only what is missing
- **M_Balloon, M_Glow and their MIs:** leave them as they are. Only check that the textures still plug in.
- **M_WarningLamp + MI_WarningLamp** (new) for the glass of the warning lamp between the radar and the speedometer:
  - Opaque, Default Lit.
  - T_Balloon_BaseColor → Base Color, Sampler Source From texture asset (Nearest).
  - Vector parameter LampColor (default 1.0, 0.38, 0.03) × scalar LampIntensity (default 0 = off) → Emissive Color.
  - Roughness 0.2, Specular 0.5.
  - Assign MI_WarningLamp to slot M_WarningLamp.
- **M_Display:** if it does not have the radar Custom node yet, add it as described in docs/Unreal_Prompt_3_RadarEnemies.md, steps 1–2 (MPC_Radar + Custom node). Edit only the existing M_Display's graph; don't replace the asset. If it already has a radar setup, compare it with that prompt and tell me the differences before changing anything.

## 5. ABP_Balloon: add, don't replace
Open the existing ABP_Balloon. Add any of these float variables that are missing:
- StopperL, StopperR, LeverL, LeverR
- FurnaceDoor, PropellerSpeed
- Temperature01, Speed01, BurnTime, AmmoLidOpen

For every bone below that is **not already driven** in the AnimGraph, add a Transform (Modify) Bone node (Add to Existing, Bone Space) at the end of the existing chain. Bones my teammates already drive (for example lever_L / stopper_L via BalloonWingLeverComponent) must not be driven twice: skip those and tell me.

| Bone | Drive |
|---|---|
| furnace_door | yaw 0..100° × FurnaceDoor |
| propeller | roll, accumulated PropellerSpeed × DeltaTime |
| gauge_needle | −240° × Temperature01 around its own long axis |
| speed_needle | −180° × Speed01 around its own long axis |
| burn_arrow | translate up 2.2 cm × BurnTime |
| ammo_lid | 30° × AmmoLidOpen around its own long axis (the hinge) |
| stopper_R / lever_R | only if nothing drives them yet: same as the L side, mirrored |

Unreal mirrors Y on FBX import. Test each new node once in the preview and flip the sign where it moves the wrong way. Tell me the axis and sign you used.

## 6. BP_Balloon: add components and functions only
Keep every existing component, variable and graph. Add, only if missing:
- **WarningLight:** PointLight attached to socket_warning_lamp. Radius 80 cm, intensity 0, no shadows.
- **SetWarningLamp:** enum E_WarningLamp (Off, Orange, Red) and a function SetWarningLamp(State).
  - It creates a DMI for slot M_WarningLamp on first use.
  - Off: LampIntensity 0, light 0. Orange: (1.0, 0.38, 0.03), 8 and 30 cd. Red: (1.0, 0.04, 0.02), 10 and 40 cd.
  - Apply only on a state change.
- **Ammo box look-at:** a 0.05 s timer line-traces 2.5 m from the player camera. If it hits bone ammo_box or ammo_lid, AmmoLidOpen eases toward 1, otherwise toward 0 (FInterpTo speed 6). Pass it to ABP_Balloon. Use a new function, called from the existing BeginPlay without changing what BeginPlay already does.

## 7. Collision: report, don't rebuild
The basket collision is created by BalloonCollisionSetupLibrary (C++) and Scripts/setup_balloon_collision.py. It was sized for the old 226 cm basket: walls 116.5 cm from the centre, floor half-size 113 cm. The new basket's inner wall faces are **83 cm** from the centre, walls about 7 cm thick and 69 cm wide, 109 cm tall.
- Place BP_Balloon in BalloonFlight_Test and check whether the wall boxes now float outside the wicker.
- Report the mismatch: how far off each wall is, and the file and line numbers in BalloonCollisionSetupLibrary.cpp to change (around lines 81–85).
- **Don't edit the C++ or the boxes.** That's for the teammate who owns that code.
- Also report anything in WeaponRackComponent, BalloonFlightComponent or BP_Balloon that refers to a bone or socket that no longer exists.

## 8. Test and report
- BalloonFlight_Test still plays: the balloon flies, the levers work, the revolver attaches to its rack (via socket_small_weapon).
- The new parts show: the weapon wall, the ammo box lid opens on look-at, the warning lamp lights up through SetWarningLamp.
- A screenshot from inside the basket.
- A list of everything you changed, added and skipped (and why).
