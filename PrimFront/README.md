# PrimFront

A small third person shooter prototype in the style of Battlefront II, written in C++ for Unreal Engine 5.8.
It's deliberately plain to look at: there are no textures or imported assets. The character, the shotgun, the
targets and the whole shooting range are made from the engine's basic shapes (cube, sphere, cylinder and cone)
with flat colours.

## What's in it

- **Player character** with a head, torso, backpack, arms (shoulder, elbow, hand) and legs (hip, knee, foot), all
  made of primitives and animated in code:
  - a walk, run and sprint cycle that works for strafing and moving backwards, with feet placed by IK
  - two-bone IK arms that keep both hands on the shotgun (right hand on the grip, left hand on the pump)
  - the torso and head follow where you're aiming, and the torso turns side-on while the gun is up
- **Pump shotgun**: 9 pellets per shot, damage that drops off with distance, a pump action you can see, muzzle flash,
  tracers, impact marks and an 8-shell tube.
- **Hip fire and aim down sights (ADS)**: aiming pulls the camera in, lowers the field of view, tightens the spread,
  slows you down and lowers mouse sensitivity.
- **Crouch** (toggle). The camera lowers smoothly and spread is tighter while crouched.
- **Shoulder swap**: the camera moves smoothly to the other shoulder.
- **Holster**: the gun goes up over the right shoulder and down onto the back. Holstered, the character turns
  towards the direction you're moving and swings its arms. Pressing fire or aim draws the gun again.
- **Melee**: with the gun out it's a rifle-butt swing. Holstered, it's a punch, alternating left and right.
  Melee damages targets and knocks crates around.
- **Sprint** (hold). The gun is carried across the chest, the camera's field of view widens, and sprinting
  stops a reload. Shooting ends a sprint, and the shot fires as soon as the gun comes up.
- **Reload**: shells go in one at a time, and the left hand fetches each shell from a belt pouch. Fire during a reload
  to stop it after the current shell. When the gun was empty, the pump is racked at the end. It also reloads
  automatically when empty.
- **Shooting range**: five lanes with benches. Pop-up dummies stand at 10, 20 and 35 m and there are moving targets
  at 27 and 45 m. Dummies take double damage to the head, show a damage number, fall over and get back up after
  3 seconds. To one side is a melee yard with dummies and stacks of physics crates; on the other is a small
  cover course. Distance signs mark the range.
- **HUD** (drawn on the canvas, so there are no widget assets): a crosshair circle that shows the real pellet
  spread, hit markers (white for a hit, yellow for a headshot, red when the target goes down), shells left, stance and
  aim state, range stats and a list of controls.

## Controls

| Key | Action |
| --- | --- |
| WASD | Move |
| Mouse | Look |
| Left mouse | Fire. When holstered, draws the gun |
| Right mouse (hold) | Aim down sights |
| Left Shift (hold) | Sprint |
| C / Left Ctrl | Crouch (toggle) |
| Space | Jump (or stand up when crouched) |
| R | Reload |
| Q | Swap shoulder |
| H / 1 | Holster or draw |
| V / F / mouse thumb button | Melee |
| T | Reset targets, ammo and stats |
| F1 | Show or hide the controls |

## Setup

1. You need Unreal Engine 5.8 and a C++ toolchain: Visual Studio 2022 with the "Game development with C++"
   workload on Windows, or Xcode on macOS.
2. Right-click `PrimFront.uproject` and choose **Generate Visual Studio project files**. If your engine
   registered under a different version, choose **Switch Unreal Engine version** first.
3. Open `PrimFront.uproject`. When it asks to rebuild the `PrimFront` module, click **Yes**. You can also
   build the `PrimFrontEditor` target from your IDE instead.
4. Press **Play**.

There's no map to open. The project starts on the engine's empty `/Engine/Maps/Entry` map, and the
`PFGameMode` game mode builds the range, lighting, targets and player when play starts.

To use your own level instead, set its GameMode Override to `PFGameMode`. If the level has no
`PFRangeBuilder`, the game mode adds one. To stop that, turn off `bAutoBuildRange`, or place a
`PFRangeBuilder` yourself and turn off `bBuildFloor` or `bSpawnLighting` if your level already has
those. If the level has a PlayerStart, the player spawns there. Otherwise the player spawns behind the firing line.

## Code layout

All the code is in `Source/PrimFront/`.

| File | What it does |
| --- | --- |
| `PFShapes` | Helpers that create coloured basic-shape parts and joints at runtime |
| `PFCharacter.h/.cpp` | The character, its key bindings (created in code, so there are no input assets), state, movement and camera |
| `PFCharacterCombat.cpp` | Firing, the shell-by-shell reload, melee and holstering |
| `PFCharacterBody.cpp` | The primitive skeleton, walk cycle, leg IK and the two-bone IK solver |
| `PFCharacterArms.cpp` | Where the gun is held for each action (hip, ADS, reload, sprint, melee, holster) and the arm IK onto it |
| `PFShotgun` | The shotgun model, pellet traces, damage falloff, tracers, impact marks and ammo |
| `PFTarget` | The pop-up dummy: health, headshots, falling over, damage numbers and optional side-to-side movement |
| `PFPhysicsProp` | A crate that's simulated by physics |
| `PFRangeBuilder` | Builds the whole range and adds lighting if the level has none |
| `PFGameMode` / `PFHUD` | The game mode and the canvas HUD |

Most of the feel can be changed from the `UPROPERTY` values on `APFCharacter`, such as movement speeds, camera
distances and field of view, spread, recoil, reload timings and melee, and on `APFShotgun`, such as pellet count,
damage, falloff, fire interval and tube size. To change them in the editor, make a Blueprint subclass of
`PFCharacter` and point the game mode's Default Pawn Class at it. Keep the game mode class as `PFGameMode`.

## Notes

- Everything is built from `/Engine/BasicShapes/*` and coloured through the `Color` parameter of
  `BasicShapeMaterial`, so the project has nothing in its Content folder.
- Mouse look works the same way as in the engine's third person template: Mouse XY with a Negate modifier on Y,
  `AddControllerYaw/PitchInput`, and `bEnableLegacyInputScales=True` in `Config/DefaultInput.ini`. If the
  vertical look is inverted on your setup, remove the Y negate in `APFCharacter::CreateInput`.
- This code hasn't been compiled against a 5.8 engine yet. It sticks to long-standing engine APIs, but the
  build log will point at anything that changed.
