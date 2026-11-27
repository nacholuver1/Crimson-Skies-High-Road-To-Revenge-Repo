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
  Longer battery life: Render resolution 75%, Upscaling FSR 1 and Sharpening 50% look close to native at 1280x800.
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
                    F12 saves a screenshot (PNG) in the screenshots folder next to the program.

Settings (saved to settings.toml next to the program; the launcher writes it)
  Game                        your disc image or game folder (game_path in settings.toml)
  Updates                     the launcher asks GitHub for a newer release when it opens and offers it in a
                              popup: Update now downloads it, checks it and restarts the launcher (settings,
                              saves and your game stay). Left/Right turns the check off (check_updates).
  Display mode, resolution    borderless, fullscreen or a window
  Aspect ratio                Fill the screen (default) uses your screen's shape; Original 4:3 shows the
                              game as on a 4:3 TV, with black bars at the sides
  Render resolution           of your screen's height; above 100% renders bigger and averages down (supersampling)
  Anti-aliasing               FXAA on the final image
  Upscaling                   FSR 1 (default): AMD's upscaler for a picture stretched to your screen
                              (render resolution below 100%, or widened to a wide screen); Bilinear is a
                              plain stretch
  Sharpening                  FSR 1 sharpening of the final picture, Off (default) to 100%
  Texture filtering           anisotropic filtering, up to 16x
  Texture pack                On (default): images in textures\replacement\ are drawn instead of the
                              game's own, at any resolution (see "Texture packs" below). "On, save the
                              game's textures" also writes each texture the game uses to textures\dumps\.
  Video upscaling             cutscenes and stills: bicubic upscaling and deinterlacing instead of a plain stretch
  AI cutscenes                optional: upscales the 60 cutscenes with AI on your PC, once (see "AI cutscenes"
                              below). When they are made, On (default) or Off.
  AI textures                 optional: upscales the game's textures with AI on your PC, once (see "AI textures"
                              below). Off until you make them; then On or Off.
  Frame rate                 30 (the original) to 240. Set it to your monitor's refresh rate. The game's speed
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
    all planes                every plane unlocked and upgradable; the unlocks aren't written to your
                              save (upgrades you buy are), and saving still works
    unlimited money           999,999 money and buying costs nothing; your save keeps its real money,
                              and money earned while it's on isn't kept
    unlimited tokens          999 upgrade tokens and upgrades cost none; same for your save
  Language                    English, German or French (text and voices), Korean or Chinese (text,
                              English voices)
  Launcher at startup         Off starts the game directly (launcher = true in settings.toml brings it back)
  Reset settings              Enter twice: every setting back to its default; your game, key bindings and saves stay
  Recommended: frame rate = your monitor's refresh, FXAA on, 16x filtering, video upscaling on; 150% or 200%
  render resolution if your GPU has room. On a slow GPU: 75% with sharpening at 50%.

Texture packs (HD textures, a sharper HUD and menus)
  1. Set Texture pack to "On, save the game's textures" and play the parts you want to change. Each
     texture the game uses is saved once to textures\dumps\, as a PNG named like
     256x256_0123456789abcdef_DXT1.png. The 16-digit part is that texture's name.
  2. Edit or upscale the images (any size: 2x or 4x is typical; keep the same picture layout, as many
     images hold several HUD pieces side by side). PNG or DDS both work.
  3. Put the edited files in textures\replacement\ (subfolders are fine). Only the 16-digit part of the
     name has to stay the same.
  4. Set Texture pack back to On; the game picks up new or changed files while it runs.
  To use someone else's pack, unzip it into textures\replacement\. Packs hold the game's own images,
  edited: share them only as far as the game's rights allow.

AI cutscenes (sharper movies)
  The game's cutscenes are 640x480. On the AI cutscenes row, Enter makes a 1280x960 copy of each one with
  Real-ESRGAN, an AI upscaler, on your own graphics card:
  1. It downloads Real-ESRGAN (45 MB) and FFmpeg (119 MB) from GitHub into the tools folder; each file is checked
     against its known SHA-256 first.
  2. It upscales the 60 cutscenes into movies\ai\ (about 0.9 GB). On a recent graphics card that takes about an
     hour; you need about 8 GB of free disk space while it runs, and a graphics driver with Vulkan.
  The launcher shows how far it is. Enter again, or starting the game, stops it; the cutscenes made so far are
  kept, and the next time it carries on. The game plays a made cutscene with its original sound and timing, and
  the original picture for any it doesn't have. Delete movies\ai\ and tools\ to remove them. In RecompLauncher,
  tick "AI-upscaled cutscenes" when you set the game up (or under Set Up Again).
  Command line: CrimsonSkies.exe --ai-movies [--game <your game>] does the same without a window.

AI textures (sharper planes, buildings and signs)
  On the AI textures row, Enter makes a 2x copy of the game's textures with Real-ESRGAN on your own graphics card,
  read straight from your game (no playing needed first):
  1. It downloads Real-ESRGAN (45 MB) into the tools folder once, checked against its known SHA-256.
  2. It upscales about 650 textures into textures\ai\ (about 150 MB): a few minutes on a recent graphics card;
     it needs about 2 GB of free disk space while it runs, and a graphics driver with Vulkan.
  Menus, the HUD and their lettering stay the game's own (AI upscaling bends letters), and so do textures that
  hold data rather than pictures. When it is done the row says On; set it to Off to see the originals. Your own
  texture pack in textures\replacement\ wins over the AI one where both have a texture. Delete textures\ai\ to
  remove them. In RecompLauncher, tick "AI-upscaled textures" when you set the game up (or under Set Up Again).
  Command line: CrimsonSkies.exe --ai-textures [--game <your game>].

Command-line options
  --no-launcher   start the game directly this once
  --launcher      show the launcher even when it is turned off
  --no-update-check  no update check this time
  --ai-movies     make the AI cutscenes (see above), then exit
  --ai-textures   make the AI textures (see above), then exit

Everything stays in this folder
  assets\        your game, if you put it here instead of picking it in the launcher
  compiler\      the translator, and in compiler\work\ the game's translated code (safe to delete; rebuilt)
  CrimsonSkies.dll  the translated game code, built on first start
  textures\      texture packs (replacement\), saved game textures (dumps\) and the AI textures (ai\), when you use them
  movies\ai\     the AI cutscenes, when you make them; tools\ the upscaler and FFmpeg they were made with
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
    translated: the game restarts at its intro and says so in a note.
  - The game was made for 30 fps. Higher frame rates were tested through the campaign without problems, but if
    something feels too fast or slow, set the frame rate to 30 and report it.
  - The game restarts itself between the menus and each level (the Xbox rebooted there too), so the NVIDIA app
    shows its "Press Alt+Z" overlay notice each time, at the Pandora hub for example. Turn the notice off in the
    NVIDIA app's settings (notifications for the in-game overlay).

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
  AI cutscenes and textures are made with Real-ESRGAN by Xintao Wang et al. (https://github.com/xinntao/Real-ESRGAN, BSD-3-Clause
  models; its ncnn Vulkan program is MIT, built on Tencent's ncnn, BSD-3-Clause) and FFmpeg (https://ffmpeg.org,
  Gyan Doshi's Windows build), downloaded on your request and not part of this release.
  Crimson Skies is a trademark of Microsoft; this project is not affiliated with or endorsed by Microsoft.
