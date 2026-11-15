#include "ExtinguisherComponent.h"
#include "StoveComponent.h"
#include "Engine/World.h"

#include "Animation/AnimSequenceBase.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Texture.h"
#include "ExtinguisherFoam.h"
#include "GameFramework/Actor.h"
#include "GunAnimInstance.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	/** SK_Extinguisher counter drums (SK_Extinguisher_Skeleton): ones, tens, hundreds. */
	const FName DrumBones[3] = { TEXT("digit_1"), TEXT("digit_10"), TEXT("digit_100") };

	/** Fastest (cm/s) the nozzle's own motion is passed on to the foam, so a snap of the view doesn't fling it. */
	constexpr float MaxFoamSourceSpeed = 1500.f;
}

UExtinguisherComponent::UExtinguisherComponent()
{
	// Imported from SK_Extinguisher.fbx: every take is bound to SK_Extinguisher_Skeleton.
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> Equip(TEXT("/Game/Assets/Fire_extinguisher/SK_ExtinguisherSK_Extinguisher_Equip.SK_ExtinguisherSK_Extinguisher_Equip"));
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> Unequip(TEXT("/Game/Assets/Fire_extinguisher/SK_ExtinguisherSK_Extinguisher_Unequip.SK_ExtinguisherSK_Extinguisher_Unequip"));
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> Idle(TEXT("/Game/Assets/Fire_extinguisher/SK_ExtinguisherSK_Extinguisher_Idle.SK_ExtinguisherSK_Extinguisher_Idle"));
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> SprayStart(TEXT("/Game/Assets/Fire_extinguisher/SK_ExtinguisherSK_Extinguisher_Spray_Start.SK_ExtinguisherSK_Extinguisher_Spray_Start"));
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> SprayLoop(TEXT("/Game/Assets/Fire_extinguisher/SK_ExtinguisherSK_Extinguisher_Spray_Loop.SK_ExtinguisherSK_Extinguisher_Spray_Loop"));
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> SprayStop(TEXT("/Game/Assets/Fire_extinguisher/SK_ExtinguisherSK_Extinguisher_Spray_Stop.SK_ExtinguisherSK_Extinguisher_Spray_Stop"));
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> Empty(TEXT("/Game/Assets/Fire_extinguisher/SK_ExtinguisherSK_Extinguisher_Empty.SK_ExtinguisherSK_Extinguisher_Empty"));
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> CounterZero(TEXT("/Game/Assets/Fire_extinguisher/SK_ExtinguisherSK_Extinguisher_Counter_Ref_000.SK_ExtinguisherSK_Extinguisher_Counter_Ref_000"));
	static ConstructorHelpers::FObjectFinder<UTexture> Texture(TEXT("/Game/Textures/Fire_extinguisher/T_Extinguisher_256.T_Extinguisher_256"));

	EquipAnim = Equip.Object;
	UnequipAnim = Unequip.Object;
	IdleAnim = Idle.Object;
	SprayStartAnim = SprayStart.Object;
	SprayLoopAnim = SprayLoop.Object;
	SprayStopAnim = SprayStop.Object;
	EmptyAnim = Empty.Object;
	CounterZeroAnim = CounterZero.Object;
	WeaponTexture = Texture.Object;

	// No powder, no casings, no bullets: the "smoke" actor draws the foam instead.
	SmokeClass = AExtinguisherFoam::StaticClass();
	CasingMesh = nullptr;

	// SK_Extinguisher's root is the carry handle on top; the tank hangs 44cm below it and the nozzle points
	// forward (+X) 13cm down. Held on the right, high enough that the counter on the tank's left side is in
	// view, turned in a little so the jet heads for the crosshair. First-pass pose, needs tuning in game.
	ViewOffset = FVector(34.f, 16.f, -3.f);
	ViewRotation = FRotator(0.f, -4.f, 0.f);
	ReloadViewOffset = ViewOffset;
	ReloadViewRotation = ViewRotation;
	// A tool, not a gun: nothing to aim down.
	bCanAimDownSights = false;

	// No kick; a heavy tank sways slowly in the hand.
	RecoilKickBack = 0.f;
	RecoilKickPitch = 0.f;
	HandSwayPitch = 0.5f;
	HandSwayYaw = 0.6f;
	HandSwaySpeed = 0.5f;

	// Foam: a thick white jet that swells as it leaves the nozzle (a hand's width at once, ~45cm clumps a
	// couple of metres out), falls in an arc (~3-4 m on the level) and piles up where it lands for a while.
	// First-pass values, need tuning in game.
	FoamSpray.Amount = 90.f;
	FoamSpray.SpeedMin = 900.f;
	FoamSpray.SpeedMax = 1200.f;
	FoamSpray.ConeAngle = 7.f;
	FoamSpray.StartRadius = 4.f;
	FoamSpray.EndRadius = 22.f;
	FoamSpray.GrowTime = 0.3f;
	// Foam doesn't thin out as it spreads like smoke: thick enough to stay nearly solid at full size.
	FoamSpray.Density = 80.f;
	FoamSpray.LifeMin = 1.2f;
	FoamSpray.LifeMax = 1.7f;
	FoamSpray.Buoyancy = 0.f;
	FoamSpray.InheritVelocity = 0.8f;
	FoamSpray.Gravity = 980.f;
	FoamSpray.StickTime = 5.f;

	// The cloud around it: wide, slow, big and see-through, it billows out and is gone in about a second.
	FoamMist.Amount = 25.f;
	FoamMist.SpeedMin = 450.f;
	FoamMist.SpeedMax = 800.f;
	FoamMist.ConeAngle = 18.f;
	FoamMist.StartRadius = 8.f;
	FoamMist.EndRadius = 45.f;
	FoamMist.GrowTime = 0.45f;
	FoamMist.Density = 10.f;
	FoamMist.LifeMin = 0.8f;
	FoamMist.LifeMax = 1.3f;
	FoamMist.Buoyancy = 0.f;
	FoamMist.InheritVelocity = 0.8f;
	FoamMist.Gravity = 150.f;
	FoamMist.StickTime = 0.f;
}

