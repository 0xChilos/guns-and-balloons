#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BalloonLevelTypes.h"
#include "BalloonEnvironmentPresetController.generated.h"

class ABalloonLevelGenerator;
class AExponentialHeightFog;
class ADirectionalLight;
class ASkyLight;
class UMaterialInterface;

/** Biome atmosphere; identical for Day and Night. Color is linear, not 0-255 sRGB. */
USTRUCT(BlueprintType)
struct GUNSANDBALLOONS_API FBalloonFogPreset
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FLinearColor Color = FLinearColor::White;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0")) float Density = 0.02f;
};

/** One fixed lighting preset. Values are prototype defaults requiring art tuning (TBD). */
USTRUCT(BlueprintType)
struct GUNSANDBALLOONS_API FBalloonLightingPresetValues
{
	GENERATED_BODY()
	/** Applied as the sun actor rotation; requires a Movable sun during play. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FRotator SunRotation = FRotator(-50.f, 180.f, 0.f);
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0")) float SunIntensity = 10.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FLinearColor SunColor = FLinearColor::White;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0")) float SkyIntensity = 1.f;
};

/**
 * M7 fixed environment. Applies exactly one Day OR Night preset, the biome ground material and the
 * biome Exponential Height Fog once per play session. There is no clock, cycle or transition API.
 * Uses existing level actors; spawns only a transient fog actor when the map has none.
 */
UCLASS(Blueprintable)
class GUNSANDBALLOONS_API ABalloonEnvironmentPresetController : public AActor
{
	GENERATED_BODY()
public:
	ABalloonEnvironmentPresetController();

	/** Source of BiomeId and fixed preset. When null, FallbackBiomeId/FallbackPreset are used. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Balloon Environment") TObjectPtr<ABalloonLevelGenerator> Generator;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Balloon Environment") FName FallbackBiomeId = TEXT("Forest");
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Balloon Environment") EBalloonLightingPreset FallbackPreset = EBalloonLightingPreset::Day;
	/** Apply at BeginPlay, or as soon as Generator reaches Ready. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Balloon Environment") bool ApplyOnBeginPlay = true;

	/** Existing ground plane (StaticMesh components). Landscapes are not supported yet. Never spawned. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Balloon Environment|Ground") TObjectPtr<AActor> Ground;
	/** Default MI_<Biome>_Ground lookup; {Biome} is replaced by BiomeId. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Balloon Environment|Ground") FString GroundMaterialPathFormat =
		TEXT("/Game/Assets/Locations/Materials/MI_{Biome}_Ground.MI_{Biome}_Ground");
	/** Per-biome overrides of the default path. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Balloon Environment|Ground") TMap<FName, TSoftObjectPtr<UMaterialInterface>> GroundMaterialOverrides;

	/** Optional explicit actors. Null means: use the single instance in the level. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Balloon Environment|Actors") TObjectPtr<AExponentialHeightFog> Fog;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Balloon Environment|Actors") TObjectPtr<ADirectionalLight> Sun;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Balloon Environment|Actors") TObjectPtr<ASkyLight> Sky;

	/** Spec section 10.3 authoring defaults: Forest, Field, City, Desert. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Balloon Environment|Presets") TMap<FName, FBalloonFogPreset> BiomeFog;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Balloon Environment|Presets") FBalloonLightingPresetValues Day;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Balloon Environment|Presets") FBalloonLightingPresetValues Night;

	/** Validates everything, then applies. During play only the first successful call applies;
	 *  repeating the same biome/preset is a no-op, anything else is rejected (BLG-064). */
	UFUNCTION(BlueprintCallable, Category="Balloon Environment") bool ApplyEnvironment(FName BiomeId, EBalloonLightingPreset Preset);
	/** Editor: selected LevelConfig directly (Random requires Ready). Play: immutable ActiveConfig once Ready. */
	UFUNCTION(BlueprintCallable, Category="Balloon Environment") bool ApplyFromGenerator();
	/** Editor authoring preview; reapplies freely outside play. */
	UFUNCTION(CallInEditor, Category="Balloon Environment", meta=(DisplayName="Preview Environment")) void PreviewEnvironment();

	UFUNCTION(BlueprintPure, Category="Balloon Environment") bool IsApplied() const { return Applied; }
	UFUNCTION(BlueprintPure, Category="Balloon Environment") EBalloonLightingPreset GetSelectedLightingPreset() const { return AppliedPreset; }
	UFUNCTION(BlueprintPure, Category="Balloon Environment") FName GetAppliedBiome() const { return AppliedBiome; }
	UFUNCTION(BlueprintPure, Category="Balloon Environment") TArray<FString> GetDiagnostics() const { return Diagnostics; }
	UFUNCTION(BlueprintPure, Category="Balloon Environment") AExponentialHeightFog* GetSpawnedFog() const { return SpawnedFog.Get(); }

	virtual void Tick(float DeltaSeconds) override;
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void Destroyed() override;
private:
	template<typename T> T* FindSingle(T* Explicit, const TCHAR* Label, bool& Ambiguous);
	void DestroySpawnedFog();
	bool Applied = false;
	FName AppliedBiome;
	EBalloonLightingPreset AppliedPreset = EBalloonLightingPreset::Day;
	TWeakObjectPtr<AExponentialHeightFog> SpawnedFog;
	UPROPERTY(VisibleInstanceOnly, Category="Balloon Environment") TArray<FString> Diagnostics;
};
