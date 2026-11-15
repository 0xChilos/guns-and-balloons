#pragma once

#include "CoreMinimal.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"

/** Shared collision setups for placeholder enemy meshes. */
class UStaticMesh;

namespace NemesisModel
{
	/** Swap a placeholder cube for the real model part: the mesh's own materials, pivot = Location. */
	inline bool Use(UStaticMeshComponent* C, UStaticMesh* Mesh, const FVector& Location = FVector::ZeroVector)
	{
		if (!C || !Mesh) return false; // asset missing: keep the cube
		C->SetStaticMesh(Mesh);
		C->EmptyOverrideMaterials();
		C->SetRelativeLocationAndRotation(Location, FRotator::ZeroRotator);
		C->SetRelativeScale3D(FVector::OneVector);
		return true;
	}
}

namespace NemesisCollision
{
	/** Only bullets (Visibility traces) hit it; nothing collides with it. For enemies and hit zones. */
	inline void Shootable(UPrimitiveComponent* C)
	{
		C->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		C->SetCollisionObjectType(ECC_WorldDynamic);
		C->SetCollisionResponseToAllChannels(ECR_Ignore);
		C->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
		C->SetGenerateOverlapEvents(false);
		C->SetCanEverAffectNavigation(false);
	}

	/** Level geometry: the balloon collides with it and it blocks sight and bullets. */
	inline void Solid(UPrimitiveComponent* C)
	{
		C->SetCollisionProfileName(TEXT("BlockAll"));
		C->SetGenerateOverlapEvents(false);
	}

	inline void None(UPrimitiveComponent* C)
	{
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetCanEverAffectNavigation(false);
	}
}
