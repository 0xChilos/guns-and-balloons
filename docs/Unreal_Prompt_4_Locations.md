You are connected to my Unreal Engine 5 editor (project "gunsandballoons"). Set up the 4 game locations: Forest, Field, City and Desert. Each location is its own level, generated randomly from the assets below. Tall structures carry loot spots where pickups (coal, ammo boxes) snap on. Work through the steps in order and verify each one. Where possible use the editor Python API (`unreal` module: AssetImportTask, AssetToolsHelpers, MaterialEditingLibrary, EditorAssetLibrary, DataTableFunctionLibrary). If something can't be authored through your tools (for example Blueprint graph nodes, AnimGraph, Niagara modules), create everything else and give me exact click-by-click steps for the missing part.

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
- Static meshes have sockets `loot_01`, `loot_02` … (FBX empties `SOCKET_loot_NN`). SK_Lighthouse has bones `loot_01`, `loot_02` instead.
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
| SM_Windmill + SM_Windmill_Blades | 31 m tower windmill, a tall landmark (own texture T_Field_Windmill). loot_01 is on the railed lookout on its roof, loot_02 on the gallery at 9 m. Attach the 15 m blades to SOCKET_blades and spin them around the hub axis (see step 5) | UCX ×3 on the windmill, none on the blades |

**City**: Victorian London.

| Asset | What | Collision |
|---|---|---|
| SM_House_Terrace_A/B/C, SM_Shop_Corner, SM_Warehouse, SM_Gasholder, SM_Lamppost | Short buildings 10–19 m (lit shop windows and lamps use the glow slot) | none |
| SM_Church | 41 m spire. Balcony on the tower front at 16 m = loot_01 | UCX ×3 |
| SM_Factory_A | Sawtooth hall, 2 chimneys (32 / 28 m), glowing skylights. Smoke from SOCKET_smoke_1 and SOCKET_smoke_2 (the chimney tops). A railed service platform on each chimney = loot_01/02 | UCX ×5 |
| SM_Factory_B | Brick block, 40 m chimney, water tank, glowing furnace. Smoke from SOCKET_smoke_1. loot_01 on the chimney deck at 30 m, loot_02 on the roof | UCX ×3 |
| SM_ClockTower | 64 m clock tower, glowing clock faces. Own texture. Balconies at 24 m and 36 m and the railed belfry ledge at 49 m = loot_01..03 | UCX ×2 |
| SM_Tower_Office_A / _B | 51 m / 61 m tall buildings. Own textures. A: 3 balconies (18, 32, 40 m). B: 2 spots on the railed roof terrace at 22 m and a balcony at 36 m. loot_01..03 each | UCX ×2 each |
| SM_JammerTower | 83 m giant antenna: lattice mast, copper coils, red warning lights (glow slot). Sockets beacon_1..3 (lights), jam (effect origin), loot_01/02 on the two platforms. Own texture | UCX ×2 |

**Desert**: sand, dunes, guard towers and lighthouses.

