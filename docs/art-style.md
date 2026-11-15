# Art style

> **Status:** Draft · **Owner:** Rostik · **Updated:** 2026-11-13

## Direction
- **Reference:** Oily Depths.
- **Models:** low-poly, PS1 style.
- **Textures:** low resolution, blocky (nearest filtering).
- **Shading:** flat.

## Rendering and post-processing (in order)
1. Render at low resolution.
2. Colour quantisation with Bayer dithering.
3. 2× upscale (nearest neighbour).
4. Grain.
5. Vignette.

## Key assets
| Asset | Parts |
| --- | --- |
| Balloon | Basket, envelope, stove engine |
| Navigation panel | Gauges, levers |
| Weapons | See [weapons.md](features/weapons.md) |
| Nemesis airship | — |
| Enemies | Planes, crop dusters, rockets, mines |
