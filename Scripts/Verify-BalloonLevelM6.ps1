param([string]$EngineRoot = 'G:\UE\UE_5.9', [switch]$Incremental)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$verifyRoot = Join-Path $projectRoot 'Saved\M6Verification'
New-Item -ItemType Directory -Force "$verifyRoot\Source\gunsandballoons\BalloonLevel", "$verifyRoot\Source\gunsandballoons\Tests" | Out-Null
Copy-Item "$projectRoot\Source\gunsandballoons\BalloonLevel\*.h", "$projectRoot\Source\gunsandballoons\BalloonLevel\*.cpp" "$verifyRoot\Source\gunsandballoons\BalloonLevel"
Copy-Item "$projectRoot\Source\gunsandballoons\Tests\BalloonLevelM*Test.cpp" "$verifyRoot\Source\gunsandballoons\Tests"
Copy-Item "$projectRoot\Source\gunsandballoons\Tests\BalloonEventTestObserver.h" "$verifyRoot\Source\gunsandballoons\Tests"
Copy-Item "$projectRoot\Source\gunsandballoons\BalloonFlightComponent.h", "$projectRoot\Source\gunsandballoons\BalloonFlightComponent.cpp" "$verifyRoot\Source\gunsandballoons"
Set-Content "$verifyRoot\M6Verification.uproject" '{"FileVersion":3,"EngineAssociation":"5.9","Modules":[{"Name":"gunsandballoons","Type":"Runtime","LoadingPhase":"Default"}]}'
Set-Content "$verifyRoot\Source\M6VerificationEditor.Target.cs" 'using UnrealBuildTool; public class M6VerificationEditorTarget : TargetRules { public M6VerificationEditorTarget(TargetInfo Target) : base(Target) { Type = TargetType.Editor; DefaultBuildSettings = BuildSettingsVersion.Latest; ExtraModuleNames.Add("gunsandballoons"); } }'
Set-Content "$verifyRoot\Source\gunsandballoons\gunsandballoons.Build.cs" 'using UnrealBuildTool; public class gunsandballoons : ModuleRules { public gunsandballoons(ReadOnlyTargetRules Target) : base(Target) { PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs; PublicDependencyModuleNames.AddRange(new string[] {"Core", "CoreUObject", "Engine"}); } }'
Set-Content "$verifyRoot\Source\gunsandballoons\gunsandballoons.cpp" '#include "Modules/ModuleManager.h"
IMPLEMENT_PRIMARY_GAME_MODULE(FDefaultGameModuleImpl, gunsandballoons, "gunsandballoons");'
$buildArgs = @('M6VerificationEditor', 'Win64', 'Development', "$verifyRoot\M6Verification.uproject", '-WaitMutex', '-NoHotReloadFromIDE', '-NoUBA', '-nocache', "-Log=$projectRoot\Saved\Logs\M6-IsolatedBuild.log")
if (-not $Incremental) { $buildArgs += '-Rebuild' }
# Force compiler visibility after Copy-Item preserves original source timestamps.
Get-ChildItem "$verifyRoot\Source" -Recurse -File | ForEach-Object { $_.LastWriteTime = Get-Date }
& "$EngineRoot\Engine\Build\BatchFiles\Build.bat" @buildArgs
if ($LASTEXITCODE -ne 0) { throw "Isolated M6 build failed: $LASTEXITCODE" }
& "$EngineRoot\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "$verifyRoot\M6Verification.uproject" -unattended -nop4 -nosplash -NullRHI '-ExecCmds=Automation RunTests GunsAndBalloons.BalloonLevel' '-TestExit=Automation Test Queue Empty' "-ReportExportPath=$verifyRoot\TestReport" "-abslog=$projectRoot\Saved\Logs\M6-Automation.log"
if ($LASTEXITCODE -ne 0) { throw "Isolated M6 automation failed: $LASTEXITCODE" }
$report = Get-Content "$verifyRoot\TestReport\index.json" -Raw | ConvertFrom-Json
if ($report.failed -ne 0 -or $report.notRun -ne 0 -or $report.inProcess -ne 0 -or $report.succeeded -lt 7) {
    throw "M6 automation report failed or incomplete: passed=$($report.succeeded), failed=$($report.failed)"
}


