You are connected to my Unreal Engine 5 editor (project "gunsandballoons"). Upgrade the balloon's radar screen so it shows enemies, and make the warning lamp on the console react to nearby enemies. Work through the steps in order and verify each one. If something can't be authored through your tools (for example Blueprint graph nodes), create everything else and give me exact click-by-click steps for the missing part.

## What the radar has to do
- The radar is the round screen on the left of the balloon's navigation console. Its material is M_Display (instance MI_Display) in /Game/Assets/Balloon. The background texture is T_Balloon_Radar: balloon icon in the centre, 3 range rings, a crosshair and bearing ticks.
- It shows up to 16 enemies as bright green blips, viewed top-down, with the balloon in the centre and its flight direction pointing up the screen. There are 3 blip kinds:
  1. **Small dot:** rockets, mines (radius about 3 px of the 256 px screen).
  2. **Larger dot:** planes (about 6 px).
  3. **Oval:** zeppelins (about 41 × 18 px of the 256 px screen, about three times a plane dot; it was 240 × 115 px and hid everything near it, changed 2026-11-15), with its long axis turned to the zeppelin's heading.
- Distance is shown on a square-root scale (2026-11-15): an enemy at a tenth of the range sits at a third of the radius, so the close ones aren't hidden under the balloon icon; the range rings are no longer even steps in metres.
- Enemies move in and out of range. A blip fades in as it crosses the edge of the screen and fades out as it leaves. Anything further out isn't drawn.
- Blips are pixel-crisp: snapped to the radar texture's pixel grid, to match the PS1 look.
- The rotating sweep stays. Blips stay visible all the time and flash brighter when the sweep passes over them. BlipAfterglow = 1 gives a classic radar look, where blips only show after the sweep.

## 1. Material Parameter Collection
Create MPC_Radar in /Game/Assets/Balloon with:
- 16 vector parameters, Enemy0 … Enemy15, default (0, 0, 0, 0). Each one encodes one blip:
  - x, y = position relative to the balloon, divided by the radar range. +x = right, +y = forward. 1.0 = the outer ring.
  - z = kind: 0 = empty slot, 1 = small dot (rocket, mine), 2 = plane, 3 = zeppelin.
  - w = the enemy's heading relative to the balloon's forward direction, in radians, clockwise (only used to turn the zeppelin oval).
- Scalar RadarRangeM, default 300. Gameplay reads it; the material doesn't use it.
- Scalar RadarJam, default 0: jammer strength at the balloon (0 = clear, 1 = right next to a jammer). BP_JammerTower writes the strongest jammer's value (docs/Unreal_Prompt_4_Locations.md); the material turns it into the jammed state.

## 2. M_Display: replace the radar Custom node
Keep the material Unlit, with Emissive Color driven by one Custom node, Output Type CMOT Float3. Custom node inputs:

| Input | Source | Default |
|---|---|---|
| UV | TexCoord 0 | |
| Time | Time node | |
| Base | T_Balloon_Radar, texture parameter RadarTexture, RGB | |
| SweepSpeed | scalar parameter | 0.5 turns per second |
| Brightness | scalar parameter | 1.6 |
| BlipAfterglow | scalar parameter | 0.35 (0 = constant brightness, 1 = only lit after the sweep) |
| Jam | CollectionParameter node reading MPC_Radar RadarJam | |
| E0 … E15 | CollectionParameter nodes reading MPC_Radar Enemy0 … Enemy15 | |

