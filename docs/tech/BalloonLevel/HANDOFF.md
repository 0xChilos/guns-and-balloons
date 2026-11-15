# Balloon level generation handoff

Updated: 2026-11-15. Current milestone: M3. Stop before M4. M1/M2/M3 source implemented. Final clean UE build and M1/M2/M3 automation PASSED (3 tests, exit 0); all 16 source/test copies verified byte-identical. Details and intermediate failed attempts are recorded in TEST_REPORT.md. Full-project integration remains blocked by existing FirstPersonCameraPawn.h merge markers (UHT line 335).

## Build and scope

Actual module/export: gunsandballoons / GUNSANDBALLOONS_API. Engine: UE 5.9.0, installed G:/UE/UE_5.9. Read supplied specification v1.1 section 6 and M3. User requested M3 only. No Pawn movement, native BP assets, pursuit AI, scenery realization or lighting changes. Ready still means descriptors, never a playable-level guarantee. No runtime radius API until M5/M6 safety transactions exist.

## M3 files and APIs

- BalloonLevelTypes.h: reflected FBalloonBorderInfluence, typed Blocked/Damaging/Pushback/VisualOnly policy; plan stores border descriptors, left/right sampled boundaries and envelope snapshot.
- BalloonLevelConfig.h/.cpp: independent BordersEnabled, optional BorderExactCount (-1 density), BorderDensityPerKm, radii, intrusion fraction, spacing, overlap permission, policy, vertical center offset and horizontal/vertical balloon envelope dimensions. All prototype values need tuning; envelope defaults zero for geometry preview, not approved physical dimensions.
- BalloonBorderPlanner.h/.cpp: pure Build, RightAt, ProjectedRadius, CrossSection and UnsafeWidth. Build consumes M2 geometry, creates stationary spheres with deterministic per-item Border/v1 streams, exact alternating side count, variable radii and bounded spacing rejection. Failure leaves no partial border descriptors. Combined-edge density, maximum 10000 count and 5000000 diagnostic pairs, otherwise explicit failure.
- BalloonBorderThreat.h/.cpp: non-ticking Blueprintable sphere proxy, InitializeInfluence before BeginPlay only, GetInfluence, OnInfluenceInitialized and OnBalloonInfluenceEntered extension events. Blocked blocks Pawn/WorldDynamic; Damaging/Pushback overlap, VisualOnly disables collision. Actual balloon primitives use BlockAllDynamic and flight sweeps each primitive. Blueprint effect handler must filter overlaps to intended balloon. No damage numbers/pushback force invented. Generator does not spawn proxy actors yet (M8).
- BalloonLevelGenerator: GenerateLevel integrates border planning; GetBorderInfluences and DrawBorderDebug(Duration). Debug draws sampled boundaries, world spheres, flight-plane projected discs and ID/side/radius/intrusion labels; no tick or global debug flush.
- Tests/BalloonLevelM3Test.cpp and Scripts/Verify-BalloonLevelM3.ps1 provide source-isolated engine compilation and M1/M2/M3 automation.
- API.md documents configuration, units, initialization lifecycle and geometry contracts. DESIGN_DECISIONS.md records M3 choices.

## Geometry and safety

Meters convert once into centimeter descriptors. Spheres intersect the fixed flight envelope Z slab; only the intersecting spherical slice becomes a horizontal influence disc. Horizontal balloon radius inflates this disc conservatively. VisualOnly is excluded from occupancy; every other policy is unsafe. Exact lateral line/disc intersections are clipped to current corridor width and unioned, so overlapping threats cannot double-count unsafe width. Curved borders use the local M2 polyline right vector and exact snapshotted width query.

Per-item stream: uint32(seed) XOR 0x424F5231 XOR (uint32(index)*0x9E3779B9). Radius drawn once per item, longitudinal positions retried at most 128 times; sides alternate left-first. Same geometry/config produces the same descriptors; no catalog or geometry RNG is consumed. Overlap permission is explicit. Intrusion diagnostics are local fixed-height influence/corridor intersection without horizontal balloon inflation; UnsafeWidth accepts inflation explicitly. Sampled closures at geometry stations and threat centers are diagnostics only, not a connectivity proof.

M5 must bind/tune the envelope against the actual multi-primitive balloon and validate world collision sweeps, safe start/finish, turning/inertia and horizontal reachability. Default zero envelope must never be accepted as a physical proof. Radius expansion/animation is intentionally unavailable until current-player-to-finish validation exists. Out-of-bounds consequences remain an existing design open question; M3 invents none.

## Verification and remaining limits

See TEST_REPORT.md for commands, logs, final build/test results and failed intermediate attempts. Harness compiles exact feature copies; it cannot certify project asset integration. No BP subclass asset, visual PIE/debug inspection or profiling is claimed. Existing .claude/ and docs/temp/ untouched. No commit.

Next milestone only when requested: M4 biome catalog and six placement rules, preserving exact row schema and independent RNG streams. Resolve the unrelated Pawn merge conflict for full-project integration. Full route proof remains M5; realization M8.
