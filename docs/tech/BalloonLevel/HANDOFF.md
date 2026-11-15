# Balloon level generation handoff

Updated: 2026-11-15. Current milestone: M1 only. Source implemented; isolated UE build and GunsAndBalloons.BalloonLevel.M1 automation PASSED. Full project integration BLOCKED by pre-existing Pawn merge conflicts. Details and commands in TEST_REPORT.md. Stop before M2.

## Baseline and scope

No existing HANDOFF was found in the workspace. Read docs/README.md, docs/overview.md, docs/features/biomes.md, specification sections 3–5 and M1. Specification: user attachment version 1.1 dated 2026-11-15, mirrored at docs/temp/Balloon_Level_Generation_LLM_Implementation_Plan.v1.md (pre-existing untracked file).

Actual module: gunsandballoons, export GUNSANDBALLOONS_API. Project/installed engine: UE 5.9.0, changelist 60351404 at G:/UE/UE_5.9, overriding specification's 5.8 example. Existing BalloonFlightComponent controls horizontal forward movement/turning; it was not modified. Current Pawn collision is composite and must be inspected for M5, not replaced by guessed example radii.

Pre-existing work left intact: modified docs/decisions.md and docs/features/biomes.md; untracked .claude/ and docs/temp/. Full project also contains committed merge markers in FirstPersonCameraPawn.h, causing UHT failure at line 335. No attempt made to resolve unrelated gameplay merges.

## Implemented files and APIs

- Source/gunsandballoons/BalloonLevel/: BalloonLevelTypes.h, BalloonAssetCatalog.h/.cpp, BalloonLevelConfig.h/.cpp, BalloonLevelGenerator.h/.cpp.
- Source/gunsandballoons/Tests/BalloonLevelM1Test.cpp: GunsAndBalloons.BalloonLevel.M1 automation.
- Content/BalloonLevel/Rules/README.md: exact twelve-column contract, all six rule semantics, merge/error behavior and preview instructions.
- Scripts/Verify-BalloonLevelM1.ps1: reproducible isolated build/test harness under ignored Saved/M1Verification.
- This directory: HANDOFF.md, DESIGN_DECISIONS.md, TEST_REPORT.md.
- docs/README.md: discoverable handoff link in documentation index.

FBalloonLevelAssetRow has exactly Asset, Category, Collision, DensityPerKm2, ScaleMin, ScaleMax, MinSpacingM, RandomYaw, AlignToSlope, ClusterSize, Rule, Notes. Native reflected schema is separate from legacy S_LevelAsset (which permits Creature and Blueprint scenery). Existing tables and Blueprints are not migrated. No native-schema .uasset tables/config/BP generator assets have been authored.

BalloonAssetCatalog::ValidateRow / Merge load soft references on game thread, check StaticMesh vs spawnable Blueprint-generated Actor class, renderer/category/collision/rule compatibility and finite numeric ranges. All rows validate before biome whole-row overrides; lexical row-name sort, source-table provenance, no duplicates. Null tables permit empty geometry previews.

UBalloonLevelConfig is a Blueprint DataAsset: seed, length, width, target segment length, fixed flight height, StartTransform, biome, shared/biome tables, catalog version, fixed Day/Night enum. Length/height/segment defaults are provisional and need tuning; width follows current 600m design. Future subsystem config fields arrive with their milestones.

BalloonLevelPlanning::BuildStraightPlan creates pure descriptors and segment bounds in centimetres, fixed absolute world Z, world XY/yaw from config StartTransform. Scale/pitch/roll/Z and generator placement do not change the authored datum. Segments use [start,end), final inclusive. 100000-segment guard rejects excess allocation. No RNG draw is needed for straight M1 geometry; stable streams belong to M2 onward.

ABalloonLevelGenerator exposes LevelConfig, Spline, GenerateLevel, RegenerateLevel, ClearGeneratedLevel, GetGenerationStatus, GetDiagnostics in Blueprint/Details. C++ GetPlan returns const descriptor reference. Calls are explicit, not automatic at BeginPlay. Clear/regenerate remove only owned spline/plan. Failure removes stale data; success reaches Ready, never Complete: no route proof or realized actors. All specified generation status enum values exist; later milestones own their transitions. There is no progress API until M2.

## Known limits and next action

Full integration cannot be certified until pre-existing Pawn merge conflicts are resolved and the main Editor target plus automation are rerun. Isolated harness only verifies copied M1 sources against real UE; it does not load project assets. Native DataTables/config assets and visual editor/PIE checks remain unverified.

Next milestone (only when requested): M2 curvature, variable width and horizontal progress. Prerequisites: resolve main project build blocker, rerun M1 integration, preserve the exact schema and separate plan/validation/realization. Do not add vertical maneuvering, day/night transitions, placement, hazards or realization in M1.
