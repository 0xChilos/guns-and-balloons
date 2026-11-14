You are connected to my Unreal Engine 5 editor (project "gunsandballoons"). Integrate my steampunk air balloon asset. Work through the steps in order. After each step, verify the result. If a step fails, stop and tell me what failed and why. Where possible use the editor Python API (`unreal` module: AssetImportTask, AssetToolsHelpers, MaterialEditingLibrary, EditorAssetLibrary). If something can't be authored through your tools (for example AnimGraph or Blueprint graph nodes), create everything else and give me exact click-by-click steps for the missing part.

## Source files
On disk: D:\test_git\guns-and-balloons\Content\Assets\Balloon
- SK_Balloon.fbx: one skeletal mesh, 9.8k tris, 26 bones, 4 material slots (M_Balloon, M_Glow, M_Display, M_WarningLamp). Authored in metres, Z up.
- T_Balloon_BaseColor.png (1024, colour atlas, already dithered and grimy)
- T_Balloon_ORM.png (1024, R = ambient occlusion, G = roughness, B = metallic)
- T_Balloon_Radar.png (256, radar CRT background: balloon icon in the centre, 3 range rings)

Put every created asset in /Game/Assets/Balloon. Replace existing assets with the same name.

## 1. Textures
Import the 3 PNGs (automated, replace existing). Then set on all three:
- Filter = Nearest, LODGroup = TEXTUREGROUP_Pixels2D, MipGenSettings = NoMipmaps, NeverStream = true.

Then per texture:
- T_Balloon_BaseColor and T_Balloon_Radar: sRGB on, CompressionSettings = TC_EditorIcon (UserInterface2D).
- T_Balloon_ORM: sRGB off, CompressionSettings = TC_Masks.

Save them.

## 2. Skeletal mesh
Import SK_Balloon.fbx as a skeletal mesh with these settings:
- import_mesh = true, import_as_skeletal = true
- import_materials = false, import_textures = false, import_animations = false
- create_physics_asset = true
- import_uniform_scale = 1.0, convert_scene = true, force_front_x_axis = false
- normal_import_method = Import Normals

This should create SK_Balloon, SK_Balloon_Skeleton and SK_Balloon_PhysicsAsset.

Verify, and report what you find:
- Bounds about 696 x 693 x 1184 cm. If it is 100x off, re-import with the scale fixed.
- 4 material slots: M_Balloon, M_Glow, M_Display, M_WarningLamp.
- These bones, with this hierarchy:
  - root > basket.
  - Children of basket: ropes, balloon, stove, fuel_bucket, propeller, stopper_L, stopper_R, lever_L, lever_R, display, ammo_box, plus the 7 sockets listed in step 4.
  - stove > gauge > gauge_needle.
  - stove > furnace_door.
  - stove > burn_arrow.
  - display > speed_needle.
  - ammo_box > ammo_lid.

## 3. Materials
Create 4 materials and a Material Instance of each (MI_Balloon, MI_Glow, MI_Display, MI_WarningLamp). Assign the instances to SK_Balloon by slot name. In every material, sample the textures with Sampler Source = "From texture asset" (Nearest).

**M_Balloon** (Opaque, Default Lit)
- T_Balloon_BaseColor → Base Color.
- T_Balloon_ORM (Sampler Type Masks): R → Ambient Occlusion, G → Roughness, B → Metallic.
- Specular = 0.3.
- Optional PS1 vertex wobble: scalar parameter SnapCm (default 1.5, 0 = off). World Position Offset = `floor(P / SnapCm + 0.5) * SnapCm - P`, where P = Absolute World Position (Excluding Material Offsets). Wrap it in an If, or multiply by `step(0.001, SnapCm)`, so that 0 disables it.

**M_Glow** (Opaque, Default Lit). These faces are the furnace coals, the door vents and the lantern.
- T_Balloon_BaseColor → Base Color.
- Also → Emissive Color × scalar EmissiveStrength (default 6) × flicker `0.85 + 0.15 * sin(Time * 9) * sin(Time * 3.7)`.

**M_Display** (Unlit): the round radar screen, which shows enemies as green blips (small dot = rocket/mine, larger dot = plane, oval = zeppelin). Build it exactly as described in docs/Unreal_Prompt_3_RadarEnemies.md, steps 1 and 2 (MPC_Radar + the radar Custom node). Steps 3 to 5 of that prompt (feeding enemies from BP_Balloon) can be done later.

**M_WarningLamp** (Opaque, Default Lit): the glass of the warning lamp at the top centre of the navigation console, between the radar and the speedometer. It has 3 states: off, glowing orange, glowing red.
- T_Balloon_BaseColor → Base Color (the unlit glass).
- Vector parameter LampColor (default 1.0, 0.38, 0.03 = orange) × scalar LampIntensity (default 0 = off) → Emissive Color.
- Roughness 0.2, Specular 0.5.
- Gameplay switches the state through a Dynamic Material Instance (see BP_Balloon below and docs/Unreal_Prompt_3_RadarEnemies.md, step 6).

## 4. Animated and interactive parts
Each part is rigidly skinned to one bone, and the bone origin is the pivot. Create ABP_Balloon (Anim Blueprint for SK_Balloon_Skeleton) with float variables:
- StopperL, StopperR (0..1)
- LeverL, LeverR (-1..1)
- FurnaceDoor (0..1)
- PropellerSpeed (deg/s)
- Temperature01 (0..1)
- Speed01 (0..1)
- BurnTime (0..10)
- AmmoLidOpen (0..1)

