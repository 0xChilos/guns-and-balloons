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
