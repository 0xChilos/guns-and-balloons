# M1 verification — 2026-11-15

Engine: UE 5.9.0, changelist 60351404, G:/UE/UE_5.9. Compiler: MSVC 14.44.35229, Windows SDK 10.0.26100.0.

## Full project

Command (PowerShell):

```powershell
& 'G:\UE\UE_5.9\Engine\Build\BatchFiles\Build.bat' gunsandballoonsEditor Win64 Development 'G:\UE\gunsandballoons\gunsandballoons.uproject' -WaitMutex -NoHotReloadFromIDE -Log='G:\UE\gunsandballoons\Saved\Logs\M1-Build.log'
```

Result: FAILED before compiling M1. Existing committed conflict markers in FirstPersonCameraPawn.h (lines 37, 91, 221, 324 and associated separators). UHT first fatal diagnostic: line 335, `Member variable declaration: Expected name`. Unrelated source left untouched. Initial sandbox attempts exited 1 without a diagnostic; the authorized elevated retry produced the actionable build log above.

## Isolated M1 verification

Reproduce from repository root:

```powershell
./Scripts/Verify-BalloonLevelM1.ps1 -EngineRoot G:\UE\UE_5.9
```

The script copies M1 implementation/test sources into an ignored minimal project, builds M1VerificationEditor, then runs real UE automation with NullRHI. Logs: Saved/Logs/M1-IsolatedBuild.log and Saved/Logs/M1-Automation.log. Exported report: Saved/M1Verification/TestReport.

Result: PASSED. Build exited 0 with `Result: Succeeded`; automation exited 0, discovered one test and reported `Test Completed. Result={Success} Name={M1} Path={GunsAndBalloons.BalloonLevel.M1}` followed by `Automation Test Queue Empty 1 tests performed`. File hashes confirmed all seven implementation files plus the test exactly matched the harness copies.

Initial isolated compilation found a test-only UE 5.9 `GWorld` proxy template deduction error. Fixed by assigning `UWorld* World = GWorld` before assertions; the complete script rerun then passed. Startup reported an unrelated default engine MCP listener port 8000 bind conflict; it did not affect the automation result.

Isolation does not prove main project integration or native-schema project asset compatibility. Tests cover reflected field order/count, positive/invalid dimensions and fixed height, exact/partial segment partition, XY/yaw metric geometry, catalog ordering/overrides/shared biomes, row validation and Actor/mesh distinction (including real engine Blueprint asset versus generated class paths), generator status/clear/regenerate lifecycle. No .uasset tables/config assets, placement rules, reachability, visual PIE or performance checks were claimed.

## Diff

Full `git diff --check` finds pre-existing trailing whitespace at docs/decisions.md:10. That user edit was left intact. M1 files are new files, so check them separately as well; ordinary unstaged diff does not include untracked files.

M1 new files each passed `git diff --no-index --check -- NUL <path>` with no whitespace errors. `git diff --check -- docs/README.md` passed. Reviewed all new M1 sources and docs and the docs index link; existing modified biome/decision files retained their baseline diff counts. No commits or unrelated gameplay edits.


# M2 verification - 2026-11-15

Environment: UE 5.9.0 build 60351404 at G:/UE/UE_5.9; MSVC 14.44.35229, Windows SDK 10.0.26100.0. No editor assets or visual PIE coverage claimed.

## Full project

```powershell
& 'G:\UE\UE_5.9\Engine\Build\BatchFiles\Build.bat' gunsandballoonsEditor Win64 Development 'G:\UE\gunsandballoons\gunsandballoons.uproject' -WaitMutex -NoHotReloadFromIDE '-Log=G:\UE\gunsandballoons\Saved\Logs\M2-Build.log'
```

FAILED in UHT at FirstPersonCameraPawn.h:335: `Member variable declaration: Expected name`. Existing committed merge markers remain at lines 37, 91, 221, 324. No unrelated merge resolution attempted.

