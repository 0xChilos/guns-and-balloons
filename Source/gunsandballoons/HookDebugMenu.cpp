#include "HookDebugMenu.h"

#include "Engine/GameViewportClient.h"
#include "GameFramework/PlayerController.h"
#include "HookComponent.h"
#include "InputCoreTypes.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "HookDebugMenu"

void FHookDebugMenu::Open(UHookComponent* InHook, UGameViewportClient* InViewport)
{
	if (IsOpen() || !InHook || !InViewport)
	{
		return;
	}
	Hook = InHook;
	Viewport = InViewport;

	const FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Mono", 11);
	const FSlateFontInfo TitleFont = FCoreStyle::GetDefaultFontStyle("Bold", 12);
	const TWeakPtr<FHookDebugMenu> WeakThis = AsShared();

	TSharedRef<SVerticalBox> Rows = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f)
		[
			SNew(STextBlock).Font(TitleFont).ColorAndOpacity(FLinearColor(1.f, 0.8f, 0.3f))
			.Text(LOCTEXT("Title", "HOOK DEBUG  (H closes)"))
		];

	for (int32 Row = 0; Row < NumRows; ++Row)
	{
		Rows->AddSlot().AutoHeight()
		[
			SNew(STextBlock).Font(Font)
			.Text_Lambda([WeakThis, Row]
			{
				const TSharedPtr<FHookDebugMenu> Self = WeakThis.Pin();
				return Self ? Self->GetRowText(Row) : FText::GetEmpty();
			})
			.ColorAndOpacity_Lambda([WeakThis, Row]
			{
				const TSharedPtr<FHookDebugMenu> Self = WeakThis.Pin();
				return FSlateColor(Self && Self->Selected == Row ? FLinearColor(0.4f, 1.f, 0.4f) : FLinearColor(0.85f, 0.85f, 0.85f));
			})
		];
	}

	// How heavy each stroke from 0 to 100 is with the current settings.
	Rows->AddSlot().AutoHeight().Padding(0.f, 6.f, 0.f, 0.f)
	[
		SNew(STextBlock).Font(Font).ColorAndOpacity(FLinearColor(0.7f, 0.85f, 1.f))
		.Text_Lambda([WeakThis]
		{
			const TSharedPtr<FHookDebugMenu> Self = WeakThis.Pin();
			return Self ? Self->GetStrokePreview() : FText::GetEmpty();
		})
	];

	Rows->AddSlot().AutoHeight().Padding(0.f, 6.f, 0.f, 0.f)
	[
		SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular", 9)).ColorAndOpacity(FLinearColor(0.6f, 0.6f, 0.6f))
		.Text(LOCTEXT("Help", "Up/Down: pick   Left/Right: change   Shift: x10\nLMB: fire hook (at 100)   RMB hold: pump"))
	];

	Widget = SNew(SBorder)
		.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
		.BorderBackgroundColor(FLinearColor(0.f, 0.f, 0.f, 0.65f))
		.Padding(12.f)
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Top)
		[
			Rows
		];

	// Wrapped so the panel keeps its own size in the corner instead of filling the screen.
	TSharedRef<SWidget> Root = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Left).Padding(24.f, 24.f)
		[
			Widget.ToSharedRef()
		];
	Widget = Root;
	InViewport->AddViewportWidgetContent(Root, /*ZOrder=*/100);
}

void FHookDebugMenu::Close()
{
	if (UGameViewportClient* Client = Viewport.Get())
	{
		if (Widget.IsValid())
		{
			Client->RemoveViewportWidgetContent(Widget.ToSharedRef());
		}
	}
	Widget.Reset();
	Viewport.Reset();
	Hook.Reset();
}

void FHookDebugMenu::HandleInput(const APlayerController* PC)
{
	if (!IsOpen() || !PC)
	{
		return;
	}
	if (PC->WasInputKeyJustPressed(EKeys::Up))
	{
		Selected = (Selected + NumRows - 1) % NumRows;
	}
	if (PC->WasInputKeyJustPressed(EKeys::Down))
	{
		Selected = (Selected + 1) % NumRows;
	}
	const int32 Step = PC->IsInputKeyDown(EKeys::LeftShift) || PC->IsInputKeyDown(EKeys::RightShift) ? 10 : 1;
	if (PC->WasInputKeyJustPressed(EKeys::Left))
	{
		Adjust(Selected, -Step);
	}
	if (PC->WasInputKeyJustPressed(EKeys::Right))
	{
		Adjust(Selected, Step);
	}
}

