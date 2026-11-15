#include "BalloonEnvironmentPresetController.h"
#include "BalloonLevelGenerator.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"

ABalloonEnvironmentPresetController::ABalloonEnvironmentPresetController()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
	// Spec section 10.3 authoring defaults (linear color, density). Designers may override per level.
	BiomeFog.Add(TEXT("Forest"), {FLinearColor(0.35f, 0.45f, 0.45f), 0.03f});
	BiomeFog.Add(TEXT("Field"), {FLinearColor(0.75f, 0.70f, 0.55f), 0.015f});
	BiomeFog.Add(TEXT("City"), {FLinearColor(0.40f, 0.35f, 0.28f), 0.05f});
	BiomeFog.Add(TEXT("Desert"), {FLinearColor(0.85f, 0.75f, 0.60f), 0.02f});
	// Prototype lighting values, TBD: tune against the balloon post process.
	Day = FBalloonLightingPresetValues();
	Night.SunRotation = FRotator(-35.f, 180.f, 0.f);
	Night.SunIntensity = 0.5f;
	Night.SunColor = FLinearColor(0.55f, 0.65f, 1.f);
	Night.SkyIntensity = 0.15f;
}

template<typename T>
T* ABalloonEnvironmentPresetController::FindSingle(T* Explicit, const TCHAR* Label, bool& Ambiguous)
{
	if (IsValid(Explicit)) return Explicit;
	T* Found = nullptr;
	int32 Count = 0;
	for (TActorIterator<T> It(GetWorld()); It; ++It)
	{
		if (!IsValid(*It)) continue;
		Found = *It;
		++Count;
	}
	if (Count > 1)
	{
		Diagnostics.Add(FString::Printf(TEXT("BLG-071 %s: %d actors in level; assign one explicitly"), Label, Count));
		Ambiguous = true;
		return nullptr;
	}
	return Found;
}

bool ABalloonEnvironmentPresetController::ApplyEnvironment(FName BiomeId, EBalloonLightingPreset Preset)
{
	UWorld* World = GetWorld();
	if (!World) return false;
	const bool Play = World->IsGameWorld();
	if (Play && Applied)
	{
		if (AppliedBiome == BiomeId && AppliedPreset == Preset) return true;
		Diagnostics.Add(TEXT("BLG-064 InvalidLightingEvent: Day/Night and biome atmosphere are fixed for the play session"));
		return false;
	}
	Diagnostics.Reset();
	if (Preset != EBalloonLightingPreset::Day && Preset != EBalloonLightingPreset::Night)
	{
		Diagnostics.Add(TEXT("BLG-070 Preset must be Day or Night"));
		return false;
	}

	// Resolve everything first; apply only when no error exists.
	bool Error = false;
	const FBalloonFogPreset* FogPreset = BiomeFog.Find(BiomeId);
	if (!FogPreset || !FMath::IsFinite(FogPreset->Density) || FogPreset->Density < 0.f)
	{
		Diagnostics.Add(FString::Printf(TEXT("BLG-072 No valid fog preset for biome %s"), *BiomeId.ToString()));
		Error = true;
	}
	bool Ambiguous = false;
	AExponentialHeightFog* FogActor = SpawnedFog.IsValid() ? SpawnedFog.Get() : FindSingle<AExponentialHeightFog>(Fog, TEXT("ExponentialHeightFog"), Ambiguous);
	ADirectionalLight* SunActor = FindSingle<ADirectionalLight>(Sun, TEXT("DirectionalLight"), Ambiguous);
	ASkyLight* SkyActor = FindSingle<ASkyLight>(Sky, TEXT("SkyLight"), Ambiguous);
	Error |= Ambiguous;

	TArray<UStaticMeshComponent*> GroundMeshes;
	UMaterialInterface* GroundMaterial = nullptr;
	if (IsValid(Ground))
	{
		Ground->GetComponents(GroundMeshes);
		if (GroundMeshes.IsEmpty())
		{
			Diagnostics.Add(TEXT("BLG-073 Ground actor has no StaticMesh components (landscape ground is not supported yet)"));
			Error = true;
		}
		const TSoftObjectPtr<UMaterialInterface>* Override = GroundMaterialOverrides.Find(BiomeId);
		const FString Path = Override ? Override->ToSoftObjectPath().ToString() : GroundMaterialPathFormat.Replace(TEXT("{Biome}"), *BiomeId.ToString());
		GroundMaterial = Override ? Override->LoadSynchronous() : Cast<UMaterialInterface>(FSoftObjectPath(Path).TryLoad());
		if (!GroundMaterial)
		{
			Diagnostics.Add(FString::Printf(TEXT("BLG-074 Ground material %s did not load"), *Path));
			Error = true;
		}
		else if (GroundMaterial->GetName() != FString::Printf(TEXT("MI_%s_Ground"), *BiomeId.ToString()))
		{
			Diagnostics.Add(FString::Printf(TEXT("Warning: ground material %s does not follow MI_<Level>_Ground"), *GroundMaterial->GetName()));
		}
	}
	else
	{
		Diagnostics.Add(TEXT("Warning: no Ground actor assigned; ground material not applied"));
	}
	if (!SunActor) Diagnostics.Add(TEXT("Warning: no DirectionalLight; sun preset not applied"));
	if (!SkyActor) Diagnostics.Add(TEXT("Warning: no SkyLight; sky intensity not applied"));
	if (Error) return false;

	// Apply.
	if (!FogActor)
	{
		// Owned fog only when the level has none; transient so it is never saved into the map.
		FActorSpawnParameters Params;
		Params.Owner = this;
		Params.ObjectFlags |= RF_Transient;
		FogActor = World->SpawnActor<AExponentialHeightFog>(GetActorLocation(), FRotator::ZeroRotator, Params);
		SpawnedFog = FogActor;
		Diagnostics.Add(TEXT("Spawned transient ExponentialHeightFog (level had none)"));
	}
	if (UExponentialHeightFogComponent* FogComponent = FogActor ? FogActor->GetComponent() : nullptr)
	{
		if (!Play) FogComponent->Modify();
		FogComponent->SetFogInscatteringColor(FogPreset->Color);
		FogComponent->SetFogDensity(FogPreset->Density);
	}
	for (UStaticMeshComponent* Mesh : GroundMeshes)
	{
		if (!Play) Mesh->Modify();
		for (int32 Slot = 0; Slot < FMath::Max(1, Mesh->GetNumMaterials()); ++Slot) Mesh->SetMaterial(Slot, GroundMaterial);
	}
	const FBalloonLightingPresetValues& Values = Preset == EBalloonLightingPreset::Day ? Day : Night;
	if (SunActor && SunActor->GetComponent())
	{
		UDirectionalLightComponent* Light = SunActor->GetComponent();
		if (!Play) { SunActor->Modify(); Light->Modify(); }
		if (Play && Light->Mobility != EComponentMobility::Movable)
			Diagnostics.Add(TEXT("Warning: sun is not Movable; rotation/intensity may not change during play"));
		SunActor->SetActorRotation(Values.SunRotation);
		Light->SetIntensity(Values.SunIntensity);
		Light->SetLightColor(Values.SunColor);
	}
	if (SkyActor && SkyActor->GetLightComponent())
	{
		if (!Play) SkyActor->GetLightComponent()->Modify();
		SkyActor->GetLightComponent()->SetIntensity(Values.SkyIntensity);
		SkyActor->GetLightComponent()->RecaptureSky();
	}
	if (Generator && Generator->GetGenerationStatus() == EBalloonLevelGenerationStatus::Ready && GroundMeshes.Num())
	{
		FBox GroundBounds(ForceInit), Corridor(ForceInit);
		for (UStaticMeshComponent* Mesh : GroundMeshes) GroundBounds += Mesh->Bounds.GetBox();
		for (const auto& Segment : Generator->GetPlan().Segments) Corridor += Segment.BaseBounds;
		if (Corridor.IsValid && GroundBounds.IsValid && (Corridor.Min.X < GroundBounds.Min.X || Corridor.Min.Y < GroundBounds.Min.Y ||
			Corridor.Max.X > GroundBounds.Max.X || Corridor.Max.Y > GroundBounds.Max.Y))
			Diagnostics.Add(TEXT("Warning: ground plane does not cover the planned corridor bounds"));
	}
	Applied = true;
	AppliedBiome = BiomeId;
	AppliedPreset = Preset;
	Diagnostics.Add(FString::Printf(TEXT("Environment applied: biome=%s preset=%s"), *BiomeId.ToString(),
		Preset == EBalloonLightingPreset::Day ? TEXT("Day") : TEXT("Night")));
	return true;
}

