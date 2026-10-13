param([string]$Exe, [string]$Inspector, [string]$Output)
$ErrorActionPreference = 'Stop'
New-Item -ItemType Directory -Force $Output | Out-Null
$Exe = (Resolve-Path $Exe).Path
$Inspector = (Resolve-Path $Inspector).Path
$Package = (Resolve-Path 'assets/fixture/fixture.mdpa').Path
# CI에서 창/로그 계약만 검증한다. 실제 잠금·절전·원작 통과로 확대하지 않는다.
$cases = @(
    @{ name='empty'; scenario='empty'; clip=0; direction=0 },
    @{ name='walk'; scenario='walk'; clip=0; direction=5 },
    @{ name='alias'; scenario='manual'; clip=107; direction=4 },
    @{ name='eat'; scenario='manual'; clip=15; direction=7 },
    @{ name='wake'; scenario='manual'; clip=14; direction=2 }
)
foreach ($case in $cases) {
    $csv = [IO.Path]::GetFullPath("$Output/$($case.name).csv")
    $arguments = @('--package', "`"$Package`"", '--scenario', $case.scenario,
                   '--clip', "$($case.clip)", '--direction', "$($case.direction)",
                   '--scale', '3', '--duration', '3', '--metrics', "`"$csv`"")
    $process = Start-Process -FilePath $Exe -ArgumentList $arguments -PassThru
    if (-not $process.WaitForExit(30000)) { $process.Kill(); throw "$($case.name) timeout" }
    if ($process.ExitCode -ne 0) { throw "$($case.name) exit code $($process.ExitCode)" }
    python tools/check_playback_metrics.py $csv --package $Package --inspector $Inspector --scenario $case.scenario --output "$Output/$($case.name).json"
    if ($LASTEXITCODE -ne 0) { throw "$($case.name) metadata/window metrics check failed" }
}
