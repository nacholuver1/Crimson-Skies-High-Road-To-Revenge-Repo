# Packs the player download: dist\CrimsonSkiesRecomp-<version>-win64.zip
# Contents: CrimsonSkies.exe, the Visual C++ runtime, Microsoft's shader compiler (d3dcompiler_47.dll), compiler\
# (the recompiler, Clang and LLD that build the game's
# code on the player's PC at first start, from their own default.xbe), an empty assets\ folder, README.txt and
# licenses\. Never any game data or generated code: compiler\work\ stays out, and a guard refuses game files.
#
# The version is CMakeLists.txt's project(VERSION): the exe shows it and its updater compares it with GitHub's latest
# release, so bump it there first. The zip's name and folder must stay CrimsonSkiesRecomp-<version>-win64: the updater
# looks for the release file ending in -win64.zip and the folder holding CrimsonSkies.exe.
#
# Usage: powershell -ExecutionPolicy Bypass -File tools\make_release.ps1 [-Version 1.9.0] [-NoBuild]
param(
    [string]$Version,
    [switch]$NoBuild
)
$ErrorActionPreference = "Stop"
$root = Split-Path $PSScriptRoot -Parent
$cmakeVersion = [regex]::Match((Get-Content (Join-Path $root "CMakeLists.txt") -Raw), 'project\(crimsonskies VERSION ([0-9.]+)').Groups[1].Value
if (-not $cmakeVersion) { throw "no project(crimsonskies VERSION x.y.z) in CMakeLists.txt" }
if (-not $Version) { $Version = $cmakeVersion }
if ($Version -ne $cmakeVersion) { throw "-Version $Version differs from CMakeLists.txt's ${cmakeVersion}: bump it there (the exe reports it)" }
$bin = Join-Path $root "out\build\release"
$name = "CrimsonSkiesRecomp-$Version-win64"
$dist = Join-Path $root "dist"
$stage = Join-Path $dist $name
$zip = Join-Path $dist "$name.zip"

if (-not $NoBuild) {
    $env:CI = "1"  # build.bat: no prompts
    cmd /c "`"$(Join-Path $root 'build.bat')`""
    if ($LASTEXITCODE -ne 0) { throw "build failed" }
}

# the Visual C++ runtime the executables import, beside them, so a fresh Windows needs nothing more
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs = & $vswhere -latest -products * -property installationPath
$crt = Get-ChildItem "$vs\VC\Redist\MSVC\*\x64\Microsoft.VC*.CRT" -Directory | Sort-Object FullName | Select-Object -Last 1
if (-not $crt) { throw "Visual C++ runtime not found: install Visual Studio with the C++ workload" }

if (Test-Path $stage) { Remove-Item -Recurse -Force $stage }
if (Test-Path $zip) { Remove-Item -Force $zip }
New-Item -ItemType Directory -Force $stage, (Join-Path $stage "assets"), (Join-Path $stage "compiler") | Out-Null

Copy-Item (Join-Path $bin "CrimsonSkies.exe") $stage
# Microsoft's shader compiler from the Windows SDK's redistributable folder, beside the exe: Proton/Wine (Steam Deck,
# Linux) loads it before its own d3dcompiler_47, which fails on the renderer's shaders and leaves the screen black
$d3dc = Get-ChildItem "${env:ProgramFiles(x86)}\Windows Kits\10\Redist\D3D\x64\d3dcompiler_47.dll" -ErrorAction SilentlyContinue |
    Select-Object -First 1
if (-not $d3dc) { throw "d3dcompiler_47.dll not found in the Windows SDK's Redist\D3D\x64: install the Windows SDK" }
Copy-Item $d3dc.FullName $stage
foreach ($dll in "msvcp140.dll", "vcruntime140.dll", "vcruntime140_1.dll") {
    Copy-Item (Join-Path $crt.FullName $dll) $stage
    Copy-Item (Join-Path $crt.FullName $dll) (Join-Path $stage "compiler")  # xbe_recomp.exe looks beside itself
}
$compiler = Join-Path $bin "compiler"
foreach ($item in "xbe_recomp.exe", "clang.exe", "lld-link.exe", "LLVM-LICENSE.txt", "CrimsonSkies.lib", "config", "include", "xdk") {
    Copy-Item (Join-Path $compiler $item) (Join-Path $stage "compiler") -Recurse
}
Copy-Item (Join-Path $root "release\README.txt") $stage
Set-Content -Path (Join-Path $stage "assets\PUT YOUR GAME HERE.txt") -Encoding ascii -Value @(
    "Optional: put your own Crimson Skies: High Road to Revenge (Xbox, USA) disc image (.iso) or the extracted disc",
    "files (default.xbe beside its folders) here. Or pick your game in the launcher's Game row instead. See README.txt.")

# licenses: this project, the SDK, its libraries, LLVM (Clang, LLD) and the notices
$lic = Join-Path $stage "licenses"
New-Item -ItemType Directory -Force $lic | Out-Null
Copy-Item (Join-Path $root "LICENSE") (Join-Path $lic "crimson-skies-recomp.txt")
Copy-Item (Join-Path $root "THIRD_PARTY_NOTICES.md") $lic
Copy-Item (Join-Path $root "sdk\LICENSE") (Join-Path $lic "xboxxberecomp.txt")
Copy-Item (Join-Path $root "sdk\THIRD_PARTY.txt") (Join-Path $lic "xboxxberecomp-third-party.txt")
Copy-Item (Join-Path $compiler "LLVM-LICENSE.txt") (Join-Path $lic "llvm.txt")
$deps = Join-Path $bin "_deps"
foreach ($d in @(@("zydis-src", "zydis.txt"), @("tomlplusplus-src", "tomlplusplus.txt"), @("imgui-src", "dear-imgui.txt"),
                 @("sdl3-src", "sdl3.txt"))) {
    $f = Get-ChildItem (Join-Path $deps $d[0]) -File | Where-Object { $_.Name -match '^(LICEN[SC]E|COPYING)' } | Select-Object -First 1
    if ($f) { Copy-Item $f.FullName (Join-Path $lic $d[1]) }
}

# guard: nothing from the game, no generated code, no player files
$bad = Get-ChildItem $stage -Recurse -File | Where-Object {
    $_.Extension -in '.xbe', '.iso', '.xiso', '.bik', '.xmv', '.zip', '.bin', '.xbx', '.dll' -and
    $_.Name -notin 'msvcp140.dll', 'vcruntime140.dll', 'vcruntime140_1.dll', 'd3dcompiler_47.dll' -or
    $_.Name -eq 'settings.toml' -or $_.FullName -like '*\work\*' -or $_.FullName -like '*\generated\*' -or
    $_.Name -like 'x86_recomp*'
}
if ($bad) { throw "game, generated or player files in the release: $($bad.FullName -join ', ')" }

Compress-Archive -Path $stage -DestinationPath $zip -CompressionLevel Optimal
$size = [math]::Round((Get-Item $zip).Length / 1MB, 1)
"Release: $zip ($size MB)"
