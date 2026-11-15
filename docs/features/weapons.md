# Weapons

> **Status:** Draft · **Owner:** Sokin · **Updated:** 2026-11-15 (items placed in the basket)

## Why
Every shot should be a decision: the weapons are awkward, rounds are scarce, and reloading is manual and takes time while the balloon flies and the stove dies down.

## How it works
- Weapons are mostly **single-shot** (or have a very small capacity).
- **Bullet drop:** projectiles follow a ballistic trajectory; the player has to compensate.
  - Every shot is a physical bullet, not a hitscan: it flies at the weapon's speed under gravity and hits (damage, a push on physics objects) only when it arrives. Every bullet is a **tracer**: a glowing streak leaves the muzzle and converges with the crosshair at ~15 m. A bullet doesn't touch the player or anything the player is attached to (the balloon with its basket).
  - **Each weapon has its own bullet drop** (a gravity multiplier): the revolver drops the most, the rifle the least.
  - **The shotgun fires shot, not a bullet:** each shot is 9 pellets in a fan; air slows the shot quickly, so it is deadly up close but weaker and dropping steeply at range. Pellets are smaller than bullets, with short tracers.
- **Handling (tuned per weapon):** recoil kicks the aim itself up: part of it returns on its own, the player pulls the rest down with the mouse; spread deflects the bullet from the crosshair (mostly near the centre); the aim sways: the hand holding the weapon plus the rocking basket (which also tilts the horizon slightly); the rate of fire limits how often the trigger can be pulled. Tuned for all three: the revolver is middling; the rifle has almost no spread but a slow, long sway (you have to wait for the shot); the shotgun has the strongest recoil, and the second barrel 0.3 s later shoots higher.
- **Aiming down sights (ADS):** hold RMB with the revolver, rifle or shotgun in hand: the weapon rises from the hip to the eye line (sight in the centre), the view zooms in a little and the mouse slows down with the zoom; the crosshair fades out (you aim with the front sight). The eye sits on the weapon itself and looks along the barrel at the front sight (revolver: right behind and above the hammer, the front sight rests on the hammer; rifle: above the barrel just in front of the receiver, the front sight rests on the rear sight; shotgun: a little above and behind the rib between the hammers, looking down along the barrels); the action behind the eye and the stock are not visible. Release: the weapon returns to the hip. RMB on the ammo crate is still reloading; while the weapon is open for reloading you can't aim. It is visual only: spread, recoil and aim sway stay the same. The hook (RMB = pump) and the extinguisher don't aim.
- **Smoke (black powder):** after each shot a dense cloud leaves the muzzle, and the barrel keeps smoking for ~1.5–2 s with a thin trail that follows the muzzle. The smoke is physical and lives in the world, not on the weapon model: air slows it, while hot it rises, spreads and thins out, eddies and wind carry it, it flows around walls and the basket, and the player leaves it behind. When the weapon is first opened for reloading after a shot (the rifle's breech, the broken-open shotgun, the revolver's gate), a little smoke comes out of the breech; the longer you wait after the shot, the less; opening again without a new shot gives no smoke. The look follows the [art style](../art-style.md), PS1-like: blocky pixel puffs with flat two-tone shading (lighter core, darker edge), no smooth transparency: thinning smoke breaks up with Bayer dithering; the puffs move choppily (~15 fps).
- Weapons and rounds are kept **in a holster on the inner wall of the basket**.
- **Manual reloading:** the player takes a round from the holster and loads the weapon (an interaction, not an R key).
  - There is no reload key: **one RMB click on the ammo crate / holster = one round** (for every weapon; the first click also opens the crate's lid). LMB is always the trigger, including when aiming at the crate. The revolver opens its gate, turns the cylinder to the next empty or spent chamber, ejects the case (if any) and inserts that one round.
  - Aiming at the ammo crate already opens the gate (the weapon goes to its reload pose); looking away closes it.
  - RMB clicks while the gate is open queue more rounds; the gate doesn't close between them. A full cylinder takes no round (RMB on the crate then does nothing).
  - Firing during a reload: the current round finishes loading, the rest of the queue is cancelled.
  - After a shot, a **spent case** stays in the chamber (sooty, as painted in the texture, not a clean brass round), so you can see how many rounds were used. On reload the spent case is ejected and a new clean round goes in its place.
  - **Rifle (Martini-Henry, 1 round):** aiming at the ammo crate (if there is no live round in the chamber) lowers the lever and opens the breech; the spent case stays in the chamber. Then each RMB on the crate is one step: (1) while there is a spent case in the chamber, RMB: the extractor pulls it out and ejects it (no round is taken from the crate); (2) when the chamber is empty, RMB: a round goes into the chamber and the breech closes. After a shot that's 2 clicks. The breech **stays open until the next click**, even if you look away; LMB closes the breech without firing.
  - **Shotgun (Homestead 78, 2 rounds, external hammers):** the same rules. LMB fires the right barrel (front trigger); once it's spent, the left one (rear trigger); with no live rounds the trigger only clicks. Aiming at the ammo crate (if at least one barrel isn't loaded) only breaks the gun open; spent cases stay in the barrels. Then each RMB on the crate is one step: (1) while there is a spent case, RMB pulls it out and ejects it (no round is taken from the crate); (2) when no spent cases are left, RMB loads one round into the first empty barrel (right, then left), and that barrel's hammer is cocked. Two spent barrels = 4 clicks: case, case, round, round. The gun **stays open** even if you look away; when both barrels are loaded it closes by itself. LMB while it's open closes it without firing; LMB during a step finishes the step (the round in the hand is loaded) and cancels the rest of the queue.
