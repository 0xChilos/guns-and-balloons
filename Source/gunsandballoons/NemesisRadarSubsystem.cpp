#include "NemesisRadarSubsystem.h"

#include "Components/ActorComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "NemesisAirship.h"
#include "NemesisBeacon.h"
#include "NemesisBirdFlock.h"
#include "NemesisBomber.h"
#include "NemesisCropDuster.h"
#include "NemesisFlak.h"
#include "NemesisHomingMissile.h"
#include "NemesisMine.h"
#include "NemesisRocket.h"
#include "NemesisTowerCannon.h"
#include "UObject/EnumProperty.h"
#include "UObject/UnrealType.h"

namespace
{
	const TCHAR* const BlipClassPath = TEXT("/Game/Assets/Balloon/BPC_RadarBlip.BPC_RadarBlip_C");

	/** The radar's three blip kinds (E_RadarBlip): small dot, larger dot, big oval. */
	enum class ERadarKind : uint8 { Small, Plane, Zeppelin };

	/** One enemy type on the radar: its kind, and when it lights the warning lamp orange / red (m; 0 = never). TBD */
	struct FRadarBlipSpec
	{
		ERadarKind Kind;
		float WarnOrangeM;
		float WarnRedM;
	};

	/** Blip for this actor, or null if it is not an enemy the radar shows. */
	const FRadarBlipSpec* RadarBlipFor(const AActor* Actor)
	{
		static const FRadarBlipSpec Projectile{ ERadarKind::Small, 60.f, 25.f };
		static const FRadarBlipSpec Plane{ ERadarKind::Plane, 200.f, 80.f };
		static const FRadarBlipSpec Airship{ ERadarKind::Zeppelin, 300.f, 150.f };
		static const FRadarBlipSpec Tower{ ERadarKind::Plane, 180.f, 60.f };      // the cannon reaches 180 m
		static const FRadarBlipSpec Flak{ ERadarKind::Plane, 40.f, 25.f };        // engages within 25 m
		static const FRadarBlipSpec Beacon{ ERadarKind::Small, 0.f, 0.f };        // does no damage itself
		static const FRadarBlipSpec Birds{ ERadarKind::Small, 60.f, 25.f };

		if (Actor->IsA<ANemesisRocket>() || Actor->IsA<ANemesisHomingMissile>() || Actor->IsA<ANemesisMine>()) return &Projectile;
		if (Actor->IsA<ANemesisCropDuster>() || Actor->IsA<ANemesisBomber>()) return &Plane;
		if (Actor->IsA<ANemesisAirship>()) return &Airship;
		if (Actor->IsA<ANemesisTowerCannon>()) return &Tower;
		if (Actor->IsA<ANemesisFlak>()) return &Flak;
		if (Actor->IsA<ANemesisBeacon>()) return &Beacon;
		if (Actor->IsA<ANemesisBirdFlock>()) return &Birds;
		// The team's Blueprint enemies the level generator places.
		for (const UClass* C = Actor->GetClass(); C; C = C->GetSuperClass())
		{
			const FName Name = C->GetFName();
			if (Name == TEXT("BP_TurretTower_C")) return &Tower;
			if (Name == TEXT("BP_RocketBalloon_C")) return &Flak;
			if (Name == TEXT("BP_Lighthouse_C")) return &Beacon;
			if (Name == TEXT("BP_BirdFlock_C")) return &Birds;
		}
		return nullptr;
	}

