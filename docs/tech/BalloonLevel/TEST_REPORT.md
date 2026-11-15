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
