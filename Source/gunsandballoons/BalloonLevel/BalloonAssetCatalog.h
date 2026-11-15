#pragma once
#include "BalloonLevelTypes.h"

/** Game-thread-only synchronous validation. Errors always include table and row. */
namespace BalloonAssetCatalog
{
	GUNSANDBALLOONS_API bool ValidateRow(const FBalloonCatalogEntry& Entry, TArray<FString>& Errors);
	/** Validate every authored row, overlay whole biome rows, then sort lexicographically. */
	GUNSANDBALLOONS_API bool Merge(const UDataTable* Shared, const UDataTable* Biome,
		TArray<FBalloonCatalogEntry>& OutEntries, TArray<FString>& Errors);
}
