#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "../ResourceType.h"
#include "ResourceLedgerTestObserver.generated.h"

/** Reflected receiver for UResourceLedger::OnCountChanged in the automation test. */
UCLASS(Transient, NotBlueprintable)
class UResourceLedgerTestObserver : public UObject
{
	GENERATED_BODY()
public:
	int32 Changes = 0;
	EResourceType LastType = EResourceType::None;
	int32 LastCount = 0;
	int32 LastDelta = 0;
	UFUNCTION()
	void Changed(EResourceType Type, int32 NewCount, int32 Delta)
	{ ++Changes; LastType = Type; LastCount = NewCount; LastDelta = Delta; }
};
