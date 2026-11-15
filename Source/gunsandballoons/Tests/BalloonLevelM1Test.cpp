#include "../BalloonLevel/BalloonLevelGenerator.h"
#include "../BalloonLevel/BalloonAssetCatalog.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Components/SplineComponent.h"
#include "Engine/World.h"
#include "UObject/UnrealType.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBalloonLevelM1Test, "GunsAndBalloons.BalloonLevel.M1",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBalloonLevelM1Test::RunTest(const FString& Parameters)
{
	TArray<FString> Fields;
	for (TFieldIterator<FProperty> P(FBalloonLevelAssetRow::StaticStruct(), EFieldIteratorFlags::ExcludeSuper); P; ++P) Fields.Add(P->GetName());
	TestEqual(TEXT("Exact twelve designer columns"), FString::Join(Fields, TEXT(",")),
		FString(TEXT("Asset,Category,Collision,DensityPerKm2,ScaleMin,ScaleMax,MinSpacingM,RandomYaw,AlignToSlope,ClusterSize,Rule,Notes")));
	UBalloonLevelConfig* Config = NewObject<UBalloonLevelConfig>();
	FBalloonLevelPlan Plan;
	TArray<FString> Errors;
	TestTrue(TEXT("Valid geometry"), BalloonLevelPlanning::BuildStraightPlan(*Config, Plan, Errors));
	TestEqual(TEXT("1500/250 has six segments"), Plan.Segments.Num(), 6);
	Config->LengthMeters = 1620.f;
	Config->StartTransform = FTransform(FRotator(35., 90., 20.), FVector(500., -200., 90000.), FVector(2.));
	TestTrue(TEXT("Yaw-only authoring transform"), BalloonLevelPlanning::BuildStraightPlan(*Config, Plan, Errors));
	TestEqual(TEXT("1620/250 has seven segments"), Plan.Segments.Num(), 7);
	TestEqual(TEXT("Last segment is 120m"), Plan.Segments.Last().EndS - Plan.Segments.Last().StartS, 12000.);
	TestTrue(TEXT("Endpoint follows horizontal heading"), Plan.SplineSamples.Last().Equals(FVector(500., 161800., 10000.), 0.01));
	for (int32 I = 0; I < Plan.Segments.Num(); ++I)
	{
		TestEqual(TEXT("Stable segment ID"), Plan.Segments[I].SegmentId, I);
		TestEqual(TEXT("Fixed world Z"), Plan.SplineSamples[I].Z, 10000.);
		if (I) TestEqual(TEXT("Contiguous segment boundaries"), Plan.Segments[I].StartS, Plan.Segments[I - 1].EndS);
	}
	for (float Invalid : {0.f, -1.f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()})
	{
		Config->LengthMeters = Invalid; Errors.Reset();
		TestFalse(TEXT("Invalid length rejected"), Config->Validate(Errors));
		Config->LengthMeters = 1500.f; Config->BaseWidthMeters = Invalid; Errors.Reset();
		TestFalse(TEXT("Invalid width rejected"), Config->Validate(Errors));
		Config->BaseWidthMeters = 600.f; Config->SegmentTargetMeters = Invalid; Errors.Reset();
		TestFalse(TEXT("Invalid segment target rejected"), Config->Validate(Errors));
		Config->SegmentTargetMeters = 250.f;
	}
	Config->FlightHeightMeters = std::numeric_limits<float>::quiet_NaN(); Errors.Reset();
	TestFalse(TEXT("Nonfinite height rejected"), Config->Validate(Errors));
	Config->FlightHeightMeters = 100.f;
	Config->TerrainMode = EBalloonTerrainMode::WorldCollision;
	Errors.Reset();
	TestTrue(TEXT("World terrain accepts default trace bounds"), Config->Validate(Errors));
	Config->TerrainTraceBottomMeters = Config->TerrainTraceTopMeters;
	Errors.Reset();
	TestFalse(TEXT("Empty terrain trace interval rejected"), Config->Validate(Errors));
	Config->TerrainTraceBottomMeters = -1000.f;
	Config->TerrainActorTag = NAME_None;
	Errors.Reset();
	TestFalse(TEXT("World terrain requires explicit actor tag"), Config->Validate(Errors));
	Config->TerrainActorTag = TEXT("Terrain");
	Config->TerrainMode = EBalloonTerrainMode::Flat;
	UDataTable* Shared = NewObject<UDataTable>(); Shared->RowStruct = FBalloonLevelAssetRow::StaticStruct();
	UDataTable* Biome = NewObject<UDataTable>(); Biome->RowStruct = FBalloonLevelAssetRow::StaticStruct();
	FBalloonLevelAssetRow Row;
	Row.Asset = FSoftObjectPath(TEXT("/Engine/BasicShapes/Cube.Cube"));
	Row.Category = TEXT("Filler"); Row.Rule = TEXT("scatter");
	Shared->AddRow(TEXT("Zebra"), Row); Shared->AddRow(TEXT("Common"), Row);
	Row.ScaleMax = 2.f; Biome->AddRow(TEXT("Common"), Row);
	TArray<FBalloonCatalogEntry> Entries; Errors.Reset();
	TestTrue(TEXT("Catalog merge"), BalloonAssetCatalog::Merge(Shared, Biome, Entries, Errors));
	if (TestEqual(TEXT("Override does not duplicate"), Entries.Num(), 2))
	{
		TestEqual(TEXT("Lexicographic ordering"), Entries[0].RowName, FName(TEXT("Common")));
		TestEqual(TEXT("Biome replaces entire row"), Entries[0].Row.ScaleMax, 2.f);
		TestEqual(TEXT("Biome source tracked"), Entries[0].SourceTable, Biome->GetPathName());
	}
	for (const TCHAR* Id : {TEXT("Forest"), TEXT("Field"), TEXT("City"), TEXT("Desert"), TEXT("Custom")})
	{
		Config->BiomeId = Id; Errors.Reset();
		TestTrue(TEXT("Shared-only catalog works for every biome"), BalloonAssetCatalog::Merge(Shared, nullptr, Entries, Errors));
		TestEqual(TEXT("Shared rows preserved"), Entries.Num(), 2);
	}
	FBalloonCatalogEntry Entry{TEXT("BadRow"), TEXT("TestTable"), Row};
	auto Reject = [&](const TCHAR* Label, const FBalloonLevelAssetRow& Bad)
	{
		Entry.Row = Bad; Errors.Reset();
		TestFalse(Label, BalloonAssetCatalog::ValidateRow(Entry, Errors));
		TestTrue(TEXT("Diagnostic names table and row"), Errors.Num() && Errors[0].Contains(TEXT("TestTable row BadRow")));
	};
	FBalloonLevelAssetRow Bad = Row; Bad.DensityPerKm2 = -1.f; Reject(TEXT("Negative density"), Bad);
	Bad = Row; Bad.ScaleMin = 0.f; Reject(TEXT("Zero scale"), Bad);
	Bad = Row; Bad.ScaleMax = 0.5f; Reject(TEXT("Reversed scale"), Bad);
	Bad = Row; Bad.MinSpacingM = -1.f; Reject(TEXT("Negative spacing"), Bad);
	Bad = Row; Bad.ClusterSize = 0; Reject(TEXT("Empty cluster"), Bad);
	Bad = Row; Bad.Category = TEXT("Creature"); Reject(TEXT("Legacy category rejected"), Bad);
	Bad = Row; Bad.Rule = TEXT("unknown"); Reject(TEXT("Unknown rule"), Bad);
	Bad = Row; Bad.Collision = true; Reject(TEXT("Scenery collision rejected"), Bad);
	Bad = Row; Bad.Category = TEXT("Obstacle"); Reject(TEXT("Gameplay requires collision"), Bad);
	Bad = Row; Bad.Rule = TEXT("flock"); Reject(TEXT("Flock cannot use mesh"), Bad);
	Bad = Row; Bad.Asset = FSoftObjectPath(); Reject(TEXT("Empty path rejected"), Bad);
	Bad = Row; Bad.Asset = FSoftObjectPath(TEXT("/Script/Engine.Actor")); Reject(TEXT("Native class rejected"), Bad);
	Bad = Row; Bad.Asset = FSoftObjectPath(TEXT("/Engine/EngineSky/BP_Sky_Sphere.BP_Sky_Sphere"));
	Reject(TEXT("Blueprint asset is not a generated Actor class"), Bad);
	Entry.Row = Row; Entry.Row.Asset = FSoftObjectPath(TEXT("/Engine/EngineSky/BP_Sky_Sphere.BP_Sky_Sphere_C"));
	Entry.Row.Category = TEXT("Obstacle"); Entry.Row.Collision = true; Errors.Reset();
	TestTrue(TEXT("Generated Blueprint Actor class accepted for gameplay"), BalloonAssetCatalog::ValidateRow(Entry, Errors));
	Entry.Row.Category = TEXT("Filler"); Entry.Row.Collision = false;
	Reject(TEXT("Blueprint Actor cannot use scenery HISM"), Entry.Row);
	Bad = Row; Bad.DensityPerKm2 = std::numeric_limits<float>::quiet_NaN(); Reject(TEXT("Nonfinite density"), Bad);
	Bad = Row; Bad.MinSpacingM = std::numeric_limits<float>::infinity(); Reject(TEXT("Nonfinite spacing"), Bad);
	Bad = Row; Bad.ScaleMax = std::numeric_limits<float>::infinity(); Reject(TEXT("Nonfinite scale"), Bad);
	UDataTable* WrongSchema = NewObject<UDataTable>(); Errors.Reset();
	TestFalse(TEXT("Wrong schema rejected"), BalloonAssetCatalog::Merge(WrongSchema, nullptr, Entries, Errors));
	TestEqual(TEXT("Failed catalog exposes no partial entries"), Entries.Num(), 0);
	UWorld* World = GWorld;
	if (TestNotNull(TEXT("Editor world"), World))
	{
		auto* Generator = World->SpawnActor<ABalloonLevelGenerator>();
		Config->MaxHeadingDegrees = 0.f;
		Generator->LevelConfig = Config;
		TestFalse(TEXT("M5 generator requires inspected Pawn envelope"), Generator->GenerateLevel());
		TestTrue(TEXT("Unsafe plan cannot become Ready"), Generator->GetGenerationStatus() == EBalloonLevelGenerationStatus::Unreachable);
		TestEqual(TEXT("No unvalidated spline"), Generator->Spline->GetNumberOfSplinePoints(), 0);
		TestFalse(TEXT("Regenerate still requires Pawn"), Generator->RegenerateLevel());
		TestEqual(TEXT("No duplicate spline points"), Generator->Spline->GetNumberOfSplinePoints(), Generator->GetPlan().SplineSamples.Num());
		Config->LengthMeters = 0.f;
		TestFalse(TEXT("Invalid regeneration fails"), Generator->GenerateLevel());
		TestEqual(TEXT("Stale plan removed"), Generator->GetPlan().Segments.Num(), 0);
		Generator->ClearGeneratedLevel();
		TestTrue(TEXT("Clear resets status"), Generator->GetGenerationStatus() == EBalloonLevelGenerationStatus::NotStarted);
		World->DestroyActor(Generator);
	}
	return true;
}
#endif
