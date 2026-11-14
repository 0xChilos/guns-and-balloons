#include "BalloonWingLeverComponent.h"
#include "BalloonFlightComponent.h"
#include "FirstPersonCameraPawn.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
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
		States[I].DetentBreakAngle = FMath::Max(0.f, DetentBreakAngle);
		States[I].SlipChance = T * FMath::Clamp(MaxSlipChance, 0.f, 1.f);
		States[I].LeverAngle = FMath::Lerp(LeverClosedAngle, LeverOpenAngle, T);
		States[I].WingAngle = FMath::Lerp(0.f, FMath::Max(0.f, MaxWingAngle), T);
		States[I].TurnInput = T * FMath::Clamp(MaxTurnInput, 0.f, 1.f);
		States[I].SpeedMultiplier = FMath::Lerp(1.f, FMath::Clamp(OpenSpeedMultiplier, 0.f, 1.f), T);
	}
}

void UBalloonWingLeverComponent::BeginPlay()
{
	Super::BeginPlay();
	if (SlipSeed == 0) SlipRandom.GenerateNewSeed();
	else SlipRandom.Initialize(SlipSeed);
	SlipCheckElapsed = 0.f;
	if (States.Num() < 2)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: fewer than two wing states; regenerating."), *GetPathName());
		RegenerateStates();
	}
	for (int32 I = 0; I < States.Num(); ++I)
	{
		auto& Entry = States[I];
		if (!FMath::IsFinite(Entry.LeverAngle) || !FMath::IsFinite(Entry.WingAngle) ||
			!FMath::IsFinite(Entry.TurnInput) || !FMath::IsFinite(Entry.SpeedMultiplier) ||
			!FMath::IsFinite(Entry.DetentBreakAngle) || !FMath::IsFinite(Entry.SlipChance))
		{
			UE_LOG(LogTemp, Warning, TEXT("%s: invalid wing state; regenerating."), *GetPathName());
			RegenerateStates();
			break;
		}
		Entry.DetentBreakAngle = FMath::Max(0.f, Entry.DetentBreakAngle);
		Entry.SlipChance = FMath::Clamp(Entry.SlipChance, 0.f, 1.f);
		Entry.TurnInput = FMath::Clamp(Entry.TurnInput, 0.f, 1.f);
		Entry.SpeedMultiplier = FMath::Clamp(Entry.SpeedMultiplier, 0.f, 1.f);
		if (I && Entry.WingAngle < States[I - 1].WingAngle)
			UE_LOG(LogTemp, Warning, TEXT("%s: wing angles are not monotonically increasing."), *GetPathName());
	}
	const float Direction = FMath::Sign(States.Last().LeverAngle - States[0].LeverAngle);
	for (int32 I = 1; I < States.Num(); ++I)
		if (Direction == 0.f || (States[I].LeverAngle - States[I - 1].LeverAngle) * Direction <= 0.f)
			UE_LOG(LogTemp, Warning, TEXT("%s: lever angles must be strictly monotonic."), *GetPathName());
	LastDetent = FMath::Clamp(LastDetent, 0, States.Num() - 1);
	Position = TargetPosition = float(LastDetent);
	LeverAngle = States[LastDetent].LeverAngle;
	WingAngle = States[LastDetent].WingAngle;
	CurrentTurn = States[LastDetent].TurnInput;
	CurrentMultiplier = States[LastDetent].SpeedMultiplier;
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
	TargetPosition = float(LastDetent = NewState);
	MoveSpeedOverride = 0.f;
	HeldDetent = INDEX_NONE;
	EscapedDetent = INDEX_NONE;
	DetentStrain = 0.f;
	if (LastDetent == 0) StepDirection = 1;
	else if (LastDetent == States.Num() - 1) StepDirection = -1;
}

void UBalloonWingLeverComponent::StepState()
{
	if (States.Num() < 2) return;
	SetState(StepMode == EWingLeverStepMode::Cycle ? (LastDetent + 1) % States.Num() : LastDetent + StepDirection);
}

bool UBalloonWingLeverComponent::TrySlip(float Chance)
{
	if ((bGrabbed && !bSlipWhileGrabbed) || LastDetent == 0 || !States.IsValidIndex(LastDetent)) return false;
	if (!FMath::IsFinite(Chance) || Chance <= 0.f) return false;
	return (Chance >= 1.f || SlipRandom.FRand() < Chance) && Slip();
}

