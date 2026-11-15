#include "CrosshairHUD.h"

#include "Camera/PlayerCameraManager.h"
#include "Engine/Canvas.h"
#include "FirstPersonCameraPawn.h"
#include "GameFramework/PlayerController.h"
#include "GunComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

namespace CrosshairParams
{
	static const FName QuadCells(TEXT("QuadCells"));
	static const FName Origin(TEXT("Origin"));
	static const FName Gap(TEXT("Gap"));
	static const FName Len(TEXT("Len"));
	static const FName DotSize(TEXT("DotSize"));
	static const FName TickOpacity(TEXT("TickOpacity"));
	static const FName DotOpacity(TEXT("DotOpacity"));
	static const FName GrainTime(TEXT("GrainTime"));
	static const FName TickOffset(TEXT("TickOffset"));
	static const FName BoxHalf(TEXT("BoxHalf"));
	static const FName BoxArm(TEXT("BoxArm"));
	static const FName BoxThick(TEXT("BoxThick"));
}

ACrosshairHUD::ACrosshairHUD()
{
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Mat(TEXT("/Game/Materials/Crosshair/MI_Crosshair.MI_Crosshair"));
	if (Mat.Succeeded())
	{
		CrosshairMaterial = Mat.Object;
	}
}

float ACrosshairHUD::SpreadToPixels(float Degrees, float PixelSize) const
{
	// True scale: a shot Degrees off the centre lands this many crosshair pixels from it (horizontal FOV).
	const float Fov = PlayerOwner && PlayerOwner->PlayerCameraManager ? PlayerOwner->PlayerCameraManager->GetFOVAngle() : 90.f;
	const float HalfTan = FMath::Tan(FMath::DegreesToRadians(FMath::Clamp(Fov, 5.f, 170.f) * 0.5f));
	const float Angle = FMath::DegreesToRadians(FMath::Clamp(Degrees, 0.f, 80.f));
	return FMath::Tan(Angle) / HalfTan * (Canvas->ClipX * 0.5f) / PixelSize;
}

