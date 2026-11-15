You are connected to my Unreal Engine 5 editor (project "gunsandballoons"). Set up the map-edge enemy: a rocket balloon that guards the edge of every level and fires rockets at the player, with low poly effects. Work through the steps in order and verify each one. Where possible use the editor Python API (`unreal` module). If something can't be authored through your tools (Blueprint graphs, Niagara modules), create everything else and give me exact click-by-click steps for the missing part.

## Source files
On disk: D:\test_git\guns-and-balloons\Content\Assets\Map_edge. Import everything into /Game/Assets/Map_edge and replace existing assets with the same name. Everything is authored in metres with Z up: import at uniform scale 1.0 and Unreal gets centimetres. `Map_edge_Manifest.json` lists tris, size, materials, bones, sockets and physics bodies for every asset. Check your imports against it.

| File | What |
|---|---|
| SK_RocketBalloon.fbx | Enemy balloon, 12.4 m tall, 1.9k tris. It looks like the player's balloon but with red and cream striped gores, a red skirt and a red crown lamp. A slung **rocket launcher replaces the basket**: an octagonal deck with ammo drum and tank, and under it a red 6-tube rocket pod. A **bright red lamp** sits on a post at the front of the deck (glow slot). Bones: root, launcher (yaw, under the deck) > launcher_pitch (elevation, the pod), helper bones muzzle_01..06 (tube mouths, +Y = firing direction) and light (in front of the red lamp). Materials M_MapEdge_Balloon, M_MapEdge_Balloon_Glow |
| SM_Rocket.fbx | Rocket, 1.6 m: dark body, red band, red and brass nose, 4 brass fins. Pivot = centre, flies along its long axis (+Y in Blender). Socket exhaust at the nozzle. UCX box |
| SM_FX_RocketFlame.fbx | Exhaust flame: 2 nested low poly cones, 1.5 m. Pivot at the nozzle, points backwards. UV V = 0 at the nozzle, 1 at the tip. Material slot M_FX_Flame |
| SM_FX_MuzzleFlash.fbx | Launch flash: 3 crossed flame blades and a front star, 1.2 m. Pivot at the tube mouth, points forward. Slot M_FX_MuzzleFlash |
| SM_FX_Fireball.fbx | Explosion fireball, 1 m radius, lumpy icosphere (80 tris). UV V = height. Slot M_FX_Fireball |
| SM_FX_Shockwave.fbx | Flat ring, outer radius 1 m. UV V = 0 at the inner edge, 1 at the outer edge. Slot M_FX_Shockwave |
| SM_FX_Debris.fbx | Scorched metal chunk (32 tris), uses the rocket texture |
| T_MapEdge_Balloon.png, T_MapEdge_Rocket.png | Baked, pixel-dithered colour atlases |
| T_FX_Explosion.png | 256 px flipbook, 4 × 4 frames: white-hot flash → orange fire → red → dark smoke, with stepped alpha |
| T_FX_Smoke.png | 64 px smoke puff sprite with alpha (rocket smoke trail) |

## 1. Textures
- **T_MapEdge_Balloon, T_MapEdge_Rocket:** Filter Nearest, LODGroup World, mipmaps from texture group, sRGB on.
- **T_FX_Explosion, T_FX_Smoke:** Filter Nearest, no mipmaps, LODGroup Effects, compression Default (keep alpha).

## 2. Materials (in /Game/Assets/Map_edge/Materials)
1. **MI_MapEdge_Balloon**, **MI_MapEdge_Balloon_Glow**, **MI_MapEdge_Rocket**: instances of M_LocProp / M_LocGlow from the locations prompt (docs/Unreal_Prompt_4_Locations.md, step 2) with the matching atlas. If those parents don't exist yet, create them as described there.
   - On the balloon's Glow instance: EmissiveStrength 12, so the red lamp reads as a bright light from far away.
2. **M_FX_Flame** (Unlit, Additive, two-sided, no depth write):
   - Emissive = lerp((1.0, 0.95, 0.7) × 8, (1.0, 0.35, 0.05) × 4, TexCoord.V) × (1 − V)^1.2.
   - Flicker: × (0.8 + 0.2 × sin(Time × 40 + V × 12)).
   - Pan a stepped noise along V for the PS1 crackle: floor the UV to 8 steps.
3. **M_FX_MuzzleFlash** (Unlit, Additive, two-sided): emissive (1.0, 0.8, 0.35) × 10 × (1 − V), × scalar parameter `Fade` (1 → 0 over its 0.08 s life).
4. **M_FX_Fireball** (Unlit, Translucent, two-sided off):
   - Colour: the T_FX_Explosion flipbook sampled on a sphere mapping (a SubUV flipbook driven by scalar `Frame`), or simply lerp from (1, 0.9, 0.6) to (1, 0.35, 0.05) to (0.25, 0.22, 0.2) by scalar `Age` 0..1.
   - Opacity = (1 − Age) × a dithered alpha step.
5. **M_FX_Shockwave** (Unlit, Additive): emissive (1.0, 0.85, 0.6) × 3 × sin(π·V) × (1 − scalar `Age`).
6. **M_FX_Sprite** (Translucent, Unlit): texture parameter Sprite (Nearest) × Particle Color. Create MI_FX_Smoke (T_FX_Smoke) and MI_FX_ExplosionSprite (T_FX_Explosion, SubUV 4 × 4).

## 3. Import
- **SM_*:**
  - Import with auto_generate_collision false (UCX is imported), import_materials false, scale 1.0, Nanite off.
  - Assign the material instances by slot name (M_FX_Flame → M_FX_Flame and so on).
  - Collision presets: FX meshes and SM_FX_Debris get NoCollision. SM_Rocket gets a custom profile: Query Only, Overlap Pawn and WorldStatic, generate overlap events.
  - Verify SM_Rocket has socket exhaust.
