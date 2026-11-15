# Levels and biomes

> **Status:** Draft · **Owner:** TBD · **Updated:** 2026-11-15

## Why
Variety between runs: each biome forces a different route and different priorities.

## How it works
- Successful generation spawns functional shared enemies (one nemesis airship, plus edge AA balloons unless a configured border class already supplies them) and biome enemies: Field bird flocks, Desert cannon towers and beacons. Forest and City currently add no unique enemies. The Level Generator's `Enemies` category exposes `Spawn Enemies`, `Spawn Shared Enemies` and `Spawn Biome Enemies`; change them and regenerate to disable generated enemies for editing/testing. Manually placed enemies are unaffected. The master switch also suppresses catalog enemies and the optional border enemy actor, while border influence proxies remain independent. Counts (AA 8, flocks 3, cannons 3, beacons 2), start clearance (150 m), tower side offset (45 m) and airship start distance (250 m) are editable prototype tuning, not final balance. Seeded placement follows the curved route; towers require tagged terrain and report skipped positions if ground is absent. Regeneration/Clear removes generated enemy actors; failed generation retains the previous enemies.
- Generated and Flat terrain continue beyond the detailed corridor grid through four sparse, seamlessly stitched horizon rings, fading the edge elevation to the ground datum. The outer ring extends 100 km beyond each side, hiding the nearby rectangular cutoff in the distance haze in every biome without enlarging the detailed grid or changing gameplay borders. Ring distances are prototype constants requiring art tuning in `BalloonTerrainBuilder.cpp`. Authored `WorldCollision` terrain remains the map author's responsibility.
- Each biome has distinct Day/Night sky materials: misty teal Forest, blue/golden Field, smoky copper City and amber/violet Desert. The environment controller applies the generated biome's sky before recapturing ambient light, reusing engine sky spheres or its own collision-free dome. Materials live in `/Game/BalloonLevel/Skies`; `DaySkyMaterials` / `NightSkyMaterials` allow overrides. Palettes, clouds and celestial brightness are prototype art tuning (TBD); recreate assets with `Scripts/Editor/setup_biome_skies.py`. The existing fixed Day/Night rule and post process still apply.
- The level corridor can bend horizontally using the generator's heading and minimum turning radius settings; flight altitude stays fixed. Border balloons are generated with the level along its curved edges, using the generation seed. `RandomizeSeed` gives a new layout each generation; a fixed seed reproduces it. Regeneration replaces old borders and Clear removes them. `MainLevel` uses eight generated rocket balloons and randomized seeds; preview values require tuning.
- The Level Generator is the single authoring controller: choose the config/biome pool there, optionally override flight/ground height, terrain and lighting, and use Setup Level Controller to group/reuse or create the environment, sun, sky and fog. Generated entities appear beneath it. Overrides affect only the generation snapshot; source configs stay unchanged. Clear removes generated terrain/scenery; deleting the controller also removes environment actors it created, preserving reused authored actors.
- Successful generation places the player balloon (`RoutePawn`) at the generated route start and flight height, facing the initial route direction; attached riders move with it. This applies to editor generation, level start and regeneration. Failed generation retains the previous balloon position.
- Terrain is created per level: `Generated` builds seeded rolling ground; `Flat` builds a level surface. Height variation, wavelength, grid size, corridor padding and material are configured per biome; values are prototypes requiring tuning. `WorldCollision` instead uses tagged authored ground. Placement follows the surface and optionally aligns to slopes; horizon background rings retain their ground datum. Terrain collision participates in route validation and terrain is replaced/cleared with the generated level. Flight altitude stays fixed.
- Successful level generation also applies the bound environment from the generated config, for editor buttons, Blueprint calls and level start. Failed generation retains the previous environment.
- Environment ground material and fog use the generation config's `BiomeId`; fixed Day/Night lighting uses its `Preset`. Editor preview reads the selected config; play uses the successfully generated config snapshot. Random selection requires generation before preview. Lighting remains fixed during play.
- Generation: `Manual (Button)` from a button, or `Every Level Start` automatically on every BeginPlay. The plan is temporary and isn't saved in the map.
- Biome: `Selected Config`, or `Random` from the `RandomBiomeConfigs` list with their asset tables. `RandomizeSeed` gives a new seed on every generation; otherwise the config's seed is used. The source DataAsset isn't changed.
- A level = a flight through one biome. After it comes the [safe zone](safe-zone.md).
- Every generated level has a completion zone at the route finish. When the player balloon's origin enters it, `CompleteLevel()` fires once and emits `OnLevelCompleted`. Successful regeneration resets completion; failure retains it. The zone spans the corridor width; half-depth defaults to 10 m and half-height to at least 10 m (prototype values requiring tuning). The transition to the safe zone is TBD.
- A level is a **corridor** from start to finish. Rocket balloons (`BP_RocketBalloon`) patrol both edges of the corridor: flying over the edge = getting shot at.
- Each biome has its own obstacles and hazards.
- Generation merges the shared asset catalog with the selected biome catalog; biome rows replace shared rows with matching names. The preview configs enable scenery and static-mesh obstacles, using shared rocks plus the biome's location meshes. Density and spacing are prototype tuning in `Content/BalloonLevel/Rules/BalloonAssets_*.json`. Animated/functional hazard actor integration remains incomplete.
- Every level has resources for the [grappling hook](grappling-hook.md) (loot spots on tall structures).
- The biome set matches the existing assets (`/Game/Assets/Locations/<Level>`).

