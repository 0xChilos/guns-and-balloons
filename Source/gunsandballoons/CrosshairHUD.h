#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "CrosshairHUD.generated.h"

class AFirstPersonCameraPawn;
class UMaterialInstanceDynamic;
class UMaterialInterface;

/**
 * Dynamic crosshair, drawn in the game's PS1 style (see docs/features/crosshair.md).
 *
 * The look lives in /Game/Materials/Crosshair/M_Crosshair (UI material, colours and shape in
 * MI_Crosshair): chunky low-res pixels (one crosshair pixel = viewport height / VirtualHeight, the
 * art style's 2x upscale), a dark outline, colours quantized with a 4x4 Bayer dither, dithered fades
 * and grain. The HUD is drawn after post-processing, so the material does those steps itself.
 *
 * This class drives it every frame. The centre is always where the shot goes (recoil, sway and
 * the balloon all turn the real view); what moves is the gap between the four ticks:
 * - the gun's spread (SpreadAngle + pellet pattern), at true scale for the camera FOV;
 * - recoil (each shot opens it by the gun's AimRecoilPitch);
 * - turning the view (mouse, balloon turning, sway);
 * - the balloon's speed and jolts;
 * Inertia: the ticks hang on a damped spring. They trail a turn of the view, swing past when it
 * stops and settle; the balloon speeding up, braking, climbing or sinking throws them the other way.
 * The dot stays on the true aim (bInertiaMovesDot to swing it too).
 * Aiming down the sights fades the whole crosshair out with the gun's raise, and back in as it lowers.
 * - a weapon change (the crosshair opens and fades out as the gun goes down, the new gun's comes back).
 * Empty hand: only the dot. Over something to interact with (or while dragging a lever) it opens into
 * four corner brackets: the dot swells into a small square, its sides open up as it grows, and the
 * brackets snap a little past their size and settle. The dot stays in the middle. A gun with nothing to fire
 * (empty, or the hook below 100) shows faded ticks.
 *
 * All values below are first-pass, TBD: tune in game.
 */
UCLASS()
class GUNSANDBALLOONS_API ACrosshairHUD : public AHUD
{
	GENERATED_BODY()

public:
	ACrosshairHUD();

	virtual void DrawHUD() override;

	/** The crosshair's current opening (degrees from the centre), after every effect. */
	UFUNCTION(BlueprintPure, Category = "Crosshair")
	float GetCurrentSpread() const { return CurrentSpread; }

	/** The current gap between the centre and the ticks, in crosshair pixels. */
	UFUNCTION(BlueprintPure, Category = "Crosshair")
	int32 GetCurrentGapPixels() const { return CurrentGap; }

