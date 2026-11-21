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

**Target: the retail XBE** (checked below, it matches these numbers). The beta dump turned out
to be an earlier pre-release and is kept only for comparison.

## Toolkits (original Xbox)

| | [TGP482/XboxXBERecomp](https://github.com/TGP482/XboxXBERecomp) | [sp00nznet/xboxrecomp](https://github.com/sp00nznet/xboxrecomp) |
|---|---|---|
| Output | C++ | C |
| Graphics | D3D8 → **D3D12** | D3D8 → D3D11; OpenGL 3.3 on Linux/macOS |
| Audio | DirectSound → XAudio2 | software mixer + MCPX APU (from xemu) |
| Game code in releases | **none**: code is recompiled on the player's PC at first start | not stated |
| XDK matching | `xdk/<build>/signatures.toml`; signatures for 4831 and 5849 only. **5659 has `layout.toml` but no `signatures.toml`** (checked 2026-11-21), and `xbe_symbols` stops without one | function ID pass (CRT / D3D / RenderWare) |
| Host OS | Windows x64 only | Windows, Linux, macOS |
| Shipped ports | Splinter Cell: Double Agent, BLACK, 007 FRWL, Nightfire, Red Dead Revolver | Burnout 3, Nightfire playable; others booting |
| License | MIT | MIT, some parts LGPL-2.1 (from xemu) |
| Last activity | 2026-11-08 | v0.18.0, Nov 2026 |

**Recommendation: XboxXBERecomp.** It uses D3D12 like the Blitz ports, and it ships no game
code, which matches our "no game data in the repo or release" rule (the Blitz setup screen did
the same with codegen). It has **no 5659 signatures yet**, so making them is the first real job
(see [PLAN.md](PLAN.md)). It is also Windows-only, so the SOP's native Linux build is an open
question. Use the sp00nznet project and saironbusatto's write-ups as a bug map, not as code to
copy.

## Our dumps (`tools/xbe_info.py`, 2026-11-21)

**Target = retail**, from `Crimson Skies - High Road to Revenge (USA) (En,Fr,De,Zh,Ko).xiso.iso`
(plain xiso, 226 files, 4.1 GB extracted to `assets/`). Its entry point and kernel thunk match
what both other projects report.

| | Beta (comparison only) | **Retail (target)** |
|---|---|---|
| `default.xbe` SHA-256 | `daf9d2237836af7dd818a656036eb93a383cb97ed615807b0d25215029d78416` | `f923b28f390d0c036cdc7cf3a2d9becd7d9b2e61314a6c7b3278116bc25b98ba` |
| Size | 3,215,360 bytes | 3,219,456 bytes |
| Image size | 0x003382A0 | 0x00339C60 |
| Signing | **debug key** (entry/thunk XOR 0x94859D4B / 0xEFB1F152) | retail |
| Build timestamp | 0x3F5D74E9 = **2003-09-09** | 0x3F6CEA8B = 2003-09-21 |
| Entry point | `0x001C627A` | `0x001C7CA9` |
| Kernel thunk | `0x0028CBE0` | `0x0028E480` |
| Sections | 32 (same names and order) | 32 |
| Title ID | `0x4D530021` (MS-033), region 1 (NA), media 0x2 | same |
| XDK | 5659 for all 10 libs (XAPILIB, D3D8, D3DX8, XGRAPHC, DSOUND, XBOXKRNL, XONLINES, XVOICE, LIBCP, LIBC) | 5659 |

So the dump is a **pre-release built 12 days before retail**, not a later Asian build. The
2003-12-09 in the file name is the disc date, not the build date. Addresses differ from retail
by a few KB (entry is 0x1A2F lower), so nothing from the other projects carries over without
re-finding it. The XboxXBERecomp runtime handles the debug XOR keys (`runtime/xbe/xbe.h`).

The disc also has `update.xbe` (2.4 MB, dashboard updater; not needed) and XSN Sports demo
videos (`videos/*.xmv`). The game's own movies are Bink.

## Official PC release

Microsoft's Xbox backward compatibility on PC (launched July 2026) includes this game in its
first wave ([The FPS Review](https://www.thefpsreview.com/2026/07/23/xbox-backward-compatibility-launches-on-pc-conker-crimson-skies-blinx-and-fuzion-frenzy-available-now/)).
That doesn't stop a recomp, but it means the main pitch of a fan port is mods, a portable
build, Linux, uncapped settings and no store account.

## Compatibility reports

- xemu: the game appears in the performance meta-issue
  [xemu#1098](https://github.com/xemu-project/xemu/issues/1098) (30 fps target), with no
  bug notes. The xemu.app title page couldn't be read from the cloud session (host blocked).
- Cxbx-Reloaded: no report found yet.
- Reddit / YouTube: nothing specific found yet.

## Still to do

- [x] `xbe_info.py` on our dump: title ID, XDK version, build date (see above)
- [ ] Read xemu.app and the Cxbx-Reloaded compatibility entries in full
- [x] Read the XboxXBERecomp game template (in use)
- [x] Translate SOP parts 2–3 into their original-Xbox equivalents ([PLAN.md](PLAN.md))

## Phase 2 findings: XDK 5659 (2026-11-21)

**Someone has already done most of this.** `maxscipio` ported Crimson Skies on XboxXBERecomp
before MechAssault. Their [MechAssault notes](https://github.com/maxscipio/SlopAssault/blob/main/docs/PORTING_NOTES.md)
call it the "third port with this method (after Crimson Skies and Baldur's Gate)", and say the
movies played. The Crimson port itself isn't public. What is public:

- Upstream PR [TGP482/XboxXBERecomp#27](https://github.com/TGP482/XboxXBERecomp/pull/27),
  "Add XDK 5659 layout and signatures", read from this game's retail XBE. Open since
  2026-11-01, not merged. MIT repo; hashes and constants only.
- The same signatures and layout inside SlopAssault's `patches/sdk.patch` (identical to the PR,
  all 263 entries). SlopAssault has **no license**, so read it for ideas only and copy no
  code from it.

Results on our retail XBE (`xbe_symbols` with the PR data in `work/xdk`):

- **172 of 248 functions and 13 of 13 data** found. All 76 misses are entries the PR left
  empty on purpose.
- Cross-check against [XbSymbolDatabase](https://github.com/Cxbx-Reloaded/XbSymbolDatabase)
  (CLI built in `work/`): **144 functions found by both, 0 disagreements.** XbSymbolDatabase
  also finds 5 of the PR's empty entries, all sockets: `socket` 0x2757B0, `bind` 0x274762,
  `connect` 0x2747E6, `ioctlsocket` 0x273C92, `recv` 0x27519E. So the game does link XNet
  sockets (System Link), and the other socket entries (`send`, `closesocket`, `recvfrom`, ...)
  are real gaps. Left at 0 for now as a set: replacing half the socket API would hand host
  handles to guest functions.
- Filling the empty entries with 5849 signatures matches nothing new (it only adds one false
  `SetI3DL2Source`/`SetMode` collision), so the remaining gaps are real code changes between
  the two XDK builds.
- **Lighting:** on MechAssault the same search missed `SetLight` / `LightEnable` /
  `SetMaterial`, and lit models rendered black. In our D3D section no function copies a light
  (0x1A dwords) or a material (0x11 dwords) into the device. The only rep-movsd copy into the
  device is `SetTransform`. So this game most likely doesn't link them, and 0 is right.
  Check again if anything renders black.
- Live unreplaced library code (game-reachable, not replaced by the host): D3DX, XGraphics,
  Bink, XOnline, plus some D3D/DSound resource and lock helpers. That's by design: none of those
  D3D names are in the 4831/5849 lists either, and the runtime never references them. The 4
  unnamed D3D functions the game calls directly are a surface-desc wrapper, a kernel-thunk
  `jmp`, `Get2DSurfaceDesc`, and a `return 1` stub.

**Layout conflict, which matters:** upstream `xdk/5659/layout.toml` (from GTA III) and the PR's
layout (from this game) disagree on most `CDevice_*` offsets. Our code agrees with the **PR**
on every offset checked:

| Field | Our code | PR | Upstream (GTA III) |
|---|---|---|---|
| Transforms (`SetTransform` 0x2027C0) | 0xC50 | 0xC50 | 0xC60 |
| Textures (`SetTexture` 0x2034B0) | 0xF78 | 0xF78 | 0xF88 |
| RenderTarget (`GetRenderTarget2` 0x203240) | 0x19F4 | 0x19F4 | 0x1A04 |
| BackBuffer (`GetBackBuffer2` 0x202EC0) | 0x1A04 | 0x1A04 | 0x1A14 |
| ScreenSpaceOffset (`SetScreenSpaceOffset` 0x203E40) | 0xEE8 | 0xEE8 | 0xEF8 |

So the stock SDK would read the wrong fields of this game's D3D device. GTA III's "5659" may be
a different library QFE. We need the PR's layout. Until upstream sorts this out, ship it from
our repo rather than editing `sdk/`.

Recompiler run on the PR config: 14,542 functions, 2.49 of 2.61 MB code decoded, 243 jump
tables, a handful of "unimplemented" opcodes (`aaa`, `daa`, `bound`, `lds`, ...), almost
certainly data decoded as code. Check them when they show up as traps.

## What the other projects give us (checked against our retail analysis, 2026-11-21)

**saironbusatto/CrimsonSkiesRecomp** (MIT, sp00nznet toolkit, D3D11/OpenGL):

- Its 8 "missing `_initterm` initialisers" (0x271213 XNET, 0x286E2C XPP, 0x23A409 / 0x23A414 /
  0x23D000 / 0x23D00B DSOUND, 0x28E25B / 0x28E28B input) are **all found** by XboxXBERecomp's
  function discovery. Nothing to do.
- Its worst bug (stack corruption through `D3DDevice_SetVertexShader` 0x202590 and the FVF parser
  0x201910, ending in a crash in `XInputGetState` 0x286F28) doesn't apply. `SetVertexShader` and
  `XInputGetState` are host-replaced in our config.
- The 128 MB contiguous-memory crash was in that toolkit's memory layout. Watch for it in ours.
- The "4-byte return address" stack drift was a bug in its hand-written D3D overrides.
  XboxXBERecomp's host ABI handles `ret N`. Keep it in mind for our own `X86_FUNC` hooks.

**sp00nznet/crimsonskies-recomp** (MIT, notes only):

- FPO-heavy build (12% standard prologues), so `ebp` is scratch, and stack bugs show up as
  corrupted callee-saved registers.
- `IofCallDriver` / `IofCompleteRequest` / `ObfDereferenceObject` are `__fastcall`.
  XboxXBERecomp already declares them `KERNEL_FASTCALL` (`runtime/kernel/xboxkrnl.cpp`).
- The Microsoft backward-compatibility teardown (`sp00nznet/xboxrecomp` `docs/technical/ms-fusion-*`)
  is design-level: one flat code arena, and recorded indirect-branch feedback. Its one lesson
  that applies, pushing the real guest return address, XboxXBERecomp already does.

**Laeresh/CSVM** (GPL-3.0): a C# remake of **Crimson Skies (2000, PC, Zipper Interactive)**,
not this game. It's a different engine (High Road to Revenge is FASA Studio's), so nothing
carries over. It's GPL anyway, so we'd only read it.

**crazyriddler/xbased** (an emulator, no clear license): has 60 / 120 fps patches for this game.
They target the retail build. Checked: the game has its **own 30 fps frame gate**, a minimum period of
1/30 s (float at **0x2C8368**, loaded at 0x1308D9 and 0x130B23), and the gate converts frame
time to whole ms through `_ftol2` (0x1F1A28) at 0x12FBCD. So the speed is gated, not tied to the
vblank alone. A 60 fps option means changing that constant and carrying the dropped ms fraction,
which is the approach that patch takes. We'll write our own hook for it rather than copy the
patch.

## Other XboxXBERecomp ports (from the recompsoftware.com list, 2026-11-21)

Searched GitHub for every port that carries its own `xdk/<build>/` data:

- **5659 layouts:** [CrownParkComputing/xbox-recomp-suite](https://github.com/CrownParkComputing/xbox-recomp-suite)
  (no license) has a 5659 layout proven on *Grabbed by the Ghoulies* (another 5659 game). It
  **agrees with the PR #27 layout** on every offset we checked (0xC50 / 0xF78 / 0x19F4 /
  0x1A04 / 0xEE8). [possessed1911/AAROAS-XBERecomp](https://github.com/possessed1911/AAROAS-XBERecomp)
  only carries upstream's GTA III file. So two 5659 games plus our own code say upstream's
  layout is the outlier.
- **5558 signatures** (the build just before ours): Athelas (MIT), The Sims (MIT), Tiger Woods
  2004 (GPL-3.0; tested locally only). Used to fill the PR's 76 empty entries, they matched
  **nothing new**. Neither did 5849 or 4831.
- No other repo has 5659 signatures.

So, apart from the sockets, the 76 gaps are very likely functions this game never linked:
four XDK builds' signatures miss them all, and the lighting check above agrees. The exception
is the **XNet socket set**. XbSymbolDatabase shows `socket` / `bind` / `connect` / `ioctlsocket`
/ `recv` are linked, so `send`, `recvfrom`, `closesocket`, `XNetStartup`, ... almost certainly
are too. That only matters for System Link. Do it after single-player boots, by walking the
XNet section around the five known addresses.

## First boot to gameplay: bugs, fixes and sources (2026-11-21)

Each fix is in the SDK fork (`nacholuver1/XboxXBERecomp`, branch `crimson-skies`) unless it says config.

| Symptom | Cause | Fix | Sources |
|---|---|---|---|
| Trap 0.66 s in, "indirect call to 002181ED" | D3DX image loaders `longjmp` back to a `_setjmp3` (0x1F2F64) return address, which had no entry | recompiler: `[xbe] setjmp`; config `setjmp = 0x1F2F64` | SOP §2 (setjmp/longjmp); maxscipio's MechAssault notes (D3DX loaders) |
| Dirty disc error | the game indexes the disc root into `Z:\dvdcontent.bin` with an **empty** directory mask, which the SDK matched against nothing | kernel: an empty mask lists everything | xemu #2902 mentions the same error ("unless you use a fresh hdd image") |
| Null read at the title screen | `IDirectSoundBuffer_SetNotificationPositions` had no host version | dsound: implemented (events at play-cursor offsets and on stop) | PR #27 has the signature |
| Menu text doubled / flickering | one surface CPU-written per text line between draws. The lock didn't wait for queued draws, and textures were checked once per frame | gpu: re-check after a lock; d3d: texture locks wait for the draws that read them (matched by memory too) | **xemu #2902 and its fix PR #2965** (abaire): same diagnosis |
| Polling `IsBusy` could hang | the batch in progress wasn't submitted | **upstream PR #5** (cherry-picked) | TGP482/XboxXBERecomp#5 (Conker port) |
| Crash loading a mission | `IDirectSoundBuffer_SetRolloffFactor` had no host version | dsound: accepted, ignored like the stream version | |
| Trap after the mission relaunch, "indirect call to 00185110" | a factory function right after a jump table, only reached from the table at 0x2CE088 | config `[functions] 0x185110` | |

Also added: `XBE_LOG=1` (full logging in a release build), file and folder logging, and a "texture changed between draws" report.

The game restarts its own executable (`HalReturnToFirmware`, `default.xbe`) to load a mission or the attract demo. The SDK relaunches into a new process, which writes its own log.

**Known issues from other emulators to test for:**

- [xemu #2772](https://github.com/xemu-project/xemu/issues/2772): freezes on a loading screen after aircraft selection (leaving the base), and corrupts or deletes the save. Reported on xemu and Cxbx-Reloaded builds since about June 2024.
- [xemu #147](https://github.com/xemu-project/xemu/issues/147): `Gun_2D` and other `_2D` sounds play muffled, at about 11 kHz instead of 22 kHz, and `_3D` sounds are too loud at distance.
- xemu #2902 / #1098: frame rate drops near the volcano in mission 2 (on xemu).