## Biomes
| Biome | Obstacles | Special hazards |
| --- | --- | --- |
| Forest (`Forest`) | Tall pines (they took over the jungle's tall thin trees), a fire lookout tower; a ring of mountains on the horizon | — |
| Field (`Field`) | Almost no natural obstacles; windmills | Birds (flocks): avoid them, shoot them, or scare the flock with a shot in its direction (a scared flock flies off for good and does no damage) |
| City (`City`) | Tall buildings, a church, a clock tower, factories | Fog; radar jammers (shoot down the dish) + a minigame to restore the radar |
| Desert (`Desert`) | Mesas, rock pillars; tall towers with a cannon at the player's height: within range it fires single shells with a reload; the cannon can be destroyed by hits on it (revolver 4, shotgun 3, rifle 2) or with one shot at the big fuel tank that hangs under the platform, where it is seen from every side; either way the tank and the cannon blow up; the tower stays standing with an empty, smoking deck | Sandstorms (periodic); beacons: their lamp is at the player's height; they search for the balloon with an orange beam up to a set distance; the player can avoid the light or shoot the lamp before being spotted (only the lamp shatters and goes dark, the beacon tower stays); once spotted, the beam turns red, follows the balloon until the lamp is broken or the balloon gets far enough from the beacon, and calls one crop duster; while it keeps the balloon in its red beam it calls another 7 s after there is room (at most 3 crop dusters attack at once on the map) |

**AA gun (all biomes).** A small flying balloon with a turret and a searchlight under its gondola. It hangs near the map edges at the player's altitude (follows it). Outside its range it shines an orange searchlight that circles around. When the balloon is within range and in line of sight, the light turns red and it launches homing missiles, one at a time: a missile chases the balloon while it stays in range and blows up harmlessly the moment the balloon leaves; a hit deals 50; one shot destroys a missile. The missile is slow enough (~9 m/s) to give the player a few seconds to get out of range, but faster than the balloon. It lets go once the balloon is out of range. The range is small (~25 m): AA guns along the map edge mark a line the player must not cross; they don't shoot at the player all the time. The searching beam is as long as the range. Indestructible: the player can only avoid it. Range, missile speed and damage are TBD; place them so the flight corridor between AA guns stays clear.

A patrol is one crop duster flying a route; when it spots the balloon (within ~200 m) it joins the attack, unless 3 crop dusters are already attacking.

## Parameters
| Parameter | Value | Note |
| --- | --- | --- |
| Corridor width | 600 m | Default; ≥ 2 × the rocket balloons' `AggroRangeM` (300 m), so in the middle of the corridor the player is out of their range |
| Level length (flight time) | TBD | |
| Flight height | TBD | The balloon's spawn height; there is no height control |
| Levels per run | TBD | |
| Biome order | TBD | Random / fixed |

## Dependencies
[Balloon controls](balloon-controls.md), [Stove engine](engine-stove.md), [Navigation panel](ui-menus.md) (radar in the city), [Airship and enemies](nemesis-airship.md).

## Open questions
- [ ] Which biomes are in the jam version?
- [ ] What happens when the balloon flies over the corridor's edge (only getting shot at, or a hard boundary)?
