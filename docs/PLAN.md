# Recomp plan: original Xbox (x86) path

Written 2026-11-21. The Recomp SOP was written for Xbox 360 / ReXGlue. This is its translation
to an original Xbox game on [XboxXBERecomp](https://github.com/TGP482/XboxXBERecomp) (see
[RESEARCH.md](RESEARCH.md) for why that toolkit). Each phase lists what "done" means.

## 0. Decide the target build (blocks everything else)

Our dump is a **debug-signed pre-release** (built 2003-09-09, 12 days before retail). Every
address, hook and the setup-screen hash ties to one exact `default.xbe`.

- **Retail `default.xbe`** (recommended if you own the retail disc): it's the build players
  have, the build the other two projects and Microsoft's PC release used, and the one their bug
  write-ups describe.
- **This beta**: it works with the toolkit, but players would need this exact beta dump, and
  pre-release builds can have unfinished content and debug-only paths.

Done: one XBE chosen, and its SHA-256 recorded in RESEARCH.md.

**Decided 2026-11-21: retail.** SHA-256 `f923b28f…5b98ba`, extracted to `assets/`.

## 1. Repo layout (SOP §2)

- Copy `templates/game` from XboxXBERecomp into this repo. Add the SDK as `sdk/` (a git
  submodule pinned to a commit, the counterpart of the Blitz SDK junction).
- Set `GAME_TITLE` / `GAME_XBE` / `GAME_EXE` (`CrimsonSkies`) in `build.bat` and
  `release.bat`, plus `add_xbe_image(CrimsonSkies default.xbe game.toml 128 game)` in
  `CMakeLists.txt` (the number there is the count of generated-code partitions, which
  `xbe_symbols` writes into `config/game.toml`; it isn't a memory size). saironbusatto's crash
  from a too-small contiguous-memory window was in the sp00nznet runtime. Watch for the same
  thing in XboxXBERecomp's `MmAllocateContiguousMemory`.
- Game files go in `assets/` (extract with `tools/extract_xiso.py`) and are gitignored, like
  `generated/`.
- Toolchain: VS 2022 C++ workload plus clang-cl / LLD (`build.bat` offers to install the
  missing VS components; that's an admin prompt for you to accept).

Done: `build.bat` gets as far as `xbe_symbols` and stops on "no signatures for 5659".

**Done 2026-11-21.** `sdk/` is [nacholuver1/XboxXBERecomp](https://github.com/nacholuver1/XboxXBERecomp)
branch `crimson-skies` = upstream `cc9edda` + PR #27 (5659 signatures, with the PR's layout
replacing upstream's GTA III one). Remotes in `sdk/`: `origin` = the fork, `upstream` =
TGP482. To take upstream changes: `git -C sdk fetch upstream && git -C sdk rebase upstream/main`,
then push to the fork and commit the new pin.

## 2. XDK 5659 signatures (the main new work)

The toolkit has signatures for 4831 and 5849 but not 5659, so `xbe_symbols` can't write
`config/game.toml` by itself. Plan:

1. **Seed from 5849.** Run `xbe_symbols` with a temporary `xdk/5659/signatures.toml` copied
   from 5849. Signatures are lengths and hashes with addresses masked, so most XDK functions
   that didn't change between 5659 and 5849 will still match. Record the hit and miss counts.
2. **Fill the misses** with [XbSymbolDatabase](https://github.com/Cxbx-Reloaded/XbSymbolDatabase)
   (Cxbx-Reloaded's MIT library). Its OOVPA patterns cover D3D8, DSound, XAPI, XGraphics and
   XOnline across 5558-5849, so a small scanner tool built on it prints the addresses of the
   D3D / DSound / XAPI functions in our XBE. Check any still missing by hand in Ghidra.
3. **Make real 5659 signatures** from the finished config:
   `xbe_symbols --sign config/game.toml --out sdk/xdk/5659/signatures.toml`. Offer these upstream
   to TGP482 as a PR. They're hashes, not library code, so they carry no game or XDK code.

Done: `config/game.toml` has every `[symbols]` / `[data]` entry non-zero, or a deliberate stub
with a note.

**Status 2026-11-21:** `config/game.toml` written from the fork's 5659 data: 172/248 functions,
13/13 data. The 76 zeros are explained in RESEARCH.md (unlinked, apart from the XNet sockets,
which are deferred to System Link work). Step 3 (`--sign`) waits until the socket entries are
filled.

## 3. First build and boot (SOP §3-4)

Known issues to check first, from the other two Crimson Skies projects. Re-find each address
in our XBE; none carry over:

- **FPO-heavy code** (only 12% of functions have standard prologues). Expect missed functions
  and fix them with `[[function]]` entries. Stack bugs show up as corrupted callee-saved
  registers, so check that first when a crash looks impossible.
- **Two `__fastcall` kernel imports** (`IofCallDriver`, `IofCompleteRequest`).
- **`_initterm` static initialisers** that function discovery misses (saironbusatto found 8).
- **D3D COM overrides** that drop the 4-byte return address: stack drift after D3D calls.
- **Bink movies**: the toolkit has a fix for Bink playing too fast above 30 fps and crashing
  on fast reads. Check that it applies to this game's Bink build.
- **Frame rate**: xemu lists a 30 fps target. Test whether game speed follows the frame rate
  before uncapping, the way the SOP's "keep VSync on" rule came about.
- **XOnline / XNet** sections: stub Live sign-in to "offline". System Link comes from the
  toolkit.

Testing follows SOP §4: a throwaway settings file per run, FPS from the overlay in the attract
demo, you look at close-up artifacts, and one lldb session when profiling. Use
`xbe2pe.py config/game.toml` to get a flat PE for Ghidra.

Done: boots through the intro movies to the main menu, then a full mission is played.

**Status 2026-11-21:** boots, intro, title, menus (text fixed) and a mission load reach gameplay. The
bugs and fixes are in RESEARCH.md, "First boot to gameplay". Next: a full mission, saves (watch for xemu #2772),
audio rates (xemu #147), frame rate (30 fps gate, 60 fps option).

## 4. Release (SOP §5)

The same conventions as Blitz, with two original-Xbox differences:

- **No game code in the release.** XboxXBERecomp builds `CrimsonSkies.dll` on the player's PC
  at first start, from their own XBE, with a bundled Clang. That replaces the Blitz setup
  screen's codegen step. It fits "one exe, portable", but the first start takes several
  minutes.
- **Linux is an open question.** The toolkit is Windows-only (D3D12 / XAudio2). Options: (a)
  ship Windows only and document Proton/Wine; (b) port the renderer later (sp00nznet's toolkit
  has an OpenGL path to study). Decide after the game is playable.
- The rest is unchanged: MIT LICENSE, THIRD_PARTY_NOTICES (XboxXBERecomp, Zydis, toml++, ImGui,
  SDL3, XbSymbolDatabase if it's linked), credits to saironbusatto / sp00nznet for their notes,
  AI disclosure, bug-report form, private until you say go, v1.0.0, r/recomps post.

## Risks

- XboxXBERecomp is young (SDK 0.1.0) and has one maintainer. Pin the submodule and keep our
  fixes as hooks in `src/game`, not as edits to `sdk/`.
- A 5659-specific D3D8 or DSound behaviour the toolkit hasn't met yet (its ports are 4831 /
  5849). This is the likeliest place for deep bugs.
- The beta build, if we stay on it: crashes that retail fixed.