## Isolated source build and automation

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File Scripts/Verify-BalloonLevelM2.ps1 -EngineRoot G:\UE\UE_5.9
```

Harness builds M2VerificationEditor from copied feature sources and M1/M2 tests, then launches UnrealEditor-Cmd with `-unattended -nop4 -nosplash -NullRHI -ExecCmds="Automation RunTests GunsAndBalloons.BalloonLevel" -TestExit="Automation Test Queue Empty"`. Logs: Saved/Logs/M2-IsolatedBuild.log and M2-Automation.log; report Saved/M2Verification/TestReport.

The initial implementation build exited 0 (`Result: Succeeded`), both M1/M2 tests reported `Result={Success}`, and automation exited 0 with `2 tests performed`. Subsequent review added exact snapshotted rich-curve evaluation, conservative bounds, finite tangent validation, boundary/seed tests, and made the full-search normalized-progress query BlueprintCallable to avoid repeated pure-node scans. Final verification of those changes is recorded below.

Direct script execution was rejected by the host execution policy; the explicit process-local Bypass command above works. Sandboxed UBT stalled without a log; approved execution outside the sandbox reached the actual compiler. Intermediate incremental UBA runs stalled and were terminated before retrying. Harness now passes -NoUBA -nocache (UE 5.9 still labels its local executor UBA). These attempts are not counted as successful final verification.

Coverage: seed reproducibility/independence, metric arc length, fixed Z despite query Z and transform pitch/roll/scale/Z, positive forward projection/no crossings, discrete turn-radius bound, exact/partial arc partitions and boundary ownership, minimum width and between-sample curve evaluation, start/finish descriptors, current/backtracking/monotonic maximum, explicit teleport fallback, generator regeneration reset, invalid heading/radius and existing M1 schema/catalog/lifecycle regression checks. Full 3D collision sweeps, terrain, ribbon overlap and route proof remain outside M2 (M5). No profiling or playable-level claims.


Final result: PASSED. The final harness command exited 0. Build reported `Result: Succeeded` (local actions 88.43s); automation logged both `Result={Success} Name={M1}` and `Result={Success} Name={M2}` and `Automation Test Queue Empty 2 tests performed` at 2026-11-15 07:19:21 UTC. Byte comparison verified all eleven implementation/test files exactly match harness copies. Startup still reports the unrelated engine MCP listener port 8000 bind conflict; tests completed successfully.

Final `git diff --check` passed. All four new code/test/script files also passed `git diff --no-index --check -- NUL <path>` with no whitespace diagnostics (exit 1 is the expected new-file diff status). Reviewed scoped diff and planner/test files; unrelated .claude/ and docs/temp/ remain untouched. No commit. M2 gate passed in isolation; main-project integration is BLOCKED, not claimed successful.


# M3 verification ? 2026-11-15

Scope: stationary border influence geometry, optional exact count/density, variable radii, spacing/overlap policy, fixed-height balloon envelope intersection, Blueprint proxy extension points and debug draw API. No actor realization or full route proof is claimed.

Environment: installed UE 5.9.0 at G:/UE/UE_5.9, VS 14.44.35229, Windows SDK 10.0.26100.0. Full-project command:

```powershell
& G:/UE/UE_5.9/Engine/Build/BatchFiles/Build.bat gunsandballoonsEditor Win64 Development G:/UE/gunsandballoons/gunsandballoons.uproject -WaitMutex -NoHotReloadFromIDE -NoUBA -nocache -Log=G:/UE/gunsandballoons/Saved/Logs/M3-ProjectBuild.log
```

Result: FAILED, existing FirstPersonCameraPawn.h:335 UHT `Member variable declaration: Expected name`; existing committed merge markers remain at lines 37/91/221/324. No unrelated repair attempted. Full integration is blocked.

Isolated verification:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File Scripts/Verify-BalloonLevelM3.ps1 -EngineRoot G:/UE/UE_5.9
```

Harness copies exact feature source/test files, performs a clean `M3VerificationEditor Win64 Development -Rebuild -NoUBA -nocache` build, then runs UnrealEditor-Cmd with NullRHI, `Automation RunTests GunsAndBalloons.BalloonLevel` and `TestExit=Automation Test Queue Empty`. Final logs: Saved/Logs/M3-IsolatedBuild.log, M3-Automation.log. JSON report: Saved/M3Verification/TestReport/index.json. SHA256 comparisons verified all 16 feature/test files byte-identical to harness copies. No native Blueprint asset, visual PIE inspection or profiling claim.

Intermediate attempts: direct script invocation was rejected by host execution policy; process-local Bypass works. Sandboxed UBT stalled before log creation and was stopped; approved engine/toolchain access reached compiler. First isolated build and original 3 tests passed. After header/config refinements, incremental build succeeded but one editor process exited during test discovery (-1073740791) and another at M1 startup (exit 1), without a test-result failure or useful fatal stack in the automation log. Logs preserved as M3-Automation-StartupFailure.log and M3-Automation-IncrementalFailure.log. A full clean build (M3-CleanBuild.log, 65.50s local actions) followed by automation passed all three tests at 07:30:35 UTC. Cause of incremental process exits is not established; harness now forces clean rebuild. Final run additionally verifies the actual balloon WorldDynamic collision response.

Coverage: existing M1/M2 regressions; exact odd count and side balance, disabled/exact-zero/density modes, variable radius bounds, repeat seed and independent catalog/count streams, exact clipped intrusion, overlap union without double counting, radius expansion reducing horizontal safe width, balloon horizontal inflation, fixed-height slab/sphere overlap and vertical misses, VisualOnly exclusion, curved variable-width border placement, geometry independence, bounded impossible spacing and disallowed overlap failures, proxy initialization and four collision policies including actual balloon WorldDynamic response. Debug rendering is compiled but not visually inspected. Actual multi-primitive world sweeps, envelope tuning and connected route proof remain M5.

