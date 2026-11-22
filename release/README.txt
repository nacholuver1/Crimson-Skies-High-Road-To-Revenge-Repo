CRIMSON SKIES: HIGH ROAD TO REVENGE - native PC recompilation
=============================================================

An unofficial native Windows port of Crimson Skies: High Road to Revenge (original Xbox, 2003), made by static
recompilation with Xbox XBE Recomp. No emulator. This download contains none of the game's data or code: you need
your own copy of the game, and the game's code is translated on your own PC from your own copy.

What you need
  - Your own Crimson Skies: High Road to Revenge disc for the original Xbox (the USA retail release), dumped to a
    disc image (.iso, a whole dump or an xiso) or extracted to its files. Other releases and betas won't work: the
    program matches that exact game executable.
  - Windows 10 or 11, 64-bit, and a GPU with Direct3D 12. There is no native Linux build; Proton or Wine may
    work but is untested.
  - About 200 MB of free space next to the program for the game's translated code (plus about 4.5 GB if you
    extract the game files instead of using the .iso).

First run
  1. Unzip this folder anywhere you like. It is fully portable.
  2. Run CrimsonSkies.exe. The launcher opens. On its "Game" row, pick your disc image (.iso) or the extracted
     game's default.xbe, wherever it is. (Or put the .iso or the extracted files in the "assets" folder: the
     launcher finds them there by itself.)
  3. Pick your settings, then START GAME.
  4. The very first start translates the game's code for your PC, behind a progress window. It takes a few
     minutes (about 1 on an 8-core CPU), and only happens once (if you close it, the next start carries on).
     After that the game starts in a few seconds.

Controls
  Launcher        : arrows or D-pad to move, Left/Right to change, Enter or A to start, Esc or B to quit.
                    The mouse works too.
  In game         : the game's own Xbox controls (RT main guns, LT secondary, Y afterburner). Xbox,
                    PlayStation and Switch controllers work through SDL, with the game's rumble.
                    F2 shows or hides the FPS counter.

Settings (saved to settings.toml next to the program; the launcher writes it)
  Game                        your disc image or game folder (game_path in settings.toml)
  Display mode, resolution    borderless, fullscreen or a window
  Render resolution           of your screen's height; above 100% renders bigger and averages down (supersampling)
  Anti-aliasing               FXAA on the final image
  Texture filtering           anisotropic filtering, up to 16x
  Video upscaling             cutscenes and stills: bicubic upscaling and deinterlacing instead of a plain stretch
  Frame rate                  30 (the original) to 240. Set it to your monitor's refresh rate. The game's speed
                              stays right at any of them.
  Camera sway                 the camera's turbulence sway: lower it, or turn it off, if the motion makes you
                              feel sick (100% is the original)
  Field of view               80% to 130% of the original view; wider can help with motion sickness.
                              On a widescreen monitor the 3D view is true 16:9 (the HUD stays stretched).
  Trigger rumble              Xbox One / Series controllers: the right trigger vibrates with the main guns (RT),
                              the left with the secondary weapon (LT), both lightly with the afterburner (Y) and
                              with crashes
  Crash rumble                Stronger makes crashes a real jolt (the game's own is very short)
  Launcher at startup         Off starts the game directly (launcher = true in settings.toml brings it back)
  Recommended: frame rate = your monitor's refresh, FXAA on, 16x filtering, video upscaling on; 150% or 200%
  render resolution if your GPU has room.

Command-line options
  --no-launcher   start the game directly this once
  --launcher      show the launcher even when it is turned off

Everything stays in this folder
  assets\        your game, if you put it here instead of picking it in the launcher
  compiler\      the translator, and in compiler\work\ the game's translated code (safe to delete; rebuilt)
  CrimsonSkies.dll  the translated game code, built on first start
  hdd\           the Xbox hard disk: saves (hdd\E\UDATA) and the game's cache (hdd\Z)
  settings.toml  settings
  logs\          a log of each run
  CrashDumps\    crash dumps, if it ever crashes
  Delete the folder and it's all gone. When updating, copy the new files over the old ones; keep assets\, hdd\
  and settings.toml.

Known issues
  - "Demos" on the main menu starts Microsoft's demo launcher (trailers and a Voodoo Vince demo), which is not
    translated: the game stops on its error screen. Restart the program.
  - The game was made for 30 fps. Higher frame rates were tested through the campaign without problems, but if
    something feels too fast or slow, set the frame rate to 30 and report it.
  - The HUD and menus are drawn for a 4:3 screen and look stretched on a widescreen monitor (the 3D view is
    true 16:9).

Reporting a problem
  Open an issue on the project's GitHub page and attach the newest file from logs\ (and from CrashDumps\ if it
  crashed). Never upload game files.

Credits and licenses
  AI-assisted: the code, build scripts, debugging and documentation were written by Claude (Anthropic's AI, via
  Claude Code), directed and play-tested by nacholuver1.
  Built on Xbox XBE Recomp by TGP482 (https://github.com/TGP482/XboxXBERecomp), with fixes from its open pull
  requests by maxscipio (XDK 5659 signatures), paulohenriquesn and yacineolivier-maker. The frame rate unlock
  follows crazyriddler's patches for this game in xbased; the menu text fix follows the diagnosis in xemu issue
  #2902 and its fix by abaire; the launcher's design is adapted from darkuni's MIT-licensed recomp menus. See the licenses
  folder. Crimson Skies is a trademark of Microsoft; this project is not affiliated with or endorsed by Microsoft.
