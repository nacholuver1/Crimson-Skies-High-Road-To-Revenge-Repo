# Research (SOP part 1), 2026-11-21

The Recomp SOP was written for Xbox 360 / ReXGlue. This game is **original Xbox** (x86-32,
`default.xbe`, NV2A), so the Xenia trackers and ReXGlue tips don't apply. This file records
what does apply.

## Existing recomps of this game

| Project | Toolkit | State | Notes |
|---|---|---|---|
| [saironbusatto/CrimsonSkiesRecomp](https://github.com/saironbusatto/CrimsonSkiesRecomp) | vendored sp00nznet xboxrecomp | v1.1.0 (2026-11-06): intro movies with sound, input works, "first playable" release. Last commit 2026-11-01. | **Commits the lifted game code** (`src/recomp/gen/recomp_*.c`, ~55 MB), which our conventions forbid. D3D11 on Windows, OpenGL on Linux. Its README writes up the bugs it hit: a contiguous-memory window too small (raised to 128 MB), stack drift in D3D COM overrides (callee forgot the 4-byte return address), 8 unmapped `_initterm` static initialisers. MIT. |
| [sp00nznet/crimsonskies-recomp](https://github.com/sp00nznet/crimsonskies-recomp) | sp00nznet xboxrecomp | Stalled 2026-09-21. Lifting done (12,083 functions, 0 failures, 17 stubbed call targets); kernel layer 79/147; **never built or booted**. | Its notes are the most useful material either project has: the build is heavily FPO (only 12% of functions have a standard prologue), so `ebp` is scratch, function detection leans on `int3` padding, and stack bugs show up as corrupted callee-saved registers. Two kernel imports are `__fastcall` (`IofCallDriver`, `IofCompleteRequest`). Microsoft's own backward-compatibility package for this game was also a static recompile ("Fission"); sp00nznet wrote up a teardown of it. |

## Retail XBE (from both projects)

Title ID `0x4D530021`, build 2003-09-21, base `0x00010000`, entry `0x001C7CA9`, kernel thunk
`0x0028E480`, 32 sections (11 Bink), 147 kernel imports, **XDK 5659** for every library.

**Our dump is a beta dated 2003-12-09 (En/Fr/De/Zh/Ko).** That date is *after* the retail
build, so it may be the Asian release rather than a pre-release. Either way, every address
above will differ. Run `tools/xbe_info.py` on our `default.xbe` before reusing anything from
either project.

## Toolkits (original Xbox)

| | [TGP482/XboxXBERecomp](https://github.com/TGP482/XboxXBERecomp) | [sp00nznet/xboxrecomp](https://github.com/sp00nznet/xboxrecomp) |
|---|---|---|
| Output | C++ | C |
| Graphics | D3D8 → **D3D12** | D3D8 → D3D11; OpenGL 3.3 on Linux/macOS |
| Audio | DirectSound → XAudio2 | software mixer + MCPX APU (from xemu) |
| Game code in releases | **none**: code is recompiled on the player's PC at first start | not stated |
| XDK matching | `xdk/<build>/signatures.toml`; ships 4831, **5659**, 5849 | function ID pass (CRT / D3D / RenderWare) |
| Shipped ports | Splinter Cell: Double Agent, BLACK, 007 FRWL, Nightfire, Red Dead Revolver | Burnout 3, Nightfire playable; others booting |
| License | MIT | MIT, some parts LGPL-2.1 (from xemu) |
| Last activity | 2026-11-08 | v0.18.0, Nov 2026 |

**Recommendation: XboxXBERecomp.** It already has signatures for this game's XDK build, it
uses D3D12 like the Blitz ports, and it ships no game code, which matches our "no game data
in the repo or release" rule (the Blitz setup screen did the same with codegen). Use the
sp00nznet project and saironbusatto's write-ups as a bug map, not as code to copy.

## Compatibility reports

- xemu: the game appears in the performance meta-issue
  [xemu#1098](https://github.com/xemu-project/xemu/issues/1098) (30 fps target), with no
  bug notes. The xemu.app title page couldn't be read from the cloud session (host blocked).
- Cxbx-Reloaded: no report found yet.
- Reddit / YouTube: nothing specific found yet.

## Still to do

- [ ] `xbe_info.py` on our dump: title ID, XDK version, build date (on the user's PC)
- [ ] Read xemu.app and the Cxbx-Reloaded compatibility entries in full
- [ ] Read the XboxXBERecomp game template and the SCDA port's `config/` layout
- [ ] Translate SOP parts 2–3 (setup, known fixes) into their original-Xbox equivalents
