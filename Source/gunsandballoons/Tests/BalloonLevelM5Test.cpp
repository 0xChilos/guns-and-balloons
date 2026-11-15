#include "../BalloonLevel/BalloonHorizontalRouteValidator.h"
#include "../BalloonLevel/BalloonSplinePlanner.h"
#include "../BalloonLevel/BalloonPlacementPlanner.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "Components/BoxComponent.h"
#include "GameFramework/Actor.h"
#include "../BalloonFlightComponent.h"
#include "../BalloonLevel/BalloonLevelGenerator.h"
#include "Components/SplineComponent.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBalloonLevelM5Test,"GunsAndBalloons.BalloonLevel.M5",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FBalloonLevelM5Test::RunTest(const FString& Parameters)
{
 using namespace BalloonHorizontalRoute;
 auto* C=NewObject<UBalloonLevelConfig>();C->MaxHeadingDegrees=0.;C->LengthMeters=100.;C->BaseWidthMeters=40.;C->MinWidthMeters=40.;
 FBalloonLevelPlan P;TArray<FString> D;TestTrue(TEXT("geometry"),BalloonSplinePlanning::Build(*C,P,D));
 FEnvelope E;E.Radius=100.;E.HalfHeight=400.;E.Speed=300.;E.MaxYawRate=.5;E.YawAcceleration=1.;E.MinimumRadius=600.;E.Safety=20.;E.Wind=30.;
 FSettings S;S.CellCm=500.;S.MaxAttempts=3;
 const FSweep Empty=[](const FVector&,const FVector&,const FVector&){return false;};
 auto A=Validate(P,E,S,Empty);TestTrue(TEXT("open route"),A.Reachable);
 TestTrue(TEXT("inflated wind/safety clearance"),A.MinimumClearanceCm>=50.);
 for(const FVector& V:A.Points)TestEqual(TEXT("every proof point at fixed Z"),V.Z,P.Start.GetLocation().Z);
 FBalloonPlacementDescriptor Wall;Wall.Collision=true;Wall.Representation=EBalloonPlacementRepresentation::MeshActor;
 const double Z=P.Start.GetLocation().Z;
 Wall.CollisionBounds=FBox(FVector(4900.,-10000.,Z-1000.),FVector(5100.,10000.,Z+1000.));P.Placements={Wall};
 A=Validate(P,E,S,Empty);TestFalse(TEXT("complete wall no high/low bypass"),A.Reachable);
 TestTrue(TEXT("bounded attempts"),A.AttemptCount<=S.MaxAttempts);TestTrue(TEXT("diagnostic"),!A.FailureReason.IsEmpty());TestTrue(TEXT("failed proof empty"),A.Points.IsEmpty());
 const auto Replay=Validate(P,E,S,Empty);TestEqual(TEXT("deterministic visits"),A.CellsVisited,Replay.CellsVisited);TestEqual(TEXT("deterministic reason"),A.FailureReason,Replay.FailureReason);
 // The full tall balloon overlaps a wall below its center; a center-point/XY shortcut would miss it.
 P.Placements[0].CollisionBounds=FBox(FVector(4900.,-10000.,Z-390.),FVector(5100.,10000.,Z-350.));
 TestFalse(TEXT("basket/lower envelope cannot descend or ascend past wall"),Validate(P,E,S,Empty).Reachable);
 P.Placements[0].CollisionBounds=FBox(FVector(4900.,-10000.,Z-1000.),FVector(5100.,10000.,Z-500.));
 TestTrue(TEXT("strictly below full envelope does not block"),Validate(P,E,S,Empty).Reachable);
 FBalloonPlacementDescriptor Left=Wall,Right=Wall;
 Left.CollisionBounds.Max.Y=-140.;Right.CollisionBounds.Min.Y=140.;P.Placements={Left,Right};
 TestFalse(TEXT("gap smaller than wind inflated balloon"),Validate(P,E,S,Empty).Reachable);
 Left.CollisionBounds.Max.Y=-500.;Right.CollisionBounds.Min.Y=500.;P.Placements={Left,Right};
 TestTrue(TEXT("wide gap passes"),Validate(P,E,S,Empty).Reachable);
 E.Wind=500.;TestFalse(TEXT("wind closes gap"),Validate(P,E,S,Empty).Reachable);E.Wind=30.;
 P.Placements.Reset();
 FBalloonBorderInfluence T;T.Center=FVector(5000.,0.,Z);T.RadiusCm=10000.;T.Policy=EBalloonBorderInfluencePolicy::Blocked;
 P.RoutePoints={P.Start.GetLocation(),P.Finish.GetLocation()};const auto Old=P.RoutePoints;
 A=TryBorders(P,{T},P.Start.GetLocation(),E,S,Empty);TestFalse(TEXT("unsafe border proposal rejected"),A.Reachable);
 TestTrue(TEXT("border rollback"),P.Borders.IsEmpty());TestEqual(TEXT("proof rollback"),P.RoutePoints.Num(),Old.Num());
 T.Policy=EBalloonBorderInfluencePolicy::VisualOnly;
 TestTrue(TEXT("visual border transaction"),TryBorders(P,{T},P.Start.GetLocation(),E,S,Empty).Reachable);
 FVector Current=P.Start.GetLocation();Current.Z+=100.;TestFalse(TEXT("runtime altitude mismatch rejected"),Validate(P,E,S,Empty,&Current).Reachable);
 P.Borders.Reset();Current=P.Start.GetLocation()+FVector(1000.,0.,0.);
 TestTrue(TEXT("exact current XY start"),Validate(P,E,S,Empty,&Current).Reachable);
 T.Policy=EBalloonBorderInfluencePolicy::Blocked;T.Center=P.Start.GetLocation();T.RadiusCm=300.;
 TestTrue(TEXT("border behind current XY can commit"),TryBorders(P,{T},Current,E,S,Empty).Reachable);
 TestTrue(TEXT("runtime proof starts at exact current XY"),P.RoutePoints[0].Equals(Current));P.Borders.Reset();
 E.InitialYawRate=.1;TestFalse(TEXT("moving yaw requires runtime transition contract"),Validate(P,E,S,Empty,&Current).Reachable);E.InitialYawRate=0.;
 // Make a forced horizontal dogleg. A point path exists, but excessive turn radius rejects it.
 Wall.CollisionBounds=FBox(FVector(4000.,-1800.,Z-1000.),FVector(6000.,800.,Z+1000.));P.Placements={Wall};
 E.MinimumRadius=100000.;TestFalse(TEXT("sharp turn incompatible with inertia"),Validate(P,E,S,Empty).Reachable);E.MinimumRadius=600.;
 E.Speed=50.;E.MaxYawRate=5.;E.YawAcceleration=100.;E.MinimumRadius=50.;
 A=Validate(P,E,S,Empty);TestTrue(TEXT("A-star finds and continuously smooths a horizontal detour"),A.Reachable);
 TestTrue(TEXT("detour visits lattice"),A.CellsVisited>0);
 for(const FVector& V:A.Points)TestEqual(TEXT("detour never changes altitude"),V.Z,Z);
 E.Speed=300.;E.MaxYawRate=.5;E.YawAcceleration=1.;E.MinimumRadius=600.;
 // Invalid bounds cannot be silently omitted.
 P.Placements[0].CollisionBounds=FBox(ForceInit);TestFalse(TEXT("unresolved 3D actor bounds fail closed"),Validate(P,E,S,Empty).Reachable);
 P.Placements[0].Representation=EBalloonPlacementRepresentation::HISM;TestTrue(TEXT("NoCollision scenery ignored"),Validate(P,E,S,Empty).Reachable);
 S.MaxStates=1;P.Placements={Wall};TestFalse(TEXT("allocation limit"),Validate(P,E,S,Empty).Reachable);S.MaxStates=100000;
 E.Radius=-1.;TestFalse(TEXT("invalid envelope"),Validate(P,E,S,Empty).Reachable);E.Radius=100.;
 auto* RetryConfig=NewObject<UBalloonLevelConfig>();RetryConfig->ObstaclesEnabled=true;RetryConfig->ObstacleExactCount=4;
 BalloonPlacementPlanning::FResolvedRow Row;Row.Entry.RowName=TEXT("RetryObstacle");Row.Entry.Row.Asset=FSoftObjectPath(TEXT("/Engine/BasicShapes/Cube.Cube"));
 Row.Entry.Row.Category=TEXT("Obstacle");Row.Entry.Row.Collision=true;Row.Entry.Row.Rule=TEXT("scatter");Row.Entry.Row.DensityPerKm2=1.;
 Row.Metadata.SizeMeters=FVector(1.);Row.LocalCollisionBounds=FBox(FVector(-50.),FVector(50.));
 FBalloonLevelPlan First=P,Retry=P,Again=P;
 TestTrue(TEXT("first placement attempt"),BalloonPlacementPlanning::Build(*RetryConfig,First,{Row},D));
 Retry.PlacementAttempt=Again.PlacementAttempt=1;
 TestTrue(TEXT("seeded retry"),BalloonPlacementPlanning::Build(*RetryConfig,Retry,{Row},D));
 TestTrue(TEXT("seeded retry replay"),BalloonPlacementPlanning::Build(*RetryConfig,Again,{Row},D));
 TestEqual(TEXT("retry preserves exact obstacle count"),Retry.Placements.Num(),4);
 TestEqual(TEXT("retry preserves requested count across attempts"),Retry.Placements.Num(),First.Placements.Num());
 for(int32 I=0;I<Retry.Placements.Num();++I)TestTrue(TEXT("retry transforms replay"),Retry.Placements[I].Transform.Equals(Again.Placements[I].Transform));
 TestFalse(TEXT("retry changes layout seed"),First.Placements[0].Transform.Equals(Retry.Placements[0].Transform));
 // Exercise real engine 3D sweeps independently of descriptor broadphase.
 UWorld* W=UWorld::CreateWorld(EWorldType::Game,false);
 AActor* Obstacle=W->SpawnActor<AActor>();UBoxComponent* Box=NewObject<UBoxComponent>(Obstacle);Obstacle->SetRootComponent(Box);Obstacle->AddInstanceComponent(Box);
 Box->SetBoxExtent(FVector(10.,100.,100.));Box->SetCollisionProfileName(TEXT("BlockAll"));Box->RegisterComponent();Obstacle->SetActorLocation(FVector(500.,0.,1000.));
 const auto Sweep=WorldSweep(*W,nullptr);
 TestTrue(TEXT("engine swept box catches thin wall between cells"),Sweep(FVector(0.,0.,1000.),FVector(1000.,0.,1000.),FVector(20.,20.,50.)));
 TestFalse(TEXT("engine full 3D sweep ignores wall below flight band"),Sweep(FVector(0.,0.,2000.),FVector(1000.,0.,2000.),FVector(20.,20.,50.)));
 TestTrue(TEXT("engine stationary overlap catches blocked reservation"),Sweep(FVector(500.,0.,1000.),FVector(500.,0.,1000.),FVector(20.,20.,50.)));
 AActor* Pawn=W->SpawnActor<AActor>();UBoxComponent* Hull=NewObject<UBoxComponent>(Pawn);Pawn->SetRootComponent(Hull);Pawn->AddInstanceComponent(Hull);
 Hull->SetBoxExtent(FVector(100.,100.,400.));Hull->SetCollisionProfileName(TEXT("BlockAllDynamic"));Hull->RegisterComponent();Pawn->SetActorLocation(FVector(0.,0.,Z));
 auto* Flight=NewObject<UBalloonFlightComponent>(Pawn);Pawn->AddInstanceComponent(Flight);Flight->RegisterComponent();
 FEnvelope Actual;FString Error;
 TestTrue(TEXT("inspect actual flight component and registered shape"),InspectPawn(*Pawn,Actual,Error));
 TestTrue(TEXT("enclosure includes full 3D box"),Actual.Radius>=141.4 && Actual.HalfHeight>=400.);
 TestEqual(TEXT("real authored speed"),Actual.Speed,300.);
 auto* Generator=W->SpawnActor<ABalloonLevelGenerator>();Generator->LevelConfig=C;Generator->RoutePawn=Pawn;
 TestTrue(TEXT("generator Ready requires proof"),Generator->GenerateLevel());
 TestTrue(TEXT("generator stores route"),Generator->GetPlan().RoutePoints.Num()>=2);
 TestTrue(TEXT("regeneration retains valid proof"),Generator->RegenerateLevel());
 TestEqual(TEXT("no duplicate spline points"),Generator->Spline->GetNumberOfSplinePoints(),Generator->GetPlan().SplineSamples.Num());
 Obstacle->SetActorLocation(FVector(5000.,0.,Z));Box->SetBoxExtent(FVector(100.,5000.,1000.));
 const auto PreviousRoute=Generator->GetPlan().RoutePoints;
 const auto* PreviousConfig=Generator->ActiveConfig.Get();
 TestFalse(TEXT("world wall rejects regeneration"),Generator->GenerateLevel());
 TestTrue(TEXT("failed regeneration retains previous level"),Generator->GetGenerationStatus()==EBalloonLevelGenerationStatus::Ready);
 TestTrue(TEXT("failed regeneration retains exact previous proof"),Generator->GetPlan().RoutePoints==PreviousRoute);
 TestTrue(TEXT("failed regeneration restores active config"),Generator->ActiveConfig.Get()==PreviousConfig);
 W->DestroyWorld(false);
 return true;
}
#endif
