# Weapons — technical reference (C++)

> **For agents.** How the gun code works and how to connect new systems to it. Reloading and ammo are in [weapon-reload](weapon-reload.md). Design lives in [weapons](../features/weapons.md); if the code and the design disagree, the design wins. **Updated:** 2026-11-14

## Core rule
Each gun is a **`UGunComponent` subclass on the player pawn** that owns its mechanism (chambers, state machine, shot, casings) and drives the pose of its own skeletal mesh. **`AFirstPersonCameraPawn` is the only thing that reads input and places the gun on screen**: it calls the gun's virtuals (`PullTrigger`, `LoadRound`, `SetAimingAtAmmo`, `PrepareToHolster`) and moves the mesh (rest/reload pose, sway, recoil, raise/lower). A gun never reads input, and the pawn never touches chambers.

## Files
| File | Role |
| --- | --- |
| `Source/gunsandballoons/GunComponent.h/.cpp` | Abstract base: equip/holster, the shot (spawns a bullet), physics casings, texture, viewmodel pose values |
| `Source/gunsandballoons/GunProjectile.h/.cpp` | `AGunProjectile`: ballistic bullet with a tracer streak; damage and impulse where it lands |
| `Source/gunsandballoons/GunSmoke.h/.cpp` | `AGunSmoke`: world-space smoke simulation and sprites, one per gun; `FGunSmokePuffs` (one kind of smoke) |
| `Source/gunsandballoons/RevolverComponent.h/.cpp` | Pax revolver, 6 chambers, key `1` |
| `Source/gunsandballoons/RifleComponent.h/.cpp` | Martini-Henry rifle, 1 chamber, key `2` |
| `Source/gunsandballoons/ShotgunComponent.h/.cpp` | Homestead 78 double-barrel shotgun, 2 barrels, external hammers, key `3` |
| `Source/gunsandballoons/RevolverAnimInstance.h/.cpp` | Code-driven pose for `SK_Pax`: one clip + cylinder angle + per-chamber round/case scales |
| `Source/gunsandballoons/GunAnimInstance.h/.cpp` | Code-driven pose for the rifle and shotgun: one clip + named bone scales + per-bone poses taken from another clip (`SetBonePose`) |
| `Source/gunsandballoons/FirstPersonCameraPawn.h/.cpp` | Input, aim trace, gun swap, viewmodel placement (sway, recoil, raise) |
| `Content/Assets/Revolver`, `Content/Assets/Rifle_MartiniHenry`, `Content/Assets/Homestead_78` | Meshes, the clips the code uses (`SK_*Armature_*`), casing meshes, material instances |
| `Content/Textures/Pax_Revolver_Textures`, `Content/Textures/Rifle_MartiniHenry`, `Content/Textures/Homestead_78` | Diffuse maps the guns force onto their materials |

No Animation Blueprints: every pose is set from C++ each tick.

## `UGunComponent` API
| Method | Kind | Behaviour |
| --- | --- | --- |
| `SetupWeapon(Mesh, Camera)` | C++ | Mesh to animate, camera to shoot from. **Call before `BeginPlay`.** |
| `SetEquipped(bool)` | BlueprintCallable | Shows/hides the mesh and enables/disables the gun's and mesh's tick. A holstered gun keeps its rounds and spent cases, frozen. |
| `IsEquipped()` | BlueprintPure | |
| `PullTrigger()` | BlueprintCallable, virtual | Fire, click on an empty/spent chamber, or close an open gun (see reload page) |
| `LoadRound()` / `CanLoadRound()` | virtual | Take one round from an ammo supply → [weapon-reload](weapon-reload.md) |
| `EjectCase()` | BlueprintCallable, virtual | RMB on a supply, before a round is taken: a gun with a fired case to pull out uses the click for that and returns true (shotgun, rifle) |
| `SetAimingAtAmmo(bool)` | virtual | Called by the pawn **every frame**; lets the gun open ahead of the click |
| `IsReloading()` | BlueprintPure, virtual | True while open for loading; the pawn blends to the reload pose |
| `GetLiveRounds()` | BlueprintPure, virtual | Live rounds in the gun |
| `PrepareToHolster()` / `IsReadyToHolster()` | virtual | Wind down before a swap; swap waits until ready |
| `GetViewOffset/Rotation()`, `GetReloadViewOffset/Rotation()`, `GetRecoilKickBack/Pitch()` | C++ | Read by the pawn to place the mesh |

