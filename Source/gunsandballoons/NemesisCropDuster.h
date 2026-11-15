#pragma once

#include "CoreMinimal.h"
#include "NemesisPlane.h"
#include "NemesisCropDuster.generated.h"

class ANemesisBullet;

/**
 * Crop duster gunner (AI ворогів, section 4). Holds a slot beside the balloon (SideDistance to
 * the side, AheadDistance ahead) at the balloon's altitude at launch, fires bursts straight at
 * the basket (no bullet drop). Once in its slot it keeps that side and distance until shot down
 * (design 2026-11-14). Only shoots, never drops mines.
 */
UCLASS()
class GUNSANDBALLOONS_API ANemesisCropDuster : public ANemesisPlane
{
	GENERATED_BODY()

public:
	ANemesisCropDuster();

	virtual void Launch(ANemesisAirship* InAirship) override;
	virtual void Tick(float DeltaSeconds) override;

	/** +1 right of the balloon, −1 left. Set by the airship before Launch. */
	void SetSide(int32 InSide) { Side = InSide >= 0 ? 1 : -1; }
	int32 GetSide() const { return Side; }

	/** Altitude above the basket at launch, cm (then fixed). TBD */
	UPROPERTY(EditAnywhere, Category = "CropDuster")
	float HeightAboveBasket = 500.f;

	/** Fires 0.5 s before a burst: hook for the muzzle flash / cocking sound. */
	UFUNCTION(BlueprintImplementableEvent, Category = "CropDuster")
	void OnBurstWarning();

protected:
	virtual FVector GetSlotLocation() const override;
	virtual void TickAttack(float DeltaSeconds) override;

	UPROPERTY(EditAnywhere, Category = "CropDuster", meta = (ClampMin = "0"))
	float SideDistance = 3000.f;

	UPROPERTY(EditAnywhere, Category = "CropDuster")
	float AheadDistance = 1000.f;

	/** Crop dusters on the same side take slots this far apart along the course (ahead, behind,
	 *  further ahead...), so they don't sit in each other, cm. TBD */
	UPROPERTY(EditAnywhere, Category = "CropDuster", meta = (ClampMin = "0"))
	float SlotSpacing = 1500.f;

	/** Shoots only when this close to its slot, cm. */
	UPROPERTY(EditAnywhere, Category = "CropDuster", meta = (ClampMin = "0"))
	float SlotTolerance = 800.f;

	UPROPERTY(EditAnywhere, Category = "CropDuster", meta = (ClampMin = "1"))
	int32 BulletsPerBurst = 6;

	UPROPERTY(EditAnywhere, Category = "CropDuster", meta = (ClampMin = "0"))
	float BulletInterval = 0.12f;

	UPROPERTY(EditAnywhere, Category = "CropDuster", meta = (ClampMin = "0"))
	float BurstCooldown = 4.f;

	UPROPERTY(EditAnywhere, Category = "CropDuster", meta = (ClampMin = "0"))
	float BurstWarningTime = 0.5f;

	/** Cone half-angle, degrees. */
	UPROPERTY(EditAnywhere, Category = "CropDuster", meta = (ClampMin = "0"))
	float SpreadDegrees = 3.f;

	UPROPERTY(EditAnywhere, Category = "CropDuster")
	TSubclassOf<ANemesisBullet> BulletClass;

	/** Automatic gun turret on the back (turns to the basket; hits on it count as fuselage). */
	UPROPERTY(VisibleAnywhere, Category = "CropDuster")
	TObjectPtr<UStaticMeshComponent> Turret;

private:
	void FireBullet();

	int32 Side = 1;
	bool bWarned = false;
	float BurstTimer = 0.f;
	float ShotTimer = 0.f;
	int32 ShotsLeft = 0;
};
