# AI upscaling: cutscenes and textures

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

## Textures (2026-11-27)

### The level bundles
The root `*.bin` files (missions, multiplayer maps, menus) hold named entries. A texture entry is
`<u32 2><u32 name bytes><name .tga/.dds NUL><u32 total><u16 w><u16 h><u16 mips><u8><u32 game format>
<u32 level 0 bytes>` followed by the levels, the bytes exactly as they sit in Xbox memory (swizzled where the console
swizzles). Game formats: 1 DXT1, 5 DXT5, 14 R5G6B5, 15 X8R8G8B8, 16 A8R8G8B8 (swizzled), 19 P8 (palette in a
separate `_pal` entry, not used). Each bundle carries its own copies of shared textures: 12,301 entries, 1,043
unique keys. The SDK's pack key (`Hash(level 0, Hash({xbox format, w, h}))`) computed from these bytes matched 775
of the 866 textures dumped in a full playthrough; the rest are P8, six DXT3 terrain textures and textures from
elsewhere.

### Making them (`--ai-textures`, `src/common/ai_movies.cpp` `MakeTextures`)
Reads every bundle off the disc image or folder, keys and dedupes the entries, leaves out by name `ui_*`, `hud_*`,
`esrb*`, `water!*` (normal maps), `*mask*` and `*_lm` (light maps), by size 32 px and below and 1024x1024 and up,
and by look flat colour and normal maps (bluish mean). The rest is decoded by the SDK (`texturedumper::DecodeBgra`),
upscaled 64 at a time by `realesrgan-x4plus -s 4` (on an opaque copy), halved to 2x with WIC's high-quality cubic,
given the original's alpha resized (0/255 alpha stays 0/255) and written to `textures\ai\<w>x<h>_<key>_<fmt>.png`.
`done.txt` holds the recipe. 650 textures, 152 MB, 2 min 19 s on an RTX 4070 SUPER; 393 left as they are.
Model choice: on samples `realesrgan-x4plus` kept sign lettering and brick, foliage and plane detail;
`realesr-animevideov3` smeared them into a painted look.

### Drawing them (SDK `gpu/texturedumper.cpp`)
Setting `ai_textures` (off by default; the launcher's Make turns it on) lists `textures\ai\` after the player's
`textures\replacement\`, so the player's pack wins where both name a texture. PNG replacements get box-filtered
mips. A see-through look of the Sea Haven cave water was checked during testing: it is the same with no pack
at all.
