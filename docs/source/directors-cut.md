# Director's Cut: raw notes

> The original text of the latest `Director's Cut` (v2, 2026-11-13), translated from Ukrainian to English on 2026-11-15 with the content unchanged (the Ukrainian original is in git history). Don't edit by hand: replace it with a new version; earlier versions are in git history. The structured version is in [../README.md](../README.md).

```text
Genre: roguelike, roguelite
Steampunk style
Build the mechanics the way they will be in the future game, with control of an aircraft and acceleration with inertia.
Everything should be as diegetic as possible; the balloon is controlled through a panel in front of the player. The balloon's speed can be adjusted; the player can lean right or left by pressing a lever at the matching spot on the balloon. When pressed, an iron panel extends from that side of the balloon and brakes against the air, like a paddle on a kayak.
The balloon has 8 corners.

The player must keep moving forward; a big airship is chasing them, and it can't be hit (defeated). It fires rockets at the player that slowly follow the player on a fixed trajectory and can be shot down or dodged. Crop-duster planes also launch from the airship, follow the player on the right or left side and shoot at them; they can be shot down by hitting the crew, the fuel tank, the engine or the propeller. The nemesis's attack types depend on the distance to the balloon. Closest: rockets, further: mines on parachutes, further: planes, further: crop dusters.
Weapons should be as awkward as possible, preferably single-shot. They should also be diegetic and lie in a holster on the wall inside the balloon's basket, with their rounds there too, so the player reloads by interacting with them. For example a harpoon, a single-shot rifle or a shotgun.
There should be bullet drop, so the player has to account for it when shooting.
The game loop is flying over islands, plus a safe zone in the form of the airship towing the balloon after flying over a level. In this safe zone the player can restock supplies (ammo), repair and upgrade the balloon, buy new gear.
The islands differ by biome, each with its own hazards:
Rocky island – tall and wide rocks that the player has to account for when planning the route and fly around early
Jungle – tall thin trees; they're easier to fly around than mountains, but there are more of them and they stand closer together
City – has tall buildings to fly around, plus fog that makes it harder to orient yourself. Radar interference, jammers, a minigame to restore the radar.
Desert – has tall towers with guards who shoot at the player, sandstorms that come and go periodically. Beacons that identify the player and send extra planes (don't get into the light, or shoot the lamp)
Field – has no natural obstacles, but birds fly over it that must be avoided or shot down
Winter - random winds + the engine needs constant heating + snowy mounds
Lava - unexpected pillars of lava shooting up + high temperature  Resources lie on the surroundings; they can be picked up with the grappling hook. Or there are flying balloons with resources that can also be hooked.

Stove engine. Fuel gauge = 0 - default slow speed, fuel >0<90 - acceleration, fuel >90 - double acceleration, damage to the engine.

Sokin - basic 3C (camera and interaction), balloon and weapons
Vano - the AI
Rostik - Engine, Doc - design doc, menus, art
Art style
Reference: Oily Depths
Style: low-poly PS1-style model
Textures: blocky low-resolution
Shading: flat
Render and post-processing step: low-res render, colour quantize with Bayer dither, 2× nearest-neighbour upscale, grain and vignette.

Airship
Planes Rockets
Balloon (basket, envelope, stove engine)
Navigation panel ()
Weight
Weapons?


Main menu:
Continue
New Game
Settings
Credits
Leave
Legacy


Hook - works like the flamethrower in Hunt. To fire again you have to pump up the pressure.

Double-barrel shotgun

Single-shot rifle

Harpoon

Revolver


The stove engine consists of the following elements:
Engine body - a big metal object at the back of the balloon to which all the interactive elements are attached; it shows the balloon's health level; when it's damaged, damage decals appear
Stove - the place where the player throws wood so the propeller works. It has a door that must be closed to keep the heat; with the door open, heat is lost faster. Different fuel types give different amounts of heat charge.
Temperature level - also the fuel level, shows how much fuel is left in the airship  The engine has 2 gauges:
Temperature: The temperature drops by 1 degree per second, but this drop stops for 10 seconds at the moment fuel is added
If the temperature drops to 0, heating after adding wood starts 10 seconds later and the temperature rises at 5 degrees per second up to the final added temperature.
Burn time: shows how long the temperature will be held; if the time drops to 0, the temperature drops by 5 degrees per second. The maximum burn time = 10 minutes and represents the stove's maximum capacity.  Each item adds a different amount of temperature and burn time:
 Wood 3 minutes  - 20 degrees (7)
Coal 2 minutes  - 30 degrees (13)
Crystal 10 seconds - 50 degrees (24)
  The bucket holds at most 10 units of fuel


The radar shows enemy objects on the map on a retro two-colour monitor. The radar has distance marks to the vessel, showing the range to the Airship.
Minimalist speedometer like on old trains

A detailed breakdown of the navigation panel. Which elements there are and what they look like.
```
