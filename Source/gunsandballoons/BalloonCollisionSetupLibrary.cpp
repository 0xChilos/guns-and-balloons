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
	USCS_Node* ParentNode = SCS->FindSCSNode(FName("Mesh"));
	USCS_Node* Root = SCS->FindSCSNode(FName("BalloonCollision"));
	if (!ParentNode || !Root)
	{
		return false;
	}

	BP->Modify();
	SCS->Modify();
	Root->ComponentTemplate->Modify();
	CastChecked<UPrimitiveComponent>(Root->ComponentTemplate)->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	for (const TCHAR* Name : {TEXT("BalloonCollision_Envelope"), TEXT("BalloonCollision_Basket"), TEXT("BalloonCollision_Stove"), TEXT("BalloonCollision_Console")})
	{
		if (USCS_Node* Node = SCS->FindSCSNode(Name))
			CastChecked<UPrimitiveComponent>(Node->ComponentTemplate)->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	auto AddBox = [&](const FName& Name, const FVector& Location, const FRotator& Rotation, const FVector& Extent)
	{
		USCS_Node* Node = SCS->FindSCSNode(Name);
		if (!Node)
		{
			Node = SCS->CreateNode(UBoxComponent::StaticClass(), Name);
			if (Node) ParentNode->AddChildNode(Node);
		}
		if (!Node) return;
		UBoxComponent* Box = Cast<UBoxComponent>(Node->ComponentTemplate);
		if (Box)
		{
			Box->Modify();
			Box->SetBoxExtent(Extent);
			Box->SetRelativeLocation(Location);
			Box->SetRelativeRotation(Rotation);
			Box->SetMobility(EComponentMobility::Movable);
			Box->SetCollisionProfileName(TEXT("BlockAllDynamic"));
			Box->ComponentTags.AddUnique(TEXT("BasketDamage"));
		}
	};

	auto AddSphere = [&](const FName& Name, const FVector& Location, float Radius)
	{
		USCS_Node* Node = SCS->FindSCSNode(Name);
		if (!Node)
		{
			Node = SCS->CreateNode(USphereComponent::StaticClass(), Name);
			if (Node) ParentNode->AddChildNode(Node);
		}
		if (!Node) return;
		USphereComponent* Sphere = Cast<USphereComponent>(Node->ComponentTemplate);
		if (Sphere)
		{
			Sphere->Modify();
			Sphere->SetSphereRadius(Radius);
			Sphere->SetRelativeLocation(Location);
			Sphere->SetMobility(EComponentMobility::Movable);
			Sphere->SetCollisionProfileName(TEXT("BlockAllDynamic"));
		}
	};

	// Match Balloon_build.py: the basket circumradius shrank from 130 to 97.5 cm.
	// These are Mesh-space centimetres (FBX mirrors Blender Y). Keep the interior hollow.
	constexpr float BasketRadius = 97.5f;
	constexpr float WallThickness = 7.f;
	const float Apothem = BasketRadius * FMath::Cos(PI / 8.f);
	const float InnerApothem = Apothem - WallThickness;
	const float HalfWallLength = Apothem * FMath::Tan(PI / 8.f);
	for (int32 I = 0; I < 4; ++I)
		AddBox(*FString::Printf(TEXT("Floor_%d"), I), FVector(0, 0, 3.5f), FRotator(0, I * 45.f, 0),
			FVector(InnerApothem, InnerApothem * FMath::Tan(PI / 8.f), 3.5f));
	for (int32 I = 0; I < 8; ++I)
	{
		const FRotator Rotation(0, I * 45.f, 0);
		AddBox(*FString::Printf(TEXT("Wall_%d"), I), Rotation.Vector() * (Apothem - WallThickness / 2.f) + FVector(0, 0, 50.f),
			Rotation, FVector(WallThickness / 2.f, HalfWallLength, 50.f));
		// The iron rim is wider and thicker than the wicker walls.
		const float RimApothem = (BasketRadius + 7.f) * FMath::Cos(PI / 8.f);
		AddBox(*FString::Printf(TEXT("Rim_%d"), I), Rotation.Vector() * (RimApothem - 7.5f) + FVector(0, 0, 102.5f),
			Rotation, FVector(7.5f, RimApothem * FMath::Tan(PI / 8.f), 6.5f));
	}
	const float StoveY = InnerApothem - 30.f * FMath::Cos(PI / 8.f) - 1.f;
	AddBox(TEXT("StoveBlock"), FVector(0, StoveY, 64.f), FRotator::ZeroRotator, FVector(35, 35, 57.f));
	// The console retains its size, but follows the new inner front wall.
	AddBox(TEXT("ConsoleBlock"), FVector(0, -(InnerApothem - 12.5f), 73.5f), FRotator::ZeroRotator, FVector(33, 12.5f, 23.5f));
	const FVector2D Sections[] = {{430, 180}, {550, 290}, {680, 340}, {810, 300}, {940, 200}, {1040, 95}};
	for (int32 I = 0; I < UE_ARRAY_COUNT(Sections); ++I)
		AddSphere(*FString::Printf(TEXT("Envelope_%d"), I), FVector(0, 0, Sections[I].X), Sections[I].Y);
	BP->MarkPackageDirty();

	return true;
#else
	return false;
#endif
}