void ACrosshairHUD::UpdateDynamics(const AFirstPersonCameraPawn& Pawn, float DeltaSeconds)
{
	const float Dt = FMath::Max(DeltaSeconds, 1.e-4f);
	const UGunComponent* Gun = Pawn.GetActiveGun();
	const bool bHolding = Gun && !Pawn.IsHandEmpty();
	const float Lowered = Pawn.GetGunLowered();

	// --- Turning the view: mouse, the balloon turning, recoil and sway all end up here.
	const FRotator View = Pawn.GetViewRotation();
	if (!bHasLastView)
	{
		LastView = View;
		bHasLastView = true;
	}
	FVector2D TurnRate(FMath::FindDeltaAngleDegrees(LastView.Yaw, View.Yaw), FMath::FindDeltaAngleDegrees(LastView.Pitch, View.Pitch));
	TurnRate /= Dt;
	LastView = View;
	TurnRate = TurnRate.ClampAxes(-1000.f, 1000.f); // a snap of the view (possession) is not a turn
	TurnSpeed = FMath::FInterpTo(TurnSpeed, TurnRate.Size(), Dt, 12.f);

	// --- The balloon (whatever the player rides on): its speed and its jolts.
	AActor* Parent = Pawn.GetAttachParentActor();
	if (Parent != Balloon.Get())
	{
		Balloon = Parent;
		bHasBalloon = false;
		BalloonVelocity = FVector::ZeroVector;
		BalloonAcceleration = FVector::ZeroVector;
	}
	float BalloonBloomDegrees = 0.f;
	if (Parent)
	{
		const FVector Location = Parent->GetActorLocation();
		if (bHasBalloon)
		{
			const FVector RawVelocity = (Location - LastBalloonLocation) / Dt;
			if (RawVelocity.SizeSquared() < FMath::Square(100000.f)) // not a teleport
			{
				const FVector NewVelocity = FMath::VInterpTo(BalloonVelocity, RawVelocity, Dt, 10.f);
				BalloonAcceleration = FMath::VInterpTo(BalloonAcceleration, (NewVelocity - BalloonVelocity) / Dt, Dt, 6.f);
				BalloonVelocity = NewVelocity;
			}
		}
		LastBalloonLocation = Location;
		bHasBalloon = true;
		BalloonBloomDegrees = FMath::Min(MaxBalloonBloom, BalloonVelocity.Size() * BalloonSpeedBloom + BalloonAcceleration.Size() * BalloonJoltBloom);
	}

	// --- Inertia: the ticks hang on a damped spring behind the view.
	// Turning right leaves them behind on the left, looking up leaves them below. The balloon speeding up
	// to the right throws them left, climbing presses them down (screen x right, y down).
	const FRotationMatrix ViewAxes(View);
	FVector2D InertiaTarget = FVector2D(-TurnRate.X, TurnRate.Y) * TurnInertia;
	InertiaTarget += FVector2D(-FVector::DotProduct(BalloonAcceleration, ViewAxes.GetUnitAxis(EAxis::Y)),
		FVector::DotProduct(BalloonAcceleration, ViewAxes.GetUnitAxis(EAxis::Z))) * BalloonInertia;
	if (MaxInertia <= 0.f)
	{
		Inertia = InertiaVelocity = FVector2D::ZeroVector;
	}
	else
	{
		const float Omega = 2.f * PI * InertiaFrequency;
		const int32 Steps = FMath::Clamp(FMath::CeilToInt(Dt * 120.f), 1, 16); // steady at any frame rate
		const float H = Dt / Steps;
		for (int32 Step = 0; Step < Steps; ++Step)
		{
			const FVector2D Accel = (InertiaTarget - Inertia) * (Omega * Omega) - InertiaVelocity * (2.f * InertiaDamping * Omega);
			InertiaVelocity += Accel * H;
			Inertia += InertiaVelocity * H;
		}
		const float Distance = Inertia.Size();
		if (Distance > MaxInertia)
		{
			// Hits the end of its swing: no further, and no speed outward.
			const FVector2D Out = Inertia / Distance;
			Inertia = Out * MaxInertia;
			InertiaVelocity -= Out * FMath::Max(0.f, FVector2D::DotProduct(InertiaVelocity, Out));
		}
	}

	// --- How far the crosshair opens.
	float Target = 0.f;
	if (bHolding)
	{
		Target += Gun->GetCrosshairSpread();
		Target += Pawn.GetRecoilAlpha() * Gun->GetAimRecoilPitch() * RecoilBloom;
	}
	Target += FMath::Min(MaxTurnBloom, TurnSpeed * TurnBloom);
	Target += BalloonBloomDegrees;
	Target += Lowered * LoweredBloom;
	CurrentSpread = FMath::FInterpTo(CurrentSpread, Target, Dt, Target > CurrentSpread ? OpenSpeed : CloseSpeed);

	// --- Ticks: the gun in hand, faded when it can't fire or is being loaded, gone as it goes down.
	float TickTarget = 0.f;
	if (bHolding)
	{
		TickTarget = 1.f;
		if (Gun->IsReloading())
		{
			TickTarget = ReloadOpacity;
		}
		else if (Gun->GetLiveRounds() <= 0)
		{
			TickTarget = NotReadyOpacity;
		}
		TickTarget *= 1.f - Lowered;
	}
	TickOpacity = FMath::FInterpTo(TickOpacity, TickTarget, Dt, 15.f);

	// The dot always marks the centre.
	DotOpacity = 1.f;

	// --- Interact brackets: empty hand over something to interact with, or dragging a lever.
	const bool bInteract = Pawn.IsHandEmpty() && (Pawn.IsGrabbing() || Pawn.GetAimedInteractable());
	const float MorphTarget = bInteract ? 1.f : 0.f;
	if (MorphTarget != InteractMorph)
	{
		bInteractOpening = bInteract;
	}
	InteractMorph = FMath::FInterpConstantTo(InteractMorph, MorphTarget, Dt, 1.f / FMath::Max(InteractMorphTime, 0.01f));
}

int32 ACrosshairHUD::GetBracketHalfSize() const
{
	const float T = FMath::Clamp(InteractMorph, 0.f, 1.f);
	float Ease;
	if (bInteractOpening)
	{
		// Ease out with a snap past the size (ease-out-back).
		const float C = InteractOvershoot;
		const float U = T - 1.f;
		Ease = 1.f + (C + 1.f) * U * U * U + C * U * U;
	}
	else
	{
		Ease = FMath::SmoothStep(0.f, 1.f, T); // closes without the snap
	}
	return FMath::Max(0, FMath::RoundToInt(Ease * InteractBracketHalfSize));
}

