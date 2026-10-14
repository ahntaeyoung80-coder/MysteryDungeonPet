param([string]$BuildDir = 'build/direct-runtime')
$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $PSScriptRoot
Push-Location $Root
try {
    cmake -S . -B $BuildDir -A x64 -DBUILD_TESTING=OFF
    if ($LASTEXITCODE -ne 0) { throw 'configure failed' }
    cmake --build $BuildDir --config Release
    if ($LASTEXITCODE -ne 0) { throw 'build failed' }
    python tools/check_windows_runtime.py --refresh-from "$BuildDir/Release/MysteryDungeonPet.exe" --compiler 'MSVC x64 Release / static CRT'
    if ($LASTEXITCODE -ne 0) { throw 'runtime refresh failed' }
} finally { Pop-Location }
