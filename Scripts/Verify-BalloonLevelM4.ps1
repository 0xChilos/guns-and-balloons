param([string]$EngineRoot = 'G:\UE\UE_5.9')
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$verifyRoot = Join-Path $projectRoot 'Saved\M4Verification'
New-Item -ItemType Directory -Force "$verifyRoot\Source\gunsandballoons\BalloonLevel", "$verifyRoot\Source\gunsandballoons\Tests" | Out-Null
Copy-Item "$projectRoot\Source\gunsandballoons\BalloonLevel\*.h", "$projectRoot\Source\gunsandballoons\BalloonLevel\*.cpp" "$verifyRoot\Source\gunsandballoons\BalloonLevel"
Copy-Item "$projectRoot\Source\gunsandballoons\Tests\BalloonLevelM*Test.cpp" "$verifyRoot\Source\gunsandballoons\Tests"
Set-Content "$verifyRoot\M4Verification.uproject" '{"FileVersion":3,"EngineAssociation":"5.9","Modules":[{"Name":"gunsandballoons","Type":"Runtime","LoadingPhase":"Default"}]}'
Set-Content "$verifyRoot\Source\M4VerificationEditor.Target.cs" 'using UnrealBuildTool; public class M4VerificationEditorTarget : TargetRules { public M4VerificationEditorTarget(TargetInfo Target) : base(Target) { Type = TargetType.Editor; DefaultBuildSettings = BuildSettingsVersion.Latest; ExtraModuleNames.Add("gunsandballoons"); } }'
Set-Content "$verifyRoot\Source\gunsandballoons\gunsandballoons.Build.cs" 'using UnrealBuildTool; public class gunsandballoons : ModuleRules { public gunsandballoons(ReadOnlyTargetRules Target) : base(Target) { PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs; PublicDependencyModuleNames.AddRange(new string[] {"Core", "CoreUObject", "Engine"}); } }'
Set-Content "$verifyRoot\Source\gunsandballoons\gunsandballoons.cpp" '#include "Modules/ModuleManager.h"
IMPLEMENT_PRIMARY_GAME_MODULE(FDefaultGameModuleImpl, gunsandballoons, "gunsandballoons");'
& "$EngineRoot\Engine\Build\BatchFiles\Build.bat" M4VerificationEditor Win64 Development "$verifyRoot\M4Verification.uproject" -Rebuild -WaitMutex -NoHotReloadFromIDE -NoUBA -nocache "-Log=$projectRoot\Saved\Logs\M4-IsolatedBuild.log"
if ($LASTEXITCODE -ne 0) { throw "Isolated M4 build failed: $LASTEXITCODE" }
& "$EngineRoot\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "$verifyRoot\M4Verification.uproject" -unattended -nop4 -nosplash -NullRHI '-ExecCmds=Automation RunTests GunsAndBalloons.BalloonLevel' '-TestExit=Automation Test Queue Empty' "-ReportExportPath=$verifyRoot\TestReport" "-abslog=$projectRoot\Saved\Logs\M4-Automation.log"
if ($LASTEXITCODE -ne 0) { throw "Isolated M4 automation failed: $LASTEXITCODE" }
$report = Get-Content "$verifyRoot\TestReport\index.json" -Raw | ConvertFrom-Json
if ($report.failed -ne 0 -or $report.notRun -ne 0 -or $report.inProcess -ne 0 -or $report.succeeded -lt 4) {
    throw "M4 automation report failed or incomplete: passed=$($report.succeeded), failed=$($report.failed)"
}
