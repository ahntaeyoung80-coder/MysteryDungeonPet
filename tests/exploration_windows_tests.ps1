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
    if ([int]$r.decode_count -ne 2) { throw 'actor/tileset decode cache changed' }
    if ([int]$r.physical_size -ne 200) { throw 'exploration resized viewport' }
}
$positions = @($rows | ForEach-Object { "$($_.scene_x),$($_.scene_y)" } | Sort-Object -Unique)
$screens = @($rows | ForEach-Object { "$($_.window_x),$($_.window_y)" } | Sort-Object -Unique)
if ($screens.Count -ne 1) { throw 'exploration moved desktop window' }
$movement = 'pending: render paused by runner session'
if (@($rows | Where-Object { [int]$_.render_paused -eq 0 }).Count -gt 1) {
    if ($positions.Count -lt 2) { throw 'unpaused explorer did not move' }
    $movement = 'pass'
}
$finish = (& $Explorer $Package 42 $Snapshot | ConvertFrom-Json)
if ($LASTEXITCODE -ne 0 -or -not $finish.completed) { throw 'Windows snapshot cannot complete natively' }
$home = (& $Explorer $Package 99 $Snapshot | ConvertFrom-Json)
if ($LASTEXITCODE -ne 0 -or $home.newClears -ne 0 -or $home.newPickups -ne 0 -or $home.inventory -ne $finish.inventory) {
    throw 'Windows restored home replayed transaction'
}
@{status='pass'; uiMovement=$movement; decodeCount=2; physicalSize=200; snapshotRestore='pass';
  desktopWindowFixed='pass'; a3Remaining='pending'; actualWindows11Exploration='pending'} |
    ConvertTo-Json | Set-Content "$Output/exploration.json"
