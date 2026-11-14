You are connected to my Unreal Engine 5 editor (project "gunsandballoons"). Set up the 4 game locations: Forest, Field, City and Desert. Each location is its own level, generated randomly from the assets below. Tall structures carry loot spots (sockets) where glittering pickups snap on: ammo, money, logs, coal and crystals. Work through the steps in order and verify each one. Where possible use the editor Python API (`unreal` module: AssetImportTask, AssetToolsHelpers, MaterialEditingLibrary, EditorAssetLibrary, DataTableFunctionLibrary). If something can't be authored through your tools (for example Blueprint graph nodes, AnimGraph, Niagara modules), create everything else and give me exact click-by-click steps for the missing part.

## Source files
On disk: D:\test_git\guns-and-balloons\Content\Assets\Locations\<Level>\ (Forest, Field, City, Desert, plus Loot for the pickups). Every asset goes into /Game/Assets/Locations/<Level>. Replace existing assets with the same name.

Each level folder contains:
- `SM_*.fbx`: static meshes. Some contain `UCX_<Mesh>_NN` convex collision hulls and `SOCKET_<name>` empties.
- `SK_*.fbx`: skeletal meshes. The bird includes its animations.
- `T_<Group>.png`: baked colour atlases, already pixel-dithered. Each mesh uses exactly one atlas, shared within its group.
- `T_<Level>_Ground.png`: tileable 256 px ground texture.
- `<Level>_Placement.csv`: placement rules for the random level generator.
- `<Level>_Manifest.json`: tris, size in metres, texture, material slots, sockets, collision hulls and notes per asset. Read it and use it to check your imports.

City also contains T_Smoke_Puff.png (64 px smoke sprite with alpha).

**Loot spots:**
- Every tall structure has 1–3 loot spots: balconies on tall buildings, chimney platforms, belfry ledges, crates on desert towers and on the mesa, bird nests on tall pines, the windmill's rooftop lookout and gallery.
- Static meshes have sockets `loot_01`, `loot_02` … (FBX empties `SOCKET_loot_NN`). The two skeletal meshes, SK_Windmill and SK_Lighthouse, carry helper bones `loot_NN`, because FBX can't store skeleton sockets. In step 4, add a Skeleton socket with the same name on each of these bones, so gameplay only ever deals with sockets.
- A pickup's pivot is its base centre: snap it onto the socket or bone with SnapToTarget and it sits on the surface. The socket's +X points out, away from the building.
- The `loot_spots` list in each `<Level>_Manifest.json` names them.

Everything is authored in metres with Z up, the same as the balloon: import at uniform scale 1.0 and Unreal gets centimetres. Every mesh pivot is at its base centre, on the ground.

### What is in each level
**Forest**: coniferous forest.

| Asset | What | Collision |
|---|---|---|
| SM_Pine_Small_A/B/C | Short pines 4.5–8 m that fill the map. The player never touches them | none |
| SM_Pine_Tall_A/B | Tall pines 28 / 34 m that the balloon bumps into. Own sharper texture (T_Forest_Tall). Bird nest on the broken top = loot_01. B also has a nest on a long branch = loot_02 | UCX: trunk + canopy cone |
| SM_Rock_A/B, SM_Stump, SM_FallenLog | Ground clutter | none |
| SM_LogCabin | Woodcutter's cabin (one lit window, glow slot) | none |
| SM_RangerTower | 13 m fire lookout with a railed walkway, loot_01/02 on the walkway (T_Forest_Tall) | UCX ×2 |
| SM_Mountain_A/B/C | Ultra low poly snow-capped mountains, 650–1450 m wide, 230–520 m high, 40–80 tris. Background only | none |

**Field**: plain farmland with flocks of birds.

| Asset | What | Collision |
|---|---|---|
| SK_Bird | 1.1 m wingspan bird, bones root/body/wing_L/wingtip_L/wing_R/wingtip_R/tail; animations Bird_Flap (13 frames, loop), Bird_Glide | physics asset sphere (step 4) |
| SM_HayBale_Round, SM_HayStack, SM_HayCart, SM_Scarecrow | Farm clutter | none |
| SM_Fence (4 m section), SM_Hedge (6 m) | Chained into lines along field borders | none |
| SM_Farmhouse, SM_Barn, SM_Tree_Oak_A/B | Buildings and lone trees | none |
| SK_Windmill | 31 m tower windmill, a tall landmark (own texture T_Field_Windmill). Skeletal mesh: bones root, blades (the 15 m sails, spinning around the hub axis) and helper bones loot_01 (railed lookout on the roof) and loot_02 (gallery at 9 m). Animation **Windmill_Spin**: one full turn in 4 s, seamless loop. It spins constantly | physics asset with a box per sail, so the balloon bumps into the blades (step 4) |

**City**: Victorian London.