Custom node code:
```hlsl
float2 uv = (floor(UV * 256.0) + 0.5) / 256.0;                 // snap to the radar texture's pixels
float2 p = uv * 2.0 - 1.0; p.y = -p.y;                         // -1..1, +y = forward (flip if the balloon icon is upside down)
float r = length(p);
float a = frac(atan2(p.x, p.y) / 6.2831853 + 1.0);              // 0..1 clockwise from forward

// jammed state: 0 = clear, 1 = full static (reached at 25 % jammer strength)
float J = saturate(Jam * 4.0);
float tick = floor(Time * 18.0);                                // the static refreshes ~18 times a second
float n = frac(sin(dot(floor(uv * 128.0) + tick * float2(17.0, 31.0), float2(12.9898, 78.233))) * 43758.5453);
float row = frac(sin(dot(float2(floor(uv.y * 48.0), tick), float2(39.346, 11.135))) * 43758.5453);

float sweep = frac(Time * SweepSpeed + J * (row - 0.5) * 0.15);  // jammed: the sweep tears and stutters
float beam = (r < 0.97) ? pow(saturate(1.0 - frac(sweep - a) * 4.0), 3.0) * (1.0 - 0.6 * J) : 0.0;
float3 c = Base * (0.75 + 0.25 * beam) * (1.0 - J * (0.6 - 0.4 * row)) + float3(0.10, 0.45, 0.15) * beam;

float4 E[16] = {E0, E1, E2, E3, E4, E5, E6, E7, E8, E9, E10, E11, E12, E13, E14, E15};
float blip = 0.0;
for (int i = 0; i < 16; i++) {
    float kind = E[i].z;
    if (kind < 0.5) continue;                                   // empty slot
    float2 e = E[i].xy;                                          // 1.0 = radar range
    float er0 = length(e);
    e *= (er0 > 0.0001) ? sqrt(er0) / er0 : 1.0;                // square-root scale: near enemies move off the centre icon (2026-11-15)
    e *= 0.9;                                                    // outer ring = 0.9 of the screen
    float er = length(e);
    float reach = (kind < 2.5) ? 0.05 : 0.22;                   // blip radius (zeppelin = half its length)
    if (er > 0.97 + reach) continue;                            // completely outside the screen
    float2 d = p - e;
    float inside;
    if (kind < 1.5) {
        inside = step(length(d), 0.028);                         // rocket / mine: small dot
    } else if (kind < 2.5) {
        inside = step(length(d), 0.05);                          // plane: larger dot
    } else {                                                     // zeppelin: oval along its heading
        float s = sin(E[i].w), co = cos(E[i].w);
        float2 q = float2(d.x * co - d.y * s, d.x * s + d.y * co) / float2(0.10, 0.22);  // airship oval ~4x a plane dot (enlarged 2026-11-15) (was 0.45 x 0.945: it hid the planes, 2026-11-15)
        inside = step(dot(q, q), 1.0);
    }
    inside *= step(r, 0.97);                                    // never draw over the screen's rim
    float edge = saturate((0.97 - r) / 0.07);                   // per pixel: shapes fade and clip at the rim as they move in / out
    float ea = frac(atan2(e.x, e.y) / 6.2831853 + 1.0);
    float glow = lerp(1.0, saturate(1.0 - frac(sweep - ea) * 1.2), BlipAfterglow);
    float flash = 1.0 + 0.6 * saturate(1.0 - frac(sweep - ea) * 8.0);   // brighter just as the sweep passes
    blip = max(blip, inside * edge * glow * flash);
}
blip *= saturate(1.0 - J * 1.25);                               // real targets vanish while jammed

// ghost targets: look exactly like real blips, jump to new random places every ~0.7 s and flicker
float ghost = 0.0;
float gt = floor(Time * 1.5);
for (int g = 0; g < 6; g++) {
    float h1 = frac(sin(g * 91.7 + gt * 13.3) * 43758.5453);
    float h2 = frac(sin(g * 47.3 + gt * 7.1) * 24634.6345);
    float h3 = frac(sin(g * 12.9 + gt * 3.7) * 15731.7431);
    float2 gp = float2(sin(h1 * 6.2831853), cos(h1 * 6.2831853)) * sqrt(h2) * 0.85;
    float on = step(0.35, frac(h3 * 7.0 + Time * 2.3));
    ghost = max(ghost, step(length(p - gp), lerp(0.028, 0.06, h3)) * on);
}
float snow = step(1.0 - 0.45 * J, n) * (0.4 + 0.6 * n);         // sparkling static grain
float band = step(0.93, row);                                    // bright interference rows
float noise = saturate(snow + 0.5 * band) * J * step(r, 0.97);
blip = max(blip, ghost * J * step(r, 0.97));
c = lerp(c, float3(0.22, 1.0, 0.28), saturate(blip)) + float3(0.05, 0.25, 0.06) * saturate(blip - 1.0);   // bright phosphor green
c += float3(0.12, 0.55, 0.16) * noise;
c *= 0.92 + 0.08 * step(0.5, frac(UV.y * 64.0));                // CRT scanlines
return c * Brightness;
```
If M_Display still has the older Enemy1..Enemy4 vector parameters, remove them. Compile, then save M_Display and MI_Display.

