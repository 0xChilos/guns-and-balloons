# Balloon level generation — implementation status

> **Updated:** 2026-11-15 · **Branch:** `feat/level-generation` · **Spec:** [implementation plan v1.1](../../temp/Balloon_Level_Generation_LLM_Implementation_Plan.v1.md) · **Tech reference:** [level-generation.md](../level-generation.md)

## Summary
- **Done (M0–M6):** implemented and tested as a **plan → validate** pipeline.
- **M7 integrated:** controller saved in the generation test map; eight biome/preset editor property checks passed. PIE and visual acceptance remain manual; see Scripts/Editor/README_LevelGeneration.md.
- **M8 build and automation verified:** final full-project suffixed DLL built; all nine BalloonLevel tests passed (one with synthetic-world destruction warnings). See the final M8 entry in [TEST_REPORT.md](TEST_REPORT.md). Visual checks were deferred by the user.

`GenerateLevel` now realizes validated placements as scenery HISM components and gameplay Actors. Failed regeneration retains the previous Ready plan/config and realized content. Border descriptors still require explicit proxy integration. Earlier M7 and review findings below are historical unless superseded by the final M8 report.

## Milestones
| Milestone | Status | Notes |
| --- | --- | --- |
| M0 Audit | Done | UE 5.9, module `gunsandballoons`, see `DESIGN_DECISIONS.md` M1-01…M1-08 |
| M1 Config, DataTable schema, catalog merge, segments | Done | 12-column `FBalloonLevelAssetRow`; shared + biome overlay by row name |
| M2 Curved horizontal spline, width profile, progress | Done | Bounded sine heading field; no self-crossing by construction |
| M3 Border threat influences | Done | Stationary spheres projected onto the fixed flight slab |
| M4 Biome catalog, six placement rules | Done (descriptors only) | No HISM/actors are created |
| M5 Fixed-Z route proof, retries | Done | A* over (progress, lateral) + 3D box sweeps; up to 12 placement retries |
| M6 Timed/progress/custom events, border transactions | Done | Instant Set/Add/Scale radius; no lighting actions |
| Generation modes (uncommitted) | Done | `Manual`/`OnLevelStart`, `Selected`/`Random` biome, `RandomizeSeed`, Details buttons |
| M7 Fixed Day/Night, ground, biome fog | **Integrated; editor checks passed; manual acceptance pending** | `LevelGeneration_Environment` binds generator, expanded Ground, persistent fog, existing Movable sun/sky. Generator uses Every Level Start. All eight combinations passed material, fog color/density, sun rotation/intensity and sky intensity assertions using the existing compiled module. No visual tests, per user request. PIE test stopped before play on a script enum error. Fresh build attempt blocked by open-editor hot-reload DLL lock. Lighting remains TBD prototype tuning; landscape support remains outside this ground-plane integration. See `Scripts/Editor/README_LevelGeneration.md`. |
| M8 Realization (HISM/actors), culling, cleanup, editor preview | **Build/automation verified; visual checks pending** | Final full-project build succeeded using DLL suffix 8815; 9/9 tests passed. Verified HISM/Actor realization, cull properties, cleanup, rollback and previous-level retention. No rendered culling or authored flock integration check. |
| M9 QA, sample maps, handoff | Partial | API/HANDOFF/TEST_REPORT exist; no biome × preset PIE matrix |

## Earlier validation performed 2026-11-15
| Check | Result |
| --- | --- |
| Code review of all 18 `BalloonLevel/*` files against the spec | Findings below |
| Isolated harness (`Scripts/Verify-BalloonLevelM6.ps1`), 11:54:45 | 8/8 passed; harness copies byte-identical to working tree |
| **Full project** automation `GunsAndBalloons.BalloonLevel.*`, DLL built 11:56:03 | **8/8 passed**: M1–M5, M6.Events, M6.RuntimeSafety, GenerationModes. This is the first full-project evidence. The `FirstPersonCameraPawn.h` merge-marker blocker was removed in `e061a95`. |
| PIE `test_level_generation_pie.py` | Last pass at 11:15 Kyiv: Ready, 8 borders, flight. It predates the generation-modes change and was not re-run. |
| Profiling, visual QA, biome assets | Not done; no native `DT_BalloonAssets_*` tables exist yet |

