# Heavy Shooter: Unreal third-person prototype

A third-person shooter prototype that aims for a heavy, Battlefront II-style feel. You play an armoured trooper
with a pump-action shotgun on a test range. The camera follows your mouse instantly. The body and the gun have
weight: they trail behind when you look around, and they trail more when you aim down sights.

Everything is C++. The trooper, the shotgun, the targets and the range are built at runtime from the engine's
basic shapes, so there are no art assets to import and nothing to set up in the editor.

## Getting it running

Requirements: Unreal Engine 5.3 or newer (the project is set to 5.5), plus a C++ toolchain: Visual Studio 2022
with the "Game development with C++" workload, or Rider. On macOS you need Xcode.

1. Double-click `HeavyShooter.uproject`. If you use a different engine version, right-click it and choose
   **Switch Unreal Engine version** first.
2. When the editor says the `HeavyShooter` module is missing, click **Yes** to build it. You can also
   right-click the `.uproject` file, choose **Generate Visual Studio project files**, and build the
   `HeavyShooterEditor` target from your IDE.
3. The editor opens on an empty, dark level. Press **Play**. The game mode sees that the level is empty and
   builds a lit firing range around you.

To use your own level, give it a Player Start and some floor, and leave `HeavyGameMode` as the game mode (it is
the project default). The range is only built automatically when the level has nothing to stand on. You can
also drag `HeavyTestRange` into any level.

## Controls

| Action         | Keyboard / mouse       | Gamepad               |
|----------------|------------------------|-----------------------|
| Move           | WASD                   | Left stick            |
| Look           | Mouse                  | Right stick           |
| Fire           | Left mouse (hold to keep pumping) | Right trigger |
| Aim down sights| Right mouse (hold)     | Left trigger          |
| Sprint         | Left Shift (hold)      | L3 (toggle)           |
| Jump           | Space                  | A                     |
| Crouch         | C or Left Ctrl         | B                     |
| Reload         | R                      | X                     |
| Swap shoulder  | V                      | R3                    |
| Help / tuning readout | H               |                       |

## What makes it feel heavy

Every moving part is a damped spring chasing a target (`Source/HeavyShooter/HeavySpring.h`). A spring has
mass: it accelerates, carries momentum, and swings slightly past its target before it settles. Nothing snaps.

**The gun lags behind your view.** The camera turns as soon as you move the mouse, and the shotgun follows on its
own spring. A fast flick leaves it up to 10° behind, and it settles in about 0.2 s with a little overshoot. When
you aim down sights the spring gets softer. The gun then settles in about 0.25 s, and a steady turn leaves it
around 5° behind.

**The lag affects your aim.** The reticle is drawn where the barrel is really pointing, and the pellets go there.
A faint dot marks the screen centre, so you can see how far the gun is trailing. Set
`bWeaponLagAffectsAim = false` if you want the lag to be cosmetic only.

**The body turns more slowly than the gun.** The torso twists towards the gun first and the legs follow. While
sprinting, the body turns to face the direction you're running.

**Movement has momentum.** Acceleration is slow, and sprint builds up even more slowly. Low ground friction means
that when you change direction, your momentum carries you on for a moment. Gravity is strong and air control is
low.

**The body reacts to your movement.** The torso leans into acceleration and into turns, and it rocks back when
you stop. Hard landings make the body sink, dip the gun and the camera, and briefly slow you down.

**The shotgun kicks hard.** Each shot kicks the view up a little (you pull it back down), flips the muzzle, drives
the gun back into the shoulder, rocks the torso back, punches the camera and shakes it. All of these are
impulses into the same springs, so the recovery feels natural. Aiming and crouching reduce recoil.

**The gun has to come up before it can fire.** When sprinting, the gun is carried low across the body. Pressing
fire stops the sprint and buffers the shot, and it fires once the gun is raised.

## The shotgun

`AHeavyShotgun` is modelled on a classic pump-action: a blued steel receiver and barrel, a magazine tube, a
walnut stock with a ribbed fore-end, a bead front sight, and a four-shell side saddle.

- **Pellets:** 9 per shot, fired in a consistent pattern: one near the centre and a jittered ring around it.
  The reticle circle shows the true spread. Spread grows when you're moving or in the air, and with bloom from
  repeated shots. Damage is full out to 9 m and falls to 25% at 28 m.
- **Firing:** after each shot the fore-end racks back, ejects a red hull that bounces off the floor with real
  physics, and slams forward. Your left hand follows the pump.
