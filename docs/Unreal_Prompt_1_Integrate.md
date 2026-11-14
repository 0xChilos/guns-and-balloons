You are connected to my Unreal Engine 5 editor (project "MyProject"). Integrate my steampunk air balloon asset. Work through the steps in order. After each step, verify the result. If a step fails, stop and tell me what failed and why. Where possible use the editor Python API (`unreal` module: AssetImportTask, AssetToolsHelpers, MaterialEditingLibrary, EditorAssetLibrary). If something can't be authored through your tools (for example AnimGraph or Blueprint graph nodes), create everything else and give me exact click-by-click steps for the missing part.

## Source files
On disk: D:\Docs\Unreal Projects\MyProject\Content\Assets\Balloon
- SK_Balloon.fbx: one skeletal mesh, 8.9k tris, 23 bones, 3 material slots (M_Balloon, M_Glow, M_Display). Authored in metres, Z up.
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
- 3 material slots: M_Balloon, M_Glow, M_Display.
- These bones, with this hierarchy:
  - root > basket.
  - Children of basket: ropes, balloon, stove, fuel_bucket, propeller, stopper_L, stopper_R, lever_L, lever_R, display, plus the 6 sockets listed in step 4.
  - stove > gauge > gauge_needle.
  - stove > furnace_door.
  - stove > burn_arrow.
  - display > speed_needle.

## 3. Materials
Create 3 materials and a Material Instance of each (MI_Balloon, MI_Glow, MI_Display). Assign the instances to SK_Balloon by slot name. In every material, sample the textures with Sampler Source = "From texture asset" (Nearest).

**M_Balloon** (Opaque, Default Lit)
- T_Balloon_BaseColor → Base Color.
- T_Balloon_ORM (Sampler Type Masks): R → Ambient Occlusion, G → Roughness, B → Metallic.
- Specular = 0.3.
- Optional PS1 vertex wobble: scalar parameter SnapCm (default 1.5, 0 = off). World Position Offset = `floor(P / SnapCm + 0.5) * SnapCm - P`, where P = Absolute World Position (Excluding Material Offsets). Wrap it in an If, or multiply by `step(0.001, SnapCm)`, so that 0 disables it.

**M_Glow** (Opaque, Default Lit). These faces are the furnace coals, the door vents and the lantern.
- T_Balloon_BaseColor → Base Color.
- Also → Emissive Color × scalar EmissiveStrength (default 6) × flicker `0.85 + 0.15 * sin(Time * 9) * sin(Time * 3.7)`.

**M_Display** (Unlit): the round radar screen. Set up its inputs:
- Texture Sample T_Balloon_Radar (texture parameter RadarTexture) with TexCoord 0.
- One Custom node, output CMOT Float3, with inputs:
  - UV (TexCoord 0)
  - Time
  - Base (RadarTexture RGB)
  - SweepSpeed (scalar, default 0.5)
  - Enemy1, Enemy2, Enemy3, Enemy4 (vector parameters, default (0, 0, 0, 0))
  - Brightness (scalar, default 1.6)
- Output → Emissive Color.

Custom node code:
```hlsl
float2 p = UV * 2.0 - 1.0; p.y = -p.y;                          // -1..1, +y = forward
float r = length(p);
float a = frac(atan2(p.x, p.y) / 6.2831853 + 1.0);               // 0..1 clockwise from forward
float sweep = frac(Time * SweepSpeed);
float behind = frac(sweep - a);
float beam = (r < 0.97) ? pow(saturate(1.0 - behind * 4.0), 3.0) : 0.0;   // beam + fading afterglow
float3 c = Base * (0.75 + 0.25 * beam) + float3(0.10, 0.45, 0.15) * beam;
float4 E[4] = {Enemy1, Enemy2, Enemy3, Enemy4};
for (int i = 0; i < 4; i++) {
    float ea = frac(atan2(E[i].x, E[i].y) / 6.2831853 + 1.0);
    float fade = saturate(1.0 - frac(sweep - ea) * 1.2);          // blip flashes when the beam passes
    c += float3(0.45, 1.0, 0.45) * E[i].z * fade * step(length(p - E[i].xy), 0.045);
}
c *= 0.92 + 0.08 * step(0.5, frac(UV.y * 64.0));                 // CRT scanlines
return c * Brightness;
```

Enemy parameters: xy = enemy position in radar space (-1..1, the outer ring = 0.9), z = 1 visible or 0 hidden. If the balloon icon shows upside down, remove `p.y = -p.y`.

## 4. Animated and interactive parts
Each part is rigidly skinned to one bone, and the bone origin is the pivot. Create ABP_Balloon (Anim Blueprint for SK_Balloon_Skeleton) with float variables:
- StopperL, StopperR (0..1)
- LeverL, LeverR (-1..1)
- FurnaceDoor (0..1)
- PropellerSpeed (deg/s)
- Temperature01 (0..1)
- Speed01 (0..1)
- BurnTime (0..10)

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

The gauge_needle and speed_needle bones point straight out of their dial faces, so rotate them around that axis. Unreal mirrors Y on FBX import, so test each bone once in the Skeleton editor and flip the sign or axis wherever it moves the wrong way. Tell me which axis and sign each bone ended up with.

Item attachment bones (non-deforming, each one's long axis = the item's length):
- Weapon rack: socket_long_gun_1, socket_long_gun_2, socket_small_weapon.
- Tool rack: socket_tool_horizontal, socket_tool_vertical_1, socket_tool_vertical_2.

Add Skeleton sockets with the same names on these bones.

## 5. BP_Balloon (Actor)
- **Root:** SkeletalMeshComponent "Mesh" with SK_Balloon, using ABP_Balloon.
- **Mesh collision:** Mesh collision = query only, so line traces return the bone name.
- **Interaction:** map hits on these bones to interactions: lever_L, lever_R, furnace_door, fuel_bucket (the bucket is empty; coal comes from gameplay), stove, display.
- **Walkable collision** (box components, Block All). The basket floor is at the actor origin. The basket is an octagon with an inner flat-to-flat width of 226 cm and walls 109 cm tall.
  - Floor box: top surface at Z = 7 cm.
  - 8 wall boxes, about 7 cm thick, rotated in 45 deg steps, each with its inner face 113 cm from the centre and 94 cm wide.
  - Stove box: about 70 × 70 × 125 cm, at the "stove" bone.
  - Navigation console box: about 66 cm wide, sticking out about 25 cm from the front wall, spanning Z 50–97 cm, centred on the "display" bone.
- **Lights:**
  - Warm orange PointLight about 15 cm in front of the furnace_door bone (radius 300, flicker the intensity in the Event Graph).
  - Small warm PointLight at the lantern, which hangs outside the rim at front-left, about 152 cm from the centre and 98 cm high. Find it by its glowing faces.

## 6. Test
Place BP_Balloon in the current level next to the default Mannequin (or any 180 cm character). Then report:
- The measured rim height and inner width.
- Whether the character fits inside.
- A screenshot from inside the basket, if you can take one.
