param([string]$EngineRoot = 'G:\UE\UE_5.9', [switch]$Incremental)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$verifyRoot = Join-Path $projectRoot 'Saved\M8Verification'
New-Item -ItemType Directory -Force "$verifyRoot\Source\gunsandballoons\BalloonLevel", "$verifyRoot\Source\gunsandballoons\Tests" | Out-Null
# Socket realization uses the real resource meshes, materials, sounds and Niagara asset.
if (-not (Test-Path "$verifyRoot\Content")) {
    New-Item -ItemType Junction -Path "$verifyRoot\Content" -Target "$projectRoot\Content" | Out-Null
}
Copy-Item "$projectRoot\Source\gunsandballoons\BalloonLevel\*.h", "$projectRoot\Source\gunsandballoons\BalloonLevel\*.cpp" "$verifyRoot\Source\gunsandballoons\BalloonLevel"
Copy-Item "$projectRoot\Source\gunsandballoons\Tests\BalloonLevel*Test.cpp" "$verifyRoot\Source\gunsandballoons\Tests"
Copy-Item "$projectRoot\Source\gunsandballoons\Tests\BalloonEventTestObserver.h" "$verifyRoot\Source\gunsandballoons\Tests"
Copy-Item "$projectRoot\Source\gunsandballoons\BalloonFlightComponent.h", "$projectRoot\Source\gunsandballoons\BalloonFlightComponent.cpp" "$verifyRoot\Source\gunsandballoons"
Copy-Item "$projectRoot\Source\gunsandballoons\HookPickup.h", "$projectRoot\Source\gunsandballoons\HookPickup.cpp", "$projectRoot\Source\gunsandballoons\ResourceType.h" "$verifyRoot\Source\gunsandballoons"
Set-Content "$verifyRoot\M8Verification.uproject" '{"FileVersion":3,"EngineAssociation":"5.9","Modules":[{"Name":"gunsandballoons","Type":"Runtime","LoadingPhase":"Default"}],"Plugins":[{"Name":"ProceduralMeshComponent","Enabled":true}]}'
Set-Content "$verifyRoot\Source\M8VerificationEditor.Target.cs" 'using UnrealBuildTool; public class M8VerificationEditorTarget : TargetRules { public M8VerificationEditorTarget(TargetInfo Target) : base(Target) { Type = TargetType.Editor; DefaultBuildSettings = BuildSettingsVersion.Latest; ExtraModuleNames.Add("gunsandballoons"); } }'
Set-Content "$verifyRoot\Source\gunsandballoons\gunsandballoons.Build.cs" 'using UnrealBuildTool; public class gunsandballoons : ModuleRules { public gunsandballoons(ReadOnlyTargetRules Target) : base(Target) { PCHUsage = PCHUsageMode.NoPCHs; PublicDependencyModuleNames.AddRange(new string[] {"Core", "CoreUObject", "Engine", "ProceduralMeshComponent", "Niagara"}); } }'
Set-Content "$verifyRoot\Source\gunsandballoons\gunsandballoons.cpp" '#include "Modules/ModuleManager.h"
IMPLEMENT_PRIMARY_GAME_MODULE(FDefaultGameModuleImpl, gunsandballoons, "gunsandballoons");'
$buildArgs = @('M8VerificationEditor', 'Win64', 'Development', "$verifyRoot\M8Verification.uproject", '-WaitMutex', '-NoHotReloadFromIDE', '-NoUBA', '-nocache', "-Log=$projectRoot\Saved\Logs\M8-IsolatedBuild.log")
if (-not $Incremental) { $buildArgs += '-Rebuild' }
# Force compiler visibility after Copy-Item preserves original source timestamps.
Get-ChildItem "$verifyRoot\Source" -Recurse -File | ForEach-Object { $_.LastWriteTime = Get-Date }
& "$EngineRoot\Engine\Build\BatchFiles\Build.bat" @buildArgs
if ($LASTEXITCODE -ne 0) { throw "Isolated M8 build failed: $LASTEXITCODE" }
& "$EngineRoot\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "$verifyRoot\M8Verification.uproject" /Engine/Maps/Entry -unattended -NoSound -nop4 -nosplash -NullRHI '-ExecCmds=Automation RunTests GunsAndBalloons.BalloonLevel' '-TestExit=Automation Test Queue Empty' "-ReportExportPath=$verifyRoot\TestReport" "-abslog=$projectRoot\Saved\Logs\M8-Automation.log"
if ($LASTEXITCODE -ne 0) { throw "Isolated M8 automation failed: $LASTEXITCODE" }
$report = Get-Content "$verifyRoot\TestReport\index.json" -Raw | ConvertFrom-Json
$passed = $report.succeeded + $report.succeededWithWarnings
if ($report.failed -ne 0 -or $report.notRun -ne 0 -or $report.inProcess -ne 0 -or $passed -lt 10) {
    throw "M8 automation report failed or incomplete: passed=$passed, failed=$($report.failed)"
}