## Earlier definition-of-done review (superseded for M8 by final verification)
| Requirement | State |
| --- | --- |
| Designers edit length/width/biome/height/seed without C++ | ✅ via `UBalloonLevelConfig` |
| Same seed + config + catalog → same plan; independent streams | ✅ tested (RNG-01/02 equivalents) |
| No vertical maneuver; every accepted level has a horizontal route | ✅ |
| Border radius changes reshape corridor; runtime changes prevalidated | ✅ |
| Obstacles/enemies independently optional; shared + biome rows | ✅ planning; ⚠ enemies cannot pass validation (F1) |
| Row schema/asset/category/rule validation | ✅ |
| Six placement rules | ✅ descriptors; ⚠ `line` usability (F2) |
| One HISM per mesh, NoCollision; colliding actors; flock exception | ❌ M8 |
| Cull distances applied | ❌ M8 (values are only annotated on descriptors) |
| `MI_<Level>_Ground` + biome fog | âŒ M7 |
| Fixed Day/Night | ⚠ preset stored in plan/snapshot, never applied |
| Events via Blueprint, no Day/Night action | ✅ |
| Regeneration never duplicates or destroys unrelated content | ✅ trivially (nothing is spawned) |
| Automated tests for the full matrix | Partial: no INS/ENV/LIF/PERF tests |

## Findings (most severe first)
| # | Severity | Finding | Where |
| --- | --- | --- | --- |
| F1 | High | **Enabling interior enemies or Blueprint obstacles makes generation always fail.** Colliding non-HISM placements need `CollisionBounds`, but `Resolve` computes bounds only for StaticMesh rows. `InteriorEnemy` must be a Hazard Blueprint with `Collision=true`, so any planned enemy fails with "colliding placement lacks certified 3D bounds" on every retry. Today only raw-mesh obstacles can pass. Fix needs a Blueprint collision-bounds resolver (for example, from the class CDO). Needs a human decision because authored `SizeMeters` is deliberately not trusted as collision proof. | `BalloonPlacementPlanner.cpp:46-57,322`; `BalloonHorizontalRouteValidator.cpp:94-96` |
| F2 | High (usability) | **`line` succeeds only with an exact piece count.** Count = `DensityPerKm2 × corridor km²`. Too many pieces fails ("chain count exceeds field borders") and too few fails ("uncovered field border"). Designers would have to back-solve density. Suggest deriving the count from chain length for `line` rows. | `BalloonPlacementPlanner.cpp:244,335` |
| F3 | Resolved by M8 | **Historical: failed (re)generation discarded the last good plan; now retained and regression-tested.** `GenerateLevel` clears first (spec §11.1 says keep the last good plan). With `OnLevelStart` + `RandomizeSeed`, one bad seed leaves the level with no plan. | `BalloonLevelGenerator.cpp` `GenerateLevel` start |
| F4 | Medium | **Nothing places the balloon at the plan start.** The proof starts at `Plan.Start`, not at the balloon. Runtime border changes require balloon Z == `FlightHeightMeters×100` (±0.01 cm) and zero current yaw rate. The preview script syncs the config to the balloon by hand. With a mismatch, generation succeeds but every border event is rejected. A border event that fires while the player is turning is also rejected, and a `Once` event is consumed. | `GenerateLevel`; `BalloonHorizontalRouteValidator.cpp:91-93,175` |
| F5 | Medium | **World sweeps also hit the generator's own border proxies.** The sweep uses all object types and ignores only `RoutePawn` and its riders. On regeneration, old registered proxies still in the world block the new route. A safe shrink can be rejected by the old, larger sphere (the shrink case is documented). | `WorldSweep` |
| F6 | Low | Random biome and `RandomizeSeed` use global `FMath::Rand`. A run is reproducible only from the logged `Generation biome=… seed=…` diagnostic or `ActiveConfig`. | `GenerateLevel` |
| F7 | Low | Rule/biome gating is hard-coded (`ring`=Forest, `line`/`flock`=Field, `street`=City, `wind_aligned`=Desert). Rows for other biomes are skipped **without a diagnostic**, and new biome IDs get only `scatter`. | `BalloonPlacementPlanner.cpp:87-95` |
| F8 | Low | `OnLevelStart` does not start the event timeline. Callers must still call `EventManager->StartEventTimeline()`. | `BeginPlay` |
| F9 | Low (docs) | The new `docs/decisions.md` row is preceded by a blank line, so it renders outside the table. | `docs/decisions.md` last row |

No defects were found in the event ordering/exact-once logic, the catalog merge, seed independence or the border transaction rollback. Those paths match the spec and are covered by tests.

## Earlier next steps (M8 build/automation and F3 now resolved)
1. Decide on F1 (Blueprint collision bounds) and F2 (`line` count) before authoring biome tables.
2. Keep the last good plan on failure (F3), and decide who moves the balloon to `GetStartTransform()` (F4).
3. M7: apply the fixed preset, ground material and fog once at load.
4. M8: realize descriptors (HISM per mesh, colliding actors, `BP_BirdFlock`, border proxies), with owned cleanup and cull distances.
5. Author `DT_BalloonAssets_Shared/Forest/Field/City/Desert`, then run the PIE matrix (4 biomes × Day/Night).