- **SK_RocketBalloon:**
  - Import as skeletal mesh, no animations, create_physics_asset = true.
  - Add Skeleton sockets with the same names on the helper bones: `muzzle_01` … `muzzle_06` and `light`.
  - Physics asset: delete the generated bodies and build these. They are also in the manifest, centres in component space, cm. Blender +Y = Unreal −Y; mirror Y if the pod faces the wrong way.

    | Bone | Shape | Size (cm) | Centre |
    |---|---|---|---|
    | root | sphere | radius 360 | (0, 0, 840): envelope |
    | root | capsule | radius 240, length 600 | (0, 0, 620): lower envelope + skirt |
    | root | box | 290 × 340 × 90 | (0, −20, 245): deck + lamp |
    | launcher_pitch | box | 200 × 280 × 130 | (0, −15, 95): rocket pod |

  - All bodies Kinematic, Block All. The pod body moves with the launcher bones.

## 4. Effects (Niagara, in /Game/Assets/Map_edge/FX). Keep them cheap and low poly:
1. **NS_RocketTrail** (attached to the rocket's socket exhaust):
   - Mesh renderer: SM_FX_RocketFlame, one persistent particle, scale flicker 0.85–1.15 at 20 Hz.
   - Sprite emitter: MI_FX_Smoke.
     - Spawn 25 per 100 cm travelled (spawn per unit), lifetime 1.2–1.8 s.
     - Size 40 → 220 cm, colour (0.6, 0.58, 0.55) alpha 0.7 → 0.
     - A small random velocity, slight upward drift.
   - A warm PointLight on the rocket (radius 500 cm), via a light renderer or a component on the rocket.
2. **NS_MuzzleFlash** (one shot, at a muzzle socket):
   - SM_FX_MuzzleFlash mesh particle for 0.08 s, with random roll.
   - A puff of 6 smoke sprites pushed forward 300 cm/s.
   - A flash light for 0.1 s.
3. **NS_RocketExplosion** (one shot):
   - SM_FX_Fireball mesh: scale 0.3 → 4 over 0.4 s, Age 0 → 1.
   - SM_FX_Shockwave mesh: scale 0.5 → 12 over 0.35 s, facing the camera or flat to the impact normal.
   - 8 MI_FX_ExplosionSprite sprites: random 2–5 m size, SubUV frames played over 0.6 s.
   - 6–10 SM_FX_Debris mesh particles: random scale 0.5–1.5, speed 800–1600 cm/s outward and up, gravity, spin, lifetime 2 s.
   - Smoke sprites lingering 3 s.
   - Light flash: radius 2500 cm, 0.2 s.

## 5. Blueprints (in /Game/Assets/Map_edge/Blueprints)
1. **BP_Rocket** (Actor):
   - Components:
     - SM_Rocket (root)
     - Projectile Movement: speed 3500 cm/s, no gravity, homing on the player balloon with HomingAcceleration 1500 for the first 3 s
     - NS_RocketTrail on exhaust
   - Lifetime 8 s.
   - On overlap or hit with the player or world: spawn NS_RocketExplosion and apply radial damage (inner 300 cm full, outer 800 cm) to the player through `OnRocketHit(Location, Damage)` (Blueprint Interface). Then destroy.
   - It can be shot down: hit by a player projectile → explode harmlessly.
2. **BP_RocketBalloon** (Actor, Pawn-less AI):
   - Components:
     - SkeletalMesh SK_RocketBalloon
     - PointLight `RedLight` on socket `light`: colour (1.0, 0.05, 0.03), intensity 60 cd, radius 2500 cm, no shadows. Pulse it with a 1.2 s sine (0.6–1.0), together with the Glow MID's EmissiveStrength.
     - SpotLight on `light` pointing forward: outer cone 12°, attenuation 6000 cm, red, used as a searchlight
   - **Role: guards the map edge.** Spawned by the level generator along the edge, every `EdgeSpacingM` = 400. It bobs slowly (±1.5 m over 6 s) and drifts along the edge.
   - When the player balloon comes within `AggroRangeM` = 300 of the edge or of it:
     - yaw the `launcher` bone toward the player (AnimBP or Transform Modify Bone, 60°/s)
     - pitch `launcher_pitch` (−20° … +45°)
     - fire a salvo of 3 rockets, one from each next muzzle in turn (muzzle_01 → 06), 0.35 s apart, with NS_MuzzleFlash
     - reload for 4 s
   - The edge's message to the player: flying past the edge means getting shot at.
   - **Damage:**
     - `Health` 6 hits on the envelope body, ×2 on the rocket pod body (it explodes with NS_RocketExplosion, disables firing and the balloon sinks).
     - At 0 health: the envelope deflates by scaling down the root's envelope over 3 s, the balloon falls, and it explodes on impact.
   - **ABP_RocketBalloon:** float LauncherYaw and LauncherPitch.
     - Transform (Modify) Bone on `launcher`: yaw, Component Space, Add to Existing.
     - Transform (Modify) Bone on `launcher_pitch`: pitch, Bone Space.

## 6. Test
Create L_Test_MapEdge: a ground plane, one BP_RocketBalloon at 120 m height, a camera or the player balloon.
- The rocket balloon shows red and cream stripes, the rocket pod under the deck and a bright pulsing red lamp. Nothing is pink or grey.
- When approached it turns the launcher, fires 3-rocket salvos with muzzle flashes, and the rockets fly with flames and smoke trails, home in and explode with fireball, shockwave, sprites and debris.
- Shooting a rocket destroys it. Shooting the pod disables the launcher.
- Report the axis fixes you needed for the muzzle and exhaust directions, and a screenshot.
