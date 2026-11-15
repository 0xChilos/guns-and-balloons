param([string]$EngineRoot = 'G:\UE\UE_5.9')
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$verifyRoot = Join-Path $projectRoot 'Saved\M1Verification'
New-Item -ItemType Directory -Force "$verifyRoot\Source\gunsandballoons\BalloonLevel", "$verifyRoot\Source\gunsandballoons\Tests" | Out-Null
Copy-Item "$projectRoot\Source\gunsandballoons\BalloonLevel\*.h", "$projectRoot\Source\gunsandballoons\BalloonLevel\*.cpp" "$verifyRoot\Source\gunsandballoons\BalloonLevel"
Copy-Item "$projectRoot\Source\gunsandballoons\Tests\BalloonLevelM1Test.cpp" "$verifyRoot\Source\gunsandballoons\Tests"
Set-Content "$verifyRoot\M1Verification.uproject" '{"FileVersion":3,"EngineAssociation":"5.9","Modules":[{"Name":"gunsandballoons","Type":"Runtime","LoadingPhase":"Default"}]}'
Set-Content "$verifyRoot\Source\M1VerificationEditor.Target.cs" 'using UnrealBuildTool; public class M1VerificationEditorTarget : TargetRules { public M1VerificationEditorTarget(TargetInfo Target) : base(Target) { Type = TargetType.Editor; DefaultBuildSettings = BuildSettingsVersion.Latest; ExtraModuleNames.Add("gunsandballoons"); } }'
Set-Content "$verifyRoot\Source\gunsandballoons\gunsandballoons.Build.cs" 'using UnrealBuildTool; public class gunsandballoons : ModuleRules { public gunsandballoons(ReadOnlyTargetRules Target) : base(Target) { PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs; PublicDependencyModuleNames.AddRange(new string[] {"Core", "CoreUObject", "Engine"}); } }'
Set-Content "$verifyRoot\Source\gunsandballoons\gunsandballoons.cpp" '#include "Modules/ModuleManager.h"
IMPLEMENT_PRIMARY_GAME_MODULE(FDefaultGameModuleImpl, gunsandballoons, "gunsandballoons");'
& "$EngineRoot\Engine\Build\BatchFiles\Build.bat" M1VerificationEditor Win64 Development "$verifyRoot\M1Verification.uproject" -WaitMutex -NoHotReloadFromIDE "-Log=$projectRoot\Saved\Logs\M1-IsolatedBuild.log"
if ($LASTEXITCODE -ne 0) { throw "Isolated M1 build failed: $LASTEXITCODE" }
& "$EngineRoot\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "$verifyRoot\M1Verification.uproject" -unattended -nop4 -nosplash -NullRHI '-ExecCmds=Automation RunTests GunsAndBalloons.BalloonLevel.M1' '-TestExit=Automation Test Queue Empty' "-ReportExportPath=$verifyRoot\TestReport" "-abslog=$projectRoot\Saved\Logs\M1-Automation.log"
if ($LASTEXITCODE -ne 0) { throw "Isolated M1 automation failed: $LASTEXITCODE" }
