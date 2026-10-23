param(
    [ValidateRange(60,14400)][int]$Seconds = 1800,
    [string]$ExePath = '',
    [string]$OutputDirectory = '',
    [switch]$SkipRelaunch
)
$ErrorActionPreference = 'Stop'
if ($env:OS -ne 'Windows_NT') { throw 'This collector requires native Windows. Wine observations use a separate tool.' }
$QualityRoot = Split-Path -Parent $PSScriptRoot
if (-not $ExePath) { $ExePath = Join-Path $QualityRoot 'MysteryDungeonPet.exe' }
$ExePath = (Resolve-Path -LiteralPath $ExePath).Path
if (Get-Process -Name 'MysteryDungeonPet' -ErrorAction SilentlyContinue) {
    throw 'Close MysteryDungeonPet before starting this check. The collector never stops an existing game.'
}
if (-not $OutputDirectory) {
    $OutputDirectory = Join-Path (Join-Path $env:LOCALAPPDATA 'MysteryDungeonPet') ('quality-results\' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
}
$OutputDirectory = [IO.Path]::GetFullPath($OutputDirectory)
if (Test-Path -LiteralPath $OutputDirectory) { throw 'Use a new output directory to preserve previous results.' }
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
$GameData = Join-Path $env:LOCALAPPDATA 'MysteryDungeonPet'
$EventFile = Join-Path $GameData 'events.log'
$EventOffset = if (Test-Path -LiteralPath $EventFile) { (Get-Item -LiteralPath $EventFile).Length } else { 0 }
$WindowState = Join-Path $GameData 'window.state'
$WindowBackup = Join-Path $OutputDirectory 'window-state-before.bin'
$HadWindowState = Test-Path -LiteralPath $WindowState
if ($HadWindowState) { Copy-Item -LiteralPath $WindowState -Destination $WindowBackup }
$Snapshot = Join-Path $OutputDirectory 'quality-state.mdes'
$Result = [ordered]@{
    schemaVersion = 1
    nativeWindows = $true
    started = (Get-Date).ToString('o')
    osVersion = [Environment]::OSVersion.Version.ToString()
    osCaption = ''
    logicalCores = [Environment]::ProcessorCount
    powershellVersion = $PSVersionTable.PSVersion.ToString()
    exeSha256 = (Get-FileHash -LiteralPath $ExePath -Algorithm SHA256).Hash.ToLowerInvariant()
    requestedSeconds = $Seconds
    detailedFrameTrace = $false
    snapshotIsolated = $true
    runs = @()
    status = 'running'
    error = ''
    manualChecks = [ordered]@{ drag = 'unverified'; dpi = 'unverified'; lock = 'unverified'; displayOff = 'unverified'; suspendResume = 'unverified' }
}
try { $Result.osCaption = (Get-CimInstance Win32_OperatingSystem).Caption } catch { $Result.osCaption = 'unavailable' }
$QualityProcess = $null
function Invoke-QualityRun([string]$Name, [int]$RunSeconds) {
    $RunDirectory = Join-Path $OutputDirectory $Name
    New-Item -ItemType Directory -Path $RunDirectory | Out-Null
    $MetricsFile = Join-Path $RunDirectory 'metrics.csv'
    $ReportFile = Join-Path $RunDirectory 'report.txt'
    $LaunchArguments = @('--duration', [string]$RunSeconds, '--snapshot', ('"' + $Snapshot + '"'), '--metrics', ('"' + $MetricsFile + '"'), '--report', ('"' + $ReportFile + '"'))
    $script:QualityProcess = Start-Process -FilePath $ExePath -ArgumentList $LaunchArguments -WorkingDirectory $QualityRoot -PassThru
    $Samples = New-Object 'System.Collections.Generic.List[object]'
    $Clock = [Diagnostics.Stopwatch]::StartNew()
    $FailedReport = $false
    while (-not $QualityProcess.HasExited) {
        Start-Sleep -Seconds 2
        if ($QualityProcess.HasExited) { break }
        $QualityProcess.Refresh()
        $Samples.Add([pscustomobject]@{
            elapsedSeconds = $Clock.Elapsed.TotalSeconds
            cpuTotalSeconds = $QualityProcess.TotalProcessorTime.TotalSeconds
            workingSetBytes = $QualityProcess.WorkingSet64
            privateBytes = $QualityProcess.PrivateMemorySize64
            handles = $QualityProcess.HandleCount
        })
        if ((Test-Path -LiteralPath $ReportFile) -and ((Get-Content -LiteralPath $ReportFile -Raw) -match 'status=fail')) {
            $FailedReport = $true
            Stop-Process -Id $QualityProcess.Id
            break
        }
        if ($Clock.Elapsed.TotalSeconds -gt ($RunSeconds + 120)) {
            Stop-Process -Id $QualityProcess.Id
            throw ('Test process exceeded watchdog: ' + $Name)
        }
    }
    $QualityProcess.WaitForExit()
    $ExitCode = $QualityProcess.ExitCode
    $Samples | Export-Csv -LiteralPath (Join-Path $RunDirectory 'external-metrics.csv') -NoTypeInformation -Encoding UTF8
    if (Test-Path -LiteralPath $Snapshot) { Copy-Item -LiteralPath $Snapshot -Destination (Join-Path $RunDirectory 'state-after.mdes') }
    $RunResult = [ordered]@{ name = $Name; seconds = $Clock.Elapsed.TotalSeconds; requestedSeconds = $RunSeconds; exitCode = $ExitCode; failedReport = $FailedReport; externalSamples = $Samples.Count }
    $Result.runs += $RunResult
    if ($ExitCode -ne 0 -or $FailedReport) { throw ('Game failed during ' + $Name + '. See the report and events log in the result folder.') }
    if (-not (Test-Path -LiteralPath $MetricsFile)) { throw ('Missing application metrics: ' + $Name) }
    $MetricsRows = @(Import-Csv -LiteralPath $MetricsFile)
    if ($MetricsRows.Count -lt 2) { throw ('Insufficient application metrics: ' + $Name) }
    $MeasuredSeconds = [double]::Parse($MetricsRows[-1].elapsed_seconds, [Globalization.CultureInfo]::InvariantCulture)
    if ($MeasuredSeconds -lt ($RunSeconds - 3)) { throw ('Game ended before requested duration: ' + $Name) }
    $script:QualityProcess = $null
}
try {
    Write-Host '실사용 품질 점검을 시작합니다. 기존 모험 저장은 변경하지 않습니다.'
    Write-Host ('예상 관측 시간: ' + [math]::Ceiling($Seconds / 60) + '분. 종료 후 결과 ZIP을 생성합니다.')
    Invoke-QualityRun 'normal' $Seconds
    if (-not $SkipRelaunch) { Invoke-QualityRun 'relaunch' 15 }
    $Result.status = 'collected'
} catch {
    $Result.status = 'fail'
    $Result.error = $_.Exception.Message
    Write-Warning $Result.error
} finally {
    if ($QualityProcess -and -not $QualityProcess.HasExited) { Stop-Process -Id $QualityProcess.Id -ErrorAction SilentlyContinue }
    if (Test-Path -LiteralPath $EventFile) {
        $EventStream = [IO.File]::Open($EventFile, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::ReadWrite)
        try {
            if ($EventStream.Length -lt $EventOffset) { $EventOffset = 0 }
            [void]$EventStream.Seek($EventOffset, [IO.SeekOrigin]::Begin)
            $DestinationStream = [IO.File]::Create((Join-Path $OutputDirectory 'events.log'))
            try { $EventStream.CopyTo($DestinationStream) } finally { $DestinationStream.Dispose() }
        } finally { $EventStream.Dispose() }
    }
    if ($HadWindowState) { Copy-Item -LiteralPath $WindowBackup -Destination $WindowState -Force }
    elseif (Test-Path -LiteralPath $WindowState) { Remove-Item -LiteralPath $WindowState }
    $Result.finished = (Get-Date).ToString('o')
    $Result | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $OutputDirectory 'collection.json') -Encoding UTF8
    'collected는 자료 수집 완료이며 잠금/절전/DPI 수동 검증 합격을 뜻하지 않습니다.' | Set-Content -LiteralPath (Join-Path $OutputDirectory 'README.txt') -Encoding UTF8
    $Archive = $OutputDirectory + '.zip'
    Compress-Archive -Path (Join-Path $OutputDirectory '*') -DestinationPath $Archive -Force
    Write-Host ('결과 ZIP: ' + $Archive)
}
if ($Result.status -eq 'fail') { exit 1 }
