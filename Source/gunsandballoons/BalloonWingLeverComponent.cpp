#include "BalloonWingLeverComponent.h"
#include "BalloonFlightComponent.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/Actor.h"
#include "UObject/UnrealType.h"

UBalloonWingLeverComponent::UBalloonWingLeverComponent()
{
	bAutoActivate = true;
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
	RegenerateStates();
}

void UBalloonWingLeverComponent::RegenerateStates()
{
	NumStates = FMath::Clamp(NumStates, 2, 256);
	States.SetNum(NumStates);
	for (int32 I = 0; I < NumStates; ++I)
	{
		const float T = float(I) / float(NumStates - 1);
		States[I].LeverAngle = FMath::Lerp(LeverClosedAngle, LeverOpenAngle, T);
		States[I].WingAngle = FMath::Lerp(0.f, FMath::Max(0.f, MaxWingAngle), T);
		States[I].TurnInput = T * FMath::Clamp(MaxTurnInput, 0.f, 1.f);
		States[I].SpeedMultiplier = FMath::Lerp(1.f, FMath::Clamp(OpenSpeedMultiplier, 0.f, 1.f), T);
	}
}

void UBalloonWingLeverComponent::BeginPlay()
{
	Super::BeginPlay();
	if (States.Num() < 2)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: fewer than two wing states; regenerating."), *GetPathName());
		RegenerateStates();
	}
	for (int32 I = 0; I < States.Num(); ++I)
	{
		auto& Entry = States[I];
		if (!FMath::IsFinite(Entry.LeverAngle) || !FMath::IsFinite(Entry.WingAngle) ||
			!FMath::IsFinite(Entry.TurnInput) || !FMath::IsFinite(Entry.SpeedMultiplier))
		{
			UE_LOG(LogTemp, Warning, TEXT("%s: invalid wing state; regenerating."), *GetPathName());
			RegenerateStates();
			break;
		}
		Entry.TurnInput = FMath::Clamp(Entry.TurnInput, 0.f, 1.f);
		Entry.SpeedMultiplier = FMath::Clamp(Entry.SpeedMultiplier, 0.f, 1.f);
		if (I && Entry.WingAngle < States[I - 1].WingAngle)
			UE_LOG(LogTemp, Warning, TEXT("%s: wing angles are not monotonically increasing."), *GetPathName());
	}
	State = FMath::Clamp(State, 0, States.Num() - 1);
	LeverAngle = States[State].LeverAngle;
	WingAngle = TransitionWingStart = States[State].WingAngle;
	CurrentTurn = TransitionTurnStart = States[State].TurnInput;
	CurrentMultiplier = TransitionSpeedStart = States[State].SpeedMultiplier;
	Flight = GetOwner()->FindComponentByClass<UBalloonFlightComponent>();
	if (Flight)
	{
		Flight->AddTickPrerequisiteComponent(this);
		PublishModifier();
	}
	else UE_LOG(LogTemp, Warning, TEXT("%s: missing BalloonFlightComponent."), *GetPathName());
}

void UBalloonWingLeverComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	if (IsValid(Flight))
	{
		Flight->ClearFlightModifier(GetFName());
		Flight->RemoveTickPrerequisiteComponent(this);
	}
	Super::EndPlay(Reason);
}

void UBalloonWingLeverComponent::SetState(int32 NewState)
{
	if (States.IsEmpty()) return;
	NewState = FMath::Clamp(NewState, 0, States.Num() - 1);
	if (NewState == State) return;
	TransitionWingStart = WingAngle;
	TransitionTurnStart = CurrentTurn;
	TransitionSpeedStart = CurrentMultiplier;
	State = NewState;
	if (State == 0) StepDirection = 1;
	else if (State == States.Num() - 1) StepDirection = -1;
}

void UBalloonWingLeverComponent::StepState()
{
	if (States.Num() < 2) return;
	SetState(StepMode == EWingLeverStepMode::Cycle ? (State + 1) % States.Num() : State + StepDirection);
}

