# Level generation preview

Open `/Game/Scenes/LevelGeneration_Test`. Both test maps use instances of the
shared `/Game/Assets/Balloon/BP_Balloon`. Edit that Blueprint's components and
compile/save it to update both levels. Keep gameplay tuning in the Blueprint;
only placement and rider attachments belong to each map. The old
`BP_LevelGenerationBalloon` asset is unused by this setup.

The camera/player is embedded in `BP_Balloon` as the `PlayerCamera` child actor
(`BP_BalloonPlayer`). Adjust its seat position in the balloon Blueprint. Do not
place another player Pawn in these maps.

The placed `LevelGeneration_Preview` actor references that balloon and
`/Game/Blueprints/Tests/DA_LevelGenerationPreview`. Click **Generate Level** or
**Regenerate Level** in its Details, then inspect Status and Diagnostics.
Plans are transient, so generate again after reopening the map.

To rebuild the fixture and draw its boundaries, border spheres and green route
for ten minutes, run this in the editor console:

```text
py G:/UE/gunsandballoons/Scripts/Editor/setup_level_generation_preview.py
```

The script resets the preview config: seed 12345, straight 500 m route, 200 m
width, absolute flight Z 100 m, eight borders, no placement catalog. These are
test fixture values requiring tuning, not approved gameplay parameters.
No scenery or obstacle actors are spawned by this planning milestone.

To verify generation and actual flight in PIE (the script ends PIE afterward):

```text
py G:/UE/gunsandballoons/Scripts/Editor/test_level_generation_pie.py
```

Success logs `LEVEL_GENERATION_PIE_PASS`: Ready, eight borders, attached player
possession, original balloon controls, and more than 100 cm travel in three seconds.

Generation controls (after rebuilding the project): select `LevelGeneration_Preview`
and open **Balloon Level > Generation** in Details.
- **Generation Mode**: **Manual (Button)** uses Generate Level / Regenerate Level;
  **Every Level Start** generates automatically on each BeginPlay (including PIE).
- **Biome Selection**: **Selected Config** uses Level Config; **Random** picks a
  valid entry from **Random Biome Configs**. Assign one DataAsset per biome with
  its matching BiomeId, asset tables and placement metadata. An empty list fails
  with a diagnostic; null entries are ignored and duplicate configs count once.
- Enable **Randomize Seed** for fresh layouts; leave disabled to use the config seed.
  Active Config shows the selected biome and effective seed without editing assets.

Manual plans are transient and are not baked into maps or copied into PIE.
Runtime/manual Blueprint calls use the same Generate Level API. These controls
plan and validate routes; they do not add world realization or scenery spawning.


M8 source implementation now realizes transient HISM/Actor preview content through the same Details buttons. Clear Generated Level removes tracked generated content; failed regeneration retains the prior successful level. Draw Generation Debug exposes route, spline, segment, placement and HISM toggles. Final-source build, automation and visual checks are pending after the user-requested stop; see docs/tech/BalloonLevel/HANDOFF.md. Earlier descriptor-only statements above describe the pre-M8 fixture.
