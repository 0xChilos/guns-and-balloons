#include "ResourceCounterBoard.h"

#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameOptions.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "ResourceLedger.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	// Layout in cm, on the board's front (+X). The viewer's right is the board's -Y.
	constexpr float PlateWidth = 46.f;
	constexpr float PlateHeight = 74.f;
	constexpr float PlateDepth = 3.f;
	constexpr float FrontX = PlateDepth * 0.5f + 0.5f;
	constexpr float TitleZ = 28.f;
	constexpr float FirstRowZ = 14.f;
	constexpr float RowStep = 11.5f;
	constexpr float IconY = 12.f;
	constexpr float TextY = 3.f;
	constexpr float TextSize = 9.f;
	/** Loot models are about 1 m across; on the board they are ~9 cm. */
	constexpr float IconSize = 9.f;
	constexpr float IdleSpinSpeed = 25.f;

	const TCHAR* const IconPaths[ResourceTypes::Num] = {
		TEXT("/Game/Assets/Locations/Loot/SM_Loot_AmmoBox.SM_Loot_AmmoBox"),
		TEXT("/Game/Assets/Locations/Loot/SM_Loot_MoneyPouch.SM_Loot_MoneyPouch"),
		TEXT("/Game/Assets/Locations/Loot/SM_Loot_Log.SM_Loot_Log"),
		TEXT("/Game/Assets/Locations/Loot/SM_Loot_Coal.SM_Loot_Coal"),
		TEXT("/Game/Assets/Locations/Loot/SM_Loot_Crystal.SM_Loot_Crystal"),
	};
	const TCHAR* const RowNames[ResourceTypes::Num] = { TEXT("Ammo"), TEXT("Money"), TEXT("Wood"), TEXT("Coal"), TEXT("Crystal") };

	void NoCollision(UPrimitiveComponent* Component)
	{
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Component->SetGenerateOverlapEvents(false);
		Component->SetCanEverAffectNavigation(false);
	}

	FText CountText(float Value)
	{
		return FText::FromString(FString::Printf(TEXT("%03d"), FMath::Clamp(FMath::FloorToInt(Value), 0, 999)));
	}
}

AResourceCounterBoard::AResourceCounterBoard()
{
	PrimaryActorTick.bCanEverTick = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShapeMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	static ConstructorHelpers::FObjectFinder<USoundBase> Tick(TEXT("/Game/Audio/Sounds/SFX_Counter_Tick.SFX_Counter_Tick"));
	TickSound = Tick.Object;

	Plate = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Plate"));
	Plate->SetupAttachment(Root);
	NoCollision(Plate);
	if (Cube.Succeeded())
	{
		Plate->SetStaticMesh(Cube.Object);
	}
	if (ShapeMaterial.Succeeded())
	{
		Plate->SetMaterial(0, ShapeMaterial.Object);
	}
	// The engine cube is 100 cm.
	Plate->SetRelativeScale3D(FVector(PlateDepth, PlateWidth, PlateHeight) / 100.f);

	Title = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Title"));
	Title->SetupAttachment(Root);
	Title->SetRelativeLocation(FVector(FrontX, 0.f, TitleZ));
	Title->SetHorizontalAlignment(EHTA_Center);
	Title->SetVerticalAlignment(EVRTA_TextCenter);
	Title->SetWorldSize(TextSize * 0.8f);
	Title->SetText(FText::FromString(TEXT("LOOT")));
	Title->SetTextRenderColor(FColor(200, 170, 110));

	RowColors = {
		FLinearColor(1.f, 0.7f, 0.2f),    // Ammo: warm brass
		FLinearColor(1.f, 0.85f, 0.25f),  // Money: gold
		FLinearColor(0.6f, 1.f, 0.4f),    // Wood: green
		FLinearColor(1.f, 0.35f, 0.1f),   // Coal: ember orange
		FLinearColor(0.35f, 0.9f, 1.f),   // Crystal: cyan
	};

	for (int32 i = 0; i < ResourceTypes::Num; ++i)
	{
		const float Z = FirstRowZ - RowStep * i;

		UStaticMeshComponent* Icon = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Icon_%s"), RowNames[i]));
		Icon->SetupAttachment(Root);
		NoCollision(Icon);
		Icon->CastShadow = false;
		ConstructorHelpers::FObjectFinder<UStaticMesh> IconMesh(IconPaths[i]);
		if (IconMesh.Succeeded())
		{
			Icon->SetStaticMesh(IconMesh.Object);
		}
		Icon->SetRelativeLocation(FVector(FrontX + 4.f, IconY, Z - IconSize * 0.45f));
		Icon->SetRelativeScale3D(FVector(IconSize / 100.f));
		Icons.Add(Icon);

		UTextRenderComponent* Count = CreateDefaultSubobject<UTextRenderComponent>(*FString::Printf(TEXT("Count_%s"), RowNames[i]));
		Count->SetupAttachment(Root);
		Count->SetRelativeLocation(FVector(FrontX, TextY, Z));
		Count->SetHorizontalAlignment(EHTA_Left);
		Count->SetVerticalAlignment(EVRTA_TextCenter);
		Count->SetWorldSize(TextSize);
		Count->SetText(CountText(0.f));
		Counts.Add(Count);
	}
}

