#include "NemesisFlak.h"

#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/SpotLightComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "NemesisHomingMissile.h"
#include "NemesisPlaceholder.h"
#include "NemesisSearchlight.h"
#include "NemesisTargetSubsystem.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	constexpr float BarrelLength = 240.f;
	constexpr float EnvelopeZ = 420.f; // envelope centre above the gondola
	/** The model faces −Y; the actor faces +X. */
	const FRotator FlakModelToActor(0.f, 90.f, 0.f);
	/** Model: the launcher bone is this high above the mesh origin; it sits at the actor's origin
	 *  (the player's altitude). */
	constexpr float FlakModelLauncherZ = 200.f;
	const FName LauncherBone(TEXT("launcher"));
	const FName LauncherPitchBone(TEXT("launcher_pitch"));

	void FlakMakeSolid(UPrimitiveComponent* C)
	{
		NemesisCollision::Solid(C);
		C->SetHiddenInGame(true);
	}
}

ANemesisFlak::ANemesisFlak()
{
	PrimaryActorTick.bCanEverTick = true;
	MissileClass = ANemesisHomingMissile::StaticClass();

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Iron(TEXT("/Game/Assets/Nemesis/MI_Nemesis_Iron.MI_Nemesis_Iron"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Canvas(TEXT("/Game/Assets/Nemesis/MI_Nemesis_Canvas.MI_Nemesis_Canvas"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Wood(TEXT("/Game/Assets/Nemesis/MI_Nemesis_Wood.MI_Nemesis_Wood"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Dark(TEXT("/Game/Assets/Nemesis/MI_Nemesis_Dark.MI_Nemesis_Dark"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Glow(TEXT("/Game/Assets/Nemesis/MI_Nemesis_Glow.MI_Nemesis_Glow"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> BeamMesh(TEXT("/Game/Assets/Enemies/FBX/SM_LightBeam.SM_LightBeam"));

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Gondola = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Gondola"));
	Gondola->SetupAttachment(Root);
	Gondola->SetStaticMesh(Cube.Object);
	Gondola->SetMaterial(0, Wood.Object);
	Gondola->SetRelativeScale3D(FVector(2.f, 2.f, 0.9f));
	NemesisCollision::Solid(Gondola);

	Envelope = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Envelope"));
	Envelope->SetupAttachment(Root);
	Envelope->SetStaticMesh(Sphere.Object);
	Envelope->SetMaterial(0, Canvas.Object);
	Envelope->SetRelativeLocation(FVector(0.f, 0.f, EnvelopeZ));
	Envelope->SetRelativeScale3D(FVector(5.f, 5.f, 6.f));
	NemesisCollision::Solid(Envelope);

	// Four ropes from the gondola's corners up to the envelope.
	const float RopeBottom = 45.f, RopeTop = EnvelopeZ - 200.f;
	for (int32 i = 0; i < 4; ++i)
	{
		UStaticMeshComponent* Rope = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Rope%d"), i));
		Rope->SetupAttachment(Root);
		Rope->SetStaticMesh(Cube.Object);
		Rope->SetMaterial(0, Dark.Object);
		Rope->SetRelativeLocation(FVector((i & 1) ? 90.f : -90.f, (i & 2) ? 90.f : -90.f, (RopeBottom + RopeTop) * 0.5f));
		Rope->SetRelativeScale3D(FVector(0.05f, 0.05f, (RopeTop - RopeBottom) / 100.f));
		NemesisCollision::None(Rope);
		Ropes.Add(Rope);
	}

	// The turret hangs under the gondola so it can turn all the way round.
	TurretYaw = CreateDefaultSubobject<USceneComponent>(TEXT("TurretYaw"));
	TurretYaw->SetupAttachment(Root);
	TurretYaw->SetRelativeLocation(FVector(0.f, 0.f, -45.f));

	TurretBody = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TurretBody"));
	TurretBody->SetupAttachment(TurretYaw);
	TurretBody->SetStaticMesh(Cube.Object);
	TurretBody->SetMaterial(0, Iron.Object);
	TurretBody->SetRelativeLocation(FVector(0.f, 0.f, -25.f));
	TurretBody->SetRelativeScale3D(FVector(1.2f, 1.2f, 0.5f));
	NemesisCollision::Solid(TurretBody);

	TurretPitch = CreateDefaultSubobject<USceneComponent>(TEXT("TurretPitch"));
	TurretPitch->SetupAttachment(TurretYaw);
	TurretPitch->SetRelativeLocation(FVector(0.f, 0.f, -60.f));

	Barrel = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Barrel"));
	Barrel->SetupAttachment(TurretPitch);
	Barrel->SetStaticMesh(Cube.Object);
	Barrel->SetMaterial(0, Iron.Object);
	Barrel->SetRelativeLocation(FVector(BarrelLength * 0.5f, 0.f, 0.f));
	Barrel->SetRelativeScale3D(FVector(BarrelLength / 100.f, 0.25f, 0.25f));
	NemesisCollision::None(Barrel);

	Lamp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Lamp"));
	Lamp->SetupAttachment(TurretPitch);
	Lamp->SetStaticMesh(Cube.Object);
	Lamp->SetMaterial(0, Glow.Object);
	Lamp->SetRelativeLocation(FVector(20.f, 0.f, -35.f));
	Lamp->SetRelativeScale3D(FVector(0.5f, 0.6f, 0.6f));
	NemesisCollision::None(Lamp);

	Light = CreateDefaultSubobject<USpotLightComponent>(TEXT("Light"));
	Light->SetupAttachment(TurretPitch);
	Light->SetRelativeLocation(FVector(50.f, 0.f, -35.f));
	Light->SetLightColor(NemesisSearchlight::Searching);
	Light->SetIntensity(5000000.f);
	Light->SetCastShadows(false);

	BeamCone = NemesisSearchlight::CreateBeam(this, TurretPitch, BeamMesh.Object, TEXT("BeamCone"));
	BeamCone->SetRelativeLocation(FVector(50.f, 0.f, -35.f));

	// The team's model. Collision shapes follow its manifest (component space of the mesh, cm).
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> BalloonMesh(TEXT("/Game/Assets/Map_edge/SK_RocketBalloon.SK_RocketBalloon"));
	Body = CreateDefaultSubobject<UPoseableMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(Root);
	Body->SetRelativeLocationAndRotation(FVector(0.f, 0.f, -FlakModelLauncherZ), FlakModelToActor);
	EnvelopeCollision = CreateDefaultSubobject<USphereComponent>(TEXT("EnvelopeCollision"));
	EnvelopeCollision->SetupAttachment(Body);
	EnvelopeCollision->InitSphereRadius(360.f);
	EnvelopeCollision->SetRelativeLocation(FVector(0.f, 0.f, 840.f));
	SkirtCollision = CreateDefaultSubobject<UCapsuleComponent>(TEXT("SkirtCollision"));
	SkirtCollision->SetupAttachment(Body);
	SkirtCollision->InitCapsuleSize(240.f, 300.f);
	SkirtCollision->SetRelativeLocation(FVector(0.f, 0.f, 620.f));
	DeckCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("DeckCollision"));
	DeckCollision->SetupAttachment(Body);
	DeckCollision->InitBoxExtent(FVector(145.f, 170.f, 45.f));
	DeckCollision->SetRelativeLocation(FVector(0.f, -20.f, 245.f));
	if (BalloonMesh.Object)
	{
		bModel = true;
		Body->SetSkinnedAssetAndUpdate(BalloonMesh.Object);
		for (UPrimitiveComponent* C : { (UPrimitiveComponent*)EnvelopeCollision, (UPrimitiveComponent*)SkirtCollision, (UPrimitiveComponent*)DeckCollision }) FlakMakeSolid(C);
		// Hide the placeholder parts.
		TArray<UStaticMeshComponent*> Cubes = { Gondola, Envelope, TurretBody, Barrel, Lamp };
		for (UStaticMeshComponent* R : Ropes) Cubes.Add(R);
		for (UStaticMeshComponent* C : Cubes)
		{
			C->SetVisibility(false);
			NemesisCollision::None(C);
		}
		// The searchlight sits at the model's red lamp; it is aimed from code (UpdateModelPose).
		Light->SetupAttachment(Body, TEXT("light"));
		Light->SetRelativeLocation(FVector::ZeroVector);
		BeamCone->SetupAttachment(Body, TEXT("light"));
		BeamCone->SetRelativeLocation(FVector::ZeroVector);
	}
	else
	{
		for (UPrimitiveComponent* C : { (UPrimitiveComponent*)EnvelopeCollision, (UPrimitiveComponent*)SkirtCollision, (UPrimitiveComponent*)DeckCollision }) NemesisCollision::None(C);
	}
}

void ANemesisFlak::UpdateModelPose()
{
	if (!bModel) return;
	// Keep each bone's own scale (the model's root bone carries a ×100 import scale).
	// Rotations about Z are the same in the actor's and the mesh's frame; elevation is about the
	// mesh's X (its forward is −Y), negative angle = muzzle up.
	const FQuat YawQ(FVector::UpVector, FMath::DegreesToRadians(Yaw));
	const FQuat PitchQ(FVector::ForwardVector, FMath::DegreesToRadians(-Pitch));
	Body->SetBoneTransformByName(LauncherBone,
		FTransform(YawQ * RefLauncher.GetRotation(), YawQ.RotateVector(RefLauncher.GetLocation()), RefLauncher.GetScale3D()), EBoneSpaces::ComponentSpace);
	Body->SetBoneTransformByName(LauncherPitchBone,
		FTransform(YawQ * PitchQ * RefLauncherPitch.GetRotation(), YawQ.RotateVector(RefLauncherPitch.GetLocation()), RefLauncherPitch.GetScale3D()), EBoneSpaces::ComponentSpace);
	// The lamp is fixed on the deck; its beam points where the launcher aims.
	const FRotator Aim = (GetActorQuat() * FRotator(Pitch, Yaw, 0.f).Quaternion()).Rotator();
	Light->SetWorldRotation(Aim);
	BeamCone->SetWorldRotation(Aim);
}

void ANemesisFlak::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	NemesisSearchlight::Configure(Light, BeamCone, BeamAngle, BeamLength > 0.f ? BeamLength : EngageRadius);
	Pitch = SearchPitch;
	TurretPitch->SetRelativeRotation(FRotator(Pitch, 0.f, 0.f));
}

void ANemesisFlak::BeginPlay()
{
	Super::BeginPlay();
	Pitch = SearchPitch;
	HoverZ = GetActorLocation().Z;
	NemesisSearchlight::SetLockedState(Light, BeamCone, false);
	if (bModel)
	{
		RefLauncher = Body->GetBoneTransformByName(LauncherBone, EBoneSpaces::ComponentSpace);
		RefLauncherPitch = Body->GetBoneTransformByName(LauncherPitchBone, EBoneSpaces::ComponentSpace);
		UpdateModelPose();
	}
}

bool ANemesisFlak::CanSeeBasket() const
{
	UNemesisTargetSubsystem* T = UNemesisTargetSubsystem::Get(this);
	AActor* Balloon = T ? T->GetBalloon() : nullptr;
	if (!Balloon) return false;

	const FVector From = TurretPitch->GetComponentLocation();
	const FVector Basket = T->GetBasketLocation();
	const float Radius = EngageRadius + (bEngaged ? ReleaseMargin : 0.f);
	if (FVector::DistSquared(From, Basket) > FMath::Square(Radius)) return false;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(FlakSight), false, this);
	Params.AddIgnoredActor(Balloon);
	TArray<AActor*> Riders;
	Balloon->GetAttachedActors(Riders, true, true);
	Params.AddIgnoredActors(Riders);
	FHitResult Hit;
	return !GetWorld()->LineTraceSingleByObjectType(Hit, From, Basket, FCollisionObjectQueryParams(ECC_WorldStatic), Params);
}

void ANemesisFlak::SetEngaged(bool bNow)
{
	if (bNow == bEngaged) return;
	bEngaged = bNow;
	NextLaunchIn = WarmUp;
	NemesisSearchlight::SetLockedState(Light, BeamCone, bEngaged);
	UE_LOG(LogTemp, Log, TEXT("%s: %s"), *GetName(), bEngaged ? TEXT("engaging the balloon") : TEXT("searching"));
	OnEngagedChanged(bEngaged);
}

void ANemesisFlak::UpdateAltitude(float DeltaSeconds)
{
	BobTime += DeltaSeconds;
	UNemesisTargetSubsystem* T = UNemesisTargetSubsystem::Get(this);
	if (bMatchPlayerAltitude && T && T->GetBalloon())
	{
		const float TargetZ = T->GetBasketLocation().Z + AltitudeOffset;
		// First frame with a balloon: start at its altitude instead of climbing from where it was placed.
		HoverZ = bAltitudeSnapped ? FMath::FInterpConstantTo(HoverZ, TargetZ, DeltaSeconds, ClimbSpeed) : TargetZ;
		bAltitudeSnapped = true;
	}
	FVector Loc = GetActorLocation();
	Loc.Z = HoverZ + BobAmplitude * FMath::Sin(BobTime * UE_TWO_PI / BobPeriod);
	SetActorLocation(Loc);
}

void ANemesisFlak::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateAltitude(DeltaSeconds);

	SetEngaged(CanSeeBasket());

	if (!bEngaged)
	{
		// Searching: the light circles the sky.
		Yaw = FRotator::NormalizeAxis(Yaw + SearchSpeed * DeltaSeconds);
		Pitch = FMath::FInterpConstantTo(Pitch, SearchPitch, DeltaSeconds, TrackSpeed);
		TurretYaw->SetRelativeRotation(FRotator(0.f, Yaw, 0.f));
		TurretPitch->SetRelativeRotation(FRotator(Pitch, 0.f, 0.f));
		UpdateModelPose();
		return;
	}

	// Engaged: the launcher and the light track the basket.
	UNemesisTargetSubsystem* T = UNemesisTargetSubsystem::Get(this);
	const FVector Launcher = TurretPitch->GetComponentLocation();
	const FVector Basket = T->GetBasketLocation();
	const FRotator Want = GetActorTransform().InverseTransformVector(Basket - Launcher).Rotation();
	const FRotator Now = FMath::RInterpConstantTo(FRotator(Pitch, Yaw, 0.f), Want, DeltaSeconds, TrackSpeed);
	Pitch = FMath::Clamp(Now.Pitch, -60.f, 60.f);
	Yaw = FRotator::NormalizeAxis(Now.Yaw);
	TurretYaw->SetRelativeRotation(FRotator(0.f, Yaw, 0.f));
	TurretPitch->SetRelativeRotation(FRotator(Pitch, 0.f, 0.f));
	UpdateModelPose();

	// One missile in the air at a time; the cooldown runs once it is gone.
	if (ActiveMissile.IsValid()) return;
	NextLaunchIn -= DeltaSeconds;
	if (NextLaunchIn > 0.f) return;
	const FVector AimDir = (Basket - Launcher).GetSafeNormal();
	const float ErrorDeg = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(TurretPitch->GetForwardVector(), AimDir), -1.f, 1.f)));
	if (ErrorDeg > FireTolerance) return; // still turning
	LaunchMissile();
	NextLaunchIn = MissileCooldown;
}

void ANemesisFlak::LaunchMissile()
{
	if (!MissileClass) return;
	FVector Muzzle = TurretPitch->GetComponentLocation() + TurretPitch->GetForwardVector() * BarrelLength;
	if (bModel)
	{
		// The model's six tubes, in turn.
		const FName Tube(*FString::Printf(TEXT("muzzle_%02d"), NextMuzzle + 1));
		NextMuzzle = (NextMuzzle + 1) % 6;
		Muzzle = Body->GetSocketLocation(Tube);
	}
	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (ANemesisHomingMissile* Missile = GetWorld()->SpawnActor<ANemesisHomingMissile>(MissileClass, Muzzle, TurretPitch->GetComponentRotation(), Params))
	{
		Missile->Launch(this);
		ActiveMissile = Missile;
		UE_LOG(LogTemp, Log, TEXT("%s: missile launched"), *GetName());
	}
	OnShot();
}
