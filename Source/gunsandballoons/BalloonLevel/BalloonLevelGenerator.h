#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BalloonLevelConfig.h"
#include "BalloonSplinePlanner.h"
#include "BalloonLevelEventManager.h"
#include "BalloonEnvironmentPresetController.h"
#include "BalloonLevelGenerator.generated.h"

UENUM(BlueprintType)
enum class EBalloonGenerationMode : uint8
{
 Manual UMETA(DisplayName="Manual (Button)"),
 OnLevelStart UMETA(DisplayName="Every Level Start")
};

UENUM(BlueprintType)
enum class EBalloonBiomeSelection : uint8
{
 Selected UMETA(DisplayName="Selected Config"),
 Random UMETA(DisplayName="Random")
};

class UBalloonLevelRealizer;
class USplineComponent;
class ABalloonBorderThreat;
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FBalloonBorderChanged);

UCLASS(Blueprintable)
class GUNSANDBALLOONS_API ABalloonLevelGenerator : public AActor
{
	GENERATED_BODY()
public:
	ABalloonLevelGenerator();
 /** One authoring entry point: reuse assigned/single existing actors or create missing environment actors. */
 UFUNCTION(BlueprintCallable, CallInEditor, Category="Balloon Level|Controller") void SetupLevelController();
 UFUNCTION(CallInEditor, Category="Balloon Level|Controller") void PreviewEnvironment();
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Balloon Level|Controller") bool ManageEnvironment = true;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Balloon Level|Controller") TObjectPtr<ABalloonEnvironmentPresetController> EnvironmentController;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Balloon Level|Controller") TObjectPtr<ADirectionalLight> Sun;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Balloon Level|Controller") TObjectPtr<ASkyLight> Sky;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Balloon Level|Controller") TObjectPtr<AExponentialHeightFog> Fog;
 /** Optional authored ground, used only with WorldCollision terrain. */
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Balloon Level|Controller") TObjectPtr<AActor> AuthoredGround;
 /** Overrides apply to the generation copy, never to the source config asset. */
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Balloon Level|Height") bool OverrideHeights = false;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Balloon Level|Height", meta=(EditCondition="OverrideHeights", Units="m")) float FlightHeightMeters = 100.f;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Balloon Level|Height", meta=(EditCondition="OverrideHeights", Units="m")) float GroundHeightMeters = 0.f;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Balloon Level|Terrain") bool OverrideTerrain = false;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Balloon Level|Terrain", meta=(EditCondition="OverrideTerrain")) EBalloonTerrainMode TerrainMode = EBalloonTerrainMode::Generated;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Balloon Level|Terrain", meta=(EditCondition="OverrideTerrain", ClampMin="0", Units="m")) float TerrainHeightVariationMeters = 15.f;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Balloon Level|Terrain", meta=(EditCondition="OverrideTerrain", ClampMin="1", Units="m")) float TerrainWavelengthMeters = 200.f;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Balloon Level|Terrain", meta=(EditCondition="OverrideTerrain", ClampMin="1", Units="m")) float TerrainCellMeters = 20.f;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Balloon Level|Terrain", meta=(EditCondition="OverrideTerrain", ClampMin="0", Units="m")) float TerrainPaddingMeters = 100.f;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Balloon Level|Terrain", meta=(EditCondition="OverrideTerrain")) TSoftObjectPtr<UMaterialInterface> TerrainMaterial;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Balloon Level|Environment") bool OverrideLightingPreset = false;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Balloon Level|Environment", meta=(EditCondition="OverrideLightingPreset")) EBalloonLightingPreset LightingPreset = EBalloonLightingPreset::Day;
 /** Enable to tune the environment here instead of on a legacy environment actor. */
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Balloon Level|Environment") bool OverrideEnvironmentSettings = false;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Balloon Level|Environment", meta=(EditCondition="OverrideEnvironmentSettings")) FBalloonLightingPresetValues Day;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Balloon Level|Environment", meta=(EditCondition="OverrideEnvironmentSettings")) FBalloonLightingPresetValues Night;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Balloon Level|Environment", meta=(EditCondition="OverrideEnvironmentSettings")) TMap<FName, FBalloonFogPreset> BiomeFog;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Balloon Level|Generation") EBalloonGenerationMode GenerationMode = EBalloonGenerationMode::Manual;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Balloon Level|Generation") EBalloonBiomeSelection BiomeSelection = EBalloonBiomeSelection::Selected;
 /** One config per biome, including its matching asset table and placement settings. */
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Balloon Level|Generation", meta=(EditCondition="BiomeSelection == EBalloonBiomeSelection::Random")) TArray<TObjectPtr<UBalloonLevelConfig>> RandomBiomeConfigs;
 /** Fresh seed on each button press/start; disable to reproduce the authored seed. */
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Balloon Level|Generation") bool RandomizeSeed = false;
 /** Immutable per-generation copy; source DataAssets are never changed. */
 UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category="Balloon Level|Generation") TObjectPtr<UBalloonLevelConfig> ActiveConfig;
