#include "BalloonCollisionSetupLibrary.h"

#if WITH_EDITOR
#include "Engine/Blueprint.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#endif

bool UBalloonCollisionSetupLibrary::AddBalloonCollisionPrimitives(const FString& BlueprintPath)
{
#if WITH_EDITOR
	UBlueprint* BP = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
	if (!BP || !BP->SimpleConstructionScript)
	{
		return false;
	}

	USimpleConstructionScript* SCS = BP->SimpleConstructionScript;
	USCS_Node* ParentNode = SCS->FindSCSNode(FName("BalloonCollision"));
	if (!ParentNode)
	{
		// Fall back to the default scene root if BalloonCollision is missing.
		ParentNode = SCS->GetDefaultSceneRootNode();
	}

	BP->Modify();
	SCS->Modify();

	auto AddBox = [&](const FName& Name, const FVector& Location, const FRotator& Rotation, const FVector& Extent)
	{
		USCS_Node* Node = SCS->CreateNode(UBoxComponent::StaticClass(), Name);
		if (!Node) return;
		UBoxComponent* Box = Cast<UBoxComponent>(Node->ComponentTemplate);
		if (Box)
		{
			Box->SetBoxExtent(Extent);
			Box->SetRelativeLocation(Location);
			Box->SetRelativeRotation(Rotation);
			Box->SetMobility(EComponentMobility::Movable);
			Box->SetCollisionProfileName(TEXT("Pawn"));
		}
		if (ParentNode)
		{
			Node->ParentComponentOrVariableName = ParentNode->GetVariableName();
			Node->bIsParentComponentNative = false;
		}
		SCS->AddNode(Node);
	};

	auto AddSphere = [&](const FName& Name, const FVector& Location, float Radius)
	{
		USCS_Node* Node = SCS->CreateNode(USphereComponent::StaticClass(), Name);
		if (!Node) return;
		USphereComponent* Sphere = Cast<USphereComponent>(Node->ComponentTemplate);
		if (Sphere)
		{
			Sphere->SetSphereRadius(Radius);
			Sphere->SetRelativeLocation(Location);
			Sphere->SetMobility(EComponentMobility::Movable);
			Sphere->SetCollisionProfileName(TEXT("Pawn"));
		}
		if (ParentNode)
		{
			Node->ParentComponentOrVariableName = ParentNode->GetVariableName();
			Node->bIsParentComponentNative = false;
		}
		SCS->AddNode(Node);
	};

	// Approximate the balloon envelope + basket.
	AddSphere(FName("BalloonCollision_Envelope"), FVector(0.f, 0.f, 120.f), 130.f);
	AddBox(FName("BalloonCollision_Basket"), FVector(0.f, 0.f, -40.f), FRotator::ZeroRotator, FVector(60.f, 60.f, 40.f));
	AddBox(FName("BalloonCollision_Stove"), FVector(0.f, 80.f, -20.f), FRotator::ZeroRotator, FVector(30.f, 30.f, 60.f));
	AddBox(FName("BalloonCollision_Console"), FVector(0.f, -80.f, 0.f), FRotator::ZeroRotator, FVector(30.f, 40.f, 30.f));

	return true;
#else
	return false;
#endif
}
