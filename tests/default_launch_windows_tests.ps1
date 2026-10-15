param([string]$Exe, [string]$Output)
$ErrorActionPreference = 'Stop'
$Exe = (Resolve-Path $Exe).Path
$Output = [IO.Path]::GetFullPath($Output)
New-Item -ItemType Directory -Force $Output | Out-Null
$Data = Join-Path $env:LOCALAPPDATA 'MysteryDungeonPet'
New-Item -ItemType Directory -Force $Data | Out-Null
$EventPath = Join-Path $Data 'events.log'
$Snapshot = Join-Path $Data 'exploration.mdes'
$Metrics = Join-Path $Data 'metrics.csv'
Add-Type @'
using System;
using System.Text;
using System.Runtime.InteropServices;
public static class LaunchWindow {
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr FindWindow(string cls, string title);
    [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr hwnd, uint msg, IntPtr w, IntPtr l);
    [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr hwnd, uint msg, IntPtr w, IntPtr l);
    [DllImport("user32.dll")] public static extern int GetMenuItemCount(IntPtr menu);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetMenuString(IntPtr menu, uint item, StringBuilder text, int count, uint flags);
}
'@
$Backups = @{}
$Process = $null
try {
    # Isolate the exact no-argument installed launch without erasing an existing save.
    foreach ($Name in @('exploration.mdes', 'exploration.mdes.tmp', 'metrics.csv', 'window.state')) {
        $Path = Join-Path $Data $Name
        if (Test-Path $Path) {
            $Backup = Join-Path $Output ("saved-" + $Name)
            Move-Item $Path $Backup
            $Backups[$Path] = $Backup
        }
    }
    $Status = 'pass'
    $RunIds = @()
    for ($Run = 0; $Run -lt 2; $Run++) {
        $Before = if (Test-Path $EventPath) { @(Get-Content $EventPath).Count } else { 0 }
        # No --explore, --package, --scenario, or any other arguments. CWD is not the EXE directory.
        $Process = Start-Process -FilePath $Exe -WorkingDirectory $Output -PassThru
        $Window = [IntPtr]::Zero
        for ($Attempt = 0; $Attempt -lt 100 -and $Window -eq [IntPtr]::Zero; $Attempt++) {
            Start-Sleep -Milliseconds 100
            $Window = [LaunchWindow]::FindWindow('MysteryDungeonPet.A2', 'MysteryDungeonPet')
            if ($Process.HasExited) { throw "default launch exited early: $($Process.ExitCode)" }
        }
        if ($Window -eq [IntPtr]::Zero) { throw 'default product window absent (possibly diagnostics or asset error)' }
        Start-Sleep -Seconds 4
        if ($Run -eq 0) {
            [LaunchWindow]::PostMessage($Window, 0x205, [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null # WM_RBUTTONUP
            $Popup = [IntPtr]::Zero
            for ($Attempt = 0; $Attempt -lt 30 -and $Popup -eq [IntPtr]::Zero; $Attempt++) {
                Start-Sleep -Milliseconds 100
                $Popup = [LaunchWindow]::FindWindow('#32768', $null)
            }
            if ($Popup -eq [IntPtr]::Zero) { throw 'default product menu absent' }
            $Menu = [LaunchWindow]::SendMessage($Popup, 0x1e1, [IntPtr]::Zero, [IntPtr]::Zero) # MN_GETHMENU
            $Count = [LaunchWindow]::GetMenuItemCount($Menu)
            if ($Count -le 0) { throw 'cannot read product menu' }
            $Labels = @()
            for ($Item = 0; $Item -lt $Count; $Item++) {
                $Text = [Text.StringBuilder]::new(128)
                [LaunchWindow]::GetMenuString($Menu, $Item, $Text, 128, 0x400) | Out-Null
                $Labels += $Text.ToString()
            }
            $Labels | Set-Content "$Output/menu.txt"
            if ($Labels -contains 'Animation' -or $Labels -contains 'Direction' -or $Labels -contains 'Loop' -or
                $Labels -contains 'Finite' -or $Labels -contains 'Stop (explicit Sleep end)' -or
                $Labels -contains 'Test background' -or $Labels -contains 'Reload package') {
                throw 'animation diagnostics leaked into default product menu'
            }
            if ($Labels -notcontains 'Exit') { throw 'product exit menu absent' }
            [LaunchWindow]::PostMessage($Window, 0x1f, [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null # WM_CANCELMODE
        }
        [LaunchWindow]::PostMessage($Window, 0x10, [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null # WM_CLOSE
        if (-not $Process.WaitForExit(15000)) { throw 'default launch close timeout' }
        if ($Process.ExitCode -ne 0) { throw "default launch failed: exit code $($Process.ExitCode), run $Run" }
        if (-not (Test-Path $Snapshot)) { throw 'default launch did not save exploration' }
        $Events = @(Get-Content $EventPath | Select-Object -Skip $Before)
        $Events | Set-Content "$Output/launch-$Run-events.txt"
        Copy-Item $Metrics "$Output/launch-$Run.csv"
        Copy-Item $Snapshot "$Output/launch-$Run.mdes"
        $Joined = $Events -join "`n"
        if ($Joined -notmatch 'mode=automatic-exploration') { throw 'default launch did not own Explorer' }
        $Expected = if ($Run -eq 0) { 'new' } else { 'restored' }
        if ($Joined -notmatch "exploration snapshot=$Expected day=1 runId=(\d+)") { throw 'default snapshot start/restore failed' }
        $RunIds += $Matches[1]
        $Rows = @(Import-Csv $Metrics)
        if ($Rows.Count -lt 3) { throw 'default launch metrics absent' }
        if (@($Rows | Where-Object { [int]$_.decode_count -lt 4 -or [int]$_.physical_size -ne 200 }).Count) {
            throw 'default launch failed to load dungeon/gameplay assets or changed viewport'
        }
        $Positions = @($Rows | ForEach-Object { "$($_.scene_x),$($_.scene_y)" } | Sort-Object -Unique)
        if (@($Rows | Where-Object { [int]$_.render_paused -eq 0 }).Count -gt 1) {
            if ($Positions.Count -lt 2 -and $Joined -notmatch 'gameplay onceKey=|life=Combat|life=Dialogue') { throw 'default explorer did not advance' }
        } else { $Status = 'pending: runner session paused rendering' }
        $Process = $null
    }
    if ($RunIds[0] -ne $RunIds[1]) { throw 'no-argument relaunch replaced the saved run' }
    $SavedHash = (Get-FileHash $Snapshot).Hash
    $Before = @(Get-Content $EventPath).Count
    $DiagnosticCsv = "$Output/diagnostics.csv"
    $Process = Start-Process -FilePath $Exe -WorkingDirectory $Output -PassThru -ArgumentList @(
        '--diagnostics', '--duration', '2', '--metrics', "`"$DiagnosticCsv`"")
    if (-not $Process.WaitForExit(15000) -or $Process.ExitCode -ne 0) { throw 'explicit diagnostics failed' }
    $DiagnosticEvents = @(Get-Content $EventPath | Select-Object -Skip $Before)
    $DiagnosticEvents | Set-Content "$Output/diagnostics-events.txt"
    if (($DiagnosticEvents -join "`n") -notmatch 'mode=animation-diagnostics') { throw 'explicit diagnostics started Explorer' }
    if ((Get-FileHash $Snapshot).Hash -ne $SavedHash) { throw 'diagnostics changed exploration save' }
    if (@(Import-Csv $DiagnosticCsv | Where-Object { [int]$_.decode_count -ne 1 }).Count) { throw 'diagnostics loaded gameplay assets' }
    $Process = $null
    @{status=$Status; launch='installed EXE with zero arguments from another directory';
      snapshot='new then restored same runId'; diagnosticsMenu='absent'; explicitDiagnostics='pass, exploration save unchanged';
      physicalSize=200; actualWindows11='pending'} |
        ConvertTo-Json | Set-Content "$Output/default-launch.json"
} catch {
    # Preserve the actual failure before the isolated save/metrics are restored.
    if ($Process -and -not $Process.HasExited) { $Process.Kill(); $Process.WaitForExit() }
    $_ | Out-String | Set-Content "$Output/failure.txt"
    if (Test-Path $EventPath) {
        $FailureEvents = @(Get-Content $EventPath | Select-Object -Skip $Before)
        $FailureEvents | Set-Content "$Output/failure-events.txt"
        $FailureEvents | Select-Object -Last 60 | Write-Host
    }
    foreach ($Path in @($Snapshot, $Metrics)) {
        if (Test-Path $Path) { Copy-Item $Path (Join-Path $Output ("failure-" + [IO.Path]::GetFileName($Path))) }
    }
    throw
} finally {
    if ($Process -and -not $Process.HasExited) { $Process.Kill(); $Process.WaitForExit() }
    foreach ($Name in @('exploration.mdes', 'exploration.mdes.tmp', 'metrics.csv', 'window.state')) {
        $Path = Join-Path $Data $Name
        if (Test-Path $Path) { Remove-Item $Path }
        if ($Backups.ContainsKey($Path)) { Move-Item $Backups[$Path] $Path }
    }
}
