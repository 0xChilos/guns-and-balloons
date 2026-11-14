You are connected to my Unreal Engine 5 editor (project "MyProject"). Give the whole game a PS1-era "oily, gritty" look: dark, sepia-green, low resolution, ordered dither, film grain and optional scanlines, like the game The Oily Depths. Work through the steps in order and verify each one. If something can't be done through your tools, give me exact manual steps for it. Put assets in /Game/PostProcess.

## 1. Post-process material: PPM_OilyDepths
- Material Domain = Post Process.
- Blendable Location = Scene Color After Tonemapping.
- Add a SceneTexture node set to PostProcessInput0. Connect its Color to the Custom node's "Dummy" input. Otherwise the scene texture may not be bound for SceneTextureLookup.
- Add one Custom node, Output Type = CMOT Float3. Connect its output to Emissive Color.
- Custom node inputs (in this order; create a parameter for each so a Material Instance can tweak them):

| Input | Source | Default | What it does |
|---|---|---|---|
| UV | ScreenPosition, ViewportUV output | | |
| Res | vector param, RG used | (480, 270) | virtual resolution; try (320, 180) for chunkier pixels |
| Time | Time node | | |
| Desat | scalar | 0.32 | |
| Tint | vector param, RGB | (1.0, 1.02, 0.84) | sepia-green oil grade |
| Contrast | scalar | 1.15 | |
| Levels | scalar | 24 | colour steps per channel; lower = more banding |
| Dither | scalar | 1.0 | 0 = off |
| Vignette | scalar | 0.30 | |
| Grain | scalar | 0.04 | |
| Scanlines | scalar | 0.04 | 0 = off; 0.1 = strong CRT |
| Dummy | SceneTexture PostProcessInput0 Color | | |

Custom node code:
```hlsl
float2 vuv = (floor(UV * Res.xy) + 0.5) / Res.xy;                            // low virtual resolution
float3 c = SceneTextureLookup(ViewportUVToSceneTextureUV(vuv, 14), 14, false).rgb;
float l = dot(c, float3(0.299, 0.587, 0.114));
c = lerp(c, l.xxx, Desat);                                                   // desaturate
c *= Tint.rgb;                                                               // colour grade
c = saturate((c - 0.45) * Contrast + 0.45);                                  // contrast
float B[16] = {0,8,2,10, 12,4,14,6, 3,11,1,9, 15,7,13,5};
int2 q = int2(floor(UV * Res.xy)) % 4;
float t = (B[q.y * 4 + q.x] / 16.0 - 0.5) * Dither;
c = floor(c * Levels + t + 0.5) / Levels;                                    // 4x4 ordered dither + banding
float2 d = (UV - 0.5) * 2.0;
c *= saturate(1.0 - Vignette * dot(d, d));                                   // vignette
float n = frac(sin(dot(floor(UV * Res.xy * 2.0) + Time * 60.0, float2(12.9898, 78.233))) * 43758.5453);
c += (n - 0.5) * Grain;                                                      // film grain
c *= 1.0 - Scanlines * step(1.0, fmod(floor(UV.y * Res.y * 2.0), 2.0));      // scanlines
return saturate(c);
```

Make a Material Instance MI_OilyDepths from it.

## 2. Post Process Volume
Add a PostProcessVolume to the level (or reuse an existing one) and set:
- Infinite Extent (Unbound) = true.
- Post Process Materials (Array): add MI_OilyDepths as an Asset reference, Weight 1.
- Exposure: Metering Mode = Manual, Exposure Compensation about 0 to +1. Turn auto exposure off so the dark mood doesn't get brightened.
- Bloom: Intensity about 0.3. Lens Flares Intensity 0. Chromatic Aberration 0. Motion Blur Amount 0.
- Global Illumination / Reflections: Lumen is fine. For a flatter PS1 feel, try GI Method = None with a low Sky Light.

## 3. Project settings
In Rendering > Default Settings:
- Anti-Aliasing Method = None (or FXAA), for crisp pixel edges.
- Motion Blur off.
- Auto Exposure off.

## 4. World mood
- **Exponential Height Fog:** Fog Density about 0.04, Fog Inscattering Color dark olive (0.06, 0.07, 0.05), Start Distance about 500 cm, Volumetric Fog off.
- **Directional Light:** dim warm (1.0, 0.88, 0.70), intensity about 2 lux, light shafts off.
- **Sky Light:** low intensity (about 0.3), greenish-grey tint.
- **Sky:** no bright blue sky. Use a dark gradient or smog skybox, or set Sky Atmosphere to a low sun and darken it.

## 5. Check
Take a screenshot of the level from a player's view, with and without MI_OilyDepths (Weight 0 / 1), and show me both.

Tuning hints for me:
- Chunkier pixels: lower Res.
- More banding: lower Levels.
- Old CRT feel: raise Scanlines.
- Fewer artefacts on UI: draw UI widgets after this pass (UMG is unaffected by default).