| Asset | What | Collision |
|---|---|---|
| SM_House_Terrace_A/B/C, SM_Shop_Corner, SM_Warehouse, SM_Gasholder, SM_Lamppost | Short buildings 10–19 m (lit shop windows and lamps use the glow slot) | none |
| SM_Church | 41 m spire. Balcony on the tower front at 16 m = loot_01 | UCX ×3 |
| SM_Factory_A | Sawtooth hall, 2 chimneys (32 / 28 m), glowing skylights. Smoke from SOCKET_smoke_1 and SOCKET_smoke_2 (the chimney tops). A railed service platform on each chimney = loot_01/02 | UCX ×5 |
| SM_Factory_B | Brick block, 40 m chimney, water tank, glowing furnace. Smoke from SOCKET_smoke_1. loot_01 on the chimney deck at 30 m, loot_02 on the roof | UCX ×3 |
| SM_ClockTower | 64 m clock tower, glowing clock faces. Own texture. Balconies at 24 m and 36 m and the railed belfry ledge at 49 m = loot_01..03 | UCX ×2 |
| SM_Tower_Office_A / _B | 51 m / 61 m tall buildings. Own textures. A: 3 balconies (18, 32, 40 m). B: 2 spots on the railed roof terrace at 22 m and a balcony at 36 m. loot_01..03 each | UCX ×2 each |
| SM_JammerTower | 80 m giant antenna: lattice mast, copper coils, red warning lights (glow slot). Sockets beacon_1..3 (lights), jam (effect origin), loot_01/02 on the two platforms, **dish** (top of the mast). Own texture | UCX ×2 |
| SM_JammerDish | Rotating satellite dish that sits on SOCKET_dish: turntable, yoke, 7 m copper bowl tilted up, feed struts, glowing violet emitter (glow slot) and counterweight. Pivot = turntable base, rotates around Z. Socket emitter. **Shooting it down stops the jammer** | UCX ×2 (own collision, separate from the tower) |

**Desert**: sand, dunes, guard towers and lighthouses.