protected:
 virtual void BeginPlay() override;
 virtual void Destroyed() override;
 virtual void EndPlay(const EEndPlayReason::Type Reason) override;
public:
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Balloon Level|Events") TObjectPtr<UBalloonLevelEventManager> EventManager;
 UPROPERTY(BlueprintAssignable, Category="Balloon Level|Events") FBalloonBorderChanged OnBorderChanged;
 /** Explicit registration for authored border proxies; cleanup never destroys these. */
 UFUNCTION(BlueprintCallable, Category="Balloon Level|Events") bool RegisterBorderProxy(ABalloonBorderThreat* Proxy);
 bool TryEventBorderChange(const FBalloonLevelEventDefinition& Event, FString& Reason);
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Balloon Level") TObjectPtr<UBalloonLevelConfig> LevelConfig;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Balloon Level") TObjectPtr<USplineComponent> Spline;
 /** Player balloon instance. Successful generation places it at the route start and flight height, with its attached riders. */
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Balloon Level") TObjectPtr<AActor> RoutePawn;
	/** Ready requires fixed-Z route proof and successful world realization. */
	UFUNCTION(BlueprintCallable, Category="Balloon Level") bool GenerateLevel();
	/** Details buttons must have no parameters or return value. */
	UFUNCTION(CallInEditor, Category="Balloon Level", meta=(DisplayName="Generate Level")) void GenerateLevelInEditor();
	UFUNCTION(BlueprintCallable, CallInEditor, Category="Balloon Level") void ClearGeneratedLevel();
	UFUNCTION(BlueprintCallable, Category="Balloon Level") bool RegenerateLevel();
	UFUNCTION(CallInEditor, Category="Balloon Level", meta=(DisplayName="Regenerate Level")) void RegenerateLevelInEditor();
	UFUNCTION(BlueprintPure, Category="Balloon Level") EBalloonLevelGenerationStatus GetGenerationStatus() const { return Status; }
	UFUNCTION(BlueprintPure, Category="Balloon Level") TArray<FString> GetDiagnostics() const { return Diagnostics; }
	/** Stateless full XY projection; for tracked event progression call UpdateProgress instead. */
	UFUNCTION(BlueprintCallable, Category="Balloon Level") float GetNormalizedProgress(const FVector& WorldPosition) const;
	UFUNCTION(BlueprintCallable, Category="Balloon Level") void UpdateProgress(const FVector& WorldPosition, bool Teleported = false);
	UFUNCTION(BlueprintPure, Category="Balloon Level") float GetCurrentProgressCm() const { return Progress.CurrentS; }
	UFUNCTION(BlueprintPure, Category="Balloon Level") float GetMaxReachedProgressCm() const { return Progress.MaxReachedS; }
	UFUNCTION(BlueprintPure, Category="Balloon Level") FTransform GetStartTransform() const { return Plan.Start; }
	UFUNCTION(BlueprintPure, Category="Balloon Level") FTransform GetFinishTransform() const { return Plan.Finish; }
	UFUNCTION(BlueprintPure, Category="Balloon Level") float GetWidthAtDistanceCm(float DistanceCm) const { return BalloonSplinePlanning::WidthAt(Plan, DistanceCm); }
	UFUNCTION(BlueprintPure, Category="Balloon Level") TArray<FBalloonBorderInfluence> GetBorderInfluences() const;
 UFUNCTION(BlueprintCallable, Category="Balloon Level") void DrawBorderDebug(float Duration = 10.f) const;
 const FBalloonLevelPlan& GetPlan() const { return Plan; }
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Balloon Level|Realization") TObjectPtr<UBalloonLevelRealizer> Realizer;
 UPROPERTY(EditAnywhere, Category="Balloon Level|Debug") bool DebugPlacements = true;
 UPROPERTY(EditAnywhere, Category="Balloon Level|Debug") bool DebugRoute = true;
 UPROPERTY(EditAnywhere, Category="Balloon Level|Debug") bool DebugSpline = true;
 UPROPERTY(EditAnywhere, Category="Balloon Level|Debug") bool DebugSegments = true;
 UPROPERTY(EditAnywhere, Category="Balloon Level|Debug") bool DebugHISM = true;
 UFUNCTION(CallInEditor, Category="Balloon Level|Debug") void DrawGenerationDebug() const;
private:
 bool ConfigureEnvironment();
 void ClearOwnedEnvironment();
 UPROPERTY(Transient) TArray<TObjectPtr<AActor>> OwnedEnvironmentActors;
 bool BuildAndRealizeLevel();
 UPROPERTY(Transient) TMap<int32, TObjectPtr<ABalloonBorderThreat>> BorderProxies;
	UPROPERTY(VisibleInstanceOnly, Category="Balloon Level") EBalloonLevelGenerationStatus Status = EBalloonLevelGenerationStatus::NotStarted;
	UPROPERTY(VisibleInstanceOnly, Category="Balloon Level") TArray<FString> Diagnostics;
	FBalloonLevelPlan Plan;
	FBalloonLevelProgress Progress;
};
