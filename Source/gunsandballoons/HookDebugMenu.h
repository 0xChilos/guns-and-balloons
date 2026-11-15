#pragma once

#include "CoreMinimal.h"

class APlayerController;
class SWidget;
class UGameViewportClient;
class UHookComponent;

/**
 * Debug menu for tuning the grappling hook in play (H on the player pawn).
 *
 * A Slate overlay in the top-left corner that never takes the mouse, so the game keeps playing while it is
 * open: Up/Down pick a row, Left/Right change it (hold Shift for 10x steps). Rows: pump strokes from 0 to
 * 100, the pressure itself, range, throw speed, reeling speed, where the lever gets heavy and how slow the
 * last stroke is; below them, the speed of every stroke. Values live on the UHookComponent and
 * last until the end of the play session; copy the ones you like into its defaults.
 */
class GUNSANDBALLOONS_API FHookDebugMenu : public TSharedFromThis<FHookDebugMenu>
{
public:
	void Open(UHookComponent* InHook, UGameViewportClient* InViewport);
	void Close();
	bool IsOpen() const { return Widget.IsValid(); }

	/** Reads this frame's arrow keys. Call every tick while open. */
	void HandleInput(const APlayerController* PC);

	static constexpr int32 NumRows = 7;

private:
	FText GetRowText(int32 Row) const;
	FText GetStrokePreview() const;
	void Adjust(int32 Row, int32 Steps);

	TWeakObjectPtr<UHookComponent> Hook;
	TWeakObjectPtr<UGameViewportClient> Viewport;
	TSharedPtr<SWidget> Widget;
	int32 Selected = 0;
};
