#include "FuelHandComponent.h"
#include "StoveFuelType.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
UFuelHandComponent::UFuelHandComponent()
{
	SmokeClass = nullptr;
	ProjectileClass = nullptr;
	CasingMesh = nullptr;
	// TBD: prototype value; tune the rest pose in the editor.
	ViewOffset = FVector(34.f, 16.f, -15.f);
	ViewRotation = FRotator::ZeroRotator;
	ReloadViewOffset = ViewOffset;
	ReloadViewRotation = ViewRotation;
}
void UFuelHandComponent::SetFuel(const UStoveFuelType* Type)
{
	Fuel = Type;
	if (!FuelProp && GetMesh())
	{
		FuelProp = NewObject<UStaticMeshComponent>(GetOwner(), TEXT("FuelProp"));
		FuelProp->SetupAttachment(GetMesh());
		FuelProp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		FuelProp->SetGenerateOverlapEvents(false);
		FuelProp->CastShadow = false;
		FuelProp->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::FirstPerson;
		FuelProp->RegisterComponent();
	}
	if (FuelProp)
	{
		FuelProp->SetStaticMesh(Type ? Type->HandMesh.LoadSynchronous() : nullptr);
		FuelProp->SetRelativeTransform(Type ? Type->HandTransform : FTransform::Identity);
		FuelProp->SetVisibility(Type && IsEquipped());
	}
}
