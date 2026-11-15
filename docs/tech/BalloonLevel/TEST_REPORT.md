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