AnimGraph: one Transform (Modify) Bone node per bone, Add to Existing, Bone Space:
| Bone | Drive | Notes |
|---|---|---|
| stopper_L / stopper_R | yaw (around the vertical axis) 0..90 deg × StopperL/R | 0 = folded flat against the basket, opens outward; the two are mirrored |
| lever_L / lever_R | pitch ±50 deg × LeverL/R | lever position should drive its stopper (StopperL = LeverL remapped to 0..1) |
| furnace_door | yaw 0..100 deg × FurnaceDoor | swings open into the basket |
| propeller | roll, accumulated PropellerSpeed × DeltaTime | spins around the flight axis |
| gauge_needle | -240 deg × Temperature01 around the bone's own long axis | thermometer on the stove lid; rest = 0 (lower-left); scale 60% green, 30% orange, 10% red |
| speed_needle | -180 deg × Speed01 around the bone's own long axis | speedometer; rest = 0 (pointing left), full = pointing right |
| burn_arrow | translate up 2.2 cm × BurnTime | burn-time scale on the stove; rest = 0 (bottom line), 22 cm = 10 |
| ammo_lid | 30 deg × AmmoLidOpen around the bone's own long axis (the hinge) | ammo box on the weapon wall; rest = closed, the lid lifts at the front edge. In Blender -30 deg around bone Y opens it |

The gauge_needle and speed_needle bones point straight out of their dial faces, so rotate them around that axis. The ammo_lid bone lies along the lid's hinge. Unreal mirrors Y on FBX import, so test each bone once in the Skeleton editor and flip the sign or axis wherever it moves the wrong way. Tell me which axis and sign each bone ended up with.

Item attachment bones (non-deforming, each one's long axis = the item's length):
- Weapon wall (the wall to the right of the navigation console, seen from inside). Everything is on this one wall, from left to right:
  - socket_revolver: on the revolver hook. The revolver hangs by its trigger guard; the bone's long axis = barrel, pointing down.
  - socket_shotgun, socket_rifle: the two places in the upright gun rack. The bone head = where the butt rests in its slot in the base; the long axis runs up along the gun, leaning slightly back into the notch of the top shelf. Attach each gun with its butt at the socket and its barrel along the axis.
  - The ammo box (bones ammo_box and ammo_lid) is on the right.
- Warning lamp: socket_warning_lamp (at the lamp glass, long axis = pointing out of the console face). Attach the lamp's point light here.
- Tool rack: socket_tool_horizontal, socket_tool_vertical_1, socket_tool_vertical_2.

Add Skeleton sockets with the same names on these bones.

## 5. BP_Balloon (Actor)
- **Root:** SkeletalMeshComponent "Mesh" with SK_Balloon, using ABP_Balloon.
- **Mesh collision:** Mesh collision = query only, so line traces return the bone name.
- **Interaction:** map hits on these bones to interactions: lever_L, lever_R, furnace_door, fuel_bucket (the bucket is empty; coal comes from gameplay), stove, display, ammo_box, ammo_lid.
- **Warning lamp:** on BeginPlay, create a Dynamic Material Instance for the slot M_WarningLamp and store it. Add a function SetWarningLamp(State: E_WarningLamp). E_WarningLamp has 3 entries: Off, Orange, Red.
  | State | LampColor | LampIntensity | WarningLight |
  |---|---|---|---|
  | Off | (unchanged) | 0 | intensity 0 |
  | Orange | (1.0, 0.38, 0.03) | 8 | same colour, 30 cd |
  | Red | (1.0, 0.04, 0.02) | 10 | same colour, 40 cd |
  Only apply it when the state changes. What sets the state (enemies within a radius) is in docs/Unreal_Prompt_3_RadarEnemies.md, step 6.
- **Ammo box look-at:** every tick (or on a 0.05 s timer), line-trace from the player camera, about 2.5 m. If it hits bone ammo_box or ammo_lid, ease AmmoLidOpen toward 1, otherwise back toward 0 (FInterpTo, speed about 6). This lifts the lid by 30 deg while the player looks at the box. Pass the value to ABP_Balloon.
- **Walkable collision** (box components, Block All). The basket floor is at the actor origin. The basket is an octagon with an inner flat-to-flat width of 166 cm and walls 109 cm tall.
  - Floor box: top surface at Z = 7 cm.
  - 8 wall boxes, about 7 cm thick, rotated in 45 deg steps, each with its inner face 83 cm from the centre and 69 cm wide.
  - Stove box: about 70 × 70 × 125 cm, at the "stove" bone.
  - Navigation console box: about 66 cm wide, sticking out about 25 cm from the front wall, spanning Z 50–97 cm, centred on the "display" bone.
  - Gun rack box: about 29 × 28 × 91 cm (along the wall × out from the wall × tall), standing on the floor against the weapon wall, centred 3.5 cm right of that wall's middle. Put it between the sockets socket_shotgun and socket_rifle.
- **Lights:**
  - Warm orange PointLight about 15 cm in front of the furnace_door bone (radius 300, flicker the intensity in the Event Graph).
  - Warning lamp PointLight "WarningLight" attached to socket_warning_lamp: radius 80 cm, intensity 0 (off), no shadows. Its colour and intensity follow the lamp state.
  - Small warm PointLight at the lantern, which hangs outside the rim at front-left, about 120 cm from the centre and 98 cm high. Find it by its glowing faces.

## 6. Test
Place BP_Balloon in the current level next to the default Mannequin (or any 180 cm character). Then report:
- The measured rim height and inner width.
- Whether the character fits inside.
- A screenshot from inside the basket, if you can take one.