void UExtinguisherComponent::BeginPlay()
{
	Super::BeginPlay();

	// The pawn places the extinguisher in front of the eyes first, so the foam leaves from where the nozzle is now.
	if (AActor* Owner = GetOwner())
	{
		PrimaryComponentTick.AddPrerequisite(Owner, Owner->PrimaryActorTick);
	}

	Amount = FMath::Clamp(InitialAmount, 0.f, 100.f);
	DisplayedCount = GetCounterTarget();
	// The pawn equips (Equip clip) or holsters it right after this.
	SetAction(EExtinguisherAction::Idle, IdleAnim);
	UpdatePose();
}

// --- Input -----------------------------------------------------------------------------------

void UExtinguisherComponent::SetAmount(float NewAmount)
{
	Amount = FMath::Clamp(NewAmount, 0.f, 100.f);
}

bool UExtinguisherComponent::IsSpraying() const
{
	return (Action == EExtinguisherAction::SprayStart || Action == EExtinguisherAction::Spray) && HasAmount();
}

void UExtinguisherComponent::PullTrigger()
{
	// With something left, spraying follows the held trigger (SetTriggerHeld). Empty: the lever only sputters.
	if (!HasAmount() && !bHolstering && (Action == EExtinguisherAction::Idle || Action == EExtinguisherAction::SprayStop))
	{
		SetAction(EExtinguisherAction::Empty, EmptyAnim);
		OnDryFire.Broadcast();
	}
}

void UExtinguisherComponent::SetTriggerHeld(bool bHeld)
{
	bTriggerHeld = bHeld;
}

bool UExtinguisherComponent::ShouldUseInsteadOfInteract(const FHitResult& Hit) const
{
	if (AActor* HitActor = Hit.GetActor())
	{
		TInlineComponentArray<UStoveComponent*> Stoves(HitActor);
		for (const UStoveComponent* Stove : Stoves)
			if (Stove->AcceptsCoolingHit(Hit)) return true;
	}
	return false;
}

void UExtinguisherComponent::SetEquipped(bool bInEquipped)
{
	Super::SetEquipped(bInEquipped);

	bTriggerHeld = false;
	bHasLastTip = false;
	FoamOwed[0] = FoamOwed[1] = 0.f;

	if (bInEquipped)
	{
		// Brings itself up into view; the counter shows what is left.
		bHolstering = false;
		DisplayedCount = GetCounterTarget();
		SetAction(EExtinguisherAction::Equip, EquipAnim);
	}
	else
	{
		// Put away: rests at the end of Unequip, out of view.
		SetAction(EExtinguisherAction::Unequip, UnequipAnim);
		ActionTime = AnimLength(UnequipAnim);
		if (bWasSpraying)
		{
			bWasSpraying = false;
			OnSprayStopped.Broadcast();
		}
	}
	UpdatePose();
}

void UExtinguisherComponent::PrepareToHolster()
{
	// Let go of the lever; once it is back up the extinguisher takes itself out of view (Tick).
	bHolstering = true;
	bTriggerHeld = false;
}