- **Hand and items:** the player starts with an **empty hand** and holds one item at a time. The revolver, the rifle, the double-barrel shotgun (Homestead 78), the [grappling hook](grappling-hook.md) and the [fire extinguisher](fire-extinguisher.md) each sit in their own place in the basket. There are no weapon-switch keys.
  - **Where they are** (2026-11-15): on the walls of the basket, where the balloon model has a place for each. Weapon wall (right of the navigation display): the revolver hangs on its hook by the trigger guard, barrel down; the rifle and the shotgun stand upright in the gun rack, butts in the pockets, barrels in the notches of the top shelf. Tool board (left of the display): the wrench on its peg, the grappling hook diagonally on its two brackets, the extinguisher in its floor cup. Every item can be reached and clicked from the player's place. The revolver's hook sits behind the display housing, so from the player's place only the top of the revolver shows (clicking it works).
  - **Take:** LMB on an item with an empty hand: it rises into the hand (the extinguisher with its own animation).
  - **Put back:** LMB on the item's empty place: the item is lowered out of view and lies in its place; the hand is empty.
  - **Swap:** LMB on another item while holding one: the held item goes back to where it was taken from, and the new one rises.
  - An item first finishes what it started (a shot, the hook returning), and only then is lowered. A weapon can't be put back or swapped during a reload (while it's open). In its place an item keeps its rounds, cases, pressure and supply; it isn't loaded there.
  - Each place fits its own item; another item can't be put in its place.
  - With an empty hand you can't shoot or take rounds, but you can work the [levers](balloon-controls.md).
- New weapons and ammo come from the [safe zone](safe-zone.md).

## Arsenal
| Weapon | Rounds | Role |
| --- | --- | --- |
| Single-shot rifle | 1 | Precise long-range shot |
| Double-barrel shotgun | 2 | Close targets, birds, rockets |
| Harpoon | 1 | Heavy projectile, strong bullet drop |
| Revolver | TBD | The exception to "single-shot" |
| [Grappling hook](grappling-hook.md) | — | Utility, not a combat weapon |
| [Fire extinguisher](fire-extinguisher.md) | Supply 100 | Utility, not a combat weapon |