Events (`BlueprintAssignable`): `OnFired` (bullet left the barrel), `OnShotHit(Hit)` (bullet landed, after its flight), `OnDryFire`, `OnReloadStarted`, `OnCaseEjected`, `OnRoundInserted`, `OnReloadFinished`. Hook sounds, muzzle flash and camera shake here; the only VFX in code is the smoke (see **Smoke** below).

Protected helpers for subclasses: `FireShot()` (spawns `ProjectileClass` from the camera along `GetViewRotation()` at `MuzzleVelocity`, tracer from `MuzzleOffset`, then `OnFired`; see **Bullets** below), `FirstPersonToWorld(Vector)` (a point on the first-person mesh → where it appears in the world), `ResetCaseSamples()` / `SampleCase()` / `ReleaseCasing()` (hand an animated case on `CaseEjectBone` over to a physics `AStaticMeshActor` with the same position and velocity, corrected for the first-person FOV).

Parameters (`EditAnywhere`, set per gun in its constructor): `Damage`, `Range`, `HitImpulse`, `TraceChannel`, `CasingMesh`, `CaseEjectBone`, `CasingLifeSpan`, `CasingScatter`, `CasingSpin`, `CasingRotationOffset`, `WeaponTexture`, `WeaponTextureParameter` (`DiffuseColorMap`), `ViewOffset`, `ViewRotation`, `ReloadViewOffset`, `ReloadViewRotation`, `RecoilKickBack`, `RecoilKickPitch`. Damage values are stubs; the design lists them as `TBD`. Debug: `bDebugShots`.

## The guns
| | Revolver (`URevolverComponent`) | Rifle (`URifleComponent`) | Shotgun (`UShotgunComponent`) |
| --- | --- | --- | --- |
| Mesh / skeleton | `SK_Pax` / `SK_Pax_Skeleton` | `SK_MartiniHenry` / `SK_MartiniHenry_Skeleton` | `SK_Homestead78` / `SK_Homestead78_Skeleton` |
| Anim instance | `URevolverAnimInstance` | `UGunAnimInstance` | `UGunAnimInstance` |
| Capacity | 6 (`NumChambers`), start `InitialRounds` = 6 | 1, start loaded (`bStartLoaded`) | 2 (`EShotgunBarrel` Right, Left), start `InitialRounds` = 2 |
| Fire | Double action: `++CylinderStep`, shot at `HammerDropTime`; `FirePlayRate` 2 | Shot at `ShotTime`; `FirePlayRate` 1 | Right barrel if live, else left; shot at `ShotTime` (hammer strike, 0.133); no live barrel → `Trigger_Dry_Right`, `OnDryFire` at `DryClickTime`; `FirePlayRate` 1.5 |
| Damage / Range | 5 / 10000 | 143 / 30000 | 220 per shot (24.4 per pellet × 9) / 5000 |
| Reload | per-clip `ReloadPlayRate` 1.5 | `ReloadTime` 2.7 s (open + insert + close clips → play rate) | `ReloadTime` 4.7 s (open + 2 ejects + 2 inserts + close → play rate) |
| Muzzle velocity / muzzle | 330 m/s / 27 cm | 400 m/s / 80 cm | 300 m/s / 76 cm, ±1.5 cm per barrel |
| Bullet drop (`GravityScale`) / drag (`SpeedHalvingDistance`) | ×4 / none | ×2 / none | ×3 / 30 m |
| Per shot (`ProjectilesPerShot`, `PelletSpreadAngle`, `ProjectileVisualScale`) | 1 bullet | 1 bullet | 9 pellets, 3.5° pattern, 0.55 size |
| Round/case bones | `chamber_N`, `chamber_N_spent`, `case_eject` (scaled by the anim instance) | `chamber`, `chamber_spent`, `round_load`, `case_eject` | `shell_L/R`, `shell_L/R_spent`, `shell_load`, `case_eject_L/R`; `hammer_L/R` held cocked from code on live barrels |
| Texture | `T_Pax_256` | `T_MartiniHenry_256` | `T_Homestead78_256` |

