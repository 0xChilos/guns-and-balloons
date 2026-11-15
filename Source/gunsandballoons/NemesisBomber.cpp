#include "NemesisBomber.h"

#include "NemesisTargetSubsystem.h"
#include "NemesisMine.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"
#include "NemesisPlaceholder.h"

ANemesisBomber::ANemesisBomber()
{
	MineClass = ANemesisMine::StaticClass();
	// Bigger and slower than the crop duster.
	Fuselage->SetRelativeScale3D(FVector(5.f, 1.4f, 1.2f));
	Wings->SetRelativeScale3D(FVector(1.6f, 11.f, 0.2f));
	Engine->SetRelativeLocation(FVector(300.f, 0.f, 0.f));
	Engine->SetRelativeScale3D(FVector(0.8f, 1.4f, 1.2f));
	MaxSpeed = 2000.f;

	// The bomber always flies ahead of the balloon tail-first, so its nose engine hides behind the
	// fuselage. Two wing engines stick out where the player sees them from behind; a hit on any
	// engine counts toward the same engine HP.
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Glow(TEXT("/Game/Assets/Nemesis/MI_Nemesis_Glow.MI_Nemesis_Glow"));
	UStaticMeshComponent* WingEngines[2];
	int32 k = 0;
	for (const float Side : { -1.f, 1.f })
	{
		UStaticMeshComponent* WingEngine = MakeCube(Side < 0.f ? TEXT("EngineL") : TEXT("EngineR"),
			FVector(60.f, 300.f * Side, 0.f), FVector(1.4f, 0.9f, 0.9f), Glow.Object);
		EngineZones.Add(WingEngine);
		WingEngines[k++] = WingEngine;
	}

	// Model (Content/Assets/Enemies/Bomber.blend). Wing engines: Blender +Y (EngineL) = Unreal -Y.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> BodyMesh(TEXT("/Game/Assets/Enemies/FBX/SM_Bomber_Body.SM_Bomber_Body"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> NoseMesh(TEXT("/Game/Assets/Enemies/FBX/SM_Bomber_EngineNose.SM_Bomber_EngineNose"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> LMesh(TEXT("/Game/Assets/Enemies/FBX/SM_Bomber_EngineL.SM_Bomber_EngineL"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> RMesh(TEXT("/Game/Assets/Enemies/FBX/SM_Bomber_EngineR.SM_Bomber_EngineR"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> PropNose(TEXT("/Game/Assets/Enemies/FBX/SM_Bomber_PropNose.SM_Bomber_PropNose"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> PropWing(TEXT("/Game/Assets/Enemies/FBX/SM_Bomber_PropWing.SM_Bomber_PropWing"));
	if (NemesisModel::Use(Fuselage, BodyMesh.Object))
	{
		NemesisModel::Use(Engine, NoseMesh.Object);
		NemesisModel::Use(WingEngines[0], LMesh.Object);
		NemesisModel::Use(WingEngines[1], RMesh.Object);
		NemesisCollision::None(Wings);
		Wings->SetVisibility(false);
		const TPair<const TCHAR*, FVector> PropSpots[] = {
			{ TEXT("PropNose"), FVector(342.f, 0.f, 0.f) }, { TEXT("PropL"), FVector(197.f, -300.f, -12.f) }, { TEXT("PropR"), FVector(197.f, 300.f, -12.f) } };
		for (int32 i = 0; i < 3; ++i)
		{
			if (UStaticMeshComponent* P = MakePart(PropSpots[i].Key, i == 0 ? PropNose.Object : PropWing.Object, PropSpots[i].Value))
			{
				Props.Add(P);
				EngineZones.Add(P);
			}
		}
	}
}

void ANemesisBomber::Launch(ANemesisAirship* InAirship)
{
	Super::Launch(InAirship);
	// First mine comes a bit sooner than the rest, once the bomber is in place.
	DropTimer = DropIntervalMin * 0.5f;
	Lane = FMath::RandRange(-1, 1);
}

void ANemesisBomber::PickNextLane()
{
	// Always a different release point than last time, so mines don't line up.
	int32 Next = Lane;
	while (Next == Lane) Next = FMath::RandRange(-1, 1);
	Lane = Next;
}

FVector ANemesisBomber::GetSlotLocation() const
{
	UNemesisTargetSubsystem* T = UNemesisTargetSubsystem::Get(this);
	const AActor* B = T ? T->GetBalloon() : nullptr;
	if (!B) return GetActorLocation();
	// Fixed slot on the balloon's course: once there, the bomber never changes position.
	const FVector Right = FVector::CrossProduct(FVector::UpVector, GetBalloonForward());
	return B->GetActorLocation() + GetBalloonForward() * AheadDistance + Right * SlotSideOffset;
}

void ANemesisBomber::TickAttack(float DeltaSeconds)
{
	Age += DeltaSeconds;
	if (LifeTime > 0.f && Age > LifeTime)
	{
		Leave();
		return;
	}

	DropTimer -= DeltaSeconds;
	if (DropTimer > 0.f || !IsInSlot(SlotTolerance)) return;

	UNemesisTargetSubsystem* A = UNemesisTargetSubsystem::Get(this);
	if (!MineClass || !A || !A->CanSpawnMine())
	{
		DropTimer = 0.5f; // limit reached: try again shortly
		return;
	}
	DropTimer = FMath::FRandRange(DropIntervalMin, DropIntervalMax);

	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	// Released from the left wing, the belly or the right wing (a different one each time), so the
	// mines land left of, on and right of the balloon's course while the bomber holds still.
	const FVector Right = FVector::CrossProduct(FVector::UpVector, GetBalloonForward());
	const FVector At = GetActorLocation() - GetActorForwardVector() * 400.f + Right * (LaneOffset * Lane) - FVector(0.f, 0.f, 300.f);
	if (ANemesisMine* Mine = GetWorld()->SpawnActor<ANemesisMine>(MineClass, At, FRotator(0.f, GetActorRotation().Yaw, 0.f), Params))
	{
		// Time the descent so the mine is at basket height when the balloon gets there.
		const FVector Basket = A->GetBasketLocation();
		const float Ahead = FVector::DotProduct(At - Basket, GetBalloonForward());
		const float BalloonSpeed = FMath::Max(A->GetBalloonVelocity().Size(), 30.f);
		const float Drop = At.Z - Basket.Z;
		Mine->Arm(Ahead > 0.f ? Drop * BalloonSpeed / Ahead : 0.f);
		A->RegisterMine(Mine);
		OnMineDropped(Mine);
	}
	PickNextLane();
}