## 3. Marking enemies: BPC_RadarBlip
- Create an enum E_RadarBlip with entries Small (rocket/mine) = 1, Plane = 2, Zeppelin = 3.
- Create an Actor Component BPC_RadarBlip with these instance-editable variables:
  - Kind (E_RadarBlip).
  - WarnOrangeRadiusM (float): the warning lamp glows orange while this enemy is closer than this. 0 = this enemy never warns.
  - WarnRedRadiusM (float): the lamp glows red while it is closer than this (smaller than the orange radius). 0 = never red.
  - Defaults by Kind, set when Kind changes (Construction Script) unless already edited:
    - Small (mine/rocket): orange 60 m, red 25 m.
    - Plane: orange 200 m, red 80 m.
    - Zeppelin: orange 300 m, red 150 m.
  - Each enemy Blueprint or placed instance can override them. This is how a specific enemy gets its own radius.
- Enemies get the component this way:
  - Every enemy Blueprint that should appear on radar (rockets, mines, planes, zeppelins) gets this component, with Kind set.
  - Existing enemy Blueprints: add it to them and tell me which ones you changed.
  - If there are none yet, just create the component.

## 4. Feeding the radar from BP_Balloon
In BP_Balloon, add a looping timer every 0.05 s (started on BeginPlay) that runs UpdateRadar:
1. Range = RadarRangeM (from MPC_Radar) × 100. Unreal units are cm.
2. Balloon frame, flattened onto the ground plane:
   - Fwd = normalize(display bone location − propeller bone location, with Z set to 0). The display is on the front wall and the propeller is at the back.
   - Right = Cross(Up, Fwd).
   - Origin = the basket bone location.
3. Get every actor that has a BPC_RadarBlip (Get All Actors With Component, or keep a registry that components add themselves to on BeginPlay and remove themselves from on EndPlay, which is better for performance). For each one:
   - Delta = ActorLocation − Origin.
   - x = Dot(Delta, Right) / Range.
   - y = Dot(Delta, Fwd) / Range.
   - Skip it if √(x² + y²) > 1.15, or > 2.15 for zeppelins (their big oval starts showing before its centre is on screen).
   - Heading = atan2(Dot(EnemyForward, Right), Dot(EnemyForward, Fwd)) in radians, with EnemyForward flattened.
4. Sort by distance and keep the nearest 16. Write them to Enemy0… as (x, y, Kind, Heading) with Set Vector Parameter Value (collection MPC_Radar). Write (0, 0, 0, 0) into every unused slot, so blips that left range disappear.
5. Optional: ignore enemies more than N m above or below the balloon (expose a variable RadarHeightLimitM, default 0 = off).

## 6. Warning lamp: orange / red when an enemy is within its radius
The lamp at the top centre of the console (material slot M_WarningLamp, light WarningLight, function SetWarningLamp from the integration prompt) shows the worst threat:
1. In the same UpdateRadar loop, before the radar-range skip (an enemy can warn even off the radar), compute DistM = 3D distance from the basket bone to the enemy / 100.
2. Per enemy:
   - Red if WarnRedRadiusM > 0 and DistM < WarnRedRadiusM.
   - Otherwise Orange if WarnOrangeRadiusM > 0 and DistM < WarnOrangeRadiusM.
   - Otherwise Off.
3. Lamp state = the highest level over all enemies (Red > Orange > Off).
4. Hysteresis, so the lamp doesn't flicker at a boundary: once lit, a level only drops after the enemy is 5% beyond that radius.
5. Call SetWarningLamp only when the state changes.

## 5. Test
- Create three test actors, BP_RadarTest_Mine, BP_RadarTest_Plane and BP_RadarTest_Zeppelin. Each has a simple mesh, a BPC_RadarBlip with the matching Kind, and Tick movement in a straight line at 15 / 60 / 20 m/s.
- Place them in a test level around BP_Balloon so they start outside the 300 m range, cross through it, and leave on the other side.
- Check the warning lamp: off with nothing near, orange as each test actor crosses its orange radius, red inside its red radius, then back to orange and off as it leaves. Give BP_RadarTest_Plane a custom WarnOrangeRadiusM of 120 m to check that per-enemy radii work.
- Check, from a camera looking at the radar screen:
  - Each kind has the right shape and size.
  - The zeppelin oval turns with its heading.
  - Blips fade in and out at the edge.
  - Forward (the direction the balloon's display faces) is up on the screen.
- Report the result, and a screenshot if you can.
