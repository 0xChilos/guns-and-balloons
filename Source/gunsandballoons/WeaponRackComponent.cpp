#include "WeaponRackComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "FirstPersonCameraPawn.h"
#include "GameFramework/Actor.h"
#include "GunComponent.h"
#include "FuelHandComponent.h"
#include "RevolverComponent.h"

UWeaponRackComponent::UWeaponRackComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	ItemClass = URevolverComponent::StaticClass();
	// Its mesh is the item's own, set in RefreshDisplay.
	DisplayMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("DisplayMesh"));
	DisplayMesh->SetupAttachment(this);
	DisplayMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	DisplayMesh->SetGenerateOverlapEvents(false);
	DisplayMesh->SetSimulatePhysics(false);
	DisplayMesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	DisplayMesh->bPauseAnims = true;
	DisplayMesh->SetVisibility(false);
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
	if (AActor* Owner = GetOwner(); Owner && GetAttachSocketName().IsNone() && !AttachSocket.IsNone())
	{
		TInlineComponentArray<USkeletalMeshComponent*> Meshes(Owner);
		for (USkeletalMeshComponent* Mesh : Meshes)
		{
			if (Mesh != DisplayMesh && Mesh->GetFName() == TEXT("Mesh") && Mesh->DoesSocketExist(AttachSocket))
			{
				AttachToComponent(Mesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, AttachSocket);
				// Keep the mesh's axes, not the socket bone's (their orientation and scale are an export artefact):
				// DisplayOffset / DisplayRotation are then in the balloon mesh's space, wherever the socket is.
				SetRelativeRotation(Mesh->GetSocketTransform(AttachSocket, RTS_Component).GetRotation().Inverse());
				break;
			}
		}
	}
	RefreshDisplay();
	DisplayMesh->SetVisibility(HasBegunPlay() ? bHasWeapon : bStartsWithWeapon);
	InteractShape->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractShape->SetCollisionResponseToChannel(InteractTraceChannel, ECR_Block);
	if (!DisplayMesh->IsRegistered()) DisplayMesh->RegisterComponent();
	if (!InteractShape->IsRegistered()) InteractShape->RegisterComponent();
}

const UGunComponent* UWeaponRackComponent::GetItemDefaults() const
{
	return GetDefault<AFirstPersonCameraPawn>()->FindGun(ItemClass);
}

void UWeaponRackComponent::RefreshDisplay()
{
	// The same mesh the player ends up holding: taken from the pawn's own item of this kind.
	const USkeletalMeshComponent* ItemMesh = GetDefault<AFirstPersonCameraPawn>()->GetGunMesh(GetItemDefaults());
	USkeletalMesh* Asset = ItemMesh ? ItemMesh->GetSkeletalMeshAsset() : nullptr;
	if (Asset && DisplayMesh->GetSkeletalMeshAsset() != Asset)
	{
		DisplayMesh->SetSkeletalMeshAsset(Asset);
	}
	DisplayMesh->SetRelativeLocationAndRotation(DisplayOffset, DisplayRotation);
	if (Asset && bFitInteractShape)
	{
		const FBoxSphereBounds ItemBounds = Asset->GetBounds();
		InteractShape->SetRelativeLocationAndRotation(DisplayOffset + DisplayRotation.RotateVector(ItemBounds.Origin), DisplayRotation);
		InteractShape->SetBoxExtent(ItemBounds.BoxExtent + FVector(InteractPadding), /*bUpdateOverlaps=*/false);
	}
}

void UWeaponRackComponent::BeginPlay()
{
	Super::BeginPlay();
	// The item's materials are FBX-importer instances that lose their texture on reimport: force it, as the item in hand does.
	if (const UGunComponent* Item = GetItemDefaults())
	{
		Item->ApplyWeaponTextureTo(DisplayMesh);
	}
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
	const auto* Pawn = Cast<AFirstPersonCameraPawn>(InstigatorPawn);
	return IsValid(Pawn) && (Pawn->IsHandEmpty() || Pawn->GetActiveGun() != Pawn->GetFuelHand());
}
void UWeaponRackComponent::Interact_Implementation(APawn* InstigatorPawn)
{
	if (AFirstPersonCameraPawn* Pawn = Cast<AFirstPersonCameraPawn>(InstigatorPawn))
	{
		// Each call checks the hand state itself, so a click mid-transition or mid-reload does nothing.
		if (Pawn->IsHandEmpty()) Pawn->DrawWeapon(this);
		else if (bHasWeapon) Pawn->SwapWeapon(this);
		else Pawn->HolsterWeapon(this);
	}
}
