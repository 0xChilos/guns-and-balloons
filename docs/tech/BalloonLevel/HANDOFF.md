# Balloon level generation handoff

Updated: 2026-11-15. Current milestone: M2. Stop before M3. M1/M2 source implemented; final isolated UE build and both automation tests PASSED (2 tests). Eleven source/test copies verified byte-identical. See TEST_REPORT.md for commands and evidence. Full project integration remains blocked by existing FirstPersonCameraPawn.h merge markers (UHT line 335).

## Build and scope

Actual module/export: gunsandballoons / GUNSANDBALLOONS_API. Engine: UE 5.9.0, changelist 60351404, G:/UE/UE_5.9. Read specification v1.1 sections 5, 8 and M2. User requested M2 only. No Pawn movement, assets, collision configuration, hazards, route proof or realization changed. Fixed world Z is an authoring datum; queries use XY regardless of query Z. Ready means descriptors only, never a playable-level guarantee.

## Files and public APIs

- Source/gunsandballoons/BalloonLevel/BalloonSplinePlanner.h/.cpp: pure Build, PositionAt, WidthAt, SegmentAt, NearestDistance and per-player FBalloonLevelProgress::Update.
- BalloonLevelConfig.h/.cpp: MaxHeadingDegrees, MinimumTurningRadiusMeters, GeometrySampleMeters, MinWidthMeters, WidthProfile (FRuntimeFloatCurve), validation and allocation limits.
- BalloonLevelTypes.h: cumulative centimeter sample distances, sampled widths, immutable rich-curve snapshot, length and horizontal Start/Finish transforms.
- BalloonLevelGenerator.h/.cpp: GenerateLevel now builds M2; Blueprint GetNormalizedProgress (stateless full search), UpdateProgress(WorldPosition, Teleported), GetCurrentProgressCm, GetMaxReachedProgressCm, GetStartTransform, GetFinishTransform, GetWidthAtDistanceCm. GetPlan remains const C++ API.
- Tests/BalloonLevelM2Test.cpp; M1Test retains original straight-plan checks and adjusts generator assertions for dense M2 sampling.
- Scripts/Verify-BalloonLevelM2.ps1 copies exact feature sources and both tests to ignored Saved/M2Verification, builds and runs GunsAndBalloons.BalloonLevel with NullRHI.

## Geometry and progress contract

Geometry/v1 seed is uint32(Seed) XOR 0x47454F31, independent of other subsystems. Smooth sinusoidal heading field sampled into the canonical linear USplineComponent. Exact chord arc length equals requested LengthMeters; segments partition cumulative arc distance, with [start,end) ownership except final inclusive. First edge matches StartTransform yaw. Pitch, roll, scale and transform Z never alter authored dimensions/flight height.

Max heading is limited to 45 degrees about the start heading; strict positive forward projection rules out centerline crossings and hairpins by construction. Heading derivative is bounded by authored minimum radius; sampling step is at most radius/20. This is a sampled smooth curve, not a continuously differentiable UE cubic spline; discrete turn bounds are tested. No rejection retries/fallback needed within this restricted path family. Ribbon overlap, actual collider clearance, terrain intersection, inertia/wind safety and start/finish clearance belong to M5, not M2. No collision safety claim.

WidthAt(s) evaluates max(MinWidth, BaseWidth * snapshotted WidthProfile(s/Length)); empty curve means multiplier 1. SampleWidths are cached descriptors, not the authoritative width query. Bounds use a conservative global curve control-hull/extrapolation allowance, deliberately broader than individual segment widths. External curve edits after generation do not mutate the plan. All prototype geometry numbers require tuning with the actual balloon envelope.

Call UpdateProgress for the balloon explicitly; the generator does not tick or take control of a Pawn. Each tracker stores CurrentS, monotonic MaxReachedS and previous edge. Normal updates search the previous logical segment and neighbors; pass Teleported=true for a global search after teleport/large discontinuity. First update searches globally. Current progress can decrease on backtracking; maximum cannot. Clear and regenerate reset generator progress; independent C++ trackers must be reset by their owner. No event triggers or finish completion side effects until later milestones.

## Verification and limits

Commands/results are in TEST_REPORT.md. Logs: Saved/Logs/M2-Build.log, M2-IsolatedBuild.log, M2-Automation.log; report Saved/M2Verification/TestReport. Isolation compiles exact source copies against UE but cannot certify project asset integration. No native config/BP assets or visual PIE checks were authored or claimed. Existing .claude/ and docs/temp/ untracked content left intact. No commits.

Next milestone, only when requested: M3 border influence geometry, after reading its specification and resolving the full-project integration blocker. Preserve exact asset row schema, deterministic stream separation, fixed flight Z, fixed lighting and plan -> validate -> realize architecture. Actual horizontal route proof is required in M5 before any playable generation claim.