void UBalloonWingLeverComponent::TickComponent(float Dt, ELevelTick Type, FActorComponentTickFunction* Tick)
{
	Super::TickComponent(Dt, Type, Tick);
	if (!States.IsValidIndex(State)) return;
	const auto& Target = States[State];
	LeverAngle = FMath::FInterpConstantTo(LeverAngle, Target.LeverAngle, Dt, FMath::Max(.01f, LeverMoveSpeed));
	WingAngle = FMath::FInterpConstantTo(WingAngle, Target.WingAngle, Dt, FMath::Max(.01f, WingMoveSpeed));
	const float Distance = Target.WingAngle - TransitionWingStart;
	const float Alpha = FMath::IsNearlyZero(Distance) ? 1.f : FMath::Clamp((WingAngle - TransitionWingStart) / Distance, 0.f, 1.f);
	CurrentTurn = FMath::Lerp(TransitionTurnStart, Target.TurnInput, Alpha);
	CurrentMultiplier = FMath::Lerp(TransitionSpeedStart, Target.SpeedMultiplier, Alpha);
	PublishModifier();
}

void UBalloonWingLeverComponent::PublishModifier()
{
	const float Turn = (Side == EBalloonSide::Left ? -1.f : 1.f) * CurrentTurn;
	if (IsValid(Flight) && (!bPublished || Turn != LastTurn || CurrentMultiplier != LastMultiplier))
	{
		Flight->SetFlightModifier(GetFName(), Turn, CurrentMultiplier);
		LastTurn = Turn;
		LastMultiplier = CurrentMultiplier;
		bPublished = true;
	}
}

float UBalloonWingLeverComponent::GetWingOpen01() const
{
	if (States.Num() < 2 || FMath::IsNearlyEqual(States[0].WingAngle, States.Last().WingAngle)) return 0.f;
	return FMath::Clamp(FMath::GetRangePct(States[0].WingAngle, States.Last().WingAngle, WingAngle), 0.f, 1.f);
}

UBalloonWingLeverComponent* UBalloonWingLeverComponent::FindWingLever(AActor* Actor, EBalloonSide InSide)
{
	if (!IsValid(Actor)) return nullptr;
	TInlineComponentArray<UBalloonWingLeverComponent*> Levers(Actor);
	for (auto* Lever : Levers) if (Lever->Side == InSide) return Lever;
	return nullptr;
}

bool UBalloonWingLeverComponent::AcceptsHit_Implementation(const FHitResult& Hit) const
{
	return Hit.GetActor() == GetOwner() && ((!LeverBone.IsNone() && Hit.BoneName == LeverBone) ||
		(IsValid(InteractShape) && Hit.GetComponent() == InteractShape));
}

bool UBalloonWingLeverComponent::CanInteract_Implementation(APawn* Pawn) const
{
	return IsActive() && IsValid(Flight) && States.Num() >= 2;
}

void UBalloonWingLeverComponent::Interact_Implementation(APawn* Pawn) { StepState(); }

#if WITH_EDITOR
void UBalloonWingLeverComponent::PostEditChangeProperty(FPropertyChangedEvent& Event)
{
	const FName Name = Event.GetPropertyName();
	if (Name == GET_MEMBER_NAME_CHECKED(UBalloonWingLeverComponent, NumStates) ||
		Name == GET_MEMBER_NAME_CHECKED(UBalloonWingLeverComponent, LeverClosedAngle) ||
		Name == GET_MEMBER_NAME_CHECKED(UBalloonWingLeverComponent, LeverOpenAngle) ||
		Name == GET_MEMBER_NAME_CHECKED(UBalloonWingLeverComponent, MaxWingAngle) ||
		Name == GET_MEMBER_NAME_CHECKED(UBalloonWingLeverComponent, MaxTurnInput) ||
		Name == GET_MEMBER_NAME_CHECKED(UBalloonWingLeverComponent, OpenSpeedMultiplier)) RegenerateStates();
	Super::PostEditChangeProperty(Event);
}
#endif