| Asset | What | Collision |
|---|---|---|
| SM_Dune_A/B/C | Crescent dunes 70–200 m long, 8–23 m high. All face the same wind: downwind = +Y in Blender (see step 5 for the axis after import) | none (the balloon flies over them) |
| SM_Mesa (34 m), SM_Hoodoo (20 m) | Rock formations. Smugglers' crates on the mesa top = loot_01..03. A crate on a plank platform on the hoodoo cap = loot_01 | UCX ×1 each |
| SM_Rock_Desert_A/B, SM_Cactus_A/B, SM_Tent, SM_Wreck_Airship, SM_Ribcage | Clutter: rocks, cacti, nomad tent, crashed zeppelin, giant skeleton | none |
| SM_TurretTower | 20 m iron stilt tower with a railed gun platform. Sockets head (turret ring) and engine (engine bed), loot_01/02 on crates. Own texture T_Desert_Turret | UCX ×2: legs and deck only |
| SM_TurretTower_Head | Armoured turret head. Sits on SOCKET_head and rotates around Z. Socket guns (elevation pivot) | UCX ×2, own collision |
| SM_TurretTower_Guns | Twin cannons with muzzle brakes. Sits on the head's SOCKET_guns and elevates around X. Sockets muzzle_1, muzzle_2 | UCX ×1, own collision |
| SM_TurretTower_Engine | Copper steam boiler with firebox (glowing door), chimney, flywheel, pistons and a red safety valve. Sits on SOCKET_engine. Smoke from SOCKET_smoke. **The turret's weak spot** | UCX ×3, own collision |
| SK_Lighthouse | 38 m lighthouse. Bones: root > head (rotates) > lamp (light source, can be destroyed); non-deforming socket_beam (on head, at the lamp, pointing along the beam); non-deforming loot_01/02 (on root, on the crate lids on the gallery, so they don't turn with the head). Own texture, glow slot = lens | physics asset (step 4) |
| SM_LighthouseBeam | 90 m light cone, apex at its pivot. Attach it to socket_beam | none |

**Loot** (folder Loot): the pickups that snap onto loot spots.

| Asset | What | Collision |
|---|---|---|
| SM_Loot_AmmoBox | Open wooden case with brass corners, heaped with a mixed pile of red shotgun shells, short pistol rounds and long pointed rifle rounds, a few spilled beside it. 0.85 × 0.6 × 0.75 m | UCX box (for grab traces) |
| SM_Loot_MoneyPouch | Leather drawstring pouch with gold coins spilling out, 0.75 × 0.75 × 0.7 m | UCX box |
| SM_Loot_Log | One thick log with a branch stub and a patch of moss, 1.0 × 0.4 × 0.4 m | UCX box |
| SM_Loot_Coal | Big glossy black lump of coal with two smaller pieces. Nothing orange in the model: the orange glow and sparks come from its Unreal effect (BP_Loot_Coal). 0.8 × 0.7 × 0.6 m | UCX box |
| SM_Loot_Crystal | Cluster of 5 glowing cyan crystals on a rock (glow slot), 0.75 × 0.75 × 0.9 m | UCX box |

The folder also has **T_Loot_Spark.png**: a 32 px white 4-point star sprite with stepped alpha, for the pickup sparkle. The items are deliberately a bit oversized so they read from the balloon.

## 1. Textures
Import every PNG (automated, replace existing). Then:
- **All `T_<Group>` atlases and `T_<Level>_Ground`:**
  - Filter = Nearest
  - LODGroup = TEXTUREGROUP_World
  - MipGenSettings = FromTextureGroup (mipmaps ON: this is what stops distant objects and the ground from shimmering)
  - sRGB on, CompressionSettings = Default
- **T_Smoke_Puff** and **T_Loot_Spark:**
  - Filter = Nearest
  - CompressionSettings = Default (with alpha)
  - MipGenSettings = NoMipmaps
  - LODGroup = TEXTUREGROUP_Effects

Save them.

## 2. Materials (in /Game/Assets/Locations/Materials)
1. **M_LocProp** (Opaque, Default Lit):
   - Texture parameter `Atlas` → Base Color, sampler From texture asset.
   - Scalar `Roughness` (0.85), Specular 0.2.
   - Optional PS1 vertex wobble, same as the balloon's M_Balloon: scalar SnapCm, default 0 = off for locations.
2. **M_LocGlow**: copy of M_LocProp plus Emissive Color = Atlas × scalar `EmissiveStrength` (default 4) × scalar `Flicker` (default 1; gameplay can pulse it).
3. **M_LightBeam** (Translucent, Unlit, Two Sided, Blend Mode Additive):
   - Emissive = vector `BeamColor` (1.0, 0.95, 0.80) × scalar `BeamIntensity` (2.0) × `pow(1 - TexCoord.V, 1.5)` (fades along the cone; V = 0 at the lamp, 1 at the far end) × `saturate(DepthFade(300))`.
   - Also multiply by a soft edge: `pow(abs(dot(PixelNormalWS, CameraVector)), 1.5)`.
   - Disable depth write and shadows.
4. **M_Ground** (Opaque, Default Lit), so the ground never flickers at a distance:
   - UV = AbsoluteWorldPosition.xy / scalar `TileCm` (default 800).
   - Base Color = lerp(Texture parameter `Ground` sampled at UV, vector `FarColor`, saturate((PixelDepth − `FadeStartCm`) / (`FadeEndCm` − `FadeStartCm`))). Defaults: FadeStartCm 4000, FadeEndCm 20000.
   - Roughness 1, Specular 0.1.
5. **M_LocPickup** (Opaque, Default Lit). Pickups pulse and glint so the player knows they can be grabbed:
   - Same as M_LocProp (Atlas → Base Color).
   - Emissive = vector `HighlightColor` × scalar `HighlightStrength` (default 3) × pulse `0.5 + 0.5 * sin(Time * 4)` × Fresnel(exponent 2.5), so only the rim glows.
   - Plus Atlas × scalar `SelfGlow` (default 0; 4 for the crystal's glow-slot faces).
   - Create `MI_Loot` (parent M_LocPickup, Atlas = T_Loot) and `MI_Loot_Glow` (parent M_LocPickup, Atlas = T_Loot, SelfGlow = 4). Each BP_Loot child sets its own HighlightColor through a Dynamic Material Instance (step 5.7).
6. **M_LootSpark** (Translucent, Unlit, Additive): T_Loot_Spark (Nearest) × Particle Color, two-sided.
7. **Material instances:**
   - For every texture group, create `MI_<Group>` (parent M_LocProp, Atlas = T_<Group>) and `MI_<Group>_Glow` (parent M_LocGlow, same atlas). The groups are Forest_Props, Forest_Tall, Forest_Mountains, Field_Props, Field_Bird, Field_Windmill, City_Props, City_ClockTower, City_TowerA, City_TowerB, City_Jammer, Desert_Props, Desert_Turret and Desert_Lighthouse.
   - `MI_LightBeam` (parent M_LightBeam).
   - `MI_<Level>_Ground` (parent M_Ground) with Ground = T_<Level>_Ground and FarColor (linear) = the texture's average:

     | Level | FarColor (linear RGB) |
     |---|---|
     | Forest | (0.181, 0.174, 0.095) |
     | Field | (0.387, 0.410, 0.176) |
     | City | (0.244, 0.234, 0.219) |
     | Desert | (0.729, 0.558, 0.349) |

## 3. Static meshes
Import every `SM_*.fbx` from the 5 folders (including Loot) with these settings:
- import_mesh = true, combine_meshes = true
- auto_generate_collision = **false** (UCX hulls are imported instead)
- import_materials = false, import_textures = false
- import_uniform_scale = 1.0, convert_scene = true, force_front_x_axis = false
- normal_import_method = Import Normals, generate_lightmap_u_vs = false
- Nanite off

Then per mesh:
- **Materials:** the FBX slot names are `M_<Group>`, `M_<Group>_Glow` and `M_LightBeam`. Assign `MI_<Group>`, `MI_<Group>_Glow` and `MI_LightBeam` by slot name. Loot meshes get MI_Loot / MI_Loot_Glow.
- **Collision:**
  - Meshes with UCX hulls (Collision column above): collision preset BlockAll, Collision Complexity = Use Simple Collision As Complex.
  - Loot meshes: keep their UCX box. Collision preset = custom: Query Only, block the Visibility trace (so a grab trace or crosshair hits them), ignore Pawn (the balloon flies through them).
  - All other meshes: collision preset NoCollision, and remove any simple collision. Short trees, clutter and short buildings never touch the player.
- **Sockets:** check against the manifest that these exist:
  - SM_Factory_A: smoke_1, smoke_2
  - SM_Factory_B: smoke_1
  - SM_JammerTower: beacon_1, beacon_2, beacon_3, jam, dish
  - SM_JammerDish: emitter
  - SM_TurretTower: head, engine. SM_TurretTower_Head: guns. SM_TurretTower_Guns: muzzle_1, muzzle_2. SM_TurretTower_Engine: smoke
  - Every `loot_NN` listed under `loot_spots` in the manifests

  If one is missing, add it by hand at the location in `<Level>_Manifest.json`. The FBX socket empties are `SOCKET_<name>`, so their positions can also be read from the FBX.
- **Distance culling:** set `Min/Max Draw Distance` hints in step 6 rather than LODs. The meshes are already 24–2100 tris.

Report the triangle count and bounds of every mesh against `<Level>_Manifest.json` (size_m × 100 = cm). If a mesh is 100× off, re-import it with the scale fixed.

## 4. Skeletal meshes
- **SK_Bird.fbx:**
  - Import as skeletal mesh with import_animations = true and create_physics_asset = true.
  - Expect SK_Bird, SK_Bird_Skeleton, SK_Bird_PhysicsAsset and the animation sequences for Bird_Flap and Bird_Glide. Rename them to `A_Bird_Flap` and `A_Bird_Glide`. Set A_Bird_Flap to loop.
  - Physics asset: one sphere, radius 35 cm, on bone body. Delete the other bodies.
  - Slot M_Field_Bird → MI_Field_Bird.
- **SK_Windmill.fbx:**
  - Import as skeletal mesh with import_animations = true and create_physics_asset = true.
  - Expect SK_Windmill, SK_Windmill_Skeleton, SK_Windmill_PhysicsAsset and the sequence for Windmill_Spin. Rename it to `A_Windmill_Spin`, set it to loop, and make sure it is linear: no interpolation easing at the loop point, so the sails never hitch.
  - **Physics asset**: the balloon must be able to bump into the tower *and* the spinning blades.
    - Delete the generated bodies and build exactly these. They are also listed in `Field_Manifest.json` → SK_Windmill → `physics_bodies`.
    - Centres are in component space at the reference pose, in cm. Blender +Y became Unreal −Y, as with the balloon: the sails are on the −Y side of the tower. If they show up on +Y after import, mirror the Y values.
    - The Physics Asset editor can place bodies in world space. Relative to the `blades` bone, a body's offset = centre − (0, −460, 2740).

      | Bone | Shape | Size (cm) | Centre (X, Y, Z) | Rotation |
      |---|---|---|---|---|
      | root | capsule (along Z) | radius 430, total length 2720 | (0, 0, 1360) | – |
      | root | box | 720 × 800 × 460 | (0, −40, 2830) | – (cap and roof lookout) |
      | root | box | 1120 × 1120 × 130 | (0, 0, 940) | – (gallery) |
      | root | box | 1000 × 1000 × 200 | (0, 0, 100) | – (plinth) |
      | blades | sphere | radius 80 | (0, −460, 2740) | – (hub) |
      | blades | box | X 1460 × Y 40 × Z 280 | (445.5, −460, 3383.5) | Pitch +45° (sail up-right) |
      | blades | box | X 1460 × Y 40 × Z 280 | (−643.5, −460, 3185.5) | Pitch −45° (sail up-left) |
      | blades | box | X 1460 × Y 40 × Z 280 | (−445.5, −460, 2096.5) | Pitch +45° (sail down-left) |
      | blades | box | X 1460 × Y 40 × Z 280 | (643.5, −460, 2294.5) | Pitch −45° (sail down-right) |

      Each sail box's long axis (X) runs along the spar from the hub out to the tip, its Z is the sail's width, and its thin Y is along the hub axis.
    - All bodies: Physics Type = **Kinematic**, Collision Response = Enabled, Block All. The blade bodies then follow the spinning bone from the animation.
    - Skeletal mesh asset: no per-poly collision. Verify in the Physics Asset editor with the animation playing that the 4 boxes stay on the sails.
  - Slots → MI_Field_Windmill.
  - Add Skeleton sockets `loot_01` and `loot_02` on the bones with the same names (zero offset).
- **SK_Lighthouse.fbx:**
  - Import as skeletal mesh, no animations, create_physics_asset = true.
  - Replace the generated bodies with:
    - a capsule on `root`, radius 430 cm, from the ground to 3100 cm, Block All
    - a sphere on `lamp`, radius 110 cm, profile BlockAllDynamic (the destroyable light source)
  - Slots → MI_Desert_Lighthouse and MI_Desert_Lighthouse_Glow.
  - Verify the bones root > head > lamp, and socket_beam under head.
  - Add a Skeleton socket `BeamSocket` on bone socket_beam (zero offset), and Skeleton sockets `loot_01` and `loot_02` on the bones with the same names.

## 5. Blueprints (in /Game/Assets/Locations/Blueprints)
Axis note: FBX conversion turns Blender +Y (the "forward" used in the notes above) into Unreal −Y or +X depending on the importer. Check each asset once in the viewport and use the axis that matches, then tell me which you used.

1. **NS_FactorySmoke** (Niagara system, sprite emitter) and **M_SmokeSprite**:
   - M_SmokeSprite: Translucent, Unlit, T_Smoke_Puff with Nearest sampler × Particle Color.
   - Spawn rate 4/s, lifetime 7–10 s.
   - Velocity up 250–400 cm/s plus a wind drift of 150 cm/s along +X (expose `Wind` as a vector user parameter).
   - Sprite size 300 → 1600 cm over life. Colour grey (0.55, 0.53, 0.50), alpha 0.8 → 0. Random rotation ±20°/s.
   - Bounds fixed at 6000 cm.
2. **BP_Factory**:
   - Static mesh component (default SM_Factory_A).
   - On BeginPlay, spawn NS_FactorySmoke attached to every socket whose name starts with "smoke". This works for both factories: create BP_Factory_A and BP_Factory_B as children with their meshes.
3. **BP_Windmill**: the windmill spins constantly.
   - SkeletalMeshComponent with SK_Windmill.
   - Animation Mode = Use Animation Asset, Anim To Play = A_Windmill_Spin, Looping on, Playing on. No AnimBP needed.
   - On BeginPlay:
     - random play rate 0.8–1.2, so windmills next to each other don't spin in sync
     - random start position
   - VisibilityBasedAnimTickOption = AlwaysTickPose, so it keeps turning when barely on screen. The blade collision follows the pose, so the pose must keep ticking even off screen.
   - Mesh collision: Collision Enabled = Query and Physics, preset BlockAll, Kinematic Bones Update To Physics = SkipSimulatingBones (the default). The physics asset bodies are Kinematic, so they follow the animation.
   - **Blades hitting the balloon:**
     - A spinning kinematic body only blocks things that move into it. It doesn't push a balloon that is standing still.
     - So, on BeginPlay (while the pose is still the reference pose), add 4 Box Collision components with the same sizes and component-space transforms as the 4 sail boxes in the table.
     - Attach them to the mesh at socket/bone `blades` with Keep World Transform, so they spin with the sails.
     - Set them to Overlap Pawn and generate overlap events.
     - On BeginOverlap with the player balloon, call `OnWindmillHit(HitLocation, PushDirection)` (Blueprint Interface) on the balloon. PushDirection = the blade's velocity at the hit point: cross(hub axis × angular speed, HitLocation − hub), normalised, plus a little outward.
     - The balloon applies the knock-back and damage. Re-trigger at most once per 0.5 s per blade.
   - Add AC_LootSpots.
4. **BP_JammerTower**:
   - SM_JammerTower, plus 3 small red PointLights (radius 1500 cm) attached to sockets beacon_1..3.
   - **Dish:**
     - A second StaticMeshComponent `Dish` (SM_JammerDish) attached to socket `dish`, with its own collision (BlockAll, generate hit events).
     - A Rotating Movement on the dish: 25°/s around Z.
     - A violet PointLight (0.75, 0.35, 1.0; radius 800 cm) on the dish's `emitter` socket.
     - A violet pulsing beam or ripple at the emitter while jamming (optional sprite emitter).
     - `DishHealth` (default 3 hits). When the dish takes AnyDamage or a projectile hit on the Dish component, DishHealth −= 1. At 0:
       - `bJamming` = false: stop the jam timer and broadcast OnRadarJam(0)
       - stop the rotation and turn off the emitter light and glow (MID `Flicker` 0 on the dish)
       - detach the dish, enable physics so it tumbles off the mast, spawn sparks and smoke at the socket, and destroy the falling dish after 10 s
     - The tower itself keeps its collision and the beacons keep blinking.
   - A timer blinks them together with the Glow MID's `Flicker` (0 ↔ 1, 1 s period, 50% duty).
   - Jamming only runs while `bJamming` is true (the dish is intact). Expose `JamRadiusM` (default 250). Each 0.2 s, compute JamStrength = 1 − saturate(distance to the player balloon / JamRadius) from SOCKET_jam, and broadcast it through a Blueprint Interface event `OnRadarJam(float Strength)` on the player's balloon. Later the radar can use it for static/noise. If MPC_Radar exists (docs/Unreal_Prompt_3_RadarEnemies.md), also add a scalar `RadarJam` to it and set it to the strongest jammer.
5. **BP_Bird** and **BP_BirdFlock** (Field). Birds fly in flocks and can collide with the player:
   - **BP_Bird:**
     - Sphere collision (radius 35 cm, profile OverlapAllDynamic, generate overlap events) as root, plus SK_Bird.
     - Plays A_Bird_Flap looping, with play rate 0.9–1.2 random. Switches to A_Bird_Glide while descending, for 1–3 s at random.
     - When it overlaps the player balloon:
       - call `OnBirdStrike(BirdActor)` (Blueprint Interface) on the balloon
       - spawn a small feather puff (Niagara burst of 12 dark sprites)
       - destroy the bird
   - **BP_BirdFlock:**
     - Spawns `Count` birds (random within the CSV's flock range, 8–24).
     - Moves them with simple boids on a 0.05 s timer:
       - separation (radius 2 m), alignment and cohesion (radius 12 m)
       - steer toward a wander target that changes every 6–12 s
       - stay inside the level bounds
       - keep height between 15 and 120 m above the ground
       - speed 9–14 m/s
     - If the player balloon comes within 30 m, the flock scatters away from it for 3 s, then regroups.
     - Rotate each bird to face its velocity.
6. **ABP_Lighthouse** + **BP_Lighthouse** (Desert): rotating head, beam that changes colour when it spots the player, and a destroyable light:
   - **ABP_Lighthouse** (for SK_Lighthouse_Skeleton): float `HeadYaw`.
     - AnimGraph: Transform (Modify) Bone on `head`, Rotation, Add to Existing, **Component Space**, Yaw = HeadYaw.
     - The head bone points straight up, so component-space yaw spins it around the tower axis.
     - Bone `lamp` follows the head.
   - **BP_Lighthouse:**
     - SK_Lighthouse with ABP_Lighthouse.
     - SM_LighthouseBeam attached to BeamSocket, with a Dynamic Material Instance of MI_LightBeam. Point the cone's long axis along the socket's forward; it is +Y in Blender, check after import.
     - A SpotLight attached to BeamSocket: outer cone 7°, attenuation 9000 cm, intensity 40000 cd, no shadows.
   - **State:**
     - `RotationSpeed` (default 30°/s)
     - `BeamRangeM` (90)
     - `BeamHalfAngle` (6°)
     - `LampHealth` (default 1 hit)
     - `bDestroyed`
     - `SpotColor` (1.0, 0.08, 0.04)
     - `IdleColor` (1.0, 0.95, 0.80)
   - **Tick:** if not destroyed, HeadYaw += RotationSpeed × DeltaTime (pass it to the AnimBP).
   - **Detection** (0.1 s timer):
     - The player is spotted when all of these hold:
       - it is closer than BeamRangeM to BeamSocket
       - the angle between the socket forward and the direction to the player is below BeamHalfAngle (plus the player's angular radius)
       - a visibility line trace from the socket reaches it
     - **On spotted:**
       - lerp the beam DMI `BeamColor` and the spotlight colour to SpotColor over 0.3 s, and BeamIntensity 2 → 4
       - stop rotating and track the player for 3 s
       - broadcast `OnPlayerSpotted(Location)` so other enemies can react
     - **On lost:** after 2 s return to IdleColor and resume rotating.
   - **Destruction:**
     - Bind OnComponentHit and AnyDamage. If the hit or damage is on bone `lamp` (hit result BoneName), LampHealth −= 1.
     - At 0:
       - `bDestroyed` = true
       - HideBoneByName(`lamp`)
       - hide the beam and turn the spotlight off
       - stop rotation and detection for good
       - spawn a glass-shard burst plus sparks at BeamSocket
     - The lighthouse keeps its collision but has no function any more.
7. **Loot spots** (works for every tall structure):
   - **BP_Loot** (parent class):
     - Variables:
       - `LootType` (enum E_LootType: Ammo, Money, Wood, Coal, Crystal)
       - `Amount` (int)
       - `HighlightColor` (linear colour)
     - Components:
       - StaticMesh root
       - Sphere `GrabZone` (radius 250 cm, OverlapAllDynamic)
       - Niagara `Sparkle` (NS_LootSparkle, below)
       - PointLight `Glint`: radius 400 cm, intensity 8 cd, colour = HighlightColor, no shadows
     - **Highlight, so the player can see it is a pickup:**
       - On BeginPlay, create a DMI of the mesh material(s) and set HighlightColor.
       - Set the Sparkle user parameter `SparkColor` = HighlightColor.
       - Hide Sparkle and Glint, and stop the pulse (HighlightStrength 0), once the item is picked up.
       - Optional: double the sparkle rate while the balloon is within 30 m.
     - Grabbing:
       - When the player balloon overlaps GrabZone, or a grab trace from the player hits the mesh, call `OnLootPickedUp(LootType, Amount)` (Blueprint Interface) on the balloon.
       - Play a small pickup puff, then DestroyActor.
       - Expose `bAutoPickup` (default true). If false, it only reacts to the grab trace.
     - Idle: a 1.2 s gentle bob of ±5 cm on the mesh only, never on the actor, so it stays snapped. Optional.
   - **NS_LootSparkle** (Niagara, sprite emitter, material M_LootSpark):
     - Spawn 6 particles per second, lifetime 0.6–1.0 s.
     - Spawn on a sphere surface, radius 70 cm, centred 40 cm above the pivot.
     - Small upward drift (20–60 cm/s).
     - Sprite size 12–28 cm, with a scale curve 0 → 1 → 0 (twinkle).
     - Random rotation 0 or 45°.
     - Colour = user parameter `SparkColor` × 2.5 (HDR, so it blooms).
     - Plus a second, slow emitter: one big soft spark (60 cm) at the centre that pulses every 1.5 s.
     - Bounds fixed at 200 cm. Keep it cheap: these sit on many buildings.
   - **Children:**

     | Blueprint | Mesh | LootType | Amount | HighlightColor (linear) |
     |---|---|---|---|---|
     | BP_Loot_AmmoBox | SM_Loot_AmmoBox | Ammo | 30 | (1.0, 0.7, 0.2) warm brass |
     | BP_Loot_MoneyPouch | SM_Loot_MoneyPouch | Money | 25 | (1.0, 0.85, 0.25) gold |
     | BP_Loot_Log | SM_Loot_Log | Wood | 1 | (0.6, 1.0, 0.4) green |
     | BP_Loot_Coal | SM_Loot_Coal | Coal | 30 | (1.0, 0.35, 0.1) ember orange |
     | BP_Loot_Crystal | SM_Loot_Crystal | Crystal | 1 | (0.35, 0.9, 1.0) cyan |
   - **AC_LootSpots** (Actor Component). Add it to every Blueprint of a tall structure (BP_Factory, BP_Windmill, BP_JammerTower, BP_TurretTower, BP_Lighthouse) and, for HISM/static-mesh placement, run it from the level generator.
     - Variables:
       - `SpawnChance` (0.6)
       - `LootTable`: array of {Class, Weight}. Defaults: Coal 30, AmmoBox 25, Log 20, MoneyPouch 18, Crystal 7. Per level, crystals can be rarer or more common, for example Desert 12, City 4.
       - `bRespawn` (false)
       - `RespawnTime` (90 s)
     - On BeginPlay, for the owner's first Static Mesh or Skeletal Mesh component:
       - list its socket names: GetAllSocketNames. This includes the Skeleton sockets added on SK_Windmill and SK_Lighthouse.
       - keep the ones starting with `loot_`
       - for each, with probability SpawnChance, spawn a weighted-random loot class and AttachToComponent(mesh, socket, SnapToTarget for location, rotation and scale)
     - Keep track of which socket each loot belongs to. When one is picked up and bRespawn is true, respawn it after RespawnTime.
     - If the structure is destroyed, detach its loot and enable physics on it, so it falls. Optional.
   - Seed the random stream from the level generator's seed, so a level always gives the same loot.
   - **BP_Loot_Coal's orange glow** is all in Unreal (the model is plain black coal):
     - Its NS_LootSparkle uses SparkColor orange (1.0, 0.35, 0.1).
     - Plus a second emitter, `Embers`: 10 per second, tiny orange-yellow sparks (6–12 cm) rising 80–160 cm/s with sideways jitter, flickering alpha, lifetime 0.8–1.4 s, colour lerping from (1.0, 0.75, 0.3) to (1.0, 0.25, 0.05).
     - The Glint point light is orange and flickers (0.7–1.3 × intensity, noise at 8 Hz).
     - The rim pulse (HighlightColor) is orange too, so the lump looks hot without any orange in its texture.
8. **BP_TurretTower** (Desert): a gun tower whose steam engine is its weak spot.
   - **Components:**
     - `Tower` (SM_TurretTower, root)
     - `Head` (SM_TurretTower_Head) on socket `head`
     - `Guns` (SM_TurretTower_Guns) on the Head's socket `guns`
     - `Engine` (SM_TurretTower_Engine) on socket `engine`

     Each has its own UCX collision, BlockAll, generate hit events, so hits tell you which part was struck.
   - **Engine smoke:** NS_FactorySmoke at a smaller scale (sprite size 150 → 700 cm, spawn rate 6/s, darker grey 0.3) attached to the Engine's socket `smoke`, plus a warm flickering PointLight at the firebox door.
   - **Aiming** (0.05 s timer, only if the player balloon is within `RangeM` = 250 and visible):
     - Head yaw tracks the balloon at up to 45°/s (RInterpConstantTo).
     - Guns pitch tracks it between −5° and +60°.
   - **Firing:**
     - every `FireInterval` (1.5 s) when the aim error is below 4°
     - alternate muzzle_1 and muzzle_2
     - spawn a projectile (or a hitscan trace) with a muzzle flash; SM_FX_MuzzleFlash from the Map_edge folder fits
   - **Damage:**
     - Hits on Head or Guns do little (×0.25, armour).
     - Hits on Tower do nothing.
     - Hits on **Engine** do full damage (×1). The engine is the weak spot: `EngineHealth` default 4 hits.
     - Each engine hit spawns sparks and briefly thickens the smoke.
   - **Engine destroyed:**
     - turret disabled: stop aiming and firing, the guns droop to −10°
     - smoke turns black and heavy for 30 s
     - the firebox light goes out, the engine mesh gets a darker MID
     - small explosion at the engine
     - broadcast `OnTurretDestroyed`
   - Add AC_LootSpots.

## 6. Random level generation data
1. Create the struct **S_LevelAsset** with fields matching the CSV header (the first column is the row name):
   - Asset (Soft Object reference to Static Mesh, or class for Blueprints)
   - Category (Name)
   - Collision (bool)
   - DensityPerKm2 (float)
   - ScaleMin, ScaleMax (float)
   - MinSpacingM (float)
   - RandomYaw (bool)
   - AlignToSlope (bool)
   - ClusterSize (int)
   - Rule (Name)
   - Notes (String)
2. Import each `<Level>_Placement.csv` as DataTable **DT_<Level>_Placement**. The Asset column contains mesh names; resolve them to `/Game/Assets/Locations/<Level>/<Name>`. Map these rows to Blueprints instead of meshes:

   | Row | Use |
   |---|---|
   | SM_Factory_A / _B | BP_Factory_A / _B |
   | SK_Windmill | BP_Windmill |
   | SM_JammerTower | BP_JammerTower (includes the dish) |
   | SM_TurretTower | BP_TurretTower (includes head, guns, engine) |
   | SK_Lighthouse | BP_Lighthouse |
   | SK_Bird | BP_BirdFlock |
   | SM_Pine_Tall_A/B, SM_RangerTower, SM_Church, SM_ClockTower, SM_Tower_Office_A/B, SM_Mesa, SM_Hoodoo | spawn as Static Mesh actors with AC_LootSpots added |
3. Rules for the generator, which I will build. Document them as comments in the DataTable or in a README next to it:
   - `scatter`: random positions at DensityPerKm2, at least MinSpacingM apart, in clusters of ClusterSize.
   - `ring`: Forest mountains. Place them only outside the playable area, in a ring 1.5–3 km from the centre, facing inward, scaled ScaleMin–ScaleMax, overlapping so the horizon is closed.
   - `line`: fences and hedges, chained end to end along field borders.
   - `street`: city blocks. Snap to a street grid with yaw in 90° steps, buildings back to back, lampposts along the streets.
   - `wind_aligned`: dunes. All share one yaw ±10° per level.
   - `flock`: spawn BP_BirdFlock, 3–6 flocks per Field level.
   - Category: Filler, Terrain or Background go into Hierarchical Instanced Static Mesh components, one per mesh, NoCollision. Obstacle and Hazard go in as separate actors with collision.
   - Cull distances (HISM): small clutter 15000 cm, short trees 30000 cm, buildings and dunes 80000 cm. Mountains and obstacles: never cull.
4. Ground: a large plane (or the generator's landscape) with MI_<Level>_Ground.
5. Atmosphere per level: Exponential Height Fog matching the balloon's post-process look.

   | Level | Fog colour | Density | Notes |
   |---|---|---|---|
   | Forest | blue-green (0.35, 0.45, 0.45) | 0.03 | hides the mountain bases |
   | Field | warm haze (0.75, 0.70, 0.55) | 0.015 | |
   | City | brown smog (0.40, 0.35, 0.28) | 0.05 | |
   | Desert | hot pale haze (0.85, 0.75, 0.60) | 0.02 | |

## 7. Test
Create a test level `L_Test_<Level>` for each location: a 2 × 2 km ground plane with MI_<Level>_Ground and a hand-placed sample of every asset (5–20 of each filler, 2–3 of each obstacle). Place BP_Balloon if it exists, otherwise a 7 m reference cube.

Check, and report:
- Every mesh shows its texture: nothing pink, nothing grey.
- Glow slots glow: cabin window, shop windows, lampposts, factory skylights and furnace, clock faces, jammer lights, lighthouse lens.
- Ground and distant props don't shimmer when the camera moves, at 50 m and at 500 m.
- Collision: a line trace hits each obstacle and passes through each filler.
- Factory smoke rises from the chimney tops.
- The windmill blades spin around the right axis.
- Birds flock, scatter near the balloon and get destroyed on contact.
- Windmill: the sails spin constantly and smoothly, with no hitch at the loop, at slightly different speeds per windmill. Flying the balloon into a sail, or parking it in the sails' path, makes it collide or get knocked back. The tower, gallery and cap block it too.
- Coal shows an orange glow and rising embers in game, though its model has no orange in it.
- Pickups sparkle and their rims pulse in their colour (brass, gold, green, orange, cyan). They are visible from 100 m away.
- Loot: every tall structure shows loot on its loot spots, sitting on the balcony, platform, crate or nest, not floating and not buried. Flying the balloon into a loot item picks it up and calls OnLootPickedUp. The lighthouse loot stays put while the head rotates.
- Jammer: the dish rotates. Shooting it 3 times knocks it off the mast, and jamming stops (OnRadarJam(0)).
- Turret tower: the head and guns track the balloon and fire. Hits on the head or guns barely hurt it. The smoking engine is the weak spot, and destroying it disables the turret.
- The lighthouse head rotates. Its beam turns red when the camera or pawn stands in it, and after shooting/damaging the lamp, the light is gone and it stops working.
- Screenshot of each test level.
