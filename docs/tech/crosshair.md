# Crosshair — technical reference (C++)

> **For agents.** Design: [crosshair](../features/crosshair.md). **Updated:** 2026-11-14

## Files
| File | Role |
| --- | --- |
| `Source/gunsandballoons/CrosshairHUD.h/.cpp` | `ACrosshairHUD : AHUD`, set as `HUDClass` by `AFirstPersonCameraGameMode`. Computes spread, opacity and inertia each frame, draws the material |
| `Content/Materials/Crosshair/M_Crosshair` | UI-domain, translucent material; one Custom HLSL node draws the shape |
| `Content/Materials/Crosshair/MI_Crosshair` | The look: `Fill`, `Outline`, `OutlineOn`, `Len`, `Thick`, `Grain`, `Levels` |
| `Scripts/Editor/create_crosshair_material.py` | Builds or updates both assets in place. It never deletes: `DeleteAllMaterialExpressions` asserts (`!IsRooted`) and crashed the editor |

## How it works
- **Drawing:** the HUD draws after post-processing, so the material does the art style itself. One cell = `round(ViewportHeight / VirtualHeight)` screen pixels. The quad is `QuadCells`² cells, snapped to the cell grid. The Bayer threshold uses `Origin` (the quad's first cell on the screen grid), so the dither pattern doesn't crawl when the crosshair moves. Opacity is exactly 0 or 1. Don't use the Masked blend mode: canvas ignores the opacity mask, and the whole quad draws dark.
- **Parameters set per frame:** `QuadCells`, `Origin`, `Gap`, `DotSize`, `TickOpacity`, `DotOpacity`, `GrainTime`, and `TickOffset` (cells; the inertia swing, ticks only).
- **Spread (degrees):**
  - gun `GetCrosshairSpread()` (`SpreadAngle` + `PelletSpreadAngle` when more than one projectile);
  - plus `RecoilAlpha × AimRecoilPitch × RecoilBloom`;
  - plus `min(TurnSpeed × TurnBloom, MaxTurnBloom)`;
  - plus the balloon's speed and acceleration (`min(…, MaxBalloonBloom)`);
  - plus `GunLowered × LoweredBloom`.
  - It opens at `OpenSpeed` and closes at `CloseSpeed`. Converted to cells with the camera's horizontal FOV, then clamped to `MinGap`..`MaxGap`.
- **Inertia:** a 2D damped spring (`InertiaFrequency`, `InertiaDamping`), substepped at 120 Hz and clamped to `MaxInertia`.
  - Its target: `(-yawRate, +pitchRate) × TurnInertia`, plus the balloon's acceleration projected on the view's right and up axes, `× BalloonInertia`.
  - The balloon is `GetAttachParentActor()`; its velocity and acceleration come from position deltas.
  - With `bInertiaMovesDot`, the whole quad moves instead of only the ticks.
- **Pawn and gun accessors added:** `AFirstPersonCameraPawn::GetGunLowered()`, `GetRecoilAlpha()`; `UGunComponent::GetCrosshairSpread()`, `GetAimRecoilPitch()`.

## Testing
`HighResShot` leaves out the HUD. To check the look, render `MI_Crosshair` with `RenderingLibrary.draw_material_to_render_target` and export it. To check the motion, read `GetCurrentSpread`, `GetCurrentGapPixels` and `GetInertiaOffset` in PIE.