	/** Sets a Blueprint enum variable to the entry whose name contains Entry ("Small", "Plane", "Zeppelin"). */
	void SetBlueprintEnum(UObject* Object, FName PropertyName, const TCHAR* Entry)
	{
		FProperty* Property = Object->GetClass()->FindPropertyByName(PropertyName);
		UEnum* Enum = nullptr;
		FNumericProperty* Numeric = nullptr;
		if (FEnumProperty* EnumProperty = CastField<FEnumProperty>(Property))
		{
			Enum = EnumProperty->GetEnum();
			Numeric = EnumProperty->GetUnderlyingProperty();
		}
		else if (FByteProperty* ByteProperty = CastField<FByteProperty>(Property))
		{
			Enum = ByteProperty->Enum;
			Numeric = ByteProperty;
		}
		if (!Enum || !Numeric) return;
		for (int32 i = 0; i < Enum->NumEnums() - 1; ++i) // the last entry is the hidden _MAX
		{
			if (Enum->GetDisplayNameTextByIndex(i).ToString().Contains(Entry) || Enum->GetNameStringByIndex(i).Contains(Entry))
			{
				Numeric->SetIntPropertyValue(Property->ContainerPtrToValuePtr<void>(Object), Enum->GetValueByIndex(i));
				return;
			}
		}
	}

	void SetBlueprintNumber(UObject* Object, FName PropertyName, double Value)
	{
		if (FNumericProperty* Numeric = CastField<FNumericProperty>(Object->GetClass()->FindPropertyByName(PropertyName)))
		{
			void* Ptr = Numeric->ContainerPtrToValuePtr<void>(Object);
			if (Numeric->IsFloatingPoint()) Numeric->SetFloatingPointPropertyValue(Ptr, Value);
			else Numeric->SetIntPropertyValue(Ptr, static_cast<int64>(Value));
		}
	}
}

bool UNemesisRadarSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld() && Super::ShouldCreateSubsystem(Outer);
}

void UNemesisRadarSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	BlipClass = LoadClass<UActorComponent>(nullptr, BlipClassPath);
	if (!BlipClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("Radar: %s not found, enemies won't show on the radar"), BlipClassPath);
		return;
	}
	for (TActorIterator<AActor> It(&InWorld); It; ++It) RegisterEnemy(*It);
	SpawnHandle = InWorld.AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateUObject(this, &UNemesisRadarSubsystem::HandleActorSpawned));
}

void UNemesisRadarSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld()) World->RemoveOnActorSpawnedHandler(SpawnHandle);
	Super::Deinitialize();
}

void UNemesisRadarSubsystem::RemoveFromRadar(AActor* Actor)
{
	UWorld* World = Actor ? Actor->GetWorld() : nullptr;
	UNemesisRadarSubsystem* Radar = World ? World->GetSubsystem<UNemesisRadarSubsystem>() : nullptr;
	if (!Radar || !Radar->BlipClass) return;
	TInlineComponentArray<UActorComponent*> Components(Actor);
	for (UActorComponent* C : Components)
	{
		// Destroying it runs its EndPlay, so BP_Balloon drops it on its next radar update.
		if (C && C->IsA(Radar->BlipClass)) C->DestroyComponent();
	}
}

void UNemesisRadarSubsystem::HandleActorSpawned(AActor* Actor)
{
	RegisterEnemy(Actor);
}

bool UNemesisRadarSubsystem::RegisterEnemy(AActor* Actor)
{
	if (!Actor || !BlipClass || Actor->FindComponentByClass(BlipClass)) return false;
	const FRadarBlipSpec* Spec = RadarBlipFor(Actor);
	if (!Spec) return false;

	UActorComponent* Blip = NewObject<UActorComponent>(Actor, BlipClass, TEXT("RadarBlip"));
	static const TCHAR* const KindNames[] = { TEXT("Small"), TEXT("Plane"), TEXT("Zeppelin") };
	SetBlueprintEnum(Blip, TEXT("Kind"), KindNames[static_cast<uint8>(Spec->Kind)]);
	SetBlueprintNumber(Blip, TEXT("WarnOrangeRadiusM"), Spec->WarnOrangeM);
	SetBlueprintNumber(Blip, TEXT("WarnRedRadiusM"), Spec->WarnRedM);
	Actor->AddInstanceComponent(Blip);
	Blip->RegisterComponent(); // after the fields are set, so its BeginPlay already sees them
	return true;
}
