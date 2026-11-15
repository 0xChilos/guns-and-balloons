#include "BalloonBorderThreat.h"
#include "Components/SphereComponent.h"

ABalloonBorderThreat::ABalloonBorderThreat()
{
 PrimaryActorTick.bCanEverTick=false;
 InfluenceCollision=CreateDefaultSubobject<USphereComponent>(TEXT("InfluenceCollision"));
 SetRootComponent(InfluenceCollision);
 InfluenceCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 InfluenceCollision->SetCollisionResponseToAllChannels(ECR_Ignore);
 InfluenceCollision->OnComponentBeginOverlap.AddDynamic(this,&ABalloonBorderThreat::HandleOverlap);
}

bool ABalloonBorderThreat::InitializeInfluence(const FBalloonBorderInfluence& T)
{
 if(HasActorBegunPlay() || T.Center.ContainsNaN() || !FMath::IsFinite(T.RadiusCm) || T.RadiusCm<=0.f ||
    static_cast<uint8>(T.Policy)>static_cast<uint8>(EBalloonBorderInfluencePolicy::VisualOnly)) return false;
 Influence=T;
 SetActorLocation(T.Center);
 SetActorScale3D(FVector::OneVector);
 InfluenceCollision->SetSphereRadius(T.RadiusCm);
 InfluenceCollision->SetCollisionObjectType(ECC_WorldDynamic);
 InfluenceCollision->SetCollisionResponseToAllChannels(ECR_Ignore);
 const bool Visual=T.Policy==EBalloonBorderInfluencePolicy::VisualOnly;
 const bool Blocking=T.Policy==EBalloonBorderInfluencePolicy::Blocked;
 InfluenceCollision->SetCollisionResponseToChannel(ECC_Pawn,Blocking?ECR_Block:ECR_Overlap);
 // Existing balloon primitives use BlockAllDynamic (WorldDynamic), not Pawn.
 InfluenceCollision->SetCollisionResponseToChannel(ECC_WorldDynamic,Blocking?ECR_Block:ECR_Overlap);
 InfluenceCollision->SetGenerateOverlapEvents(!Visual && !Blocking);
 InfluenceCollision->SetCollisionEnabled(Visual?ECollisionEnabled::NoCollision:ECollisionEnabled::QueryAndPhysics);
 OnInfluenceInitialized(Influence);
 return true;
}

void ABalloonBorderThreat::HandleOverlap(UPrimitiveComponent*, AActor* Other, UPrimitiveComponent*, int32, bool, const FHitResult&)
{
 if(Other && Other!=this && (Influence.Policy==EBalloonBorderInfluencePolicy::Damaging || Influence.Policy==EBalloonBorderInfluencePolicy::Pushback))
  OnBalloonInfluenceEntered(Other,Influence.Policy);
}

void ABalloonBorderThreat::CommitValidatedInfluence(const FBalloonBorderInfluence& Descriptor)
{
 Influence=Descriptor;
 InfluenceCollision->SetSphereRadius(Descriptor.RadiusCm,false);
}