bool UBalloonWingLeverComponent::Slip()
{
	if ((bGrabbed && !bSlipWhileGrabbed) || LastDetent == 0 || !States.IsValidIndex(LastDetent)) return false;
	const int32 From = LastDetent;
	if (bGrabbed) IGrabbable::Execute_EndGrab(this, GrabPawn.Get());
	SetState(0);
	MoveSpeedOverride = FMath::Max(.01f, SlipMoveSpeed);
	SlipCheckElapsed = 0.f;
	OnSlipped.Broadcast(From, 0);
	return true;
}

void UBalloonWingLeverComponent::TickComponent(float Dt, ELevelTick Type, FActorComponentTickFunction* Tick)
{
	Super::TickComponent(Dt, Type, Tick);
	if (States.Num() < 2) return;
	if (bGrabbed && HeldDetent != INDEX_NONE)
		DetentStrain = FMath::FInterpConstantTo(DetentStrain, 0.f, Dt, FMath::Max(0.f, DetentStrainDecay));
	if (!bGrabbed)
		LeverAngle = FMath::FInterpConstantTo(LeverAngle, SampleByPosition(TargetPosition).LeverAngle, Dt,
			FMath::Max(.01f, MoveSpeedOverride > 0.f ? MoveSpeedOverride : LeverMoveSpeed));
	Position = PositionFromLeverAngle(LeverAngle);
	if (!bGrabbed && LeverAngle == SampleByPosition(TargetPosition).LeverAngle)
		MoveSpeedOverride = 0.f;
	SlipCheckElapsed += FMath::Max(0.f, Dt);
	const float CheckInterval = FMath::Max(.01f, SlipCheckInterval);
	if (SlipCheckElapsed >= CheckInterval)
	{
		SlipCheckElapsed = FMath::Fmod(SlipCheckElapsed, CheckInterval);
		// Free release may rest between detents; only a settled, remembered detent rolls.
		if (!bGrabbed && States.IsValidIndex(LastDetent) &&
			Position == TargetPosition && Position == float(LastDetent))
			TrySlip(States[LastDetent].SlipChance);
	}
	WingAngle = FMath::FInterpConstantTo(WingAngle, SampleByPosition(TargetPosition).WingAngle, Dt, FMath::Max(.01f, WingMoveSpeed));
	const auto Effect = SampleEffectByWingAngle(WingAngle);
	CurrentTurn = Effect.TurnInput;
	CurrentMultiplier = Effect.SpeedMultiplier;
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

#if WITH_EDITOR
void UBalloonWingLeverComponent::PostEditChangeProperty(FPropertyChangedEvent& Event)
{
	const FName Name = Event.GetPropertyName();
	if (Name == GET_MEMBER_NAME_CHECKED(UBalloonWingLeverComponent, DetentBreakAngle) ||
		Name == GET_MEMBER_NAME_CHECKED(UBalloonWingLeverComponent, MaxSlipChance) ||
		Name == GET_MEMBER_NAME_CHECKED(UBalloonWingLeverComponent, NumStates) ||
		Name == GET_MEMBER_NAME_CHECKED(UBalloonWingLeverComponent, LeverClosedAngle) ||
		Name == GET_MEMBER_NAME_CHECKED(UBalloonWingLeverComponent, LeverOpenAngle) ||
		Name == GET_MEMBER_NAME_CHECKED(UBalloonWingLeverComponent, MaxWingAngle) ||
		Name == GET_MEMBER_NAME_CHECKED(UBalloonWingLeverComponent, MaxTurnInput) ||
		Name == GET_MEMBER_NAME_CHECKED(UBalloonWingLeverComponent, OpenSpeedMultiplier)) RegenerateStates();
	Super::PostEditChangeProperty(Event);
}
#endif

FBalloonWingState UBalloonWingLeverComponent::SampleByPosition(float S) const
{
	if (States.IsEmpty()) return FBalloonWingState();
	S = FMath::Clamp(S, 0.f, float(States.Num() - 1));
	const int32 I = FMath::FloorToInt(S);
	const auto& A = States[I];
	const auto& B = States[FMath::Min(I + 1, States.Num() - 1)];
	const float T = S - I;
	FBalloonWingState Result = A;
	Result.LeverAngle = FMath::Lerp(A.LeverAngle, B.LeverAngle, T);
	Result.WingAngle = FMath::Lerp(A.WingAngle, B.WingAngle, T);
	Result.TurnInput = FMath::Lerp(A.TurnInput, B.TurnInput, T);
	Result.SpeedMultiplier = FMath::Lerp(A.SpeedMultiplier, B.SpeedMultiplier, T);
	Result.SlipChance = FMath::Lerp(A.SlipChance, B.SlipChance, T);
	return Result;
}

float UBalloonWingLeverComponent::PositionFromLeverAngle(float Angle) const
{
	if (States.Num() < 2) return 0.f;
	const bool bIncreasing = States.Last().LeverAngle > States[0].LeverAngle;
	if (bIncreasing ? Angle <= States[0].LeverAngle : Angle >= States[0].LeverAngle) return 0.f;
	for (int32 I = 1; I < States.Num(); ++I)
	{
		if (bIncreasing ? Angle <= States[I].LeverAngle : Angle >= States[I].LeverAngle)
		{
			const float Span = States[I].LeverAngle - States[I - 1].LeverAngle;
			return I - 1 + (FMath::IsNearlyZero(Span) ? 0.f : FMath::Clamp((Angle - States[I - 1].LeverAngle) / Span, 0.f, 1.f));
		}
	}
	return float(States.Num() - 1);
}

FBalloonWingState UBalloonWingLeverComponent::SampleEffectByWingAngle(float Angle) const
{
	if (States.Num() < 2 || Angle <= States[0].WingAngle) return SampleByPosition(0.f);
	for (int32 I = 1; I < States.Num(); ++I)
	{
		if (Angle <= States[I].WingAngle)
		{
			const float Span = States[I].WingAngle - States[I - 1].WingAngle;
			return SampleByPosition(I - 1 + (FMath::IsNearlyZero(Span) ? 0.f : FMath::Clamp((Angle - States[I - 1].WingAngle) / Span, 0.f, 1.f)));
		}
	}
	return SampleByPosition(float(States.Num() - 1));
}

float UBalloonWingLeverComponent::GetLeverAngle() const
{
	const float Break = States.IsValidIndex(HeldDetent) ? FMath::Max(0.f, States[HeldDetent].DetentBreakAngle) : 0.f;
	return LeverAngle + FMath::Clamp(DetentStrain, -Break, Break) * DetentGive;
}

bool UBalloonWingLeverComponent::CanGrab_Implementation(APawn* Pawn, const FHitResult& Hit) const
{
	const auto* Player = Cast<AFirstPersonCameraPawn>(Pawn);
	return AcceptsHit_Implementation(Hit) && IsActive() && IsValid(Flight) && States.Num() >= 2 &&
		Player && Player->IsHandEmpty() && !bGrabbed;
}

void UBalloonWingLeverComponent::BeginGrab_Implementation(APawn* Pawn, const FHitResult& Hit)
{
	if (!CanGrab_Implementation(Pawn, Hit)) return;
	USkeletalMeshComponent* Mesh = Cast<USkeletalMeshComponent>(Hit.GetComponent());
	if (!Mesh || Mesh->GetBoneIndex(LeverBone) == INDEX_NONE)
	{
		Mesh = nullptr;
		TInlineComponentArray<USkeletalMeshComponent*> Meshes(GetOwner());
		for (auto* Candidate : Meshes)
			if (Candidate->GetBoneIndex(LeverBone) != INDEX_NONE) { Mesh = Candidate; break; }
	}
	if (!Mesh) return;
	GrabMesh = Mesh;
	GrabPawn = Pawn;
	LocalGrabPoint = Mesh->GetSocketTransform(LeverBone, RTS_World).InverseTransformPosition(Hit.ImpactPoint);
	bGrabbed = true;
	MoveSpeedOverride = 0.f;
	EscapedDetent = INDEX_NONE;
	DetentStrain = 0.f;
	Position = TargetPosition = PositionFromLeverAngle(LeverAngle);
	HeldDetent = INDEX_NONE;
	for (int32 I = 0; I < States.Num(); ++I)
		if (FMath::IsNearlyEqual(LeverAngle, States[I].LeverAngle)) { HeldDetent = LastDetent = I; break; }
}

FVector UBalloonWingLeverComponent::GetGrabLocation_Implementation() const
{
	return GrabMesh.IsValid() ? GrabMesh->GetSocketTransform(LeverBone, RTS_World).TransformPosition(LocalGrabPoint) : GetOwner()->GetActorLocation();
}

void UBalloonWingLeverComponent::UpdateGrab_Implementation(APawn* Pawn, FVector Delta)
{
	if (!bGrabbed || GrabPawn.Get() != Pawn || !GrabMesh.IsValid() || Delta.ContainsNaN()) return;
	const FTransform Bone = GrabMesh->GetSocketTransform(LeverBone, RTS_World);
	const FVector Offset = Bone.TransformPosition(LocalGrabPoint) - Bone.GetLocation();
	const FVector Tangent = FVector::CrossProduct(Bone.TransformVectorNoScale(LeverAxis).GetSafeNormal(), Offset).GetSafeNormal();
	float Radius = Offset.Size();
	if (Radius < MinGrabRadius) Radius = HandleLength;
	if (Radius <= UE_SMALL_NUMBER) return;
	const float MaxDelta = FMath::Max(0.f, LeverDragMaxSpeed) * GetWorld()->GetDeltaSeconds();
	const float DeltaAngle = FMath::Clamp(FMath::RadiansToDegrees(FVector::DotProduct(Delta, Tangent) / Radius) * DragSign, -MaxDelta, MaxDelta);
	ApplyDetents(LeverAngle, DeltaAngle);
	Position = PositionFromLeverAngle(LeverAngle);
	TargetPosition = HeldDetent != INDEX_NONE ? float(HeldDetent) : Position;
}

void UBalloonWingLeverComponent::ApplyDetents(float OldAngle, float DeltaAngle)
{
	if (States.Num() < 2 || FMath::IsNearlyZero(DeltaAngle)) return;
	int32 Escaped = INDEX_NONE;
	if (States.IsValidIndex(EscapedDetent) &&
		FMath::IsNearlyEqual(OldAngle, States[EscapedDetent].LeverAngle) && FMath::Sign(DeltaAngle) == EscapeDirection)
		Escaped = EscapedDetent;
	else EscapedDetent = INDEX_NONE;
	if (States.IsValidIndex(HeldDetent))
	{
		const float D = States[HeldDetent].LeverAngle;
		const float Break = FMath::Max(0.f, States[HeldDetent].DetentBreakAngle);
		DetentStrain += DeltaAngle;
		const float Direction = FMath::Sign(DetentStrain);
		const bool bAtStop = Direction > 0.f ? D >= FMath::Max(States[0].LeverAngle, States.Last().LeverAngle) : D <= FMath::Min(States[0].LeverAngle, States.Last().LeverAngle);
		LeverAngle = D;
		if (FMath::Abs(DetentStrain) < Break || bAtStop)
		{
			DetentStrain = FMath::Clamp(DetentStrain, -Break, Break);
			return;
		}
		Escaped = EscapedDetent = HeldDetent;
		EscapeDirection = Direction;
		HeldDetent = INDEX_NONE;
		OldAngle = D;
		DeltaAngle = Direction * (FMath::Abs(DetentStrain) - Break);
		DetentStrain = 0.f;
	}
	const float End = OldAngle + DeltaAngle;
	int32 First = INDEX_NONE;
	float Distance = TNumericLimits<float>::Max();
	for (int32 I = 0; I < States.Num(); ++I)
	{
		const float D = States[I].LeverAngle;
		if (I != Escaped && D >= FMath::Min(OldAngle, End) && D <= FMath::Max(OldAngle, End) && FMath::Abs(D - OldAngle) < Distance)
		{
			First = I;
			Distance = FMath::Abs(D - OldAngle);
		}
	}
	if (First != INDEX_NONE)
	{
		LeverAngle = States[First].LeverAngle;
		HeldDetent = LastDetent = First;
		DetentStrain = 0.f;
	}
	else LeverAngle = FMath::Clamp(End, FMath::Min(States[0].LeverAngle, States.Last().LeverAngle), FMath::Max(States[0].LeverAngle, States.Last().LeverAngle));
}

void UBalloonWingLeverComponent::EndGrab_Implementation(APawn* Pawn)
{
	if (!bGrabbed || GrabPawn.Get() != Pawn) return;
	Position = PositionFromLeverAngle(LeverAngle);
	TargetPosition = Position;
	if (ReleaseMode == EWingLeverReleaseMode::ReturnToLastDetent) TargetPosition = float(LastDetent);
	else if (ReleaseMode == EWingLeverReleaseMode::SnapToNearestDetent)
	{
		int32 Nearest = 0;
		for (int32 I = 1; I < States.Num(); ++I)
			if (FMath::Abs(States[I].LeverAngle - LeverAngle) < FMath::Abs(States[Nearest].LeverAngle - LeverAngle)) Nearest = I;
		TargetPosition = float(Nearest);
	}
	bGrabbed = false;
	HeldDetent = INDEX_NONE;
	EscapedDetent = INDEX_NONE;
	DetentStrain = 0.f;
	GrabPawn.Reset();
	GrabMesh.Reset();
}