| Asset | What | Collision |
|---|---|---|
| SM_Dune_A/B/C | Crescent dunes 70–200 m long, 8–23 m high. All face the same wind: downwind = +Y in Blender (see step 5 for the axis after import) | none (the balloon flies over them) |
| SM_Mesa (34 m), SM_Hoodoo (20 m) | Rock formations. Smugglers' crates on the mesa top = loot_01..03. A crate on a plank platform on the hoodoo cap = loot_01 | UCX ×1 each |
| SM_Rock_Desert_A/B, SM_Cactus_A/B, SM_Tent, SM_Wreck_Airship, SM_Ribcage | Clutter: rocks, cacti, nomad tent, crashed zeppelin, giant skeleton | none |
| SM_GuardTower | 28 m stilted guard tower with a searchlight (SOCKET_searchlight). Supply crates on the platform = loot_01/02 (on the crate lids). Own texture | UCX ×2 |
| SK_Lighthouse | 38 m lighthouse. Bones: root > head (rotates) > lamp (light source, can be destroyed); non-deforming socket_beam (on head, at the lamp, pointing along the beam); non-deforming loot_01/02 (on root, on the crate lids on the gallery, so they don't turn with the head). Own texture, glow slot = lens | physics asset (step 4) |
| SM_LighthouseBeam | 90 m light cone, apex at its pivot. Attach it to socket_beam | none |

**Loot** (folder Loot): the pickups that snap onto loot spots.

| Asset | What | Collision |
|---|---|---|
| SM_Loot_CoalSack | Burlap sack of coal, 0.6 × 0.55 × 0.8 m | UCX box (for grab traces) |
| SM_Loot_CoalCrate | Open crate heaped with coal, 0.7 × 0.55 × 0.45 m | UCX box |
| SM_Loot_AmmoBox | Army-green ammo box with brass plate and red band, 0.75 × 0.45 × 0.38 m | UCX box |

## 1. Textures
Import every PNG (automated, replace existing). Then:
- **All `T_<Group>` atlases and `T_<Level>_Ground`:**
  - Filter = Nearest
  - LODGroup = TEXTUREGROUP_World
  - MipGenSettings = FromTextureGroup (mipmaps ON: this is what stops distant objects and the ground from shimmering)
  - sRGB on, CompressionSettings = Default
- **T_Smoke_Puff:**
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
5. **Material instances:**
   - For every texture group, create `MI_<Group>` (parent M_LocProp, Atlas = T_<Group>) and `MI_<Group>_Glow` (parent M_LocGlow, same atlas). The groups are Forest_Props, Forest_Tall, Forest_Mountains, Field_Props, Field_Bird, Field_Windmill, Loot, City_Props, City_ClockTower, City_TowerA, City_TowerB, City_Jammer, Desert_Props, Desert_GuardTower and Desert_Lighthouse.
   - `MI_LightBeam` (parent M_LightBeam).
   - `MI_<Level>_Ground` (parent M_Ground) with Ground = T_<Level>_Ground and FarColor (linear) = the texture's average:

     | Level | FarColor (linear RGB) |
     |---|---|
     | Forest | (0.181, 0.174, 0.095) |
     | Field | (0.387, 0.410, 0.176) |
     | City | (0.244, 0.234, 0.219) |
     | Desert | (0.729, 0.558, 0.349) |

## 3. Static meshes
Import every `SM_*.fbx` from the 4 folders with these settings:
- import_mesh = true, combine_meshes = true
- auto_generate_collision = **false** (UCX hulls are imported instead)
- import_materials = false, import_textures = false
- import_uniform_scale = 1.0, convert_scene = true, force_front_x_axis = false
- normal_import_method = Import Normals, generate_lightmap_u_vs = false
- Nanite off

Then per mesh:
- **Materials:** the FBX slot names are `M_<Group>`, `M_<Group>_Glow` and `M_LightBeam`. Assign `MI_<Group>`, `MI_<Group>_Glow` and `MI_LightBeam` by slot name.
- **Collision:**
  - Meshes with UCX hulls (Collision column above): collision preset BlockAll, Collision Complexity = Use Simple Collision As Complex.
  - Loot meshes: keep their UCX box. Collision preset = custom: Query Only, block the Visibility trace (so a grab trace or crosshair hits them), ignore Pawn (the balloon flies through them).
  - All other meshes: collision preset NoCollision, and remove any simple collision. Short trees, clutter and short buildings never touch the player.
- **Sockets:** check against the manifest that these exist:
  - SM_Factory_A: smoke_1, smoke_2
  - SM_Factory_B: smoke_1
  - SM_Windmill: blades
  - SM_JammerTower: beacon_1, beacon_2, beacon_3, jam
  - SM_GuardTower: searchlight
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
- **SK_Lighthouse.fbx:**
  - Import as skeletal mesh, no animations, create_physics_asset = true.
  - Replace the generated bodies with:
    - a capsule on `root`, radius 430 cm, from the ground to 3100 cm, Block All
    - a sphere on `lamp`, radius 110 cm, profile BlockAllDynamic (the destroyable light source)
  - Slots → MI_Desert_Lighthouse and MI_Desert_Lighthouse_Glow.
  - Verify the bones root > head > lamp, and socket_beam under head.
  - Add a Skeleton socket `BeamSocket` on bone socket_beam (zero offset).

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
3. **BP_Windmill**:
   - SM_Windmill, plus SM_Windmill_Blades attached to socket "blades".
   - The blades spin around the axis that points out of the windmill's front, at 30°/s with a random ±10°/s per instance. Use a Rotating Movement component on the blades with the rotation axis set accordingly.
4. **BP_JammerTower**:
   - SM_JammerTower, plus 3 small red PointLights (radius 1500 cm) attached to sockets beacon_1..3.
   - A timer blinks them together with the Glow MID's `Flicker` (0 ↔ 1, 1 s period, 50% duty).
   - Expose `JamRadiusM` (default 250). Each 0.2 s, compute JamStrength = 1 − saturate(distance to the player balloon / JamRadius) from SOCKET_jam, and broadcast it through a Blueprint Interface event `OnRadarJam(float Strength)` on the player's balloon. Later the radar can use it for static/noise. If MPC_Radar exists (docs/Unreal_Prompt_3_RadarEnemies.md), also add a scalar `RadarJam` to it and set it to the strongest jammer.
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
     - Variables: `LootType` (enum E_LootType: Coal, Ammo), `Amount` (int).
     - Components:
       - StaticMesh root
       - Sphere `GrabZone` (radius 250 cm, OverlapAllDynamic)
     - Grabbing:
       - When the player balloon overlaps GrabZone, or a grab trace from the player hits the mesh, call `OnLootPickedUp(LootType, Amount)` (Blueprint Interface) on the balloon.
       - Play a small pickup puff, then DestroyActor.
       - Expose `bAutoPickup` (default true). If false, it only reacts to the grab trace.
     - Idle: a 1.2 s gentle bob of ±5 cm on the mesh only, never on the actor, so it stays snapped. Optional.
   - **Children:**
     - BP_Loot_CoalSack: SM_Loot_CoalSack, Coal, Amount 20
     - BP_Loot_CoalCrate: SM_Loot_CoalCrate, Coal, Amount 45
     - BP_Loot_AmmoBox: SM_Loot_AmmoBox, Ammo, Amount 30
   - **AC_LootSpots** (Actor Component). Add it to every Blueprint of a tall structure (BP_Factory, BP_Windmill, BP_JammerTower, BP_GuardTower, BP_Lighthouse) and, for HISM/static-mesh placement, run it from the level generator.
     - Variables:
       - `SpawnChance` (0.6)
       - `LootTable`: array of {Class, Weight}. Defaults: CoalSack 45, CoalCrate 15, AmmoBox 40.
       - `bRespawn` (false)
       - `RespawnTime` (90 s)
     - On BeginPlay, for the owner's first Static Mesh or Skeletal Mesh component:
       - list its socket names: GetAllSocketNames. For the lighthouse, bones count: also check GetBoneName / DoesSocketExist.
       - keep the ones starting with `loot_`
       - for each, with probability SpawnChance, spawn a weighted-random loot class and AttachToComponent(mesh, socket, SnapToTarget for location, rotation and scale)
     - Keep track of which socket each loot belongs to. When one is picked up and bRespawn is true, respawn it after RespawnTime.
     - If the structure is destroyed, detach its loot and enable physics on it, so it falls. Optional.
   - Seed the random stream from the level generator's seed, so a level always gives the same loot.
8. **BP_GuardTower** (optional): SM_GuardTower plus a SpotLight on SOCKET_searchlight, sweeping ±40° slowly. It can reuse the lighthouse detection logic with a 60 m range.

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
   | SM_Windmill | BP_Windmill |
   | SM_JammerTower | BP_JammerTower |
   | SM_GuardTower | BP_GuardTower |
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
- Loot: every tall structure shows loot on its loot spots, sitting on the balcony, platform, crate or nest, not floating and not buried. Flying the balloon into a loot item picks it up and calls OnLootPickedUp. The lighthouse loot stays put while the head rotates.
- The lighthouse head rotates. Its beam turns red when the camera or pawn stands in it, and after shooting/damaging the lamp, the light is gone and it stops working.
- Screenshot of each test level.
