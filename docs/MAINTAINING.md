# Maintaining this port

For whoever picks this project up later, the author included. Since 1.16.0 the port is feature complete; what is
left is bug fixes. This page says where things are and how a fix gets to players. The README has the player side,
[RESEARCH.md](RESEARCH.md) the findings behind every fix, [PLAN.md](PLAN.md) the original plan.

## The two repositories

- **This repository**: the game side. `src/common/` holds everything specific to Crimson Skies (launcher, frame
  rate, widescreen, keyboard and mouse, cheats, rumble, updater), `config/game.toml` the recompiler's settings and
  the game's hook addresses, `recomplauncher.json` the RecompLauncher manifest, `tools/` the release script.
- **`sdk/`**: a submodule on [nacholuver1/XboxXBERecomp](https://github.com/nacholuver1/XboxXBERecomp), branch
  `crimson-skies`, a fork of [TGP482/XboxXBERecomp](https://github.com/TGP482/XboxXBERecomp) (MIT). It holds the
  recompiler and the runtime: Xbox kernel, Direct3D 8 on Direct3D 12 (`runtime/gpu/d3d12.cpp`: presentation, FSR 1,
  screenshots), DirectSound on XAudio2, input through SDL3, settings (`runtime/app/`). A fix there is committed and
  pushed in the fork first, then the submodule pin moves in this repository. Move the pin one change at a time.

Neither repository may hold game data or code translated from the game: the release translates the game on the
player's PC from their own disc. Never commit anything from `assets/`, `generated/` or `out/`.

## Build

Requirements are in the README ("Building from source"). `build.bat` does everything the first time. After that:

- `cmake --build out\build\release` rebuilds what changed. A runtime-only change rebuilds the exe; the game's code
  (`CrimsonSkies.dll`, translated at first start) counts as stale when it is older than the exe, so touch
  `out\build\release\CrimsonSkies.dll` and `compiler\work\CrimsonSkies\built.txt` there, or the exe translates the
  game again at its next start (several minutes).
- Shaders in `d3d12.cpp` are HLSL strings compiled at runtime with `d3dcompiler_47.dll`. Compile a changed one with
  `fxc /T ps_5_0 /E <entry>` first: a failed compile only shows in the log, at run time.
- After every build check the exe's time and its version (CMakeLists.txt `project(VERSION)`): a failed link leaves
  the old exe in place.

## Testing

- Settings for test runs live in `out\build\release\settings.toml` and point `game_path` at the retail disc image.
  `CrimsonSkies.exe --no-launcher` skips the launcher.
- The game restarts itself between menus and levels (the Xbox rebooted there): a new process with a new log in
  `logs\`. Check the process list before deciding a run ended.
- Logs: `logs\crimsonskies.log` is the newest run. Crash dumps go to `CrashDumps\`. Warnings at Report level ship in
  release builds; LOGI does not.
- F3 dumps every guest thread's stack to the log; with `XBE_CAPTURE_DIR` set it also captures the next frame (every
  draw, target and texture) for graphics bugs.
- Steam Deck: players run the Windows zip under Proton. The bundled `d3dcompiler_47.dll` is required there.

## Releasing a fix

1. Bump `project(VERSION)` in CMakeLists.txt (patch number for fixes). The updater compares it with GitHub's latest
   release.
2. Commit the fork (if `sdk/` changed), push it, commit this repository with the new pin.
3. `powershell -ExecutionPolicy Bypass -File tools\make_release.ps1 -NoBuild` (after a build) writes
   `dist\CrimsonSkiesRecomp-<version>-win64.zip`. It refuses game files. Keep the asset name ending in
   `-win64.zip`: the built-in updater looks for exactly that, and checks GitHub's SHA-256 digest of it.
4. Unzip it into an empty folder and do a full first start as a player would (pick the disc, let it translate,
   reach the menu).
5. Check the exe's SHA-256 on VirusTotal before publishing.
6. Publish a GitHub release (tag `v<version>`) with the zip and notes: what changed, known issues, hardware tested.
   Players get it through the launcher's update popup, or RecompLauncher.

## Taking bug reports

The issue form asks for the log. Most reports so far were one of: a setting (the in-game Vibration option, an old
save), the GPU overlay notice at each restart, a controller that connects late, or a frame-rate-tied effect. The
symptom-to-cause table in RESEARCH.md ("First boot to gameplay" and the issue sections) lists each fixed one. Ask for
the newest log and the settings file; never ask for game files.

## Known limits (by design)

- The main menu's **Demos** starts a separate program on the disc (`cdx.xbe`) that is not translated; the runtime
  restarts the game and shows a note instead.
- No save states: the game's threads run as native threads, which cannot be captured and restored.
- No native Linux build: the runtime is Direct3D 12 and Win32. Proton runs the Windows build.

## Licenses

This repository and the fork are MIT. THIRD_PARTY_NOTICES.md lists everything bundled (SDL3 zlib, Clang/LLD Apache
2.0 with LLVM exceptions, AMD FSR 1 MIT, Dear ImGui, toml++, Zydis MIT, Microsoft's redistributables). AI
assistance is stated in the README; keep doing so for new work.
