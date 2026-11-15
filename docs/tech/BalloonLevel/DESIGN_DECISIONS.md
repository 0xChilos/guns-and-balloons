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