- **Reloading:** shells go in one at a time. Your left hand takes each shell from the belt and pushes it into the
  loading port. Fire during a reload to stop early. If you reload from empty, it finishes with a pump to chamber a
  round.
- **Physics:** pellets push physics objects around. Crates and barrels on the range fly when you shoot them.
- **Sound:** none is included. Assign any sounds you have to `FireSound`, `PumpBackSound`, `PumpForwardSound`,
  `InsertShellSound` and `DryFireSound` on the shotgun.

## The range

- A checkered 80 × 80 m floor. The squares make speed and momentum easy to read.
- A shooting lane with targets at 6, 12, 20 and 30 m, so you can see the damage fall off with distance.
- Two strafing targets.
- Targets wobble when hit, take extra damage to the head, fall flat when killed and stand back up after
  3 seconds.
- A cover course with walls at crouch height and standing height, plus pillars.
- A 3 m platform with a ramp for testing landings.
- Crates and barrels with physics.

## Tuning

All the feel values are properties on `HeavyCharacter`, grouped under **Heavy|...** categories: Movement,
Camera, Weapon Lag, Poses, Recoil and Spread. The shotgun's values are under **Shotgun|...**.

To try values live, press Play, then **F8** to eject, select the character and edit it in the Details panel.
Changes made this way are lost when you stop. To keep them, create a Blueprint child of `HeavyCharacter` and set
the values there. Then make a Blueprint of `HeavyGameMode` that uses it as the **Default Pawn Class**, and select
that game mode under **Project Settings → Maps & Modes**.

Press **H** in game to show a readout of your speed, current weapon lag in degrees and current spread.

The values that matter most:

| Property | Default | Effect |
|---|---|---|
| `WeaponLagFrequencyHip` / `Aim` | 5.0 / 3.4 Hz | Lower values give a heavier gun that lags more. |
| `WeaponLagDampingHip` / `Aim` | 0.65 / 0.6 | Below 1 the gun swings past the target and settles. At 1 it doesn't overshoot. |
| `WeaponMaxLagHip` / `Aim` | 10° / 7° | Hard limit on how far the gun can trail the camera. |
| `bWeaponLagAffectsAim` | true | Whether pellets follow the lagging gun or the screen centre. |
| `BodyTurnFrequency` | 1.6 Hz | How quickly the body turns to follow the camera. |
| `JogAcceleration` / `SprintAcceleration` | 1400 / 950 | How hard it is to get moving. |
| `RecoilViewKick`, `RecoilMuzzleFlip`, `RecoilBodyRock` | | How hard each shot kicks. |
| `MouseSensitivity` | 2.5 | Degrees of camera turn per unit of mouse input. |

## Code map

| File | What it does |
|---|---|
| `HeavySpring.h` | The damped spring behind every bit of motion. |
| `HeavyCharacter.*` | Input (Enhanced Input, created in code), movement, body and gun lag, procedural walk cycle, crouch and lean, two-bone arm IK onto the shotgun, recoil and the camera. |
| `HeavyShotgun.*` | Shotgun model, pellets, pump cycle, reload state machine, muzzle flash, tracers, impact effects and ejected shells. |
| `HeavyHUD.*` | Reticle at the gun's real aim point, hit markers, damage numbers, target health bars, shell counter and help. |
| `HeavyTargetDummy.*` | Reactive range targets. |
| `HeavyTestRange.*` | The procedural range, its lighting, and the physics props. |
| `HeavyGameMode.*`, `HeavyPlayerController.*` | Wiring, plus building the range in an empty level. |
| `HeavyPartComponent.*` | A coloured engine primitive. Every visible part is one of these. |
| `HeavyFx.*` | Short-lived effects and the ejected shell. |

## Notes and next steps

- The primitives are coloured through the `Color` parameter of `/Engine/BasicShapes/BasicShapeMaterial`. If
  everything comes out plain grey, your engine's material uses a different parameter name. Change it in
  `HeavyPartComponent.cpp`.
- To swap in real art, assign a skeletal mesh and an Animation Blueprint to the character's `Mesh` and hide the
  `HeavyPartComponent`s. The springs (`BodyYaw`, `AimYaw` / `AimPitch`, the lean, and the weapon offsets) are
  the values to feed into the Animation Blueprint, for example as an aim offset with spine twist and a hand IK
  target.
- The prototype is single-player only. Body rotation and weapon posing aren't replicated.