void UExtinguisherComponent::CancelHolster()
{
	bHolstering = false;
	if (Action == EExtinguisherAction::Unequip)
	{
		// Come back up from as far down as it got.
		const float UnequipLength = AnimLength(UnequipAnim);
		const float Progress = UnequipLength > 0.f ? FMath::Clamp(ActionTime / UnequipLength, 0.f, 1.f) : 1.f;
		SetAction(EExtinguisherAction::Equip, EquipAnim);
		ActionTime = (1.f - Progress) * AnimLength(EquipAnim);
	}
}

bool UExtinguisherComponent::IsReadyToHolster() const
{
	return Action == EExtinguisherAction::Unequip && IsActionDone();
}

// --- Tick ------------------------------------------------------------------------------------

void UExtinguisherComponent::SetAction(EExtinguisherAction NewAction, UAnimSequenceBase* Anim)
{
	Action = NewAction;
	ActionAnim = Anim;
	ActionTime = 0.f;
}

bool UExtinguisherComponent::IsActionDone() const
{
	return ActionTime >= AnimLength(ActionAnim);
}

float UExtinguisherComponent::GetCounterTarget() const
{
	// A part-used unit still shows: 100 until the first one is gone, 1 until the very end.
	return FMath::Max(0.f, FMath::CeilToFloat(Amount - 1.e-3f));
}

void UExtinguisherComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	ActionTime += DeltaTime;
	const bool bWantSpray = bTriggerHeld && !bHolstering && HasAmount();

	switch (Action)
	{
	case EExtinguisherAction::Equip:
	case EExtinguisherAction::Empty:
		if (IsActionDone())
		{
			SetAction(EExtinguisherAction::Idle, IdleAnim);
		}
		break;

	case EExtinguisherAction::Idle:
		if (bHolstering)
		{
			SetAction(EExtinguisherAction::Unequip, UnequipAnim);
		}
		else if (bWantSpray)
		{
			SetAction(EExtinguisherAction::SprayStart, SprayStartAnim);
		}
		break;

	case EExtinguisherAction::SprayStart:
		if (!bWantSpray)
		{
			SetAction(EExtinguisherAction::SprayStop, SprayStopAnim);
		}
		else if (IsActionDone())
		{
			SetAction(EExtinguisherAction::Spray, SprayLoopAnim);
		}
		break;

	case EExtinguisherAction::Spray:
		if (!bWantSpray)
		{
			SetAction(EExtinguisherAction::SprayStop, SprayStopAnim);
		}
		break;

	case EExtinguisherAction::SprayStop:
		if (bWantSpray)
		{
			SetAction(EExtinguisherAction::SprayStart, SprayStartAnim);
		}
		else if (IsActionDone())
		{
			SetAction(EExtinguisherAction::Idle, IdleAnim);
		}
		break;

	case EExtinguisherAction::Unequip:
		// Holds its last frame until the pawn swaps (or the swap is called off: CancelHolster).
		break;
	}

	UpdateFlow(DeltaTime);

	DisplayedCount = FMath::FInterpConstantTo(DisplayedCount, GetCounterTarget(), DeltaTime, CounterRollSpeed);
	UpdatePose();
}

void UExtinguisherComponent::UpdateFlow(float DeltaTime)
{
	if (IsSpraying())
	{
		// The final partly filled frame only sprays for the contents actually remaining.
		const float SprayTime = UsePerSecond > 0.f ? FMath::Min(DeltaTime, Amount / UsePerSecond) : DeltaTime;
		EmitFoam(SprayTime);
		Amount = FMath::Max(0.f, Amount - UsePerSecond * DeltaTime);
		if (Amount <= 0.f)
		{
			// Ran dry mid-spray: the lever sputters on nothing.
			SetAction(EExtinguisherAction::Empty, EmptyAnim);
			OnEmptied.Broadcast();
		}
	}
	else
	{
		bHasLastTip = false;
		FoamOwed[0] = FoamOwed[1] = 0.f;
	}

	const bool bSpraying = IsSpraying();
	if (bSpraying != bWasSpraying)
	{
		bWasSpraying = bSpraying;
		(bSpraying ? OnSprayStarted : OnSprayStopped).Broadcast();
	}
}

