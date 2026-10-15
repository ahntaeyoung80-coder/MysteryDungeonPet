param([string]$Exe, [string]$Tests, [string]$Output)
$ErrorActionPreference = 'Stop'
$Exe = (Resolve-Path $Exe).Path
$Tests = (Resolve-Path $Tests).Path
$Package = (Resolve-Path 'assets/original/exploration/dungeon-0/exploration.mdpe').Path
New-Item -ItemType Directory -Force $Output | Out-Null
$Output = [IO.Path]::GetFullPath($Output)
$Fixtures = Join-Path $Output 'fixtures'
$Results = @()
function New-Fixtures {
    & $Tests --fixtures $Package $Fixtures
    if ($LASTEXITCODE -ne 0) { throw 'movement fixture setup failed' }
}
function Run-Frames([string]$Name, [string]$Kind, [string]$Snapshot, [int]$Scale, [int]$Seconds) {
    $Trace = Join-Path $Output "$Name.csv"
    $Metrics = Join-Path $Output "$Name-metrics.csv"
    $Arguments = @('--solo','--explore', "`"$Package`"", '--snapshot', "`"$Snapshot`"", '--scale', $Scale,
        '--duration', $Seconds, '--metrics', "`"$Metrics`"", '--movement-trace', "`"$Trace`"")
    $Process = Start-Process -FilePath $Exe -ArgumentList $Arguments -PassThru
    if (-not $Process.WaitForExit(($Seconds+20)*1000)) { $Process.Kill(); throw "$Name timeout" }
    if ($Process.ExitCode -ne 0) { throw "$Name failed: $($Process.ExitCode)" }
    $Rows = @(Import-Csv $Metrics)
    if ($Rows.Count -lt 2) { throw "$Name missing session metrics" }
    if (@($Rows | Where-Object { [int]$_.render_paused -eq 0 }).Count -eq 0) {
        return @{name=$Name; status='pending: runner session paused'; nativeWindows11='not observed'}
    }
    foreach ($Row in $Rows) {
        if ([int]$Row.physical_size -ne 200) { throw "$Name resized viewport" }
    }
    $Windows = @($Rows | ForEach-Object { "$($_.window_x),$($_.window_y)" } | Sort-Object -Unique)
    if ($Windows.Count -ne 1) { throw "$Name moved desktop window" }
    python tools/check_movement_trace.py $Trace --kind $Kind --scale $Scale --output "$Output/$Name.json" | Out-Host
    if ($LASTEXITCODE -ne 0) { throw "$Name frame contract failed" }
    foreach ($Suffix in @('first','middle','last')) {
        if (-not (Test-Path "$Output/$Name-$Suffix.bmp")) { throw "$Name missing composed $Suffix image" }
    }
    $Result = Get-Content -Raw "$Output/$Name.json" | ConvertFrom-Json -AsHashtable
    $Result.name=$Name
    return $Result
}
foreach ($Scale in @(1,2,3)) {
    foreach ($Kind in @('straight','turn')) {
        New-Fixtures
        # The deterministic FIFO path turns after step 13 (5.2s); stop before
        # reaching the stairs at step 17 (6.8s), so fades cannot hide movement.
        $Seconds = if ($Kind -eq 'turn') { 6 } else { 4 }
        $Results += Run-Frames "$Kind-$Scale" $Kind "$Fixtures/$Kind.mdes" $Scale $Seconds
    }
    foreach ($Edge in @('top-left','top','top-right','left','right','bottom-left','bottom','bottom-right')) {
        New-Fixtures
        $Results += Run-Frames "$Edge-$Scale" 'edge' "$Fixtures/$Edge.mdes" $Scale 2
    }
}
# Interrupt a normal movement, then check the same snapshot through the regular app.
New-Fixtures
$Snapshot = "$Fixtures/straight.mdes"
$KillArgs = @('--solo','--explore', "`"$Package`"", '--snapshot', "`"$Snapshot`"", '--scale', '1',
    '--duration', '10', '--metrics', "`"$Output/interrupted-metrics.csv`"")
$Process = Start-Process -FilePath $Exe -ArgumentList $KillArgs -PassThru
if (-not $Process.WaitForInputIdle(10000)) { $Process.Kill(); throw 'interruption app never became ready' }
Start-Sleep -Milliseconds 150
$Process.Kill()
$Process.WaitForExit()
$Before = & $Tests --inspect $Package $Snapshot | ConvertFrom-Json
if ($LASTEXITCODE -ne 0) { throw 'interrupted snapshot invalid' }
$Before | ConvertTo-Json | Set-Content "$Output/interrupted-before.json"
if ($Before.actionId -eq 0) {
    $Results += @{name='restore'; status='pending: interrupted runner did not start movement'}
} else {
    $Results += Run-Frames 'restore' 'straight' $Snapshot 1 3
    $After = & $Tests --inspect $Package $Snapshot | ConvertFrom-Json
    $After | ConvertTo-Json | Set-Content "$Output/interrupted-after.json"
    if ($LASTEXITCODE -ne 0 -or $After.runId -ne $Before.runId -or $After.inventory -ne $Before.inventory -or
        $After.onceCount -ne $Before.onceCount -or $After.gameplayOnceCount -ne $Before.gameplayOnceCount -or
        $After.day -ne $Before.day -or $After.hearts -ne $Before.hearts) { throw 'movement restore changed transaction state' }
    $Trace = @(Import-Csv "$Output/restore.csv")
    if ($Trace.Count -gt 0) {
        $First = $Trace[0]
        if ([long]$First.action_id -ne $Before.actionId -or [int]$First.from_tile -ne $Before.from -or [int]$First.to_tile -ne $Before.to) { throw 'restore replaced pending movement' }
        $Elapsed = [math]::Max(0,[long]$First.utc_ms-[long]$First.started_ms)
        $Fraction = [math]::Min(1.0,$Elapsed/400.0)
        $Expected = ($Before.from % 24)*24+12+24*$Fraction
        if ([math]::Abs([double]$First.world_x-$Expected) -gt 2) { throw 'restore restarted or skipped pending movement' }
    }
}
$Status = if (@($Results | Where-Object { $_.status -ne 'pass' }).Count) { 'pending' } else { 'pass' }
@{status=$Status; cases=$Results; actualUserWindows11='not observed'; osEvents='out of scope'; longTermPerformance='out of scope'} |
    ConvertTo-Json -Depth 8 | Set-Content "$Output/summary.json"
Get-Content -Raw "$Output/summary.json"
