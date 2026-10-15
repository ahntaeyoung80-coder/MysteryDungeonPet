param([string]$Exe,[string]$Tests,[string]$Output)
$ErrorActionPreference='Stop'
$Exe=(Resolve-Path $Exe).Path
$Tests=(Resolve-Path $Tests).Path
$Package=(Resolve-Path 'assets/original/exploration/dungeon-0/exploration.mdpe').Path
New-Item -ItemType Directory -Force $Output | Out-Null
$Output=[IO.Path]::GetFullPath($Output)
& $Tests --fixtures $Package $Output
if($LASTEXITCODE -ne 0){throw 'navigation fixtures failed'}
Add-Type @'
using System;
using System.Text;
using System.Runtime.InteropServices;
public static class RecoveryWindow {
 [DllImport("user32.dll",CharSet=CharSet.Unicode)] public static extern IntPtr FindWindow(string cls,string title);
 [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr hwnd,uint msg,IntPtr w,IntPtr l);
 [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr hwnd,uint msg,IntPtr w,IntPtr l);
 [DllImport("user32.dll")] public static extern int GetMenuItemCount(IntPtr menu);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)] public static extern int GetMenuString(IntPtr menu,uint item,StringBuilder text,int count,uint flags);
}
'@
function Inspect([string]$Path){
 $Result=& $Tests --inspect $Package $Path | ConvertFrom-Json
 if($LASTEXITCODE -ne 0){throw 'navigation snapshot inspection failed'}
 return $Result
}
function Launch([string]$Name,[int]$Seconds){
 $ArgsForApp=@('--explore',"`"$Package`"",'--snapshot',"`"$Output/$Name.mdes`"",'--duration',$Seconds,
  '--metrics',"`"$Output/$Name-metrics.csv`"",'--movement-trace',"`"$Output/$Name.csv`"")
 return Start-Process $Exe -ArgumentList $ArgsForApp -PassThru
}
function Finish($Process){
 if(-not $Process.WaitForExit(30000)){$Process.Kill();throw 'navigation app timeout'}
 if($Process.ExitCode -ne 0){throw 'navigation app failed'}
}
$EventPath=Join-Path $env:LOCALAPPDATA 'MysteryDungeonPet/events.log'
$Before=if(Test-Path $EventPath){@(Get-Content $EventPath).Count}else{0}
Finish (Launch 'cycle' 8)
$Rows=@(Import-Csv "$Output/cycle.csv")
if($Rows.Count -eq 0){throw 'cycle GUI not observed'}
$Actions=@($Rows | Where-Object {$_.life -eq '0' -and $_.action_id -ne '0'} | Group-Object action_id | ForEach-Object {$_.Group[0]})
if($Actions.Count -lt 3){throw 'cycle movement not observed'}
if(@($Actions | Where-Object {[int]$_.to_tile -notin @(102,125)}).Count -eq 0 -and
   @($Rows | Where-Object {$_.life -ne '0' -or $_.scene_beat -ne '0'}).Count -eq 0){throw 'diagonal chase/retreat cycle persists'}
Copy-Item "$Output/watch.mdes" "$Output/watch-before.mdes"
$WatchBefore=Inspect "$Output/watch-before.mdes"
Finish (Launch 'watch' 1)
$Detour=Inspect "$Output/watch.mdes"
$DetourRows=@(Import-Csv "$Output/watch.csv")
$FirstDetour=@($DetourRows | Where-Object {$_.life -eq '0' -and $_.action_id -ne '0'}) | Select-Object -First 1
if(-not $FirstDetour -or [int]$FirstDetour.from_tile -ne 77 -or [int]$FirstDetour.to_tile -in @(77,102)) {throw 'persisted cycle did not choose a different adjacent direction'}
if($Detour.autoCount -ne 0 -or $Detour.returnCount -ne 0 -or $Detour.completed -or
 $Detour.runId -ne $WatchBefore.runId -or $Detour.floor -ne $WatchBefore.floor -or
 $Detour.inventory -ne $WatchBefore.inventory -or (Test-Path "$Output/watch.mdes.reset-backup")){throw 'detour left the dungeon or reset committed progress'}
Copy-Item "$Output/watch.csv" "$Output/watch-first-run.csv"
Finish (Launch 'watch' 1)
$DetourRestored=Inspect "$Output/watch.mdes"
if($DetourRestored.returnCount -ne 0 -or $DetourRestored.autoCount -ne 0 -or
 $DetourRestored.runId -ne $WatchBefore.runId -or $DetourRestored.floor -ne $WatchBefore.floor){throw 'restored detour returned or changed the run/floor'}
$Process=$null
try{
 $Process=Launch 'manual' 15
 if(-not $Process.WaitForInputIdle(10000)){throw 'reset app did not become ready'}
 $Window=[RecoveryWindow]::FindWindow('MysteryDungeonPet.A2','MysteryDungeonPet')
 if($Window -eq [IntPtr]::Zero){throw 'reset product window absent'}
 [RecoveryWindow]::PostMessage($Window,0x205,[IntPtr]::Zero,[IntPtr]::Zero)|Out-Null
 $Popup=[IntPtr]::Zero
 for($Attempt=0;$Attempt -lt 30 -and $Popup -eq [IntPtr]::Zero;$Attempt++){
  Start-Sleep -Milliseconds 50
  $Popup=[RecoveryWindow]::FindWindow('#32768',$null)
 }
 if($Popup -eq [IntPtr]::Zero){throw 'reset context menu absent'}
 $Menu=[RecoveryWindow]::SendMessage($Popup,0x1e1,[IntPtr]::Zero,[IntPtr]::Zero)
 $Labels=@()
 for($Index=0;$Index -lt [RecoveryWindow]::GetMenuItemCount($Menu);$Index++){
  $Text=[Text.StringBuilder]::new(128)
  [RecoveryWindow]::GetMenuString($Menu,$Index,$Text,128,0x400)|Out-Null
  $Labels+=$Text.ToString()
 }
 $Labels|Set-Content "$Output/reset-menu.txt"
 if($Labels -notcontains 'Reset 탐험 (귀가)'){throw 'reset command missing from product menu'}
 [RecoveryWindow]::PostMessage($Window,0x1f,[IntPtr]::Zero,[IntPtr]::Zero)|Out-Null
 [RecoveryWindow]::PostMessage($Window,0x111,[IntPtr]625,[IntPtr]::Zero)|Out-Null
 [RecoveryWindow]::PostMessage($Window,0x111,[IntPtr]625,[IntPtr]::Zero)|Out-Null
 Start-Sleep -Milliseconds 150
 [RecoveryWindow]::PostMessage($Window,0x10,[IntPtr]::Zero,[IntPtr]::Zero)|Out-Null
 Finish $Process;$Process=$null
 $Manual=Inspect "$Output/manual.mdes"
 $Backup=Inspect "$Output/manual.mdes.reset-backup"
 if($Manual.manualCount -ne 1 -or $Manual.returnCount -ne 1 -or $Manual.movement -ne 0 -or $Manual.completed -or
  $Manual.runId -ne $Backup.runId -or $Manual.inventory -ne $Backup.inventory -or $Manual.hearts -ne $Backup.hearts){throw 'reset lost progress or repeated the return'}
 Finish (Launch 'manual' 4)
 $Restored=Inspect "$Output/manual.mdes"
 if($Restored.returnCount -ne 1 -or $Restored.manualCount -ne 1 -or $Restored.meals -ne 1 -or $Restored.day -ne 1){throw 'reset relaunch repeated return/meal or skipped lifecycle'}
 @($WatchBefore,$Detour,$DetourRestored,$Backup,$Manual,$Restored)|ConvertTo-Json -Depth 5|Set-Content "$Output/reset-states.json"
}finally{if($Process -and -not $Process.HasExited){$Process.Kill();$Process.WaitForExit()}}
$Events=@(Get-Content $EventPath|Select-Object -Skip $Before)
$Events|Set-Content "$Output/events.txt"
if(($Events -join "`n") -notmatch 'navigation detour reason=repeated-joint-state' -or ($Events -join "`n") -match 'navigation reset reason=repeated-joint-state' -or ($Events -join "`n") -notmatch 'navigation reset reason=manual'){throw 'detour/manual reset reason logs invalid'}
@{status='pass';cycle='original coupled chase/retreat escaped';automatic='persisted repeat chose another adjacent direction, stayed in same run/floor through relaunch, no automatic return';manual='context menu, backup, duplicate commands, durable return, relaunch and one meal';actualWindows11='not observed'}|
 ConvertTo-Json|Set-Content "$Output/summary.json"
Get-Content -Raw "$Output/summary.json"
