param([string]$BuildDir = "build/windows", [switch]$Interactive)
$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $PSScriptRoot
Push-Location $Root
try {
    python -m pip install Pillow==12.3.0
    if ($LASTEXITCODE -ne 0) { throw 'offline render verifier dependency install failed' }
    python tools/check_windows_runtime.py
    if ($LASTEXITCODE -ne 0) { throw 'tracked runtime is stale' }
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
    & tests/playback_metrics_tests.ps1 -Exe $Exe -Inspector "$BuildDir/Release/pet_package_inspect.exe" -Output "$BuildDir/playback-results"
    & tests/exploration_windows_tests.ps1 -Exe $Exe -Explorer "$BuildDir/Release/pet_explore.exe" -Output "$BuildDir/exploration-results"
    & tests/navigation_windows_tests.ps1 -Exe $Exe -Tests "$BuildDir/Release/pet_navigation_recovery_tests.exe" -Output "$BuildDir/navigation-results"
    & tests/companion_windows_tests.ps1 -Exe $Exe -Tests "$BuildDir/Release/pet_companion_tests.exe" -Inspector "$BuildDir/Release/pet_package_inspect.exe" -Output "$BuildDir/companion-results"
    & tests/entry_social_scene_windows_tests.ps1 -Exe $Exe -Tests "$BuildDir/Release/pet_entry_social_scene_tests.exe" -Output "$BuildDir/entry-results"
    & tests/movement_windows_tests.ps1 -Exe $Exe -Tests "$BuildDir/Release/pet_movement_tests.exe" -Output "$BuildDir/movement-results"
    & tests/gameplay_windows_tests.ps1 -Exe $Exe -Tests "$BuildDir/Release/pet_gameplay_tests.exe" -Output "$BuildDir/exploration-results/gameplay"
    Get-ChildItem -Directory 'assets/original/runtime' | ForEach-Object {
        & "$BuildDir/Release/pet_package_playback.exe" "$($_.FullName)/asset.mdpa"
        if ($LASTEXITCODE -ne 0) { throw 'original native playback failed' }
        & tests/playback_metrics_tests.ps1 -Exe $Exe -Inspector "$BuildDir/Release/pet_package_inspect.exe" -Package "$($_.FullName)/asset.mdpa" -Output "$BuildDir/playback-results/$($_.Name)"
    }
    cmake --install $BuildDir --config Release --prefix "$BuildDir/package"
    if ($LASTEXITCODE -ne 0) { throw 'package failed' }
    & tests/default_launch_windows_tests.ps1 -Exe "$BuildDir/package/MysteryDungeonPet.exe" -Output "$BuildDir/exploration-results/default-launch"
    & tests/default_launch_windows_tests.ps1 -Exe "MysteryDungeonPet.exe" -Output "$BuildDir/exploration-results/pull-launch"
    foreach ($Report in @("$BuildDir/exploration-results/exploration.json", "$BuildDir/exploration-results/gameplay/life-result.json", "$BuildDir/exploration-results/gameplay/combat-result.json", "$BuildDir/exploration-results/default-launch/default-launch.json", "$BuildDir/exploration-results/pull-launch/default-launch.json")) {
        Write-Output (Get-Content -Raw $Report)
    }
    $bytes = (Get-ChildItem "$BuildDir/package" -Recurse -File | Measure-Object Length -Sum).Sum
    Write-Output "Installed selected package bytes: $bytes (selected runtime only; ROM excluded)"
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
