#include "GameOptions.h"

bool& UGameOptions::Field(EGameOption Option)
{
	switch (Option)
	{
	case EGameOption::Music: return bMusic;
	case EGameOption::Sfx: return bSfx;
	default: return bTutorial;
	}
}

bool UGameOptions::IsOn(EGameOption Option)
{
	return GetMutableDefault<UGameOptions>()->Field(Option);
}

void UGameOptions::SetOn(EGameOption Option, bool bOn)
{
	UGameOptions* Options = GetMutableDefault<UGameOptions>();
	Options->Field(Option) = bOn;
	Options->SaveConfig();
	OnChanged().Broadcast(Option, bOn);
}

UGameOptions::FOnOptionChanged& UGameOptions::OnChanged()
{
	static FOnOptionChanged Delegate;
	return Delegate;
}
