#pragma once

#include "CoreMinimal.h"
#include "ResourceType.generated.h"

/**
 * What a hooked pickup brings in (docs/features/grappling-hook.md). Mirrors the Blueprint enum E_LootType
 * of BP_Loot (Ammo, Money, Wood, Coal, Crystal); None = not a resource (the placeholder test cubes).
 */
UENUM(BlueprintType)
enum class EResourceType : uint8
{
	None,
	Ammo,
	Money,
	Wood,
	Coal,
	Crystal,
};

/** Part of what a pickup brings in: MinAmount..MaxAmount of one resource, rolled when it arrives. */
USTRUCT(BlueprintType)
struct FResourceYield
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resources")
	EResourceType ResourceType = EResourceType::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resources", meta = (ClampMin = "1"))
	int32 MinAmount = 1;

	/** Inclusive. Below MinAmount counts as MinAmount. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resources", meta = (ClampMin = "1"))
	int32 MaxAmount = 1;

	/** A random amount in MinAmount..MaxAmount (both included). */
	int32 RollAmount() const { return FMath::RandRange(MinAmount, FMath::Max(MinAmount, MaxAmount)); }
};

namespace ResourceTypes
{
	/** Every real resource, in counter order. */
	inline constexpr EResourceType All[] = { EResourceType::Ammo, EResourceType::Money, EResourceType::Wood, EResourceType::Coal, EResourceType::Crystal };
	inline constexpr int32 Num = UE_ARRAY_COUNT(All);

	/** 0..Num-1 for a real resource, INDEX_NONE for None. */
	inline int32 ToIndex(EResourceType Type)
	{
		const int32 Index = int32(Type) - 1;
		return Index >= 0 && Index < Num ? Index : INDEX_NONE;
	}
}
