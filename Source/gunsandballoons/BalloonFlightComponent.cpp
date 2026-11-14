#include "BalloonFlightComponent.h"

#include "GameFramework/Actor.h"

UBalloonFlightComponent::UBalloonFlightComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void UBalloonFlightComponent::BeginPlay()
{
	Super::BeginPlay();

	if (MinSpeed <= 0.f)
	{
		MinSpeed = 50.f; // TBD: prototype value
		UE_LOG(LogTemp, Warning, TEXT("%s: BalloonFlight MinSpeed %f is not positive, reset to %f."),
			*GetNameSafe(GetOwner()), MinSpeed, MinSpeed);
	}

	if (MaxSpeed < MinSpeed)
	{
		const float RequestedMaxSpeed = MaxSpeed;
		MaxSpeed = MinSpeed;
		UE_LOG(LogTemp, Warning, TEXT("%s: BalloonFlight MaxSpeed %f was below MinSpeed, raised to %f."),
			*GetNameSafe(GetOwner()), RequestedMaxSpeed, MaxSpeed);
	}

	CurrentSpeed = TargetSpeed = FMath::Clamp(InitialSpeed, MinSpeed, MaxSpeed);

	if (bDebugLog)
	{
		DebugAccelStartTime = FPlatformTime::Seconds();
		UE_LOG(LogTemp, Log, TEXT("BalloonFlight DEBUG: accel timing armed (MinSpeed=%.1f MaxSpeed=%.1f)."), MinSpeed, MaxSpeed);
	}
}

void UBalloonFlightComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bDebugLog)
	{
		UE_LOG(LogTemp, Log, TEXT("BalloonFlight DEBUG: speed=%.1f target=%.1f turnRate=%.1f yaw=%.1f"),
			CurrentSpeed, TargetSpeed, CurrentTurnRate, GetOwner() ? GetOwner()->GetActorRotation().Yaw : 0.f);
	}

	AActor* Owner = GetOwner();
	if (!Owner || TickType == LEVELTICK_PauseTick)
	{
		return;
	}

	if (bAutoTest)
	{
		if (AutoTestState == 0)
		{
			SetTargetSpeed(MinSpeed);
			if (CurrentSpeed <= MinSpeed + 0.5f)
			{
				AutoTestState = 1;
			}
		}
		else if (AutoTestState == 1)
		{
			SetTargetSpeed(MaxSpeed);
			if (CurrentSpeed >= MaxSpeed - 0.5f)
			{
				AutoTestState = 2;
			}
		}
		else if (AutoTestState == 2)
		{
			if (!bAutoTestTurning)
			{
				bAutoTestTurning = true;
				AutoTestTurnStartYaw = Owner->GetActorRotation().Yaw;
			}
			SetTurnInput(1.f);
			const float YawDelta = FMath::Abs(FMath::FindDeltaAngleDegrees(AutoTestTurnStartYaw, Owner->GetActorRotation().Yaw));
			if (YawDelta >= 90.f)
			{
				bAutoTestTurning = false;
				AutoTestState = 3;
				SetTurnInput(0.f);
			}
		}
		else
		{
			SetTurnInput(0.f);
			// The timing sequence is finished. Release control instead of overriding
			// keyboard/lever input with zero on every subsequent frame.
			bAutoTest = false;
		}
	}

	// Forward speed eases toward the target with a constant rate (no exponential smoothing).
	CurrentSpeed = FMath::FInterpConstantTo(CurrentSpeed, TargetSpeed, DeltaTime,
		TargetSpeed > CurrentSpeed ? Acceleration : Deceleration);

	// Turn rate grows while the magnitude increases in the same direction; once it shrinks,
	// or while reversing through zero, it falls off with the gentler TurnDeceleration.
	const float TargetTurnRate = TurnInput * MaxTurnRate;
	const bool bTurningUp = FMath::Abs(TargetTurnRate) > FMath::Abs(CurrentTurnRate) &&
		CurrentTurnRate * TargetTurnRate >= 0.f;
	CurrentTurnRate = FMath::FInterpConstantTo(CurrentTurnRate, TargetTurnRate, DeltaTime,
		bTurningUp ? TurnAcceleration : TurnDeceleration);

	// Forward motion continues while turning; only the horizontal yaw matters for the heading.
	const float Yaw = Owner->GetActorRotation().Yaw;
	const FVector Forward = FRotator(0.f, Yaw, 0.f).Vector();
	Owner->AddActorWorldOffset(Forward * (CurrentSpeed * DeltaTime), /*bSweep=*/true);
	Owner->AddActorWorldRotation(FRotator(0.f, CurrentTurnRate * DeltaTime, 0.f));

	if (bDebugLog)
	{
		if (!bDebugAccelDone)
		{
			if (CurrentSpeed <= MinSpeed + 0.5f)
			{
				DebugAccelStartTime = FPlatformTime::Seconds();
			}
			if (CurrentSpeed >= MaxSpeed - 0.5f)
			{
				const double Elapsed = FPlatformTime::Seconds() - DebugAccelStartTime;
				UE_LOG(LogTemp, Log, TEXT("BalloonFlight DEBUG: ACCEL Min->Max = %.3f s (speed=%.1f)."), Elapsed, CurrentSpeed);
				bDebugAccelDone = true;
			}
		}

		if (FMath::Abs(TurnInput) > KINDA_SMALL_NUMBER)
		{
			if (!bDebugTurnActive)
			{
				bDebugTurnActive = true;
				DebugTurnStartTime = FPlatformTime::Seconds();
				DebugTurnStartYaw = Owner->GetActorRotation().Yaw;
			}
			else
			{
				const float YawDelta = FMath::Abs(FMath::FindDeltaAngleDegrees(DebugTurnStartYaw, Owner->GetActorRotation().Yaw));
				if (YawDelta >= 90.f)
				{
					const double Elapsed = FPlatformTime::Seconds() - DebugTurnStartTime;
					UE_LOG(LogTemp, Log, TEXT("BalloonFlight DEBUG: TURN 90deg = %.3f s (yawDelta=%.1f)."), Elapsed, YawDelta);
					bDebugTurnActive = false;
				}
			}
		}
		else if (bDebugTurnActive)
		{
			bDebugTurnActive = false;
		}
	}
}

void UBalloonFlightComponent::SetTargetSpeed(float SpeedCmPerSecond)
{
	if (SpeedCmPerSecond <= 0.f)
	{
		TargetSpeed = MinSpeed;
		return;
	}
	TargetSpeed = FMath::Clamp(SpeedCmPerSecond, MinSpeed, MaxSpeed);
}

void UBalloonFlightComponent::SetTurnInput(float Input)
{
	TurnInput = FMath::Clamp(Input, -1.f, 1.f);
}