	/** Where the inertia has swung the ticks (crosshair pixels, screen right/down). */
	UFUNCTION(BlueprintPure, Category = "Crosshair")
	FVector2D GetInertiaOffset() const { return Inertia; }

protected:
	/** UI material that draws the crosshair (M_Crosshair or an instance of it). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crosshair")
	TObjectPtr<UMaterialInterface> CrosshairMaterial;

	/** Rows of the low-res frame the crosshair is drawn on. 540 = 2x nearest upscale at 1080p (art style). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crosshair|Look", meta = (ClampMin = "90.0"))
	float VirtualHeight = 540.f;

	/** Smallest gap between the centre and the ticks (crosshair pixels), so a precise gun still reads. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crosshair|Look", meta = (ClampMin = "0"))
	int32 MinGap = 2;

	/** Largest gap (crosshair pixels). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crosshair|Look", meta = (ClampMin = "1"))
	int32 MaxGap = 48;

	/** Degrees the crosshair opens per degree of the gun's AimRecoilPitch, right on the shot. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crosshair|Dynamics", meta = (ClampMin = "0.0"))
	float RecoilBloom = 0.3f;

	/** Degrees the crosshair opens per degree/second of view turning (mouse, balloon turning, sway). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crosshair|Dynamics", meta = (ClampMin = "0.0"))
	float TurnBloom = 0.01f;

	/** Most the view turning can open it (degrees). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crosshair|Dynamics", meta = (ClampMin = "0.0"))
	float MaxTurnBloom = 2.f;

	/** Degrees per cm/s of balloon speed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crosshair|Dynamics", meta = (ClampMin = "0.0"))
	float BalloonSpeedBloom = 0.0004f;

	/** Degrees per cm/s^2 of balloon acceleration (jolts, speeding up, braking, climbing). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crosshair|Dynamics", meta = (ClampMin = "0.0"))
	float BalloonJoltBloom = 0.004f;

	/** Most the balloon can open it (degrees). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crosshair|Dynamics", meta = (ClampMin = "0.0"))
	float MaxBalloonBloom = 1.5f;

	/** Extra degrees with the gun fully lowered (weapon change, rack). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crosshair|Dynamics", meta = (ClampMin = "0.0"))
	float LoweredBloom = 5.f;

	/** How fast the crosshair opens (1/s) and closes back (1/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crosshair|Dynamics", meta = (ClampMin = "0.1"))
	float OpenSpeed = 25.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crosshair|Dynamics", meta = (ClampMin = "0.1"))
	float CloseSpeed = 7.f;

	// --- Inertia: the ticks hang on a spring behind the view --------------------------------

	/** Crosshair pixels the ticks trail a turn by, per degree/second of turning. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crosshair|Inertia", meta = (ClampMin = "0.0"))
	float TurnInertia = 0.05f;

	/** Crosshair pixels the ticks are thrown by, per cm/s^2 of balloon acceleration (sideways and up/down on screen). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crosshair|Inertia", meta = (ClampMin = "0.0"))
	float BalloonInertia = 0.02f;

	/** Spring of the ticks (Hz): lower = heavier, lazier swing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crosshair|Inertia", meta = (ClampMin = "0.1"))
	float InertiaFrequency = 1.8f;

	/** Damping ratio: below 1 the ticks overshoot and settle; 1 = no overshoot. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crosshair|Inertia", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float InertiaDamping = 0.35f;

	/** Farthest the ticks swing from the centre (crosshair pixels). 0 turns the inertia off. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crosshair|Inertia", meta = (ClampMin = "0.0"))
	float MaxInertia = 16.f;

	/**
	 * Swing the dot too. Off: the dot stays on the true aim (where the shot goes) and only the ticks
	 * swing. On: the whole crosshair swings, but then it no longer shows exactly where the shot goes.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crosshair|Inertia")
	bool bInertiaMovesDot = false;

	/** Tick opacity while the gun has nothing to fire (empty, or the hook below 100). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crosshair|Look", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float NotReadyOpacity = 0.4f;

	/** Tick opacity while the gun is open for loading. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crosshair|Look", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ReloadOpacity = 0.5f;

	// --- Interact brackets: with an empty hand over something to interact with (or while dragging a
	// lever) the dot swells into a small square whose sides open up, leaving four corner brackets.

	/** Half-size of the brackets (crosshair pixels from the centre to the frame): 6 = a 13x13 square. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crosshair|Interact", meta = (ClampMin = "1"))
	int32 InteractBracketHalfSize = 6;

	/** Length of each bracket's arms, corner included (crosshair pixels). At HalfSize + 1 or more it is a full square. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crosshair|Interact", meta = (ClampMin = "1"))
	int32 InteractBracketLength = 3;

	/** Thickness of the brackets (crosshair pixels). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crosshair|Interact", meta = (ClampMin = "1"))
	int32 InteractBracketThickness = 1;

	/** Seconds for the dot to open into the brackets (and to close back). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crosshair|Interact", meta = (ClampMin = "0.01"))
	float InteractMorphTime = 0.15f;

	/** How far the brackets snap past their size on opening (0 = none; 1.7 = about 10 % past). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crosshair|Interact", meta = (ClampMin = "0.0", ClampMax = "4.0"))
	float InteractOvershoot = 1.7f;

	/** Keep the centre dot inside the brackets (it marks the true aim). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crosshair|Interact")
	bool bInteractKeepsDot = true;

public:
	/** 0 = the dot, 1 = the interact brackets. */
	UFUNCTION(BlueprintPure, Category = "Crosshair")
	float GetInteractMorph() const { return InteractMorph; }

private:
	/** The brackets' current half-size in crosshair pixels (0 = just the dot). */
	int32 GetBracketHalfSize() const;

	void UpdateDynamics(const AFirstPersonCameraPawn& Pawn, float DeltaSeconds);
	float SpreadToPixels(float Degrees, float PixelSize) const;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> Material;

	float CurrentSpread = 0.f;
	int32 CurrentGap = 0;
	float TurnSpeed = 0.f;
	FVector2D Inertia = FVector2D::ZeroVector;          // crosshair pixels, screen right/down
	FVector2D InertiaVelocity = FVector2D::ZeroVector;
	float TickOpacity = 0.f;
	float DotOpacity = 0.f;
	float InteractMorph = 0.f;                          // 0 = dot, 1 = brackets (linear in time; eased when drawn)
	bool bInteractOpening = false;

	FRotator LastView = FRotator::ZeroRotator;
	bool bHasLastView = false;

	TWeakObjectPtr<AActor> Balloon;
	FVector LastBalloonLocation = FVector::ZeroVector;
	FVector BalloonVelocity = FVector::ZeroVector;
	FVector BalloonAcceleration = FVector::ZeroVector;
	bool bHasBalloon = false;
};
