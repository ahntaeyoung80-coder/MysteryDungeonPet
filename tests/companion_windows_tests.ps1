param([string]$Exe,[string]$Tests,[string]$Inspector,[string]$Output)
$ErrorActionPreference='Stop'
$Exe=(Resolve-Path $Exe).Path
$Tests=(Resolve-Path $Tests).Path
$Inspector=(Resolve-Path $Inspector).Path
$Package=(Resolve-Path 'assets/original/exploration/dungeon-0/exploration.mdpe').Path
New-Item -ItemType Directory -Force $Output|Out-Null
$Output=[IO.Path]::GetFullPath($Output)
function Run([string]$Name,[int]$Seconds,[bool]$Restored=$false){
 $Dir=Join-Path $Output $Name
 if(-not $Restored) {
  if($Name -in @('two-sided','chase1','chase5')){ & $Tests --party-fixtures $Package $Dir }
  elseif($Name -in @('friendly','hostile')){ & $Tests --relationship-fixtures $Package $Dir }
  else { & $Tests --fixtures $Package $Dir }
  if($LASTEXITCODE -ne 0){throw 'party fixtures failed'}
 }
 $TraceName=if($Restored){'restored-trace'}else{'trace'}
 $StateName=if($Restored){'restored-state'}else{'state'}
 $ArgsForApp=@('--explore',"`"$Package`"",'--snapshot',"`"$Dir/$Name.mdes`"",'--duration',$Seconds,
  '--metrics',"`"$Dir/metrics.csv`"",'--movement-trace',"`"$Dir/$TraceName.csv`"")
 $Process=Start-Process $Exe -ArgumentList $ArgsForApp -PassThru
 if(-not $Process.WaitForExit(20000)){$Process.Kill();throw 'party app timeout'}
 if($Process.ExitCode -ne 0){throw 'party app failed'}
  $Rows=@(Import-Csv "$Dir/$TraceName.csv"|Where-Object {$_.submitted -eq '1'})
 $Minimum=if($Name -in @('friendly','hostile')){1}else{10}
 if($Rows.Count -lt $Minimum){throw 'party submitted frames absent'}
 foreach($Row in $Rows){
  if($Row.companion_enabled -ne '1'){throw 'party disabled during actual launch'}
  if($Row.life -in @('0','1','8','10')) {
   $Dx=[math]::Abs(([int]$Row.player_tile%24)-([int]$Row.companion_tile%24))
   $Dy=[math]::Abs([math]::Floor([int]$Row.player_tile/24)-[math]::Floor([int]$Row.companion_tile/24))
   if($Dx -eq 0 -and $Dy -eq 0){throw 'logical party overlap failed'}
   if(($Name -eq 'follow' -or $Row.independent_party -ne '1') -and ($Dx -gt 1 -or $Dy -gt 1)){throw 'legacy/follow party radius failed'}
   if(($Name -eq 'follow' -or $Row.independent_party -ne '1') -and ([math]::Abs([double]$Row.world_x-[double]$Row.companion_world_x) -gt 24.01 -or
      [math]::Abs([double]$Row.world_y-[double]$Row.companion_world_y) -gt 24.01)){throw 'rendered party exceeded one-tile radius'}
  }
 }
 & $Tests --inspect $Package "$Dir/$Name.mdes"|Set-Content "$Dir/$StateName.json"
 if($LASTEXITCODE -ne 0){throw 'actual party save cannot restore'}
 return $Rows
}
$Follow=@(Run 'follow' 3)
if(@($Follow|Where-Object {$_.companion_moving -eq '1' -and $_.companion_clip -eq '0'}).Count -lt 3){throw 'Charmander did not walk with leader'}
$Attack=@(Run 'attack' 4)
if(@($Attack|Where-Object {$_.companion_clip -eq '1' -and $_.combat -eq '12'}).Count -lt 2){throw 'companion attack not presented'}
$Target=@(Run 'target' 4)
if(@($Target|Where-Object {$_.enemy_targets_companion -eq '1' -and [int]$_.companion_hp -lt 5 -and $_.player_hp -eq '5'}).Count -lt 2){throw 'closer companion not hit independently'}
if(@($Target|Where-Object {$_.companion_clip -eq '6' -and $_.combat -eq '13'}).Count -lt 2){throw 'companion Hurt6 not presented'}
$Room=@(Run 'room' 2)
if(@($Room|Where-Object {$_.combat -eq '16' -and $_.companion_moving -eq '1'}).Count -lt 2){throw 'room support step absent'}
$Corridor=@(Run 'corridor' 2)
if(@($Corridor|Where-Object {$_.combat -eq '16'}).Count -ne 0){throw 'corridor support step occurred'}
$Faint=@(Run 'faint' 7)
if(@($Faint|Where-Object {$_.companion_clip -eq '33' -and $_.combat -eq '14'}).Count -lt 2 -or
 @($Faint|Where-Object {$_.life -eq '9'}).Count -lt 2){throw 'companion Faint33/death not presented'}
# Compare the actual submitted body pixels with the retained original packages.
$Two=Run 'two-sided' 16
$Both=@($Two|Where-Object {
 $_.independent_party -eq '1' -and $_.clip -eq '1' -and $_.companion_clip -eq '1' -and
 [uint64]$_.player_party_action -gt 0 -and [uint64]$_.companion_party_action -gt 0 -and
 $_.player_party_action -ne $_.companion_party_action
})
if($Both.Count -ne 0){throw 'player and companion attack animations overlapped'}
if(@($Two|Where-Object {$_.clip -eq '1'}).Count -lt 2 -or @($Two|Where-Object {$_.companion_clip -eq '1'}).Count -lt 2){throw 'sequential ally attacks missing'}
$Surviving=@($Two|Where-Object {
 $_.actor_states -notmatch '(^|;)1:' -and $_.actor_states -match '(^|;)2:[0-9]+:(1|6|33):'
})
$Finished=@($Two|Where-Object {$_.actor_states -notmatch '(^|;)(1|2):'})
$PlayerAttacks=@($Two|Where-Object {$_.clip -eq '1'}|Select-Object -ExpandProperty player_party_action -Unique)
if($Surviving.Count -lt 2 -or $Finished.Count -lt 1 -or $PlayerAttacks.Count -lt 2){
 throw 'right encounter failed to progress and finish independently of left faint animation'
}
@{status='pass';overlappingAttackFrames=$Both.Count;afterLeftRemovalFrames=$Surviving.Count;bothRemovedFrames=$Finished.Count}|
 ConvertTo-Json|Set-Content -Encoding UTF8 "$Output/two-sided-result.json"
$ChaseResults=@()
foreach($Hp in @(5,1)) {
 $Chase=@(Run "chase$Hp" 4)
 $Moves=@($Chase|Where-Object {$_.moving -eq '1' -and [long]$_.action_id -gt 0}|Group-Object action_id|Select-Object -First 4)
 if($Moves.Count -lt 4){throw "HP$Hp chase did not present four player moves"}
 $Starts=@()
 foreach($Move in $Moves) {
  $Row=$Move.Group[0]
  $Starts += [long]$Row.started_ms
  if([int]$Row.duration_ms -ne 400 -or [int]$Row.companion_duration_ms -ne 400 -or
    [long]$Row.companion_started_ms -ne [long]$Row.started_ms){throw "HP$Hp player/companion movement clocks differ"}
  $Enemy=@($Row.actor_movements -split ';'|Where-Object {$_ -match '^2:'})
  if($Enemy.Count -ne 1){throw "HP$Hp chasing enemy movement absent"}
  $Parts=$Enemy[0] -split ':'
  if([long]$Parts[1] -ne [long]$Row.started_ms -or [int]$Parts[2] -ne 400 -or
    [long]$Parts[5] -ne [long]$Row.action_id){throw "HP$Hp enemy movement serialized into an extra interval"}
  if(@($Move.Group|Select-Object -ExpandProperty world_x -Unique).Count -lt 3){throw "HP$Hp player movement lacks intermediate submitted positions"}
 }
 $Gaps=@()
 for($Index=1;$Index -lt $Starts.Count;$Index++) {
  $Gap=$Starts[$Index]-$Starts[$Index-1];$Gaps += $Gap
  if($Gap -lt 400 -or $Gap -gt 650){throw "HP$Hp walking gap $Gap ms outside the shared 400ms interval plus runner jitter"}
 }
 $ChaseResults += @{hp=$Hp;starts=$Starts;gapsMs=$Gaps;durationMs=400;status='pass'}
}
$ChaseResults|ConvertTo-Json -Depth 4|Set-Content "$Output/chase-timing.json"
$RelationshipResults=@()
foreach($Role in @('friendly','hostile')) {
 $null=Run $Role 1
 $Before=Get-Content -Raw "$Output/$Role/state.json"|ConvertFrom-Json
 $null=Run $Role 1 $true
 $After=Get-Content -Raw "$Output/$Role/restored-state.json"|ConvertFrom-Json
 $Expected=if($Role -eq 'hostile'){1}else{0}
 if($After.runId -ne $Before.runId -or $Before.hostileCount -ne $Expected -or $After.hostileCount -ne $Expected){
  throw "$Role relationship changed when restoring the ordinary app"
 }
 $RelationshipResults+=@{role=$Role;runId=$After.runId;hostileCount=$After.hostileCount;restored='pass'}
}
$RelationshipResults|ConvertTo-Json|Set-Content "$Output/relationship-restore.json"
Get-Content -Raw "$Output/chase-timing.json"
Get-Content -Raw "$Output/relationship-restore.json"
python tools/check_party_render.py --inspector $Inspector --output $Output
if($LASTEXITCODE -ne 0){throw 'party original body pixels differ'}
@{status='pass';follow='one tile; independent Walk0/motion';combat='Attack1, nearest companion Hurt6, Faint33/death';
 support='one room step, no corridor reposition';restore='every actual save validated';sprites='original Charmander + Kecleon submitted pixels';actualWindows11='not observed'}|
 ConvertTo-Json|Set-Content "$Output/summary.json"
Get-Content -Raw "$Output/summary.json"
