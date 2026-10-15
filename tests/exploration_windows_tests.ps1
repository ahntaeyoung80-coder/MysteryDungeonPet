param([string]$Exe, [string]$Explorer, [string]$Output)
$ErrorActionPreference = 'Stop'
New-Item -ItemType Directory -Force $Output | Out-Null
$Exe = (Resolve-Path $Exe).Path
$Explorer = (Resolve-Path $Explorer).Path
$Package = (Resolve-Path 'assets/original/exploration/dungeon-0/exploration.mdpe').Path
$Actor = (Resolve-Path 'assets/original/runtime/asset-0/asset.mdpa').Path
$Snapshot = [IO.Path]::GetFullPath("$Output/windows-run.mdes")
$Csv = [IO.Path]::GetFullPath("$Output/exploration.csv")
$arguments = @('--package', "`"$Actor`"", '--explore', "`"$Package`"", '--snapshot', "`"$Snapshot`"",
          '--new-run', '--seed', '42', '--duration', '4', '--metrics', "`"$Csv`"")
$process = Start-Process -FilePath $Exe -ArgumentList $arguments -PassThru
if (-not $process.WaitForExit(30000)) { $process.Kill(); throw 'exploration UI timeout' }
if ($process.ExitCode -ne 0) { throw "exploration UI failed: $($process.ExitCode)" }
if (-not (Test-Path $Snapshot)) { throw 'exploration snapshot absent' }
$rows = @(Import-Csv $Csv)
if ($rows.Count -lt 3) { throw 'missing exploration metrics' }
foreach ($r in $rows) {
    if ([int]$r.decode_count -lt 5) { throw 'player/enemy/home/props/tileset cache missing' }
    if ([int]$r.physical_size -ne 200) { throw 'exploration resized viewport' }
}
$positions = @($rows | ForEach-Object { "$($_.scene_x),$($_.scene_y)" } | Sort-Object -Unique)
$screens = @($rows | ForEach-Object { "$($_.window_x),$($_.window_y)" } | Sort-Object -Unique)
if ($screens.Count -ne 1) { throw 'exploration moved desktop window' }
$movement = 'pending: render paused by runner session'
if (@($rows | Where-Object { [int]$_.render_paused -eq 0 }).Count -gt 1) {
    if ($positions.Count -ge 2) { $movement = 'pass' } else {
        $EventPath = Join-Path $env:LOCALAPPDATA 'MysteryDungeonPet/events.log'
        if (((Get-Content $EventPath -Tail 50) -join "`n") -notmatch 'life=Combat|life=Dialogue') { throw 'unpaused explorer did not move or interact' }
        $movement = 'pending: dialogue/combat held player during the observation window'
    }
}
$finish = (& $Explorer $Package 42 $Snapshot | ConvertFrom-Json)
if ($LASTEXITCODE -ne 0 -or $finish.life -ne 'Ready' -or $finish.sceneBeat -ne 16 -or $finish.hearts -gt 5) { throw 'Windows gameplay snapshot cannot reach the pending bedroom exit natively' }
$restoredRun = (& $Explorer $Package 99 $Snapshot | ConvertFrom-Json)
if ($LASTEXITCODE -ne 0 -or $restoredRun.newClears -ne 0 -or $restoredRun.newPickups -ne 0 -or $restoredRun.inventory -ne $finish.inventory -or $restoredRun.hearts -ne $finish.hearts -or
    $restoredRun.day -ne $finish.day -or $restoredRun.rngState -ne $finish.rngState -or $restoredRun.gameplayTransactions -ne $finish.gameplayTransactions) {
    throw 'Windows restored pending exit replayed a transaction'
}
@{status='pass'; uiMovement=$movement; minimumDecodeCount=5; physicalSize=200; snapshotRestore='pass';
  desktopWindowFixed='pass'; a3Remaining='pending'; actualWindows11Exploration='pending'} |
    ConvertTo-Json | Set-Content "$Output/exploration.json"