void UExtinguisherComponent::EmitFoam(float DeltaTime)
{
	if (!Mesh || DeltaTime <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	// Out of the nozzle tip, along the nozzle (it shakes a little in the spray clip), where the first-person
	// mesh appears in the world.
	const FVector Tip = FirstPersonToWorld(Mesh->GetSocketLocation(NozzleTipBone));
	const FVector Back = FirstPersonToWorld(Mesh->GetSocketLocation(NozzleBone));
	const FVector Fallback = Camera ? Camera->GetForwardVector() : GetOwner()->GetActorForwardVector();
	const FVector Direction = (Tip - Back).GetSafeNormal(UE_KINDA_SMALL_NUMBER, Fallback);
	ApplySprayCooling(Tip, Direction, DeltaTime);
	AGunSmoke* Foam = GetSmoke();
	if (!Foam) return;

	// The nozzle's own motion (the view turning, the balloon flying) carries over to the foam.
	const FVector From = bHasLastTip ? LastTipLocation : Tip;
	const FVector SourceVelocity = bHasLastTip ? ((Tip - From) / DeltaTime).GetClampedToMaxSize(MaxFoamSourceSpeed) : FVector::ZeroVector;
	LastTipLocation = Tip;
	bHasLastTip = true;

	// The cloud around the jet, then the jet itself.
	const FGunSmokePuffs* Kinds[2] = { &FoamMist, &FoamSpray };
	for (int32 Kind = 0; Kind < 2; ++Kind)
	{
		const FGunSmokePuffs& Puffs = *Kinds[Kind];
		FoamOwed[Kind] += Puffs.Amount * DeltaTime;
		const int32 Count = FMath::FloorToInt(FoamOwed[Kind]);
		if (Count <= 0)
		{
			continue;
		}
		FoamOwed[Kind] -= Count;

		// Spread along the path the nozzle took this frame, so a sweep leaves a continuous jet, not clumps.
		for (int32 k = 0; k < Count; ++k)
		{
			Foam->Emit(Puffs, 1, FMath::Lerp(From, Tip, float(k + 1) / Count), Direction, SourceVelocity);
		}
	}
}

void UExtinguisherComponent::ApplySprayCooling(const FVector& Tip, const FVector& Direction, float DeltaTime)
{
	if (!IsSpraying() || !GetWorld() || !FMath::IsFinite(DeltaTime) || DeltaTime <= 0.f
		|| !FMath::IsFinite(StoveCoolingPerSecond) || StoveCoolingPerSecond <= 0.f
		|| !FMath::IsFinite(StoveCoolingRange) || StoveCoolingRange <= 0.f) return;
	FHitResult Hit;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(ExtinguisherCooling), false, GetOwner());
	if (!GetWorld()->LineTraceSingleByChannel(Hit, Tip, Tip + Direction.GetSafeNormal() * StoveCoolingRange,
		CoolingTraceChannel, Query)) return;
	if (AActor* HitActor = Hit.GetActor())
	{
		TInlineComponentArray<UStoveComponent*> Stoves(HitActor);
		for (UStoveComponent* Stove : Stoves)
		{
			if (Stove->AcceptsCoolingHit(Hit))
			{
				Stove->ApplyCooling(StoveCoolingPerSecond * DeltaTime);
				break;
			}
		}
	}
}

// --- Pose ------------------------------------------------------------------------------------

FVector UExtinguisherComponent::GetDrumDigits(float Value)
{
	const float V = FMath::Clamp(Value, 0.f, 999.f);
	const float Ones = FMath::Fmod(V, 10.f);
	// A drum rolls on to its next digit while every drum below it goes from 9 to 0.
	const float Carry = FMath::Max(0.f, Ones - 9.f);
	const float TensWhole = FMath::Fmod(FMath::FloorToFloat(V / 10.f), 10.f);
	const float HundredsWhole = FMath::Fmod(FMath::FloorToFloat(V / 100.f), 10.f);
	return FVector(Ones, TensWhole + Carry, HundredsWhole + (TensWhole >= 9.f ? Carry : 0.f));
}

void UExtinguisherComponent::UpdatePose()
{
	UGunAnimInstance* Anim = Mesh ? Cast<UGunAnimInstance>(Mesh->GetAnimInstance()) : nullptr;
	if (!Anim)
	{
		return;
	}

	Anim->Sequence = ActionAnim ? ActionAnim.Get() : IdleAnim.Get();
	float Time = ActionTime;
	if (Action == EExtinguisherAction::Spray)
	{
		const float Length = AnimLength(SprayLoopAnim);
		Time = Length > 0.f ? FMath::Fmod(ActionTime, Length) : 0.f;
	}
	else if (Action == EExtinguisherAction::Idle)
	{
		Time = 0.f;
	}
	Anim->Time = Time;

	// Counter: each drum from its 0 pose, turned about its own +Y by the digit it shows (the clips key them all at 0).
	const FVector Digits = GetDrumDigits(DisplayedCount);
	for (int32 i = 0; i < 3; ++i)
	{
		Anim->SetBonePose(DrumBones[i], CounterZeroAnim, 0.f);
		Anim->SetBoneRotationOffset(DrumBones[i], FQuat(FVector::YAxisVector, FMath::DegreesToRadians(DigitStep * Digits[i])));
	}
}
