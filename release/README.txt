CRIMSON SKIES: HIGH ROAD TO REVENGE - native PC recompilation
=============================================================

An unofficial native Windows port of Crimson Skies: High Road to Revenge (original Xbox, 2003), made by static
recompilation with Xbox XBE Recomp. No emulator. This download contains none of the game's data or code: you need
your own copy of the game, and the game's code is translated on your own PC from your own copy.

What you need
  - Your own Crimson Skies: High Road to Revenge disc for the original Xbox (the USA retail release), dumped to a
    disc image (.iso, a whole dump or an xiso) or extracted to its files. Other releases and betas won't work: the
    program matches that exact game executable.
  - Windows 10 or 11, 64-bit, and a GPU with Direct3D 12. Or a Steam Deck or Linux PC through Proton: see
    "Steam Deck and Linux" below. There is no native Linux build.
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

RecompLauncher
  This release works with RecompLauncher (recomplauncher.com): it picks and checks your disc, shows the settings
  below on the game's page, and keeps the game up to date itself (the in-game update check stays off there).
  Shortcuts and other launchers can name the game too: CrimsonSkies.exe --game "D:\path\to\your game.iso"
  (or the extracted game's folder) uses that copy for the run.

Steam Deck and Linux (Proton)
  Players run it at a steady 60 fps on the Steam Deck. In Desktop Mode:
  1. Download the -win64.zip and extract it with Ark (right-click it, Extract, Extract archive here). Check that
     the new folder has CrimsonSkies.exe next to the assets, compiler and licenses folders. Some SteamOS unzip
     tools drop the folders and leave every file in one place: if so, delete it and extract again with Ark.
  2. Optional: copy your .iso into the assets folder (or pick it in the launcher later).
  3. In Steam: Games, Add a Non-Steam Game to My Library, and pick CrimsonSkies.exe. In its Properties,
     Compatibility, tick "Force the use of a specific Steam Play compatibility tool" and pick Proton
     (Proton Experimental or the newest version).
  Then in Gaming Mode:
  4. Start it. The launcher works with the Deck's controls. Pick your game on the Game row and your settings
     (Display mode Fullscreen, 1280x800, Frame rate 60), then START GAME.
  5. The first start translates the game's code (several minutes on the Deck, once). After that the intro
     movies play and the game starts.
  The program comes with Microsoft's shader compiler (d3dcompiler_47.dll, next to CrimsonSkies.exe), because
  Proton's own one leaves the screen black. You no longer need Protontricks or launch options for it (releases
  before 1.9.1 did). If the game warns that shaders could not be compiled, check that d3dcompiler_47.dll is next
  to CrimsonSkies.exe (extract the zip again), or install d3dcompiler_47 for the game with Protontricks.
  Updates from the launcher work the same as on Windows.

Controls
  Launcher        : arrows or D-pad to move, Left/Right to change, Enter or A to start, Esc or B to quit.
                    The mouse works too.
  In game         : the game's own Xbox controls (RT main guns, LT secondary, Y afterburner). Xbox,
                    PlayStation and Switch controllers work through SDL, with the game's rumble.
                    F2 shows or hides the FPS counter.

Settings (saved to settings.toml next to the program; the launcher writes it)
  Game                        your disc image or game folder (game_path in settings.toml)
  Updates                     the launcher asks GitHub for a newer release when it opens and offers it in a
                              popup: Update now downloads it, checks it and restarts the launcher (settings,
                              saves and your game stay). Left/Right turns the check off (check_updates).
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
                              On a widescreen or ultrawide monitor (up to 32:9) the 3D view fills the
                              screen at its true shape.
  HUD                         Fit: on a widescreen or ultrawide monitor the HUD and menus keep their
                              shape and layout, centered. Stretch: drawn across the width.
  Input device                Auto: keyboard, mouse and controller all work, and the controller only
                              vibrates when you used it last. Or Controller only, or Keyboard and mouse.
  Keyboard player             the player the keyboard and mouse fly as. Player 2 plays split-screen
                              against controller 1; with 3 controllers, use Player 4.
  Key bindings                every keyboard and mouse control: pick one, press the new key
  Mouse moves                 Off (the keys steer), Left stick (steer) or Right stick (roll, speed and
                              the maneuvers); holding Ctrl puts the mouse on the right stick any time
  Mouse sensitivity, pitch    how far the mouse pushes the stick; pitch: on the left stick, up/down too
  Trigger rumble              Xbox One / Series controllers: the right trigger vibrates with the main guns (RT),
                              the left with the secondary weapon (LT), both lightly with the afterburner (Y) and
                              with crashes
  Crash rumble                Stronger makes crashes a real jolt (the game's own is very short)
  Cheat rows (all Off by default):
    unlimited health          the game's own god cheat, on from your next spawn
    unlimited ammo            your secondary weapon never runs out
    unlimited boost           your afterburner never runs out
    all planes                every plane unlocked; not written to your save, and saving still works
  Language                    English, German or French (text and voices), Korean or Chinese (text,
                              English voices)
  Launcher at startup         Off starts the game directly (launcher = true in settings.toml brings it back)
  Recommended: frame rate = your monitor's refresh, FXAA on, 16x filtering, video upscaling on; 150% or 200%
  render resolution if your GPU has room.

Command-line options
  --no-launcher   start the game directly this once
  --launcher      show the launcher even when it is turned off
  --no-update-check  no update check this time

Everything stays in this folder
  assets\        your game, if you put it here instead of picking it in the launcher
  compiler\      the translator, and in compiler\work\ the game's translated code (safe to delete; rebuilt)
  CrimsonSkies.dll  the translated game code, built on first start
  hdd\           the Xbox hard disk: saves (hdd\E\UDATA) and the game's cache (hdd\Z)
  settings.toml  settings
  logs\          a log of each run
  CrashDumps\    crash dumps, if it ever crashes
  Delete the folder and it's all gone. The launcher's Updates row updates in place; to update by hand, copy the
  new files over the old ones and keep assets\, hdd\ and settings.toml.

Keyboard and mouse (change any of these in the launcher's Key bindings page)
  W / S          nose down / nose up          A / D          turn left / right
  Q / E          roll left / right            R / F          speed up / slow down
  Left mouse     main guns                    Right mouse    secondary weapon
  Shift          boost                        Space          brake
  V, middle mouse  R3 (right stick click)     1 / 2 / Z / X / C  A / X / black / white / L3
  Mouse 4        Immelmann (both sticks back + R3)
  Aerial maneuvers, one key each (they click R3 and make the stick moves for you):
    3            Split S                      4              Snap Turn
    5            Power Climb                  6              Back Stall Loop
    7            High-Speed Yo-Yo
    T / Y        Snap Roll left / right       G / H          Double Snap left / right
    U / I        Barrel Roll clockwise / counter-clockwise
    J / K        Double Barrel clockwise / counter-clockwise
  Ctrl (hold)    the mouse moves the right stick
  Arrows         D-pad                        Esc            pause (START)
  Enter / Backspace  menu accept / back       Tab            BACK

Known issues
  - "Demos" on the main menu starts Microsoft's demo launcher (trailers and a Voodoo Vince demo), which is not
    translated: the game stops on its error screen. Restart the program.
  - The game was made for 30 fps. Higher frame rates were tested through the campaign without problems, but if
    something feels too fast or slow, set the frame rate to 30 and report it.

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
  folder. The Steam Deck steps follow StealthRook's and ROCraiderny's notes in issue #2.
  Crimson Skies is a trademark of Microsoft; this project is not affiliated with or endorsed by Microsoft.
