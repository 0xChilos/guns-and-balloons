#include "BalloonAssetCatalog.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"

bool BalloonAssetCatalog::ValidateRow(const FBalloonCatalogEntry& Entry, TArray<FString>& Errors)
{
	check(IsInGameThread());
	const auto& R = Entry.Row;
	bool bValid = true;
	auto Reject = [&](const TCHAR* Message)
	{
		Errors.Add(FString::Printf(TEXT("Table %s row %s: %s"), *Entry.SourceTable, *Entry.RowName.ToString(), Message));
		bValid = false;
	};
	const bool bScenery = R.Category == TEXT("Filler") || R.Category == TEXT("Terrain") || R.Category == TEXT("Background");
	const bool bGameplay = R.Category == TEXT("Obstacle") || R.Category == TEXT("Hazard");
	const bool bFlock = R.Rule == TEXT("flock");
	if (!bScenery && !bGameplay) Reject(TEXT("Category must be Filler, Terrain, Background, Obstacle or Hazard"));
	if (R.Rule != TEXT("scatter") && R.Rule != TEXT("ring") && R.Rule != TEXT("line") &&
		R.Rule != TEXT("street") && R.Rule != TEXT("wind_aligned") && !bFlock) Reject(TEXT("Unsupported Rule"));
	if ((bScenery && R.Collision) || (bGameplay && !R.Collision && !bFlock)) Reject(TEXT("Collision is incompatible with Category"));
	if (!FMath::IsFinite(R.DensityPerKm2) || R.DensityPerKm2 < 0.f) Reject(TEXT("DensityPerKm2 must be finite and nonnegative"));
	if (!FMath::IsFinite(R.ScaleMin) || !FMath::IsFinite(R.ScaleMax) || R.ScaleMin <= 0.f || R.ScaleMax < R.ScaleMin) Reject(TEXT("Invalid ScaleMin/ScaleMax interval"));
	if (!FMath::IsFinite(R.MinSpacingM) || R.MinSpacingM < 0.f) Reject(TEXT("MinSpacingM must be finite and nonnegative"));
	if (R.ClusterSize < 1) Reject(TEXT("ClusterSize must be at least one"));
	if (R.Rule == TEXT("ring") && R.Category != TEXT("Background")) Reject(TEXT("ring requires Background"));
	if (R.Rule == TEXT("wind_aligned") && !bScenery) Reject(TEXT("wind_aligned requires noncolliding scenery"));
	if (R.Asset.IsNull() || !R.Asset.IsValid())
	{
		Reject(TEXT("Asset must be a valid nonempty soft object path"));
		return false;
	}
	UObject* Object = R.Asset.TryLoad();
	UClass* ActorClass = Cast<UClass>(Object);
	const bool bActor = ActorClass && ActorClass->IsChildOf(AActor::StaticClass()) &&
		ActorClass->HasAnyClassFlags(CLASS_CompiledFromBlueprint) &&
		!ActorClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists);
	const bool bMesh = Cast<UStaticMesh>(Object) != nullptr;
	if (!Object) Reject(TEXT("Asset could not be resolved; Blueprint references must point to the generated class (..._C)"));
	else if (!bMesh && !bActor) Reject(TEXT("Asset must resolve to a StaticMesh or spawnable Blueprint-generated Actor class"));
	else if (bFlock)
	{
		// Subclasses are allowed, but arbitrary Actor classes are not bird flock substitutes.
		bool bBirdFlock = false;
		for (UClass* C = ActorClass; C; C = C->GetSuperClass()) bBirdFlock |= C->GetFName() == TEXT("BP_BirdFlock_C");
		if (!bActor || !bBirdFlock) Reject(TEXT("flock requires BP_BirdFlock generated Actor class or subclass"));
	}
	else if (bScenery && !bMesh) Reject(TEXT("Scenery requires StaticMesh for HISM; only flock permits an Actor"));
	return bValid;
}

bool BalloonAssetCatalog::Merge(const UDataTable* Shared, const UDataTable* Biome,
	TArray<FBalloonCatalogEntry>& OutEntries, TArray<FString>& Errors)
{
	check(IsInGameThread());
	OutEntries.Reset();
	TMap<FName, FBalloonCatalogEntry> Merged;
	bool bValid = true;
	for (const UDataTable* Table : {Shared, Biome})
	{
		if (!Table) continue; // Empty catalogs are useful for geometry-only authoring.
		if (Table->GetRowStruct() != FBalloonLevelAssetRow::StaticStruct())
		{
			Errors.Add(FString::Printf(TEXT("Table %s row <schema>: expected FBalloonLevelAssetRow"), *Table->GetPathName()));
			bValid = false;
			continue;
		}
		TArray<FName> Names = Table->GetRowNames();
		Names.Sort(FNameLexicalLess());
		for (FName Name : Names)
		{
			FBalloonCatalogEntry Entry{Name, Table->GetPathName(), *Table->FindRow<FBalloonLevelAssetRow>(Name, TEXT("Balloon catalog"))};
			bValid = ValidateRow(Entry, Errors) && bValid;
			Merged.Add(Name, MoveTemp(Entry));
		}
	}
	if (!bValid) return false;
	Merged.GenerateValueArray(OutEntries);
	OutEntries.Sort([](const auto& A, const auto& B) { return A.RowName.LexicalLess(B.RowName); });
	return true;
}
