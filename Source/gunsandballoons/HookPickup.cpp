#include "HookPickup.h"

#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	/** BasicShapeMaterial's tint. */
	const FName ShapeColorParameter(TEXT("Color"));
	/** MI_Loot / MI_Loot_Glow rim colour. */
	const FName HighlightColorParameter(TEXT("HighlightColor"));
	/** NS_LootSparkle user parameter. */
	const FName SparkColorParameter(TEXT("SparkColor"));
}

AHookPickup::AHookPickup()
{
	PrimaryActorTick.bCanEverTick = false;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	Mesh->SetMobility(EComponentMobility::Movable);
	Mesh->SetCollisionProfileName(UCollisionProfile::PhysicsActor_ProfileName);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShapeMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (Cube.Succeeded())
	{
		Mesh->SetStaticMesh(Cube.Object);
	}
	if (ShapeMaterial.Succeeded())
	{
		Mesh->SetMaterial(0, ShapeMaterial.Object);
	}
	// A 30cm box: a small crate of resources.
	Mesh->SetRelativeScale3D(FVector(0.3f));

	HookCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("HookCollision"));
	HookCollision->SetupAttachment(Mesh);
	HookCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	HookCollision->SetCollisionResponseToAllChannels(ECR_Ignore);
	HookCollision->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	HookCollision->SetGenerateOverlapEvents(false);

	Sparkle = CreateDefaultSubobject<UNiagaraComponent>(TEXT("Sparkle"));
	Sparkle->SetupAttachment(Mesh);
	Sparkle->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Sparkle->SetAutoActivate(true);
}

void AHookPickup::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyColor();
	ApplySparkle();
	UpdateHookCollision();
}

void AHookPickup::BeginPlay()
{
	Super::BeginPlay();
	ApplyColor();
	ApplySparkle();
	ApplyPhysics();
	UpdateHookCollision();
}

void AHookPickup::UpdateHookCollision()
{
	if (!Mesh->GetStaticMesh()) return;
	const FBox Bounds = Mesh->GetStaticMesh()->GetBoundingBox();
	const FVector Scale = Mesh->GetComponentScale().GetAbs().ComponentMax(FVector(UE_KINDA_SMALL_NUMBER));
	HookCollision->SetRelativeLocation(Bounds.GetCenter());
	HookCollision->SetBoxExtent(Bounds.GetExtent() + FVector(FMath::Max(0.f, HookCollisionPadding)) / Scale);
	HookCollision->SetCollisionEnabled(bCarried ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryOnly);
}

void AHookPickup::ApplyColor()
{
	if (!Mesh)
	{
		return;
	}
	for (int32 Index = 0; Index < Mesh->GetNumMaterials(); ++Index)
	{
		if (!Mesh->GetMaterial(Index))
		{
			continue;
		}
		UMaterialInstanceDynamic* Material = Cast<UMaterialInstanceDynamic>(Mesh->GetMaterial(Index));
		if (!Material)
		{
			Material = Mesh->CreateDynamicMaterialInstance(Index);
		}
		if (Material)
		{
			// Each material has at most one of these; setting a missing parameter does nothing.
			Material->SetVectorParameterValue(ShapeColorParameter, Color);
			Material->SetVectorParameterValue(HighlightColorParameter, Color);
		}
	}
}

void AHookPickup::ApplySparkle()
{
	if (!Sparkle)
	{
		return;
	}
	if (Sparkle->GetAsset() != SparkleSystem)
	{
		Sparkle->SetAsset(SparkleSystem);
	}
	Sparkle->SetVisibility(SparkleSystem != nullptr && !bCarried);
	if (SparkleSystem)
	{
		Sparkle->SetVariableLinearColor(SparkColorParameter, Color);
	}
}

void AHookPickup::ApplyPhysics()
{
	Mesh->SetCollisionProfileName(UCollisionProfile::PhysicsActor_ProfileName);
	Mesh->SetSimulatePhysics(!bFloating);
}

void AHookPickup::OnHooked()
{
	bCarried = true;
	Mesh->SetSimulatePhysics(false);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HookCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (Sparkle)
	{
		Sparkle->Deactivate();
	}
}

void AHookPickup::OnDelivered(const FVector& Velocity)
{
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	bCarried = false;
	UpdateHookCollision();
	Mesh->SetCollisionProfileName(UCollisionProfile::PhysicsActor_ProfileName);
	Mesh->SetSimulatePhysics(true);
	Mesh->SetPhysicsLinearVelocity(Velocity);
	if (Sparkle && SparkleSystem)
	{
		Sparkle->Activate(/*bReset=*/true);
	}
}

bool AHookPickup::MarkCollected()
{
	if (bCollected)
	{
		return false;
	}
	bCollected = true;
	return true;
}
