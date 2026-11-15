#include "BalloonLevelStartOptions.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/PackageName.h"

bool UBalloonLevelLaunchSubsystem::StartLevel(const FBalloonLevelStartOptions& InOptions, TSoftObjectPtr<UWorld> Level, FText& OutError)
{
 OutError = FText::GetEmpty();
 TArray<FString> Errors;
 if (!InOptions.Validate(Errors)) { OutError = FText::FromString(Errors[0]); return false; }
 UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
 if (!World || !World->IsGameWorld())
 { OutError = NSLOCTEXT("LevelStart", "NoWorld", "Start Level requires a running game."); return false; }
 if (Level.IsNull()) Level = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/Scenes/MainLevel.MainLevel")));
 if (!FPackageName::DoesPackageExist(Level.ToSoftObjectPath().GetLongPackageName()))
 { OutError = NSLOCTEXT("LevelStart", "MissingMap", "The selected level is unavailable."); return false; }
 // All validation precedes the one-shot handoff so rejected requests leave a queued launch intact.
 if (!QueueLevel(InOptions)) return false;
 UGameplayStatics::OpenLevelBySoftObjectPtr(World, Level);
 return true;
}

bool FBalloonLevelStartOptions::Validate(TArray<FString>& Errors) const
{
 const int32 Before = Errors.Num();
 if (Biome != TEXT("Forest") && Biome != TEXT("Field") && Biome != TEXT("City") && Biome != TEXT("Desert"))
  Errors.Add(TEXT("Start options: choose Forest, Field, City or Desert"));
 if (Cycle != EBalloonLightingPreset::Day && Cycle != EBalloonLightingPreset::Night)
  Errors.Add(TEXT("Start options: Cycle must be Day or Night"));
 if (!FMath::IsFinite(FlightHeight) || !FMath::IsFinite(LevelWidth) || LevelWidth < 1.f ||
     !FMath::IsFinite(DistanceToNemesis) || DistanceToNemesis < 0.f)
  Errors.Add(TEXT("Start options: finite flight height, width >= 1m and nemesis distance >= 0m required"));
 for (float Value : {EnemyDensity, ObstacleDensity, FogDensity})
  if (!FMath::IsFinite(Value) || Value < 0.f || Value > 10.f)
  { Errors.Add(TEXT("Start options: densities must be finite and in [0,10]")); break; }
 return Errors.Num() == Before;
}

bool UBalloonLevelLaunchSubsystem::QueueLevel(const FBalloonLevelStartOptions& InOptions)
{
 TArray<FString> Errors;
 if (!InOptions.Validate(Errors)) return false;
 Options = InOptions; Pending = true; return true;
}

bool UBalloonLevelLaunchSubsystem::Consume(FBalloonLevelStartOptions& OutOptions)
{
 if (!Pending) return false;
 OutOptions = Options; Pending = false; return true;
}
