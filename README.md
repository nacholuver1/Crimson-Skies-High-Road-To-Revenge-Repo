# Crimson Skies: High Road to Revenge — native PC recompilation

An unofficial native Windows port of **Crimson Skies: High Road to Revenge** (original Xbox, 2003), made by
*static recompilation* with [Xbox XBE Recomp](https://github.com/TGP482/XboxXBERecomp): the game's x86 code is
translated ahead of time into a normal x64 program, with the Xbox kernel, Direct3D 8, DirectSound and input
reimplemented on Windows underneath. No emulator runs at play time.

> **AI-assisted project.** The code, build scripts, debugging and documentation in this repository are written by
> [Claude](https://claude.com/claude-code) (Anthropic's AI, via Claude Code), directed and play-tested by
> [nacholuver1](https://github.com/nacholuver1). Every commit carries a `Co-Authored-By: Claude` trailer. Treat it
> like any community project: read the code, and report problems in Issues.

**This repository contains no game data.** No disc image, no game files, and none of the code translated from the
game's executable: a release translates the game's code on the player's own PC, from their own copy.

**Status: playable.** The campaign runs with sound and saves, at 30 to 240 fps, in true widescreen (16:9 up
to 32:9 ultrawide). Known issues: the main menu's **Demos** (Microsoft's demo launcher, `cdx.xbe`) is not translated and
stops the game.

## Playing

1. Download the latest `-win64.zip` from [Releases](../../releases) and unzip it anywhere. It is portable.
2. Run `CrimsonSkies.exe`. The launcher opens: on its **Game** row, pick your own **Crimson Skies: High Road to
   Revenge** disc image (original Xbox, USA retail; `.iso`, a whole dump or an xiso) or the extracted game's
   `default.xbe`, wherever it is. (Or put the `.iso` or the extracted files in the `assets` folder.)
3. Pick your settings and press **START GAME**.
4. The first start translates the game's code for your PC behind a progress window (a few minutes, about 1
   on an 8-core CPU; once). Later starts take a few seconds.

Needs Windows 10 or 11, 64-bit, and a Direct3D 12 GPU. Linux: there is no native build (the SDK targets Windows);
Proton or Wine may work but is untested, and reports are welcome. The player guide is [release/README.txt](release/README.txt).

## Settings

The launcher sets display mode, resolution, render resolution (up to 300%, supersampling above 100%), FXAA,
texture filtering, video upscaling, vsync, frame rate, FPS counter, trigger and crash rumble, and audio output. Every choice is saved in
`settings.toml` beside the executable:

| Key | Values |
|---|---|
| `game_fps` | 30 (the original) to 240; 60 by default. The renderer's `fps_cap` is raised to match. |
| `camera_sway` | the camera's turbulence sway in percent, 0 (off) to 100 (the original, default) |
| `fov` | field of view in percent of the original, 80 to 130 (100 by default). On a widescreen output the 3D view fills the monitor's shape, 16:9 up to 32:9. |
| `fit_hud` | on a widescreen output the HUD and menus keep their 4:3 shape and layout, centered (on by default); off stretches them to the width |
| `show_fps` | FPS counter in the corner; **F2** toggles it in game |
| `fxaa` | FXAA on the final image |
| `upscale_videos` | cutscenes and stills: bicubic upscaling and deinterlacing (on by default) |
| `render_percent` | of the output height; above 100 renders bigger and averages down |
| `anisotropic_filtering` | 0 (as the game asks), 2, 4, 8, 16 |
| `language` | the console language the game sees: 1 English, 3 German, 4 French (text and voices), 7 Korean, 8 Chinese traditional (text) |
| `display_mode`, `width`, `height`, `vsync`, `audio_output` | the SDK's display and audio settings |
| `game_path` | the game: a disc image (`.iso`) or the extracted game's folder; empty uses `assets` |
| `input_device` | 0 auto (keyboard, mouse and controller; the controller only vibrates when used last), 1 controller only, 2 keyboard and mouse (no controller rumble) |
| `[controls]` | the keyboard and mouse bindings, set on the launcher's **Key bindings** page |
| `mouse_mode` | what moving the mouse does: 0 nothing (default: the keys steer, the mouse buttons fire), 1 the left stick (steer), 2 the right stick (roll, speed, maneuvers) |
| `mouse_sensitivity`, `mouse_pitch` | how far the mouse pushes the stick; on the left stick, whether it pitches too |
| `impulse_triggers` | Xbox One / Series trigger motors follow the guns, afterburner and crashes (on by default) |
| `crash_rumble` | crashes held and faded into a strong jolt; false keeps the game's very short one (on by default) |
| `launcher` | false starts the game directly |

Command line: `--no-launcher` skips the launcher once, `--launcher` shows it even when it is turned off.

## Building from source

Requirements (Windows x64): Visual Studio 2022 with **Desktop development with C++**, **C++ Clang tools for
Windows** and **C++ CMake tools for Windows** (`build.bat` offers to add the last two), Git, Python 3 for the
tools, and your own disc.

```bat
git clone --recursive https://github.com/nacholuver1/Crimson-Skies-High-Road-To-Revenge-Repo.git
cd Crimson-Skies-High-Road-To-Revenge-Repo
python tools\extract_xiso.py "path\to\your.iso" assets
build.bat
```

`build.bat` translates the game (about 15 minutes on 8 cores) and writes `out\build\release\CrimsonSkies.exe`.
`powershell -ExecutionPolicy Bypass -File tools\make_release.ps1 -Version 1.0.0` packs the player download into
`dist\`, without any game data or translated code.

`sdk/` is a submodule on [nacholuver1/XboxXBERecomp](https://github.com/nacholuver1/XboxXBERecomp), branch
`crimson-skies`: upstream plus the fixes this game needed (see [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)).

## Target build

The retail USA `default.xbe`: title `0x4D530021`, built 2003-09-21, SHA-256
`f923b28f390d0c036cdc7cf3a2d9becd7d9b2e61314a6c7b3278116bc25b98ba`. Other releases and the 2003-12-09 beta have other
addresses and won't run with this config.

## How it was made

[docs/PLAN.md](docs/PLAN.md) is the plan (the recomp SOP translated from the Xbox 360 to the original Xbox), and
[docs/RESEARCH.md](docs/RESEARCH.md) records every bug found on the way, its cause, its fix and the outside
sources. Tools in `tools/`: `extract_xiso.py` (disc images), `xbe_info.py` (executable headers),
`guest_locks.ps1` (critical-section owners of a frozen game).

## Credits

- [Xbox XBE Recomp](https://github.com/TGP482/XboxXBERecomp) by TGP482, the recompiler and runtime, with fixes from
  its open pull requests by maxscipio (XDK 5659 signatures, read from this game), paulohenriquesn and
  yacineolivier-maker.
- crazyriddler's frame rate patches for this game in xbased, which the frame rate unlock follows.
- xemu issue [#2902](https://github.com/xemu-project/xemu/issues/2902) and abaire's fix, which diagnosed the menu
  text; the earlier recomps of this game by saironbusatto and sp00nznet, whose notes were checked against this one.
- [darkuni's recomp menus](https://github.com/darkuni/outrun-arcade-recomp) (MIT), by way of the Blitz recomps,
  for the launcher's design.
- Built by Claude (Anthropic) with Claude Code, directed and play-tested by nacholuver1.

Full notices: [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

## Legal

This project's own code is MIT licensed ([LICENSE](LICENSE)). Crimson Skies and its code, assets and trademarks
belong to Microsoft; this project is not affiliated with or endorsed by Microsoft. Use it only with a copy of the
game you own.
