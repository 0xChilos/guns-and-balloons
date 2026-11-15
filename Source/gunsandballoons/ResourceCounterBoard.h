#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ResourceType.h"
#include "ResourceCounterBoard.generated.h"

class UResourceLedger;
class USoundBase;
class UStaticMesh;
class UStaticMeshComponent;
class UTextRenderComponent;

/**
 * In-world counter of the resources the hook has brought in (UResourceLedger): one row per resource with a
 * small model of it and a three-digit count. A stand-in until the basket has its own counters
 * (docs/features/grappling-hook.md).
 *
 * The board faces its +X. When a count goes up the number rolls up to it a step at a time (TickSound on each
 * step), flashes and the row's model spins and swells for a moment. No collision: the hook flies through it.
 */
UCLASS()
class GUNSANDBALLOONS_API AResourceCounterBoard : public AActor
{
	GENERATED_BODY()

public:
	AResourceCounterBoard();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;

	/** The number a row shows right now (rolls toward the ledger's count). */
	UFUNCTION(BlueprintPure, Category = "Resource Counter")
	int32 GetShownCount(EResourceType Type) const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Resource Counter")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Resource Counter")
	TObjectPtr<UStaticMeshComponent> Plate;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Resource Counter")
	TObjectPtr<UTextRenderComponent> Title;

	/** One per resource, in ResourceTypes::All order. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Resource Counter")
	TArray<TObjectPtr<UStaticMeshComponent>> Icons;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Resource Counter")
	TArray<TObjectPtr<UTextRenderComponent>> Counts;

	/** Each row's colour (the BP_Loot_* HighlightColor), in ResourceTypes::All order. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resource Counter")
	TArray<FLinearColor> RowColors;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resource Counter")
	FLinearColor PlateColor = FLinearColor(0.035f, 0.025f, 0.015f);

	/** Played on every step the counter rolls (SFX_Counter_Tick). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resource Counter")
	TObjectPtr<USoundBase> TickSound;

	/** Steps per second the number rolls up. TBD: needs tuning. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resource Counter", meta = (ClampMin = "1.0"))
	float RollSpeed = 12.f;

	/** A big jump still rolls in at most this long (it speeds up). TBD: needs tuning. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resource Counter", meta = (ClampMin = "0.1"))
	float MaxRollTime = 1.f;

	/** How long a row stays lit after its count changes (s). TBD: needs tuning. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resource Counter", meta = (ClampMin = "0.05"))
	float FlashTime = 0.8f;

	/** Ticks closer together than this are skipped, so a fast roll doesn't buzz (s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resource Counter", meta = (ClampMin = "0.0"))
	float MinTickInterval = 0.06f;

private:
	UFUNCTION()
	void HandleCountChanged(EResourceType Type, int32 NewCount, int32 Delta);

	void ApplyLook();
	void UpdateRow(int32 Index, float DeltaSeconds);

	TWeakObjectPtr<UResourceLedger> Ledger;

	float Target[ResourceTypes::Num] = {};
	float Shown[ResourceTypes::Num] = {};
	float Flash[ResourceTypes::Num] = {};
	float IconYaw[ResourceTypes::Num] = {};
	float TimeSinceTick = 1.f;
};
