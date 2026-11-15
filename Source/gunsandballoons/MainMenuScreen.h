#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MainMenuScreen.generated.h"

class UMainMenuWidget;
class UMaterialInterface;
class UWidgetComponent;
class UWidgetInteractionComponent;

/**
 * Shows the main menu on a panel in the 3D scene instead of as an overlay, so the game's filter
 * (MI_OilyDepths) pixelates, dithers and grades it like everything else (docs/features/ui-menus.md).
 *
 * Every frame the panel is placed Distance in front of the active camera and sized to fill its view
 * exactly; its widget gets the viewport's aspect at DrawHeight rows. It is unlit and translucent,
 * so the widget's soft haze and fades blend into the scene behind it.
 *
 * Mouse only: a Widget Interaction Component follows the mouse and gives the widget hover; the
 * player controller (AMainMenuPlayerController) forwards LMB as PressPointer / ReleasePointer. The
 * system cursor is hidden; the widget draws its own pixel cursor at the hit point.
 */
UCLASS()
class GUNSANDBALLOONS_API AMainMenuScreen : public AActor
{
	GENERATED_BODY()

public:
	AMainMenuScreen();

	virtual void Tick(float DeltaSeconds) override;

	void PressPointer();
	void ReleasePointer();

	UFUNCTION(BlueprintPure, Category = "Menu")
	UMainMenuWidget* GetMenu() const;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Menu")
	TObjectPtr<UWidgetComponent> Screen;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Menu")
	TObjectPtr<UWidgetInteractionComponent> Pointer;

	/** The menu to show (UMainMenuWidget or a Blueprint child of it). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Menu")
	TSubclassOf<UMainMenuWidget> MenuClass;

	/** How far in front of the camera the panel floats (cm). Closer than anything in the scene. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Menu", meta = (ClampMin = "15.0"))
	float Distance = 60.f;

	/** Widget rows over the full view height. 1080 = 4 widget pixels per row of the filter's 270-row frame. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Menu", meta = (ClampMin = "270.0"))
	float DrawHeight = 1080.f;

	/** Multiplies the panel's colours, to balance the menu against the scene's exposure. TBD. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Menu", meta = (ClampMin = "0.0"))
	float Brightness = 1.f;

	/**
	 * Panel material instead of the engine's translucent widget material. In gameplay levels this is
	 * /Game/UI/PauseMenu/M_PauseMenuPanel (depth test off, so the gun and the basket can't cover the
	 * panel), made by Scripts/Editor/create_pause_menu_material.py. Empty: the engine's material.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Menu")
	TSoftObjectPtr<UMaterialInterface> PanelMaterial;

private:
	void FitToView();
};
