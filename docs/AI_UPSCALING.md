# AI upscaling: cutscenes (done) and textures (planned)

Opt-in player feature: the player's own PC upscales the game's Bink cutscenes with an AI model, and the game
draws the results. Nothing upscaled is ever shipped or committed (it is derived game data); the release ships
only our code, and the tools are downloaded when the player asks for them.

## Cutscenes (2026-11-26)

### The movies
60 Bink 1 files (`bink1\` 22, `bink2\` 38), all 640x480 at 29.97 fps, 67,953 frames, 37.8 min, 1.8 GB.
FFmpeg's `idet` finds interlacing combs in 7: AttractMode, FullEndCredits, the four MAP_Transition movies and
SS_Farewell (`kInterlaced` in `src/common/ai_movies.cpp`).

### Making them (`src/common/ai_movies.cpp`)
`CrimsonSkies.exe --ai-movies [--game=<iso or folder>]`, no window, `progress <0-1> <text>` lines on stdout:
1. Downloads into `tools\` (each refused unless its SHA-256 matches; `updater::Download`/`Unpack`):
   - `realesrgan-ncnn-vulkan-20220424-windows.zip`, Real-ESRGAN release v0.2.5.0, 45.5 MB, sha256 `abc02804…`
     (the program is MIT, the models BSD-3-Clause, ncnn BSD-3-Clause);
   - `ffmpeg-9.1-essentials_build.zip` from GyanD/codexffmpeg 9.1, 119 MB, sha256 `b9bbbb3a…` (GitHub's digest),
     only `bin/ffmpeg.exe` unpacked. Run as a separate program, never linked.
2. Per movie: copies the .bik off the disc image (SDK `kernel::Disc`), FFmpeg decodes every frame to PNG
   (`-fps_mode passthrough`, `idet,bwdif` for the combed ones), Real-ESRGAN `realesr-animevideov3` x2 upscales
   300 frames at a time, FFmpeg `h264_mf` (Windows' own H.264 encoder) encodes each chunk, and the parts are
   joined (`concat`, stream copy) into `movies\ai\<name>.mp4`, renamed into place only when whole.
3. `movies\ai\recipe.txt` names the recipe; another recipe makes them all again. Made movies are skipped, so a
   stopped run carries on. Needs about 8 GB free while running.

Model choice: on the interlaced map transition, `realesrgan-x4plus` sharpened the combs into jagged letters and
invented cracks, and ran 14x slower (1.5 fps); `realesr-animevideov3` x2 removed them and runs at about 25 fps on
an RTX 4070 SUPER (about 50 minutes for all 60). HEVC through Media Foundation needs Microsoft's paid HEVC
extension, so H.264.

Front ends: the launcher's **AI cutscenes** row (Make, progress, Enter stops, then On/Off) runs the same step as a
child process; RecompLauncher's `prepare` step runs `--prepare`, and its unticked-by-default option
`ai_cutscenes` adds `--ai-movies`.

### Playing them (SDK `runtime/gpu/movies.cpp`)
- The signature scan finds no Bink functions in this game (`BinkDoFrame = 0`), so `BinkOpen` (0x245240, checks
  the BIK magic; called from the movie player at 0x7319B, handle at player+0x120) and `BinkClose` (0x246520,
  from 0x72390) were found by hand and named in `config/game.toml`. Hooks in `runtime/xdk/bink.cpp`.
- At BinkOpen: the file's stem names `movies\ai\<stem>.mp4`; its size (2x, same shape) and length (±1 frame of
  the Bink struct's Frames at +8) are checked, else the original is drawn.
- A Media Foundation source reader decodes NV12 on its own thread, up to 4 frames ahead, converted to BGRX here
  (the reader's own RGB conversion could not keep up). Frame numbers come from sample times.
- The renderer (`D3D12Backend::MovieFrame`) swaps the 640x480 linear X8R8G8B8/A8R8G8B8 texture for the decoded
  frame whose number is the Bink struct's FrameNum (+0xC), within 2 frames; else the game's own frame.
- Log per movie: `movies: <name>.mp4 ended, N of M frames drawn upscaled; missed: ...`.
- Bugs met: a seek loop (a seek restarts at the keyframe before the target; those frames moved `next` back and
  asked for another seek) and an off-by-one that seeked backwards after every handed-over frame.

## Textures (not started)
Plan: with an option on, each new texture is dumped (SDK texture dumper), upscaled in the background with
`realesrgan-x4plus`, and loaded as a replacement next time (player packs in `textures\replacement\` win). Skip
fonts and HUD text (a sharper filter distorted letters in 1.2.0), bump maps, render-target copies and tiny
textures. Offline extraction from the level `.bin` bundles would need that format worked out first.
