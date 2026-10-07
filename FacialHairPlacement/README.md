# Facial Hair Placement (RimWorld 1.5 / 1.6)

Move beards and other facial hair on pawns with X/Y sliders, from **Options → Mod options → Facial Hair Placement**.

## Install

1. Subscribe to / install [Harmony](https://steamcommunity.com/sharedfiles/filedetails/?id=2009463077). This mod requires it.
2. Copy this whole `FacialHairPlacement` folder into your RimWorld `Mods` folder
   (e.g. `C:\Program Files (x86)\Steam\steamapps\common\RimWorld\Mods\FacialHairPlacement`).
3. Enable **Harmony**, then **Facial Hair Placement** below it, and restart.

The compiled DLLs are already included in `1.5/Assemblies` and `1.6/Assemblies`, so you don't need to build anything.

## Using it

| Section | Sliders | Notes |
|---|---|---|
| Front (facing south) | Horizontal (X), Vertical (Y) | |
| Side (facing east / west) | Forward (X), Vertical (Y) | Positive X moves toward the face. West mirrors east automatically. |
| Back (facing north) | Horizontal (X), Vertical (Y) | Most beards are hidden behind the head from this angle. |

- Positive X = right, positive Y = up. Units are map tiles (range ±0.3).
- `-` / `+` nudge by 0.005; hold **Shift** for 0.001.
- **Reset** clears one section; **Reset all** clears everything. The checkbox turns the offsets off without losing your values.
- With a save loaded, the right side shows a live preview of a pawn from all four directions (use `<` `>` to switch pawns and the Zoom slider to frame the head). Pawns on the map update while you drag, and every portrait is refreshed when you close the window.

Settings are stored in RimWorld's normal mod config folder, so they apply to every save. The mod is safe to add or remove mid-save.

## How it works

RimWorld 1.5+ draws pawns with a render tree. The beard node's position relative to the head comes from
`PawnRenderNodeWorker_Beard.OffsetFor`. A Harmony postfix adds your offset for the pawn's current facing
to that result, so the change shows up everywhere the pawn is drawn: on the map, in portraits and in the colonist bar.

Mods that draw facial hair through their own render nodes instead of the vanilla beard worker won't be affected.

## Building from source

Requires the .NET SDK (6 or newer). RimWorld and Harmony reference assemblies come from NuGet, so you don't need the game installed to build.

```sh
cd Source
./build.sh                     # builds both 1.5 and 1.6
# or one version at a time:
dotnet build FacialHairPlacement/FacialHairPlacement.csproj -c Release -p:RimWorldVersion=1.6
```

Each version has to be built separately: a few RimWorld UI methods changed signature between 1.5 and 1.6.

## Layout

```
About/About.xml                 mod metadata (name, packageId, Harmony dependency)
LoadFolders.xml                 loads 1.5/ or 1.6/ for the running game version
1.5/Assemblies, 1.6/Assemblies  compiled DLLs
Languages/English/Keyed         all settings-window text
Source/                         C# source and build script
```