Chamber contents use `EGunChamber { Empty, Live, Spent }`. A shot turns `Live` into `Spent`; a spent case stays in the gun until a reload ejects it. Shooting an `Empty`/`Spent` chamber broadcasts `OnDryFire`.

Revolver cylinder: the chamber under the hammer is `CylinderStep mod 6` (`TopChamber`), and the one at the loading gate is the one before it (`GateChamber`). `CylinderStep` only counts up. The cylinder turn is not in any clip; `UpdatePose` drives `CylinderAngle` (60° per chamber).

## Handling (per gun, all off by default)
| Property | Effect | Pax | Martini-Henry | Homestead 78 |
| --- | --- | --- | --- | --- |
| `RateOfFire` (rpm) | `ConsumeRateOfFire()` in each gun's `PullTrigger` ignores a pull sooner than `60 / RateOfFire` s after the last cycle started | 60 (1.0 s) | 0 (the reload sets the pace, ≈ 18 rpm) | 0 (≈ 24 rpm with reloads) |
| `CycleTime` (revolver, shotgun) | Seconds from pull to the next pull. The shot stays at the clip's shot time; `RateForRestOfCycle` stretches or squeezes the rest | 0.8 s | — | 0.3 s (barrel to barrel) |
| `SpreadAngle` | The shot leaves up to this many degrees off the crosshair; the off-angle is uniform in [0, max], so center-weighted | 1.2° | 0.15° | 0.75° (+ 3.5° pellet pattern) |
| `AimRecoilPitch`, `AimRecoilPitchVariance`, `AimRecoilYaw` | Aim kick per shot: pitch × (1 ± variance), yaw ± | 6°, 0.25, 1.5° | 8°, 0.2, 1° | 10°, 0.2, 2.5° |
| `AimRecoilRecovery`, `AimRecoilRecoveryTime` | Share of the kick that settles back by itself, and roughly how long that takes | 0.25, 0.6 s | 0.35, 0.9 s | 0.3, 0.7 s |
| `HandSwayPitch`, `HandSwayYaw`, `HandSwaySpeed` | Hand sway while this gun is in hand | 0.5°, 0.6°, 0.7 | 0.45°, 0.55°, 0.45 | 0.9°, 1°, 0.8 |

The balloon's share of the sway is on the pawn (`BalloonSway*`: 1°, 0.8°, roll 1.5°, speed 0.12). It's the same for every gun, and it's placeholder noise: once the balloon flies, drive it from the basket's real motion. Damage, rate of fire, cycle time, speed and reload times are design values. Recoil, spread and sway are recommendations awaiting confirmation (`decisions.md`).