void AResourceCounterBoard::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyLook();
}

void AResourceCounterBoard::ApplyLook()
{
	if (Plate && Plate->GetMaterial(0))
	{
		UMaterialInstanceDynamic* Material = Cast<UMaterialInstanceDynamic>(Plate->GetMaterial(0));
		if (!Material)
		{
			Material = Plate->CreateDynamicMaterialInstance(0);
		}
		if (Material)
		{
			Material->SetVectorParameterValue(TEXT("Color"), PlateColor);
		}
	}
	for (int32 i = 0; i < Counts.Num(); ++i)
	{
		if (Counts[i])
		{
			const FLinearColor Color = RowColors.IsValidIndex(i) ? RowColors[i] : FLinearColor::White;
			Counts[i]->SetTextRenderColor(Color.ToFColor(true));
			Counts[i]->SetText(CountText(Shown[i]));
		}
	}
}

void AResourceCounterBoard::BeginPlay()
{
	Super::BeginPlay();

	if (UResourceLedger* Found = GetWorld() ? GetWorld()->GetSubsystem<UResourceLedger>() : nullptr)
	{
		Ledger = Found;
		Found->OnCountChanged.AddDynamic(this, &AResourceCounterBoard::HandleCountChanged);
		for (int32 i = 0; i < ResourceTypes::Num; ++i)
		{
			Target[i] = Shown[i] = Found->GetCount(ResourceTypes::All[i]);
		}
	}
	for (int32 i = 0; i < ResourceTypes::Num; ++i)
	{
		IconYaw[i] = 37.f * i;
	}
	ApplyLook();
}

void AResourceCounterBoard::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UResourceLedger* Found = Ledger.Get())
	{
		Found->OnCountChanged.RemoveDynamic(this, &AResourceCounterBoard::HandleCountChanged);
	}
	Super::EndPlay(EndPlayReason);
}

void AResourceCounterBoard::HandleCountChanged(EResourceType Type, int32 NewCount, int32 Delta)
{
	const int32 Index = ResourceTypes::ToIndex(Type);
	if (Index != INDEX_NONE)
	{
		Target[Index] = NewCount;
		Flash[Index] = 1.f;
	}
}

int32 AResourceCounterBoard::GetShownCount(EResourceType Type) const
{
	const int32 Index = ResourceTypes::ToIndex(Type);
	return Index != INDEX_NONE ? FMath::FloorToInt(Shown[Index]) : 0;
}

void AResourceCounterBoard::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	TimeSinceTick += DeltaSeconds;
	for (int32 i = 0; i < ResourceTypes::Num; ++i)
	{
		UpdateRow(i, DeltaSeconds);
	}
}

void AResourceCounterBoard::UpdateRow(int32 Index, float DeltaSeconds)
{
	// Roll the number up a step at a time, faster for a big jump.
	const int32 Before = FMath::FloorToInt(Shown[Index]);
	const float Remaining = Target[Index] - Shown[Index];
	if (Remaining != 0.f)
	{
		const float Speed = FMath::Max(RollSpeed, FMath::Abs(Remaining) / MaxRollTime);
		Shown[Index] = FMath::FInterpConstantTo(Shown[Index], Target[Index], DeltaSeconds, Speed);
	}
	const int32 After = FMath::FloorToInt(Shown[Index]);
	if (After != Before && Counts.IsValidIndex(Index) && Counts[Index])
	{
		Counts[Index]->SetText(CountText(Shown[Index]));
		if (TickSound && TimeSinceTick >= MinTickInterval && UGameOptions::IsOn(EGameOption::Sfx))
		{
			UGameplayStatics::PlaySoundAtLocation(this, TickSound, Counts[Index]->GetComponentLocation());
			TimeSinceTick = 0.f;
		}
	}

	// Light the row up, then fade back to its colour; the model spins faster and swells while lit.
	const float Lit = Flash[Index];
	Flash[Index] = FMath::Max(0.f, Flash[Index] - DeltaSeconds / FlashTime);
	if (Counts.IsValidIndex(Index) && Counts[Index] && (Lit > 0.f || Flash[Index] > 0.f))
	{
		const FLinearColor Base = RowColors.IsValidIndex(Index) ? RowColors[Index] : FLinearColor::White;
		Counts[Index]->SetTextRenderColor(FMath::Lerp(Base, FLinearColor::White, Flash[Index]).ToFColor(true));
		Counts[Index]->SetWorldSize(TextSize * (1.f + 0.25f * Flash[Index]));
	}
	if (Icons.IsValidIndex(Index) && Icons[Index])
	{
		IconYaw[Index] = FMath::Fmod(IconYaw[Index] + DeltaSeconds * (IdleSpinSpeed + 540.f * Flash[Index]), 360.f);
		Icons[Index]->SetRelativeRotation(FRotator(0.f, IconYaw[Index], 0.f));
		Icons[Index]->SetRelativeScale3D(FVector(IconSize / 100.f * (1.f + 0.4f * Flash[Index])));
	}
}
