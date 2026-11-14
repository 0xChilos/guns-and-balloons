#pragma once

#include "CoreMinimal.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

/**
 * A searchlight = a spot light (lights what it hits) + a visible glowing cone (SM_LightBeam with the
 * additive M_NemesisBeam, fading along its length). Used by the beacon and the AA gun.
 * Create both components in the actor's constructor, Configure in OnConstruction/BeginPlay,
 * SetColor whenever the state changes (yellow searching, red locked).
 */
namespace NemesisSearchlight
{
	const FLinearColor Searching(1.f, 0.78f, 0.25f);
	const FLinearColor Locked(1.f, 0.08f, 0.04f);

	/** Cone mesh along the parent's +X. Returns null if the asset is missing. */
	inline UStaticMeshComponent* CreateBeam(UObject* Owner, USceneComponent* Parent, UStaticMesh* BeamMesh, const TCHAR* Name)
	{
		UStaticMeshComponent* Beam = Owner->CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Beam->SetupAttachment(Parent);
		Beam->SetStaticMesh(BeamMesh);
		Beam->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Beam->SetCastShadow(false);
		Beam->SetCanEverAffectNavigation(false);
		return Beam;
	}

	/** Full cone angle (deg) and length (cm) for the visible cone and the light. */
	inline void Configure(USpotLightComponent* Spot, UStaticMeshComponent* Beam, float AngleDeg, float Length)
	{
		const float Radius = Length * FMath::Tan(FMath::DegreesToRadians(AngleDeg * 0.5f));
		if (Beam) Beam->SetRelativeScale3D(FVector(Length / 100.f, Radius / 100.f, Radius / 100.f));
		if (Spot)
		{
			Spot->SetAttenuationRadius(Length);
			Spot->SetOuterConeAngle(AngleDeg * 0.5f);
			Spot->SetInnerConeAngle(AngleDeg * 0.35f);
			// The cone mesh draws the visible beam. Fog scattering of a light this bright shining at the
			// camera floods the whole screen with its colour, so it is off.
			Spot->SetVolumetricScatteringIntensity(0.f);
		}
	}

	inline void SetColor(USpotLightComponent* Spot, UStaticMeshComponent* Beam, const FLinearColor& Color, float BeamIntensity = 0.1f)
	{
		if (Spot) Spot->SetLightColor(Color);
		if (Beam && Beam->GetStaticMesh())
		{
			UMaterialInstanceDynamic* M = Cast<UMaterialInstanceDynamic>(Beam->GetMaterial(0));
			if (!M) M = Beam->CreateDynamicMaterialInstance(0);
			if (M)
			{
				M->SetVectorParameterValue(TEXT("Color"), Color);
				M->SetScalarParameterValue(TEXT("Intensity"), BeamIntensity);
			}
		}
	}
}