## Parameters
| Parameter | Value | Note |
| --- | --- | --- |
| Damage, projectile speed, gravity | TBD | Per weapon |
| Bullet speed | Revolver 330 m/s; the rest TBD | `MuzzleVelocity`: rifle 400, shotgun 300 m/s (placeholders) |
| Bullet drop | TBD | `GravityScale`: revolver 4, rifle 2, shotgun 3 (+ air drag) |
| Shot | TBD | `ProjectilesPerShot` 9, fan `PelletSpreadAngle` 3.5°, speed halves every `SpeedHalvingDistance` 30 m |
| Pax revolver | Damage 5 · 60 shots/min · cycle 0.8 s · 330 m/s | `Damage`, `RateOfFire`, `CycleTime`, `MuzzleVelocity` |
| Martini-Henry rifle | Damage 143 · 18 shots/min · cycle 2.7 s · 400 m/s · reload 2.7 s | Cycle = reload (`ReloadTime`); the rate follows: shot + 2.7 s ≈ 18/min |
| Homestead 78 shotgun | Damage 220 per shot · 24 shots/min · cycle 0.3 s · drop from 30 m · reload 4.7 s | 0.3 s between barrels (`CycleTime`); 4.7 s is a full reload of both barrels; rate ≈ 24/min including the reload |
| Recoil, spread, sway | TBD (see the proposal in [decisions](../decisions.md)) | `AimRecoil*`, `SpreadAngle`, `HandSway*` on the weapon; basket sway `BalloonSway*` on the player (shared) |
| Reload time | Rifle 2.7 s, shotgun 4.7 s; revolver TBD | Animation time without pauses between RMB clicks |
| Holster capacity (ammo) | TBD | `UAmmoSupplyComponent::Rounds`, currently −1 = unlimited |
| Item poses in the basket | TBD | One `UWeaponRackComponent` per item on `BP_Balloon` (`Rack_*`), snapped to its `SK_Balloon` socket (`AttachSocket`); the pose is `DisplayOffset` / `DisplayRotation` in the balloon mesh's axes, worked out by `Scripts/Editor/add_basket_item_racks.py` (tune by eye) |
| Item click zone | TBD | Model size + `InteractPadding` 3 cm (placeholder) |
| Revolver rounds at start | TBD | `URevolverComponent::InitialRounds`, currently 6 |
| Rifle round at start | TBD | `URifleComponent::bStartLoaded`, currently yes |
| Shotgun rounds at start | TBD | `UShotgunComponent::InitialRounds`, currently 2 |
| Aiming down sights (ADS) | TBD | Pose `AimViewOffset`, time `AimTime` (revolver 0.15 s, rifle 0.25, shotgun 0.2), zoom `AimFieldOfView` (80°, 70°, 82° at a base 90°), model FOV `AimFirstPersonFieldOfView`, near clip plane `AimNearClipPlane` (rifle and shotgun 1 cm: the eye is on the action) on the weapon; `AimSwayScale`, `AimBobScale`, `AimKickScale` on the player (placeholders) |
| Revolver reload animation speed | TBD | `ReloadPlayRate` 1.5× |
| Smoke | TBD | `MuzzleBlastSmoke`, `MuzzleWispSmoke`, `BreechSmoke`, `BreechSmokeClearTime` on the weapon; air and look in `AGunSmoke` (starting values, tune in game) |

## Dependencies
[Airship and enemies](nemesis-airship.md) (targets and weak points), [Safe zone](safe-zone.md).

## Open questions
- [ ] How many weapons does the player carry at once? Currently one item in the hand; the rest lie in their places.
- [ ] The revolver's hook is mostly hidden behind the display housing from the player's place: keep it there, or hang the revolver somewhere more visible?
- [ ] Do we keep the revolver if the pillar is single-shot weapons?
- [ ] Different ammo for different weapons? Currently one crate loads whatever weapon is in the hand.
