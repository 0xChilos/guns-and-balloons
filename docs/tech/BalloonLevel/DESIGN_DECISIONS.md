# Implementation decisions

| ID | Date | Decision | Reason / tradeoff |
| --- | --- | --- | --- |
| M1-01 | 2026-11-15 | Use actual project UE 5.9 (installed build 60351404), module gunsandballoons. | Specification's 5.8 target yields to observed repository. |
| M1-02 | 2026-11-15 | Native schema/catalog separate from existing S_LevelAsset tables. | Existing tables include Creature and Blueprint scenery incompatible with the exact contract; no silent conversion or asset rewrites. |
| M1-03 | 2026-11-15 | Null catalogs allowed for geometry-only authoring; validate all rows before whole-row overlay. | Empty preview is useful; invalid overridden rows remain visible authoring errors. |
| M1-04 | 2026-11-15 | M1 ends at Ready, never Complete. | Neither reachability nor realization exists yet. Explicit GenerateLevel; no automatic BeginPlay generation or Pawn changes. |
| M1-05 | 2026-11-15 | Config StartTransform supplies absolute XY/yaw; FlightHeightMeters supplies absolute world Z. | Ignore transform pitch/roll/scale/Z so they cannot introduce vertical steering or distort metric dimensions. |
| M1-06 | 2026-11-15 | Width default 600m from current biome design; length 1500m, target 250m and height 100m are editable prototype values requiring tuning. | Preserve approved width; do not present specification examples as settled TBD design. |
| M1-07 | 2026-11-15 | At most 100000 logical segments; fail rather than clamp. | Bound preview allocation for extreme finite inputs. Curvature/width profiles and other subsystem settings arrive with their owning milestones, not placeholder assets. |
| M1-08 | 2026-11-15 | Isolate M1 build/tests in Saved/M1Verification while full project UHT is blocked. | FirstPersonCameraPawn.h has existing committed conflict markers. Harness compiles copies of exact M1 sources and engine tests, but cannot establish full project integration. |

| M2-01 | 2026-11-15 | Geometry/v1 independent stream; bounded smooth heading field sampled to a canonical metric polyline. | Strict forward projection proves no self crossings, avoiding retry/fallback and hairpins. Discrete turn-radius bound, not a continuous cubic spline claim. |
| M2-02 | 2026-11-15 | Snapshot rich width curve in pure plan; evaluate exact normalized profile and clamp minimum. | Later asset edits cannot mutate an existing plan; broad conservative bounds include narrow width peaks. |
| M2-03 | 2026-11-15 | Explicit per-player progression with current and monotonic maximum; local logical-segment search with explicit teleport full-search flag. | No Pawn coupling or per-frame generator tick; reset on regeneration, leave one-shot event dispatch to its milestone. |
| M2-04 | 2026-11-15 | Prototype heading 25 degrees, radius 100m, sample 10m, minimum width 40m are editable tuning parameters. | Specification does not settle gameplay defaults; actual collision/turnability proof deferred to M5. |
| M2-05 | 2026-11-15 | Retain BuildStraightPlan as M1 compatibility helper; generator exclusively uses M2 Build. | Preserve existing callers/tests without implementing further milestones. Full integration remains blocked by unrelated committed Pawn conflicts. |

| M3-01 | 2026-11-15 | Stationary sphere threats, projected onto the fixed flight envelope Z slab and inflated horizontally. | Sphere is a section 6 permitted shape; conservative cylindrical balloon envelope has editable radius/half-height, both TBD until actual Pawn tuning in M5. No vertical routing. |
| M3-02 | 2026-11-15 | Border enable/count are independent of interior enemies (M4). -1 exact count selects density per combined edge kilometer; odd count gives left one extra. | Exact zero means zero. Stable per-item Border/v1 streams, 128 bounded spacing attempts, failure clears partial border list. Overlap allowed only by explicit config. |
| M3-03 | 2026-11-15 | Blueprint proxy initializes only before BeginPlay; no runtime radius setter. | M5/M6 own route-safe runtime transactions; M3 cannot safely authorize expansion. Actors are not automatically realized before M8. Damage/pushback are Blueprint extension events, not invented combat rules. |
| M3-04 | 2026-11-15 | Unsafe width is union of exact line/disc intervals clipped to each queried cross section. Sampled closure is a diagnostic, not route acceptance. | Handles opposite-side/overlapping threats and curved local borders; does not claim a connected safe route or physical sweep proof. Out-of-bounds gameplay remains a design open question. |
