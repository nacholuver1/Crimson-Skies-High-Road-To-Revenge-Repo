# Crimson Skies: High Road to Revenge — native PC recompilation

An unofficial native PC port of **Crimson Skies: High Road to Revenge** (original Xbox, 2003),
made by *static recompilation*: the game's code is translated ahead of time into a normal
x64 program, with a reimplemented Xbox kernel, GPU, audio and input layer underneath.

**Status: early, boots to the title screen.** The retail `default.xbe` builds and runs through the intro
movies to "PRESS START" with sound. Menus past that and gameplay are untested. Built on
[XboxXBERecomp](https://github.com/TGP482/XboxXBERecomp) through a fork with the fixes this game needed
([nacholuver1/XboxXBERecomp](https://github.com/nacholuver1/XboxXBERecomp), branch `crimson-skies`).

**This repository contains no game data.** No disc image, no game files, and none of the
code generated from the game's executable. To play, you will need your own copy of the game.

> **AI-assisted project.** The code, build scripts, debugging and documentation in this
> repository are written by [Claude](https://claude.com/claude-code) (Anthropic's AI, via
> Claude Code), directed and play-tested by [nacholuver1](https://github.com/nacholuver1).
> Every commit carries a `Co-Authored-By: Claude` trailer.

## Target build

The working dump is the **USA beta of 2003-12-09** (En, Fr, De, Zh, Ko). Addresses, hooks and
the setup-screen hash will be tied to that exact `default.xbe`; the retail release would need
its own.

## How this differs from the Blitz recomps

[Blitz-TheLeague-Recomp](https://github.com/nacholuver1/Blitz-TheLeague-Recomp) targeted the
Xbox 360 (PowerPC, `default.xex`) with the ReXGlue SDK. The original Xbox is a different
machine, and ReXGlue does not apply:

| | Xbox 360 (Blitz) | Original Xbox (this game) |
|---|---|---|
| CPU | PowerPC Xenon, big-endian | Intel Pentium III (x86-32), little-endian |
| Executable | `default.xex` | `default.xbe` |
| GPU | ATI Xenos | NVIDIA NV2A (GeForce 3 class) |
| Kernel | xboxkrnl (360) | xboxkrnl (original, ~366 exports) |
| Game code links | XDK libraries | XDK libraries statically linked (D3D8, DSound, XAPI) |

The toolchain is [XboxXBERecomp](https://github.com/TGP482/XboxXBERecomp) (x86 to C++, Xbox kernel and XDK
libraries on the host, Direct3D 12 and XAudio2), with the XDK 5659 signatures of
[TGP482/XboxXBERecomp#27](https://github.com/TGP482/XboxXBERecomp/pull/27). See [docs/PLAN.md](docs/PLAN.md)
and [docs/RESEARCH.md](docs/RESEARCH.md) for the plan, the fork's fixes and what other projects contributed.

## Tools

```bash
# List / extract the disc (plain xiso or full redump image)
python3 tools/extract_xiso.py "path/to/your.iso" game --list
python3 tools/extract_xiso.py "path/to/your.iso" game

# Title ID, sections, entry point and XDK library versions of the game executable
python3 tools/xbe_info.py game/default.xbe --sha256
```

Keep the disc image and the extracted `game/` folder on your own machine; `.gitignore` keeps
them out of the repository.

## Legal

This project's own code is MIT licensed ([LICENSE](LICENSE)). Crimson Skies and its code,
assets and trademarks belong to their owners; this project is not affiliated with or endorsed
by them. Use it only with a copy of the game you own.