## Bullets (`AGunProjectile`)
- **Bullet and tracer:** a glowing `/Engine/BasicShapes/Sphere` (`BulletColor`, `BulletSize` 1.6 cm, never below 0.22% of the camera distance) at the front, the streak behind it. It's visible at the muzzle from the frame it's fired.
- **Pellets:** with `ProjectilesPerShot` > 1, `FireShot` spawns that many projectiles. `SpreadAngle` moves the whole shot (center-weighted), then each pellet is tilted up to `PelletSpreadAngle` around it, spread evenly over the circle (`OffsetDirection`). Each pellet has its own `Damage`/`HitImpulse` and `OnShotHit`; `OnFired` (recoil) fires once per shot.
- **Drag:** `SpeedHalvingDistance` (m) becomes quadratic drag `k = ln2 / d` (`FGunShot::Drag`, 1/cm). With drag the flight is integrated in ≤ 1/120 s substeps (gravity − k·|v|·v); without it, the exact parabola is used.
- **Not hitscan.** `FireShot` spawns one bullet per shot. It starts at the eye with velocity `view direction × MuzzleVelocity`, so up close it lands on the crosshair and drops with distance (`GetGravityZ() × GravityScale`). Each tick it moves along the exact parabola and line-traces the segment it covered (complex, `TraceChannel`), so a fast bullet can't skip through thin geometry.
- **Hit:** `ApplyPointDamage` (`Damage`, instigator = the pawn's controller, causer = the pawn), `HitImpulse` on simulating physics, then the gun's `OnShotHit(Hit)`; the bullet is destroyed. With no hit it's destroyed after `Range` cm of flight (or `MaxLifeSeconds`, 10 s).
- **Tracer:** `/Engine/BasicShapes/Cylinder` stretched along the velocity, with `/Engine/EngineMaterials/EmissiveMeshMaterial` (unlit, additive; `Color` = `TracerColor`, `Texture` = `WhiteSquareTexture`). Length = speed × `TracerSeconds` (0.012 s). Width is `TracerWidth` (0.8 cm), and never below 0.12% of the distance to the camera, so far tracers stay visible.
- **Muzzle:** the physics path starts at the eye, but the streak is drawn from `MuzzleOffset` on the gun mesh (converted from the first-person FOV with `FirstPersonToWorld`). It slides onto the real path over `VisualConvergeDistance` (15 m), and its tail never reaches behind the muzzle.
- **Restyle:** make a Blueprint child of `AGunProjectile` (`TracerColor`, `TracerSeconds`, widths, material, or a Niagara component) and set it as the gun's `ProjectileClass`.
- Debug: `bDebugShots` on a gun draws each bullet's path for 2 s (red in flight, green to the hit, yellow impact point).

## Smoke (`AGunSmoke`)
- **Where it lives:** each gun spawns its own `AGunSmoke` (`SmokeClass`, default `AGunSmoke`) at `BeginPlay`, owned by the pawn, at the world origin, destroyed at `EndPlay`. The puffs are in **world space**, not on the first-person mesh, so the player moves through them and leaves them behind. The actor's tick sleeps while it has no puffs. `GetSmoke()` on the gun returns it.
- **Kinds** (`FGunSmokePuffs` on `UGunComponent`; `Amount` 0 turns one off): `MuzzleBlastSmoke` is a burst (`Duration` 0) along the barrel from `MuzzleOffset` in `FireShot`. `MuzzleWispSmoke` is a stream that follows the muzzle for `Duration` (1.6 s; rifle 2 s) and thins out to nothing over it. `BreechSmoke` is a 1 s stream out of a bone when the action opens. Positions and directions go through `FirstPersonToWorld(Vector)`, like the tracer. A stream takes the source's own motion (frame-differenced, `InheritVelocity`); a burst takes the camera's.
- **Breech smoke:** `EmitBreechSmoke(Bones)` (protected). Each shot sets the residue to 1, and it decays as `e^(-t / BreechSmokeClearTime)` (8 s). Opening emits at that strength, then zeroes the residue, so a second opening with no new shot shows nothing; below 0.05 nothing comes out. The calls: rifle at `ExtractStartTime` in `Opening` with a `Spent` chamber (`chamber_spent`); shotgun at the end of `Opening` for each `Spent` barrel (`shell_R/L_spent`); revolver at the end of `ReloadStart` if any chamber is `Spent` (`chamber_N` at the gate). `SetEquipped` stops running streams; puffs already out stay.
- **Physics, per puff, ≤ 1/60 s substeps:** buoyancy `Buoyancy · e^(-age/CoolTime)` up; turbulence from one shared Perlin field (`Turbulence`, `TurbulenceScale`, `TurbulenceSpeed`); drag `(LinearDrag + QuadraticDrag·|v−Wind|)·(v−Wind)` stepped implicitly; then a line trace on `CollisionChannel` (ignores the pawn) that stops the puff at a surface and removes its motion into it (`bCollide`). The radius eases from `StartRadius` to `EndRadius` over `GrowTime`. Opacity is Beer-Lambert: `1 − e^(−Density·(r0/r)²)`, capped at `MaxOpacity`, with a fade-in, a fade-out over the last `FadeOutShare` of its life, and a fade as the camera comes within `NearFadeDistance` + radius.
- **Drawing:** one pooled `UStaticMeshComponent` per puff (`MaxPuffs` 64; the puff closest to the end of its life gives way), `/Engine/BasicShapes/Plane` turned to face the camera with a slow roll, and a dynamic instance of `/Engine/EngineMaterials/Widget3DPassThrough_Translucent` (`SlateUI` = texture, `TintColorAndOpacity` = `Color` + opacity). The puff texture is generated at runtime (32 px, Nearest, lumpy round alpha), so there are **no smoke assets**. A custom `PuffMaterial` must take the same parameter names.
- **Tuning:** first-pass values, not design. Per gun: rifle blast 14 puffs up to 900 cm/s, radius 38; shotgun 16 puffs, 18° cone, radius 42, up to 800 cm/s; revolver keeps the base defaults (10, 700 cm/s, radius 30). For air and look, make a Blueprint child of `AGunSmoke` and set it as `SmokeClass`. `Wind` is zero: set it from a wind system, or from the balloon's airspeed, once one exists.
- **Limits:** sprites are unlit and have no soft-particle depth fade, so a big puff touching a wall shows a hard line where it meets it. No Niagara. Draw cost is one translucent draw per puff; a fast double shotgun shot plus reloads fills about 60 slots.

## What the pawn does each tick
1. Number keys `1`/`2`/`3` → `EquipGun(gun)`.
2. If not swapping: trace from the camera (`InteractRange` 400, `InteractTraceChannel`) → `SetAimingAtAmmo(supply has rounds)`. **RMB** → `TryTakeRound()`. **LMB** → `TryInteract()`, otherwise `ActiveGun->PullTrigger()`. While swapping, `SetAimingAtAmmo(false)` and clicks are ignored.
3. `UpdateLook` (mouse → control rotation, through `AddAimRotation`: pitch clamp, optional yaw limit; roll is kept, it belongs to the balloon sway).
4. `UpdateAimMotion` moves the **aim itself**, and bullets follow it:
   - **Recoil:** `OnFired` adds the gun's `RollAimRecoil()` to a pending kick, applied at `AimKickSpeed` (30/s). Then the `AimRecoilRecovery` share eases back over about `AimRecoilRecoveryTime`.
   - **Sway:** Perlin noise per axis. The hand part comes from `GetHandSway()`, blended across swaps at `HandSwayBlendSpeed`; the balloon part from `BalloonSwayPitch/Yaw/Roll` and `BalloonSwaySpeed` on the pawn. Only the change since the last tick is applied, so it never drifts away and the mouse fights it.
5. `UpdateWeaponSwap`: once `IsReadyToHolster()`, lower over `LowerTime`, then `SetEquipped(false)` on the old gun and `SetEquipped(true)` on the new one, then raise over `RaiseTime`. Picking the gun in hand mid-swap cancels the swap; picking another one redirects it.
6. `UpdateWeaponSway`: places the active mesh at `Lerp(ViewOffset, ReloadViewOffset, smoothstep(reload blend))` + raise drop + look lag + idle bob + recoil kick (`OnFired` sets `RecoilAlpha = 1`).

Camera: `FirstPersonFieldOfView` 80 and `FirstPersonScale` 0.7. All gun meshes are `EFirstPersonPrimitiveType::FirstPerson` with no collision and no shadow, so they never clip into walls.

## Tick order
`PlayerController` → `AFirstPersonCameraPawn` (`AddTickPrerequisiteActor(PC)`). Each gun (`TG_PrePhysics`) → its mesh (`Mesh->PrimaryComponentTick.AddPrerequisite(gun)`, set in `UGunComponent::BeginPlay`). There is **no** dependency between the pawn and the guns, so a click can reach the gun's pose one frame later.

## Connecting a new system
- **Find a gun:** `GetActiveGun()` (BlueprintPure) on the pawn; C++ also has `GetRevolver()`, `GetRifle()`, `GetShotgun()`. From the balloon or another actor: `UGameplayStatics::GetPlayerPawn` → cast to `AFirstPersonCameraPawn`.
- **Sound / VFX / camera shake:** bind to the gun events above. Muzzle position: a socket or bone on `GetMesh()`.
- **HUD ammo:** poll `GetLiveRounds()`; revolver also has `GetChamber(i)`, `GetQueuedRounds()`, `GetState()`; rifle has `GetChamber()`, `GetState()`; shotgun has `GetBarrel(EShotgunBarrel)`, `GetQueuedRounds()`, `GetState()`. No "ammo changed" delegate.
- **Targets taking damage:** handle `OnTakePointDamage` / `TakeDamage` on the target actor. Bullets trace complex collision on `TraceChannel` each tick and ignore the shooter plus its whole attachment tree (the balloon it rides). Damage arrives after the flight time, not on the click.
- **New ammo source:** see [weapon-reload](weapon-reload.md).
- **New gun:** subclass `UGunComponent`. In its constructor, load the mesh's clips, `CasingMesh`, `WeaponTexture` and set the view pose. Override the mechanism virtuals and write the pose to a `UGunAnimInstance` (`Sequence`, `Time`, `SetBoneScale`) every tick. In the pawn: add a `USkeletalMeshComponent` (`SetupViewmodelMesh`, `AnimClass = UGunAnimInstance`) and the component, call `SetupWeapon` in `BeginPlay` before `Super`, and add both to `GetGuns()`/`GetGunMeshes()` (**same order**). Then add the key in `Tick` and raise `StartingGunIndex`'s `ClampMax`.
- **Hammers on the shotgun:** every clip keys both hammers down unless it animates one (`Fire_X` drops it, `Reload_InsertX` cocks it). `UpdatePose` therefore calls `UGunAnimInstance::SetBonePose(hammer_X, Fire_X, 0)` for each live barrel whose hammer the current clip does not drive. Any other code-held bone pose works the same way: sample another clip at a fixed time, axis-agnostic.

## Known limitations and pitfalls
- **Merging the balloon branch:** bullets already ignore the actor the pawn is attached to (and everything attached to it), so the balloon branch's `AddIgnoredActor(GetAttachParentActor())` in `URevolverComponent::Shoot` is no longer needed. Bullets don't inherit the balloon's velocity yet.
- Shots are **hitscan**; the design asks for bullet drop. Not implemented.
- Damage values, play rates and the view poses are first-pass values that need tuning. The shotgun uses the rifle's pose, and its casing reuses the default `CasingRotationOffset` (yaw −90): checked against the FBX, the rig's `case_eject_R/L` shell and `SM_Homestead78_ShellSpent` follow the same convention as the rifle (case along the bone's forward axis, pivot at the base).
- Timing fields (`HammerDropTime`, `CylinderTurnTime`, `ShotTime`, `DryClickTime`, `ExtractStartTime`, `CaseReleaseTime`) are clip time at play rate 1. Retiming or reimporting a clip means updating them.
- `Content/Animations/Homestead_78` and `Content/Animations/Rifle_MartiniHenry` hold duplicate imports, each with its **own skeleton** (`A_*_Skeleton`). They don't play on the gun meshes; use the `SK_*Armature_*` clips in `Content/Assets/...`.
- The material instances are owned by the FBX importer, and a reimport drops their texture. That's why each gun forces `WeaponTexture` at `BeginPlay`. As a side effect, a gun shows **untextured in the editor viewport** (not in PIE) after an FBX reimport, until `DiffuseColorMap` (and `DiffuseColorMapWeight` = 1) is set on its material instance again; `M_MartiniHenry` and `M_Homestead78` have it set now. Gun textures are pixel art: set Filter `Nearest`, Mip Gen `NoMipmaps`, Compression `UserInterface2D` (as `T_MartiniHenry_256`). `T_Homestead78_256` is set the same way (applied 2026-11-14; a reimport of the PNG keeps these settings, but a delete-and-reimport resets them).
- The shells (`SM_Homestead78_Shell`, `SM_Homestead78_ShellSpent`, and the shell/case bones of `SK_Homestead78`) share UV islands in the top strip of `T_Homestead78_256` (x 32-198, y 1-23 px). The original PNG had its shell art somewhere else, so on 2026-11-14 those islands were repainted to match the UVs: brass head, rim and base with a primer, a paper hull copied from the texture's own hull art (x 133-149), and a crimped mouth on the live shell. If the PNG is exported again from the source art, repaint these islands or move the shell UVs.
- A holstered gun does not tick at all; anything time-based on it pauses until it is equipped again.
- No replication.
