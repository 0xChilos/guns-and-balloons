#include "StoveEditorLibrary.h"

#if WITH_EDITOR
#include "Animation/AnimBlueprint.h"
#include "AnimGraphNode_ModifyBone.h"
#include "AnimGraphNode_ComponentToLocalSpace.h"
#include "K2Node_VariableGet.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphSchema.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/SkeletalBodySetup.h"
#include "PhysicsEngine/BoxElem.h"
#include "Engine/SkeletalMesh.h"
#include "Rendering/SkeletalMeshModel.h"
#include "Rendering/SkeletalMeshLODModel.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "Components/BoxComponent.h"
#endif

bool UStoveEditorLibrary::ConfigureAnimationAndDoorBody()
{
#if WITH_EDITOR
	auto* BP = LoadObject<UAnimBlueprint>(nullptr, TEXT("/Game/Assets/Balloon/ABP_Balloon.ABP_Balloon"));
	auto* PA = LoadObject<UPhysicsAsset>(nullptr, TEXT("/Game/Assets/Balloon/SK_Balloon_PhysicsAsset.SK_Balloon_PhysicsAsset"));
	auto* Mesh = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Assets/Balloon/SK_Balloon.SK_Balloon"));
	if (!BP || !PA || !Mesh) return false;
	PA->UpdateBodySetupIndexMap();
	TArray<UEdGraph*> Graphs;
	BP->GetAllGraphs(Graphs);
	UEdGraph* Graph = nullptr;
	UAnimGraphNode_ComponentToLocalSpace* Convert = nullptr;
	for (UEdGraph* Candidate : Graphs)
		for (UEdGraphNode* Node : Candidate->Nodes)
			if (auto* C = Cast<UAnimGraphNode_ComponentToLocalSpace>(Node))
			{
				// Refuse ambiguous graphs instead of changing an arbitrary pose chain.
				if (Convert) return false;
				Graph = Candidate; Convert = C;
			}
	if (!Graph || !Convert) return false;
	bool bAlreadyConfigured = false;
	for (UEdGraphNode* Node : Graph->Nodes)
		if (Node->NodeComment == TEXT("STOVE_PHASE4_DOOR")) bAlreadyConfigured = true;
	BP->Modify(); Graph->Modify();
	if (!bAlreadyConfigured)
	{
		UEdGraphPin* Input = Convert->FindPin(TEXT("ComponentPose"));
		if (!Input || Input->LinkedTo.Num() != 1) return false;
		UEdGraphPin* Previous = Input->LinkedTo[0];
		Input->BreakAllPinLinks();
		const FName Bones[] = { TEXT("furnace_door"), TEXT("gauge_needle"), TEXT("burn_arrow") };
		const FName Variables[] = { TEXT("StoveDoorRotation"), TEXT("StoveGaugeRotation"), TEXT("StoveBurnTranslation") };
		for (int32 I = 0; I < 3; ++I)
		{
			FGraphNodeCreator<UAnimGraphNode_ModifyBone> Creator(*Graph);
			auto* Node = Creator.CreateNode();
			Node->Node.BoneToModify.BoneName = Bones[I];
			Node->Node.RotationMode = I < 2 ? BMM_Additive : BMM_Ignore;
			Node->Node.TranslationMode = I == 2 ? BMM_Additive : BMM_Ignore;
			Node->Node.RotationSpace = I == 1 ? BCS_ParentBoneSpace : BCS_ComponentSpace;
			Node->Node.TranslationSpace = BCS_ComponentSpace;
			Node->NodePosX = Convert->NodePosX - 900 + I * 300;
			Node->NodePosY = Convert->NodePosY;
			Node->NodeComment = I == 0 ? TEXT("STOVE_PHASE4_DOOR") : TEXT("Stove Phase 4");
			Creator.Finalize();
			const FName PinName = I == 2 ? TEXT("Translation") : TEXT("Rotation");
			for (auto& Property : Node->ShowPinForProperties)
				if (Property.PropertyName == PinName) Property.bShowPin = true;
			Node->ReconstructNode();
			FGraphNodeCreator<UK2Node_VariableGet> GetCreator(*Graph);
			auto* Get = GetCreator.CreateNode();
			Get->VariableReference.SetSelfMember(Variables[I]);
			Get->NodePosX = Node->NodePosX; Get->NodePosY = Node->NodePosY + 220;
			GetCreator.Finalize();
			const auto* Schema = Graph->GetSchema();
			if (!Schema->TryCreateConnection(Previous, Node->FindPinChecked(TEXT("ComponentPose")))
				|| !Schema->TryCreateConnection(Get->FindPinChecked(Variables[I]), Node->FindPinChecked(PinName))) return false;
			Previous = Node->FindPinChecked(TEXT("Pose"));
		}
		if (!Graph->GetSchema()->TryCreateConnection(Previous, Input)) return false;
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
	}
	// Fit in imported bone space, avoiding assumptions about the FBX bone orientation.
	{
		const auto& Ref = Mesh->GetRefSkeleton();
		const int32 DoorIndex = Ref.FindBoneIndex(TEXT("furnace_door"));
		const auto* Model = Mesh->GetImportedModel();
		if (DoorIndex == INDEX_NONE || !Model || Model->LODModels.IsEmpty()) return false;
		FTransform BoneToMesh = Ref.GetRefBonePose()[DoorIndex];
		for (int32 Parent = Ref.GetParentIndex(DoorIndex); Parent != INDEX_NONE; Parent = Ref.GetParentIndex(Parent))
			BoneToMesh *= Ref.GetRefBonePose()[Parent];
		FBox Bounds(ForceInit);
		for (const auto& Section : Model->LODModels[0].Sections)
			for (const auto& Vertex : Section.SoftVertices)
				for (int32 Influence = 0; Influence < UE_ARRAY_COUNT(Vertex.InfluenceBones); ++Influence)
					if (Vertex.InfluenceWeights[Influence] > 0 && Section.BoneMap.IsValidIndex(Vertex.InfluenceBones[Influence])
						&& Section.BoneMap[Vertex.InfluenceBones[Influence]] == DoorIndex)
					{
						Bounds += BoneToMesh.InverseTransformPosition(FVector(Vertex.Position));
						break;
					}
		if (!Bounds.IsValid) return false;
		PA->Modify();
		const int32 ExistingBody = PA->FindBodyIndex(TEXT("furnace_door"));
		auto* Body = ExistingBody == INDEX_NONE ? NewObject<USkeletalBodySetup>(PA, NAME_None, RF_Transactional)
			: PA->SkeletalBodySetups[ExistingBody].Get();
		Body->Modify();
		Body->BoneName = TEXT("furnace_door");
		Body->PhysicsType = PhysType_Kinematic;
		Body->DefaultInstance.SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Body->DefaultInstance.SetResponseToAllChannels(ECR_Ignore);
		Body->DefaultInstance.SetResponseToChannel(ECC_Visibility, ECR_Block);
		FKBoxElem Box;
		const FVector Size = Bounds.GetSize();
		// Imported vertices are in mesh units; this mesh is scaled up on the component.
		// A one-unit minimum makes the small door body cover empty space around the stove.
		Box.X = FMath::Max(KINDA_SMALL_NUMBER, Size.X);
		Box.Y = FMath::Max(KINDA_SMALL_NUMBER, Size.Y);
		Box.Z = FMath::Max(KINDA_SMALL_NUMBER, Size.Z);
		Box.Center = Bounds.GetCenter();
		Body->AggGeom.EmptyElements();
		Body->AggGeom.BoxElems.Add(Box);
		Body->InvalidatePhysicsData();
		if (ExistingBody == INDEX_NONE) PA->SkeletalBodySetups.Add(Body);
		PA->UpdateBodySetupIndexMap();
		PA->MarkPackageDirty();
	}
	// Fit the mouth behind the closed door in mesh space, once; preserve later manual tuning.
	auto* Balloon = LoadObject<UBlueprint>(nullptr, TEXT("/Game/Assets/Balloon/BP_Balloon.BP_Balloon"));
	if (!Balloon || !Balloon->SimpleConstructionScript) return false;
	auto* ShapeNode = Balloon->SimpleConstructionScript->FindSCSNode(TEXT("StoveFirebox"));
	auto* Shape = ShapeNode ? Cast<UBoxComponent>(ShapeNode->ComponentTemplate) : nullptr;
	if (!Shape) return false;
	if (!Shape->ComponentTags.Contains(TEXT("StoveFitted")))
	{
		const auto& Ref = Mesh->GetRefSkeleton();
		auto ComponentTransform = [&Ref](int32 Index)
		{
			FTransform Result = Ref.GetRefBonePose()[Index];
			for (int32 Parent = Ref.GetParentIndex(Index); Parent != INDEX_NONE; Parent = Ref.GetParentIndex(Parent))
				Result *= Ref.GetRefBonePose()[Parent];
			return Result;
		};
		const int32 DoorIndex = Ref.FindBoneIndex(TEXT("furnace_door"));
		const int32 StoveIndex = Ref.FindBoneIndex(TEXT("stove"));
		const auto* Body = PA->SkeletalBodySetups[PA->FindBodyIndex(TEXT("furnace_door"))].Get();
		if (DoorIndex == INDEX_NONE || StoveIndex == INDEX_NONE || Body->AggGeom.BoxElems.IsEmpty()) return false;
		const auto& Box = Body->AggGeom.BoxElems[0];
		const auto DoorTransform = ComponentTransform(DoorIndex);
		const FVector Center = DoorTransform.TransformPosition(Box.Center);
		FVector Outward = Center - ComponentTransform(StoveIndex).GetTranslation();
		Outward.Z = 0.f; Outward.Normalize();
		// Box dimensions are bone-local imported units; the socket scale converts to mesh cm.
		const FVector MeshSize = FVector(Box.X, Box.Y, Box.Z) * DoorTransform.GetScale3D().GetAbs();
		FVector Extent = MeshSize * .4f;
		const int32 ThinAxis = Box.X < Box.Y ? (Box.X < Box.Z ? 0 : 2) : (Box.Y < Box.Z ? 1 : 2);
		Extent[ThinAxis] = 1.5f;
		Balloon->Modify(); Shape->Modify();
		Shape->SetBoxExtent(Extent);
		Shape->SetRelativeLocation(Center - Outward * .75f);
		Shape->SetRelativeRotation(DoorTransform.GetRotation().Rotator());
		Shape->ComponentTags.Add(TEXT("StoveFitted"));
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Balloon);
	}
	return true;
#else
	return false;
#endif
}
