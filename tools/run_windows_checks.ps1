param([string]$BuildDir = "build/windows", [switch]$Interactive)
$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $PSScriptRoot
Push-Location $Root
try {
    python tools/make_fixture.py --check
    if ($LASTEXITCODE -ne 0) { throw 'fixture check failed' }
    cmake -S . -B $BuildDir -A x64
    if ($LASTEXITCODE -ne 0) { throw 'configure failed' }
    cmake --build $BuildDir --config Release
    if ($LASTEXITCODE -ne 0) { throw 'build failed' }
    ctest --test-dir $BuildDir -C Release --output-on-failure
    if ($LASTEXITCODE -ne 0) { throw 'core tests failed' }
    $Exe = "$BuildDir/Release/MysteryDungeonPet.exe"
    & tests/wic_tests.ps1 -Exe $Exe -Output "$BuildDir/wic-results"
    cmake --install $BuildDir --config Release --prefix "$BuildDir/package"
    if ($LASTEXITCODE -ne 0) { throw 'package failed' }
    $bytes = (Get-ChildItem "$BuildDir/package" -Recurse -File | Measure-Object Length -Sum).Sum
    Write-Output "Installed fixture package bytes: $bytes (A-2 only)"
    if ($Interactive) {
        New-Item -ItemType Directory -Force "$BuildDir/metrics" | Out-Null
        foreach ($scenario in @('empty','idle','walk','hide','switch')) {
            $seconds = if ($scenario -eq 'switch') { 165 } else { 90 }
            $csv = [IO.Path]::GetFullPath("$BuildDir/metrics/$scenario.csv")
            $proc = Start-Process (Resolve-Path $Exe).Path -ArgumentList @('--scenario', $scenario, '--duration', $seconds, '--metrics', "`"$csv`"") -PassThru
            if (-not $proc.WaitForExit(($seconds + 30)*1000)) { $proc.Kill(); throw "$scenario timeout" }
            if ($proc.ExitCode -ne 0) { throw "$scenario failed" }
            python tools/summarize_metrics.py $csv --output "$BuildDir/metrics/$scenario.json"
            if ($LASTEXITCODE -ne 0) { throw 'metric summary failed' }
        }
        Write-Output 'Now perform the documented real drag/DPI/lock/display-off/suspend checks; automated scenarios alone do not cover them.'
    }
} finally { Pop-Location }