bool ABalloonEnvironmentPresetController::ApplyFromGenerator()
{
	if (!Generator) return ApplyEnvironment(FallbackBiomeId, FallbackPreset);
	// Authoring preview follows the selected config, even if the last generated
	// plan is stale or no route has been generated yet. Random selection needs
	// a successful generation before its chosen config is known.
	if (GetWorld() && !GetWorld()->IsGameWorld() && Generator->BiomeSelection == EBalloonBiomeSelection::Selected)
	{
		if (IsValid(Generator->LevelConfig))
			return ApplyEnvironment(Generator->LevelConfig->BiomeId, Generator->LevelConfig->Preset);
		Diagnostics = {TEXT("BLG-075 Assign the generator's Level Config before previewing the environment")};
		return false;
	}
	if (Generator->GetGenerationStatus() != EBalloonLevelGenerationStatus::Ready) return false;
	if (!IsValid(Generator->ActiveConfig)) return false;
	return ApplyEnvironment(Generator->ActiveConfig->BiomeId, Generator->ActiveConfig->Preset);
}

void ABalloonEnvironmentPresetController::PreviewEnvironment()
{
	if (GetWorld() && GetWorld()->IsGameWorld()) return;
	Applied = false;
	if (!ApplyFromGenerator() && Generator && Generator->BiomeSelection == EBalloonBiomeSelection::Random &&
		Generator->GetGenerationStatus() != EBalloonLevelGenerationStatus::Ready)
		Diagnostics = {TEXT("Generate the level first: Generator is not Ready")};
}

void ABalloonEnvironmentPresetController::BeginPlay()
{
	Super::BeginPlay();
	if (!ApplyOnBeginPlay) return;
	// Generator may generate in its own BeginPlay (OnLevelStart); wait until it is Ready.
	if (!ApplyFromGenerator() && Generator && !Applied) SetActorTickEnabled(true);
}

void ABalloonEnvironmentPresetController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (Applied || !Generator) { SetActorTickEnabled(false); return; }
	if (Generator->GetGenerationStatus() == EBalloonLevelGenerationStatus::Ready)
	{
		ApplyFromGenerator();
		SetActorTickEnabled(false);
	}
}

void ABalloonEnvironmentPresetController::DestroySpawnedFog()
{
	if (AExponentialHeightFog* Owned = SpawnedFog.Get()) Owned->Destroy();
	SpawnedFog.Reset();
}

void ABalloonEnvironmentPresetController::EndPlay(const EEndPlayReason::Type Reason)
{
	DestroySpawnedFog();
	Applied = false;
	Super::EndPlay(Reason);
}

void ABalloonEnvironmentPresetController::Destroyed()
{
	DestroySpawnedFog();
	Super::Destroyed();
}
