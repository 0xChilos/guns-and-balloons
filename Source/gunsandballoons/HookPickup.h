#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ResourceType.h"
#include "HookPickup.generated.h"

class UNiagaraComponent;
class UBoxComponent;
class UNiagaraSystem;
class USoundBase;
class UStaticMeshComponent;

/**
 * A resource the grappling hook (UHookComponent) can grab and pull back to the player.
 *
 * Default look: an engine basic shape tinted with Color. Any static mesh with simple collision can be
 * put on Mesh instead (e.g. the SM_Loot_* models); Color then goes to the materials' HighlightColor
 * parameter (the MI_Loot rim) as well as to the shape's Color. Floating pickups hang still in the air
 * (like resources tied to balloons) until hooked; the others rest on the ground with physics.
 * While carried the pickup has no collision and no physics; when the hook brings it in it is
 * dropped in front of the player with physics on, so it can be hooked again for testing, or, with
 * bConsumeOnDelivery, it disappears.
 */
UCLASS(Blueprintable)
class GUNSANDBALLOONS_API AHookPickup : public AActor
{
	GENERATED_BODY()

public:
	AHookPickup();

	virtual void OnConstruction(const FTransform& Transform) override;

	UStaticMeshComponent* GetMesh() const { return Mesh; }

	/** False while the hook is carrying it. */
	UFUNCTION(BlueprintPure, Category = "Hook Pickup")
	bool CanBeHooked() const { return !bCarried; }

	/** True when the hook should remove it on delivery instead of dropping it. */
	bool IsConsumedOnDelivery() const { return bConsumeOnDelivery; }

	const TArray<FResourceYield>& GetContents() const { return Contents; }
	USoundBase* GetGrabSound() const { return GrabSound; }

	/** Marks it as counted (UResourceLedger). True only the first time, so a pickup that is dropped and hooked again counts once. */
	bool MarkCollected();

	/** The hook grabbed it: no physics, no collision, so it rides on the hook head. The sparkle stops. */
	void OnHooked();

	/** The hook brought it in: falls (physics) from where it is, carrying Velocity. */
	void OnDelivered(const FVector& Velocity);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hook Pickup")
	TObjectPtr<UStaticMeshComponent> Mesh;

	/** Larger query box for catching the pickup with the hook. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hook Pickup")
	TObjectPtr<UBoxComponent> HookCollision;

	/** Extra reach on each side of the mesh, in world centimetres. Needs playtest tuning. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hook Pickup", meta = (ClampMin = "0"))
	float HookCollisionPadding = 250.f;

	/** Plays SparkleSystem around the pickup until it is hooked. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hook Pickup")
	TObjectPtr<UNiagaraComponent> Sparkle;

	/** Hangs in the air until hooked (no physics). Off: rests on the ground with physics. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hook Pickup")
	bool bFloating = true;

	/** Tint of the placeholder shape, the HighlightColor of loot materials and the sparkle's SparkColor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hook Pickup")
	FLinearColor Color = FLinearColor(0.9f, 0.55f, 0.1f);

	/** Effect shown around the pickup (e.g. NS_LootSparkle). None: no effect. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hook Pickup")
	TObjectPtr<UNiagaraSystem> SparkleSystem;

	/**
	 * Disappear when the hook brings it to the player instead of dropping in front of them.
	 * Off by default (the design drops it into the basket); on in Hook_Loot_Grab.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hook Pickup")
	bool bConsumeOnDelivery = false;

	/**
	 * What it brings in when the hook delivers it (UResourceLedger): each entry adds a random amount in its range
	 * to its resource, e.g. an ammo box with Ammo 1-3 and Money 1-10. Empty or None entries: not counted.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hook Pickup")
	TArray<FResourceYield> Contents;

	/** Played when the hook grabs it (e.g. /Game/Audio/Sounds/SFX_Loot_*), so every resource sounds like what it is. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hook Pickup")
	TObjectPtr<USoundBase> GrabSound;

protected:
	virtual void BeginPlay() override;

private:
	void ApplyColor();
	void ApplySparkle();
	void ApplyPhysics();
	void UpdateHookCollision();

	bool bCarried = false;
	bool bCollected = false;
};
