#include "WeaponRackComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "FirstPersonCameraPawn.h"
#include "GameFramework/Actor.h"
#include "UObject/ConstructorHelpers.h"

UWeaponRackComponent::UWeaponRackComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	DisplayMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("DisplayMesh"));
	DisplayMesh->SetupAttachment(this);
	DisplayMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	DisplayMesh->SetGenerateOverlapEvents(false);
	DisplayMesh->SetSimulatePhysics(false);
	DisplayMesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	DisplayMesh->bPauseAnims = true;
	DisplayMesh->SetVisibility(false);
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> Mesh(TEXT("/Game/Assets/Revolver/SK_Pax.SK_Pax"));
	if (Mesh.Succeeded()) DisplayMesh->SetSkeletalMeshAsset(Mesh.Object);
	InteractShape = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractShape"));
	InteractShape->SetupAttachment(this);
	InteractShape->SetBoxExtent(FVector(25.f, 10.f, 15.f)); // Prototype: fit bounds in editor.
	InteractShape->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractShape->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractShape->SetCollisionResponseToChannel(InteractTraceChannel, ECR_Block);
	InteractShape->SetGenerateOverlapEvents(false);
	InteractShape->CanCharacterStepUpOn = ECB_No;
}

void UWeaponRackComponent::OnRegister()
{
	Super::OnRegister();
	if (AActor* Owner = GetOwner(); Owner && GetAttachSocketName().IsNone())
	{
		TInlineComponentArray<USkeletalMeshComponent*> Meshes(Owner);
		for (USkeletalMeshComponent* Mesh : Meshes)
		{
			if (Mesh != DisplayMesh && Mesh->GetFName() == TEXT("Mesh") && Mesh->DoesSocketExist(TEXT("socket_small_weapon")))
			{
				AttachToComponent(Mesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, TEXT("socket_small_weapon"));
				break;
			}
		}
	}
	DisplayMesh->SetRelativeLocationAndRotation(DisplayOffset, DisplayRotation);
	DisplayMesh->SetVisibility(HasBegunPlay() ? bHasWeapon : bStartsWithWeapon);
	InteractShape->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractShape->SetCollisionResponseToChannel(InteractTraceChannel, ECR_Block);
	if (!DisplayMesh->IsRegistered()) DisplayMesh->RegisterComponent();
	if (!InteractShape->IsRegistered()) InteractShape->RegisterComponent();
}

void UWeaponRackComponent::BeginPlay()
{
	Super::BeginPlay();
	SetHasWeapon(bStartsWithWeapon);
}
void UWeaponRackComponent::SetHasWeapon(bool bNewHasWeapon)
{
	bHasWeapon = bNewHasWeapon;
	DisplayMesh->SetVisibility(bHasWeapon);
}
bool UWeaponRackComponent::AcceptsHit_Implementation(const FHitResult& Hit) const
{
	return Hit.GetComponent() == InteractShape;
}
bool UWeaponRackComponent::CanInteract_Implementation(APawn* InstigatorPawn) const
{
	// Consume rack clicks even when reloading or transitioning, to prevent accidental shots.
	return IsValid(Cast<AFirstPersonCameraPawn>(InstigatorPawn));
}
void UWeaponRackComponent::Interact_Implementation(APawn* InstigatorPawn)
{
	if (AFirstPersonCameraPawn* Pawn = Cast<AFirstPersonCameraPawn>(InstigatorPawn))
	{
		if (Pawn->GetHandState() == EPlayerHandState::Armed) Pawn->HolsterWeapon(this);
		else if (Pawn->IsHandEmpty()) Pawn->DrawWeapon(this);
	}
}
