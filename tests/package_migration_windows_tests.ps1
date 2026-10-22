param([string]$Exe,[string]$Tests,[string]$Inspector,[string]$Output)
$ErrorActionPreference='Stop'
$Exe=(Resolve-Path $Exe).Path
$Tests=(Resolve-Path $Tests).Path
$Inspector=(Resolve-Path $Inspector).Path
$OldPackage=(Resolve-Path 'assets/original/exploration/dungeon-0/exploration.mdpe').Path
$NewPackage=(Resolve-Path 'assets/original/exploration/dungeon-14/exploration.mdpe').Path
$Output=[IO.Path]::GetFullPath($Output)
$Fixtures=Join-Path $Output 'fixtures'
New-Item -ItemType Directory -Force $Output|Out-Null
& $Tests --fixtures $OldPackage $NewPackage $Fixtures
if($LASTEXITCODE -ne 0){throw 'legacy package fixtures failed'}
function Inspect([string]$Package,[string]$Snapshot) {
 $State=& $Inspector --inspect $Package $Snapshot|ConvertFrom-Json
 if($LASTEXITCODE -ne 0){throw 'package snapshot cannot restore'}
 return $State
}
function Launch([string]$Name,[int]$Seconds) {
 $Snapshot="$Fixtures/$Name.mdes"
 # Only snapshot/duration diagnostics: default package selection stays active.
 $Arguments=@('--snapshot',"`"$Snapshot`"",'--duration',$Seconds,
  '--metrics',"`"$Output/$Name-metrics.csv`"",'--movement-trace',"`"$Output/$Name.csv`"")
 $Process=Start-Process $Exe -ArgumentList $Arguments -PassThru
 if(-not $Process.WaitForExit(($Seconds+20)*1000)){
  $Process.Kill();$Process.WaitForExit();throw "$Name timed out"
 }
 if($Process.ExitCode -ne 0){throw "$Name failed with code $($Process.ExitCode)"}
}
try {
 $Before=Inspect $OldPackage "$Fixtures/legacy-explore.mdes"
 $Before|ConvertTo-Json -Depth 4|Set-Content "$Output/legacy-explore-before.json"
 Launch 'legacy-explore' 2
 $After=Inspect $OldPackage "$Fixtures/legacy-explore.mdes"
 $After|ConvertTo-Json -Depth 4|Set-Content "$Output/legacy-explore-after.json"
 if($After.runId -ne $Before.runId -or $After.day -ne $Before.day -or $After.floorCount -ne 3 -or
    $After.player -ne $Before.player -or $After.rngState -ne $Before.rngState -or $After.oran -ne $Before.oran){
  throw 'default launch rewrote the active three-floor run'
 }
 foreach($Key in $Before.inventoryKeys){if($After.inventoryKeys -notcontains $Key){throw 'legacy active inventory lost'}}
 $Before=Inspect $OldPackage "$Fixtures/legacy-ready.mdes"
 $Before|ConvertTo-Json -Depth 4|Set-Content "$Output/legacy-ready-before.json"
 Launch 'legacy-ready' 12
 $After=Inspect $NewPackage "$Fixtures/legacy-ready.mdes"
 $After|ConvertTo-Json -Depth 4|Set-Content "$Output/legacy-ready-after.json"
 if($After.runId -ne ($Before.runId+1) -or $After.day -ne $Before.day -or $After.floorCount -ne 16 -or
    $After.dungeonId -ne 14 -or $After.meals -ne $Before.meals){
  throw 'guild startup did not enter the new sixteen-floor expedition exactly once without adding a day'
 }
 foreach($Key in $Before.inventoryKeys){if($After.inventoryKeys -notcontains $Key){throw 'legacy morning inventory lost'}}
 @{status='pass';activeLegacy='progress retained during guild alarm; old run abandoned on departure';
   newRun='new run, same day; sixteen original expedition floors; meal/inventory retained';
   actualWindows11='pending'}|ConvertTo-Json|Set-Content "$Output/summary.json"
 Get-Content -Raw "$Output/summary.json"
} catch {
 $Events=Join-Path $env:LOCALAPPDATA 'MysteryDungeonPet/events.log'
 if(Test-Path $Events){Get-Content $Events -Tail 50|Tee-Object "$Output/failure-events.txt"|Write-Host}
 throw
}