Final result: PASSED. Final clean build reported Result: Succeeded (58.28s local actions); automation exited 0 and logged Result={Success} for M1, M2 and M3, with 3 tests performed at 2026-11-15 07:32:23 UTC. The final run includes the actual balloon WorldDynamic collision-channel regression. All 16 implementation/test files match harness copies. git diff --check and checks of all new source/script/API files reported no whitespace errors. M3 gate passes in isolation; full project integration remains BLOCKED by the unrelated Pawn conflict. Stop before M4.

# M4 verification — 2026-11-15

Scope: deterministic biome placement descriptors for scatter, ring, line, street, wind_aligned and flock; exact existing row schema/biome overlay; independent optional obstacle/enemy totals; metadata, density, clusters, spacing, scale, yaw, surface normals, Actor/HISM representation and cull annotations. No world realization or route proof.

Full-project build was rerun with UE 5.9:

```powershell
& G:/UE/UE_5.9/Engine/Build/BatchFiles/Build.bat gunsandballoonsEditor Win64 Development G:/UE/gunsandballoons/gunsandballoons.uproject -WaitMutex -NoHotReloadFromIDE -NoUBA -nocache -Log=G:/UE/gunsandballoons/Saved/Logs/M4-ProjectBuild.log
```

FAILED (exit 1): existing FirstPersonCameraPawn.h:335 UHT `Member variable declaration: Expected name`; merge markers remain at 37/91/221/324. No unrelated edits. Full-project integration remains blocked.

Isolated command:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File Scripts/Verify-BalloonLevelM4.ps1 -EngineRoot G:/UE/UE_5.9
```

Final result: PASSED. Latest clean build reported `Result: Succeeded` (105.83s local actions). Final incremental compilation of the exact-count/coverage refinements succeeded (7.58s, exit 0), followed by all four M1/M2/M3/M4 tests passing at 2026-11-15 10:16:07 Europe/Kyiv (08:16:07 UTC). Final JSON: Saved/M4Verification/FinalTestReport/index.json, succeeded=4, failed=0, notRun=0, inProcess=0. Process and report-check command exited 0. Final logs: Saved/Logs/M4-FinalBuild.log and M4-FinalAutomation.log; clean harness logs remain M4-IsolatedBuild.log and M4-Automation.log. All 19 final implementation/test files match the harness copies byte-for-byte. No source changes after this final test run. Whitespace/diff review passed for tracked and new files.
Coverage: all six rules replay deterministically; scatter density on 0.9km², cluster radius/group IDs, scale and horizontal spacing; ground height/normal alignment; Forest ring annulus, inward heading, overlap exemption, no-cull and uncovered-horizon error; Field two-border chains, supplied polyline and overlapping final short remainder, insufficient-count error; City cardinal yaw, rear-touching building pairs, cross-row lot exclusions and street-edge lamps; Desert shared wind yaw ±10°; global 3–6 Field Actor flocks across multiple rows, ClusterSize not multiplying actors, other-biome skip; independent obstacle/enemy exact totals replacing even oversized density-derived counts, disabled obstacles leaving enemies stable, exact zero, missing eligible rows, bounded impossible spacing, no partial output, colliding Actor representation, same-mesh HISM cull conflicts and invalid street dimensions. Existing M1 regression verifies all twelve reflected columns, shared/biome whole-row override, lexical order, shared rows in all biomes, asset/category/class/rule/schema and numeric row errors. M2/M3 regressions remain green.

Intermediate evidence: sandboxed engine/toolchain startup exited -532462766 before compilation; approved engine access succeeded. Initial clean build compiled, but M4 assertions failed on an oversized ring-footprint fixture and a negative-coordinate lamp modulo assertion (M1–M3 passed). Unreal exited 0 despite that test failure. Fixtures/assertions were corrected, and the harness now checks the JSON report for failures/incomplete coverage in addition to process exit. A subsequent all-four-tests pass is retained in M4-Automation-IntermediatePass.log; final tests additionally include expanded polyline, back-to-back, cull-conflict and replay checks. First test-build numeric-conversion warnings were removed with explicit float literals. One intermediate incremental build retained an old planner object because Copy-Item preserved a source timestamp older than the object; the new high-density/exact-count regression exposed this (M4-TimestampStaleAutomation.log). Touching the harness planner source forced recompilation; the final run above passes. The default verification script always uses a clean rebuild. Existing MCP port 8000 startup bind conflict does not prevent automation completion.

Limitations: content-free harness validates resolved flock Actor descriptors using synthetic class-kind inputs; it does not load the project's BP_BirdFlock or native biome tables. Actual flock-class validation remains in the real catalog resolver, including generated-class ancestry checking; positive project asset integration is unverified. Terrain tests inject a pure sampler; generator uses a flat preview surface. No PIE, profiling, actual component culling/collision or fixed-height route safety is claimed. M4 gate passes in isolation; stop before M5.