void ACrosshairHUD::DrawHUD()
{
	Super::DrawHUD();

	const AFirstPersonCameraPawn* Pawn = Cast<AFirstPersonCameraPawn>(GetOwningPawn());
	if (!Pawn || !Canvas || !CrosshairMaterial || Canvas->ClipY <= 0.f)
	{
		return;
	}
	if (!Material)
	{
		Material = UMaterialInstanceDynamic::Create(CrosshairMaterial, this);
	}

	const float DeltaSeconds = GetWorld()->GetDeltaSeconds();
	if (DeltaSeconds > 0.f) // paused: hold still
	{
		UpdateDynamics(*Pawn, DeltaSeconds);
	}

	// One crosshair pixel = a whole number of screen pixels, as in the low-res frame.
	const float PixelSize = FMath::Max(1.f, FMath::RoundToFloat(Canvas->ClipY / VirtualHeight));
	CurrentGap = FMath::Clamp(FMath::RoundToInt(SpreadToPixels(CurrentSpread, PixelSize)), MinGap, FMath::Max(MinGap, MaxGap));

	float Length = 5.f;
	Material->GetScalarParameterValue(CrosshairParams::Len, Length);

	// A square of cells around the centre cell, snapped to the crosshair pixel grid.
	// Whole cells only: the swing moves in crosshair pixels, as on a low-res screen.
	const FIntPoint Swing(FMath::RoundToInt(Inertia.X), FMath::RoundToInt(Inertia.Y));
	const FIntPoint QuadShift = bInertiaMovesDot ? Swing : FIntPoint::ZeroValue;
	const FIntPoint TickShift = bInertiaMovesDot ? FIntPoint::ZeroValue : Swing;

	const int32 BracketHalf = GetBracketHalfSize();
	const int32 MaxBracketHalf = FMath::CeilToInt(InteractBracketHalfSize * (1.f + 0.1f * InteractOvershoot)) + 1;
	const int32 HalfCells = FMath::Max(FMath::Max(MinGap, MaxGap) + FMath::CeilToInt(Length) + FMath::CeilToInt(MaxInertia), MaxBracketHalf) + 3;
	const int32 Cells = HalfCells * 2 + 1;
	const int32 CentreX = FMath::FloorToInt(Canvas->ClipX * 0.5f / PixelSize) + QuadShift.X;
	const int32 CentreY = FMath::FloorToInt(Canvas->ClipY * 0.5f / PixelSize) + QuadShift.Y;
	const int32 OriginX = CentreX - HalfCells;
	const int32 OriginY = CentreY - HalfCells;

	// Aimed down the sights the sights are the crosshair: it fades out as the gun comes up, and back in
	// as it goes down (same eased blend as the gun).
	const float Shown = 1.f - Pawn->GetAimDownSightsAlpha();
	if (Shown <= 0.f)
	{
		return;
	}

	Material->SetScalarParameterValue(CrosshairParams::QuadCells, Cells);
	Material->SetVectorParameterValue(CrosshairParams::Origin, FLinearColor(OriginX, OriginY, 0.f, 0.f));
	Material->SetVectorParameterValue(CrosshairParams::TickOffset, FLinearColor(TickShift.X, TickShift.Y, 0.f, 0.f));
	Material->SetScalarParameterValue(CrosshairParams::Gap, CurrentGap);
	// The dot alone is a 1-pixel bracket square; from 2 up the brackets frame it (or replace it).
	Material->SetScalarParameterValue(CrosshairParams::DotSize, (bInteractKeepsDot || BracketHalf < 2) ? 1 : 0);
	Material->SetScalarParameterValue(CrosshairParams::BoxHalf, BracketHalf);
	Material->SetScalarParameterValue(CrosshairParams::BoxArm, InteractBracketLength);
	Material->SetScalarParameterValue(CrosshairParams::BoxThick, InteractBracketThickness);
	Material->SetScalarParameterValue(CrosshairParams::TickOpacity, TickOpacity * Shown);
	Material->SetScalarParameterValue(CrosshairParams::DotOpacity, DotOpacity * Shown);
	Material->SetScalarParameterValue(CrosshairParams::GrainTime, GetWorld()->GetTimeSeconds());

	DrawMaterial(Material, OriginX * PixelSize, OriginY * PixelSize, Cells * PixelSize, Cells * PixelSize, 0.f, 0.f, 1.f, 1.f);
}
