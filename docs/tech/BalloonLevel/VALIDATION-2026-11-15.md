# Level generation validation

Validated 2026-11-15, 18:46 Europe/Kyiv. Scope: current full-project build, all BalloonLevel automation, and targeted source review. No gameplay implementation or assets changed.

## Results

- Full-project Development Editor build passed, using `-ModuleWithSuffix=gunsandballoons,8916` to preserve the open editor. This was an incremental build, not a clean rebuild.
- A separate UnrealEditor-Cmd process loaded that DLL and ran `Automation RunTests GunsAndBalloons.BalloonLevel` on `/Engine/Maps/Entry` with `-NullRHI`.
- All ten tests passed: M1-M5, M6.Events, M6.RuntimeSafety, GenerationModes, M8.Realization, EnvironmentConfig. Report counters: succeeded=9, succeededWithWarnings=1, failed=0, notRun=0, inProcess=0. Process exit 0.
- M8.Realization emitted four synthetic-world `UWorld::DestroyActor: World has no context` warnings; its assertions passed.
- Evidence: `Saved/LevelValidation/TestReport/index.json`, `Saved/Logs/LevelValidation-Build.log`, `Saved/Logs/LevelValidation-Automation_2.log`. The initial sandboxed automation failed during Zen cache startup; the successful run used approved external cache access.

## Open findings

1. **High: colliding Blueprint placements cannot pass route validation.** `BalloonPlacementPlanner.cpp:47-57` resolves collision bounds only for static meshes; Blueprint enemies/obstacles get no certified bounds. `BalloonHorizontalRouteValidator.cpp:94-96` rejects them. This confirms STATUS F1; a passing synthetic test suite does not establish working authored enemy generation.
2. **High: generation can report success with a mismatched environment.** `BalloonLevelGenerator.cpp:60-75` commits realization before applying the environment and returns true even if application fails. In game worlds, `BalloonEnvironmentPresetController.cpp:58-62` rejects a changed biome/preset after the first application. Runtime regeneration into a different biome/preset can therefore leave Ready/ActiveConfig describing the new level while the old atmosphere remains. Initial environment application errors also do not fail generation. Source-review finding; not reproduced in PIE. EnvironmentConfig covers editor-world changes, not this game-world case.
3. **Medium: route proof starts at the configured start, not the actual balloon position.** Generation does not move the balloon. An authored start/height mismatch can leave a Ready plan disconnected from the player's position; runtime border changes reject a height mismatch (STATUS F4).
4. **Medium: existing border proxies participate in world sweeps.** `WorldSweep` ignores only the balloon and attached riders. Old proxy collision can reject regeneration or safe shrinking; successful generation clears registration without owning/destroying those proxies (STATUS F5).
5. **Usability: Field line placement requires a matching piece budget.** Excess and insufficient density-derived counts both fail (`BalloonPlacementPlanner.cpp:244,335`), confirming STATUS F2.

## Acceptance limits

No PIE flight, rendered culling, authored flock/Blueprint integration, four-biome by Day/Night play matrix, or visual acceptance was performed. Placement DataTables exist under `Content/Assets/Locations/<Biome>/DT_<Biome>_Placement`; their existence does not establish their compatibility with the native catalog schema. Older documentation claiming no realization or no placement tables is historical/stale. Build and automation pass; full playable-biome acceptance remains incomplete.