void FHookDebugMenu::Adjust(int32 Row, int32 Steps)
{
	UHookComponent* Target = Hook.Get();
	if (!Target)
	{
		return;
	}
	switch (Row)
	{
	case 0: Target->SetPumpsToFullCharge(Target->GetPumpsToFullCharge() + Steps); break;
	case 1: Target->SetCharge(Target->GetCharge() + Steps * 10.f); break;
	case 2: Target->SetMaxRange(Target->GetMaxRange() + Steps * 100.f); break;
	case 3: Target->SetThrowSpeed(Target->GetThrowSpeed() + Steps * 100.f); break;
	case 4: Target->SetReturnSpeed(Target->GetReturnSpeed() + Steps * 100.f); break;
	case 5: Target->SetHardPumpFrom(Target->GetHardPumpFrom() + Steps * 5.f); break;
	case 6: Target->SetHardPumpPlayRate(Target->GetHardPumpPlayRate() + Steps * 0.05f); break;
	default: break;
	}
}

FText FHookDebugMenu::GetStrokePreview() const
{
	const UHookComponent* Target = Hook.Get();
	if (!Target)
	{
		return FText::GetEmpty();
	}
	// Stroke speed as a percent of an easy stroke; lower = heavier lever.
	const int32 Pumps = Target->GetPumpsToFullCharge();
	const float Easy = FMath::Max(Target->GetPumpStrokeRate(0.f), KINDA_SMALL_NUMBER);
	constexpr int32 MaxShown = 12;
	FString Line = TEXT("Stroke speed   :");
	for (int32 i = 0; i < Pumps; ++i)
	{
		if (Pumps > MaxShown && i == MaxShown - 2)
		{
			Line += TEXT(" ..");
			i = Pumps - 2; // the loop's ++i lands on the last stroke
			continue;
		}
		const float From = 100.f * i / Pumps;
		Line += FString::Printf(TEXT(" %.0f%%"), 100.f * Target->GetPumpStrokeRate(From) / Easy);
	}
	return FText::FromString(Line);
}

FText FHookDebugMenu::GetRowText(int32 Row) const
{
	const UHookComponent* Target = Hook.Get();
	if (!Target)
	{
		return LOCTEXT("NoHook", "(no hook)");
	}
	const TCHAR* Cursor = Selected == Row ? TEXT("> ") : TEXT("  ");
	FString Line;
	switch (Row)
	{
	case 0:
		Line = FString::Printf(TEXT("Pumps 0 -> 100 : %d  (+%.1f per pump)"), Target->GetPumpsToFullCharge(), 100.f / Target->GetPumpsToFullCharge());
		break;
	case 1:
		Line = FString::Printf(TEXT("Pressure       : %.0f / 100%s"), Target->GetCharge(), Target->IsFullyCharged() ? TEXT("  READY") : TEXT(""));
		break;
	case 2:
		Line = FString::Printf(TEXT("Range          : %.0f cm"), Target->GetMaxRange());
		break;
	case 3:
		Line = FString::Printf(TEXT("Throw speed    : %.0f cm/s"), Target->GetThrowSpeed());
		break;
	case 4:
		Line = FString::Printf(TEXT("Reel-in speed  : %.0f cm/s"), Target->GetReturnSpeed());
		break;
	case 5:
		Line = FString::Printf(TEXT("Heavy lever at : %.0f pressure"), Target->GetHardPumpFrom());
		break;
	case 6:
		Line = FString::Printf(TEXT("Last stroke    : %.0f%% speed"), Target->GetHardPumpPlayRate() * 100.f);
		break;
	default:
		break;
	}
	return FText::FromString(Cursor + Line);
}

#undef LOCTEXT_NAMESPACE
