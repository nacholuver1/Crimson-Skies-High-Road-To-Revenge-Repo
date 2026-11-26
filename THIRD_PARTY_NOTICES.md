# Third-party notices

## Xbox XBE Recomp

https://github.com/TGP482/XboxXBERecomp — MIT License, Copyright (c) 2026 TGP482. The recompiler and the runtime
(kernel, Direct3D 8 on Direct3D 12, DirectSound on XAudio2, input) this port is built on. This project uses a fork,
[nacholuver1/XboxXBERecomp](https://github.com/nacholuver1/XboxXBERecomp) (branch `crimson-skies`), which stays
under the SDK's license and carries:

- **XDK 5659 signatures and layout** from upstream pull request
  [#27](https://github.com/TGP482/XboxXBERecomp/pull/27) by maxscipio, read from this game's retail executable.
- **Visibility test results when polled without a present** from upstream pull request
  [#25](https://github.com/TGP482/XboxXBERecomp/pull/25) by paulohenriquesn (Ratatouille port).
- **`Busy` submits the batch in progress** from upstream pull request
  [#5](https://github.com/TGP482/XboxXBERecomp/pull/5) by yacineolivier-maker (Conker: Live & Reloaded port).
- Fixes made for this game: setjmp continuations, empty directory masks, DirectSound position notifications and
  rolloff, texture refills between draws and texture-lock waits, the shadow sampler border in reversed depth,
  critical sections on host state, FXAA, supersampling, video upscaling, FSR 1 upscaling and sharpening, a 4:3
  original-aspect setting, a game_path setting, trigger rumble and logging switches.

## AMD FidelityFX Super Resolution 1

https://github.com/GPUOpen-Effects/FidelityFX-FSR — MIT License. The runtime's `fsr` upscaling (EASU) and
`sharpening` (RCAS) are a pixel-shader port of the FP32 path of `ffx_fsr1.h`.

> Copyright (c) 2021 Advanced Micro Devices, Inc. All rights reserved.
>
> Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated
> documentation files (the "Software"), to deal in the Software without restriction, including without limitation
> the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to
> permit persons to whom the Software is furnished to do so, subject to the following conditions:
>
> The above copyright notice and this permission notice shall be included in all copies or substantial portions of
> the Software.
>
> THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO
> THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
> AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
> TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
> SOFTWARE.

The SDK's executables include these libraries (their licenses are in the release's `licenses` folder):

| Library | License |
|---|---|
| [Zydis](https://github.com/zyantific/zydis) (and Zycore) | MIT |
| [toml++](https://github.com/marzer/tomlplusplus) | MIT |
| [Dear ImGui](https://github.com/ocornut/imgui) | MIT |
| [SDL3](https://github.com/libsdl-org/SDL) | zlib |
| [LLVM](https://llvm.org) Clang and LLD, shipped in `compiler\` to build the game's code on the player's PC | Apache-2.0 with LLVM exceptions |
| Microsoft Visual C++ runtime (`msvcp140.dll`, `vcruntime140.dll`, `vcruntime140_1.dll`) | Visual Studio redistributable |
| Microsoft shader compiler (`d3dcompiler_47.dll`, from the Windows SDK's `Redist\D3D` folder) | Windows SDK redistributable |

## darkuni's recomp menus

The launcher (`src/common/launcher.cpp`) follows the layout and behaviour of the menus in
[darkuni/outrun-arcade-recomp](https://github.com/darkuni/outrun-arcade-recomp), by way of the Blitz recomps'
launcher.

```
MIT License

Copyright (c) 2026 darkuni

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

## Dear ImGui example code

The launcher's window and Direct3D 11 device setup follow Dear ImGui's `example_win32_directx11` (MIT License,
Copyright (c) 2014-2024 Omar Cornut; see `licenses/dear-imgui.txt`).

## Projects studied (no code taken)

These informed the work; nothing was copied from them unless listed above.

- **crazyriddler/xbased**: its 60 and 120 fps patches for this game describe the frame gate (period at 0x2C8368,
  loads at 0x1308D9 / 0x130B23) and the dropped milliseconds at 0x12FBCD. The unlock here is written from that
  description as hooks.
- **xemu** issue [#2902](https://github.com/xemu-project/xemu/issues/2902) and abaire's fix
  [#2965](https://github.com/xemu-project/xemu/pull/2965): the menu text drawn through one CPU-written surface.
  Also issues [#2772](https://github.com/xemu-project/xemu/issues/2772) and
  [#147](https://github.com/xemu-project/xemu/issues/147).
- **Cxbx-Reloaded's [XbSymbolDatabase](https://github.com/Cxbx-Reloaded/XbSymbolDatabase)** (MIT): used during
  research to check the XDK function addresses; not part of the build.
- **saironbusatto/CrimsonSkiesRecomp** and **sp00nznet/crimsonskies-recomp** (MIT): earlier recomps of this game on
  another toolkit, whose notes were checked against this build.
- **CrownParkComputing/xbox-recomp-suite**: its XDK 5659 layout (from Grabbed by the Ghoulies) confirmed the
  device offsets.
- **maxscipio/SlopAssault** (no license; read only): its notes on MechAssault pointed to the setjmp and lighting
  issues on this toolkit.

Crimson Skies and Crimson Skies: High Road to Revenge are trademarks of Microsoft. This project is not affiliated
with or endorsed by Microsoft, and contains none of the game's data or code.
