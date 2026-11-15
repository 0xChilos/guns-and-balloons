# Prompt 7: Rebuild after the run-flow changes

Rebuild the Unreal project "guns-and-balloons" after C++ changes. Do only the steps below. Don't change any code
unless the build fails in one of the files listed under "Changed files", and if it does, report the error before
fixing it.

## Paths
- Engine: `E:\Unreal\UE_5.9`
- Project: `D:\test_git\guns-and-balloons\gunsandballoons.uproject`
- Editor target: `gunsandballoonsEditor` (Win64, Development)

## Why a full rebuild is needed
The changes add new UCLASS/UPROPERTY/UFUNCTION members (`UVictoryWidget`, new `UBalloonRunSubsystem` functions
and properties, `UBalloonRunWorldHook::HandleCollected`). They also delete `MainMenuLevelSetup.cpp` and
`Tests/MainMenuLevelSetupTest.cpp`. Live Coding can't apply changes like these, so the editor has to be closed.

## Changed files (`Source/gunsandballoons/`)
- `BalloonRunSubsystem.h/.cpp`
- `BalloonLevel/BalloonLevelGenerator.h/.cpp`
- `EngineHealthComponent.h/.cpp`
- `MainMenuWidget.h/.cpp`
- `LevelShopScreen.h/.cpp`
- `VictoryWidget.h/.cpp` (new)
- Deleted: `MainMenuLevelSetup.cpp`, `Tests/MainMenuLevelSetupTest.cpp`

## Steps
1. Check whether `UnrealEditor.exe` and `LiveCodingConsole.exe` are running. If the editor is open, ask me to save
   and close it myself. Don't kill it, and don't continue until both processes are gone.
2. Build the editor target:
   ```
   & "E:\Unreal\UE_5.9\Engine\Build\BatchFiles\Build.bat" gunsandballoonsEditor Win64 Development "-Project=D:\test_git\guns-and-balloons\gunsandballoons.uproject" -WaitMutex
   ```
   If you see "unresolved external" or stale-generated-header errors that point at the deleted files, run it
   again once with `-Clean` added, then without it.
3. Run the automation tests headless and report each result:
   ```
   & "E:\Unreal\UE_5.9\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\test_git\guns-and-balloons\gunsandballoons.uproject" -ExecCmds="Automation RunTests GunsAndBalloons.Run+GunsAndBalloons.BalloonLevel; Quit" -unattended -nopause -nosplash -nullrhi -log
   ```
   Read the results from `Saved/Logs/gunsandballoons.log`.
4. Start the editor again (`UnrealEditor.exe` with the .uproject) and tell me when it's up.

## Known issue (don't fix it)
The standalone game target (`gunsandballoons`, not the Editor target) fails in
`BalloonLevel/BalloonEnvironmentPresetController.cpp:161` because `ADirectionalLight::GetComponent` is
editor-only. That's not part of this change. Mention it only if the Editor build fails there too.

## Report
- Build result
- Test pass/fail list with the messages from any failures
- Any warnings in the changed files

Then list this manual PIE check for me to do (don't do it yourself):
1. MainMenu → New Run: a Forest level opens right away, with no setup page.
2. Hook a fuel pickup and an ammo pickup: the bucket and crate counts go up (max 6 per fuel, 60 rounds).
3. Fly into the finish zone: the Shop opens and gold has gone up by 30 plus the hooked money.
4. Continue the travel: the next level is City, Desert or Field.
5. Let the engine be destroyed: after the delay the Shop opens again with the resources from the last
   checkpoint, and Continue replays the same biome.
6. Complete 4 levels: a VICTORY screen appears instead of the Shop, and MAIN MENU goes back to the main menu.
