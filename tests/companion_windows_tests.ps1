param([string]$Exe,[string]$Tests,[string]$Inspector,[string]$Output)
$ErrorActionPreference='Stop'
$Exe=(Resolve-Path $Exe).Path
$Tests=(Resolve-Path $Tests).Path
$Inspector=(Resolve-Path $Inspector).Path
$Package=(Resolve-Path 'assets/original/exploration/dungeon-0/exploration.mdpe').Path
New-Item -ItemType Directory -Force $Output|Out-Null
$Output=[IO.Path]::GetFullPath($Output)
function Run([string]$Name,[int]$Seconds){
 $Dir=Join-Path $Output $Name
 if($Name -eq 'two-sided'){ & $Tests --party-fixtures $Package $Dir }
 else { & $Tests --fixtures $Package $Dir }
 if($LASTEXITCODE -ne 0){throw 'party fixtures failed'}
 $ArgsForApp=@('--explore',"`"$Package`"",'--snapshot',"`"$Dir/$Name.mdes`"",'--duration',$Seconds,
  '--metrics',"`"$Dir/metrics.csv`"",'--movement-trace',"`"$Dir/trace.csv`"")
 $Process=Start-Process $Exe -ArgumentList $ArgsForApp -PassThru
 if(-not $Process.WaitForExit(20000)){$Process.Kill();throw 'party app timeout'}
 if($Process.ExitCode -ne 0){throw 'party app failed'}
 $Rows=@(Import-Csv "$Dir/trace.csv"|Where-Object {$_.submitted -eq '1'})
 if($Rows.Count -lt 10){throw 'party submitted frames absent'}
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
 & $Tests --inspect $Package "$Dir/$Name.mdes"|Set-Content "$Dir/state.json"
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
$Two=Run 'two-sided' 6
$Both=@($Two|Where-Object {
 $_.independent_party -eq '1' -and $_.clip -eq '1' -and $_.companion_clip -eq '1' -and
 [uint64]$_.player_party_action -gt 0 -and [uint64]$_.companion_party_action -gt 0 -and
 $_.player_party_action -ne $_.companion_party_action
})
if($Both.Count -lt 2){throw 'independent player and companion attacks were not presented together'}
$Surviving=@($Two|Where-Object {
 $_.actor_states -notmatch '(^|;)1:' -and $_.actor_states -match '(^|;)2:' -and $_.clip -eq '1'
})
if($Surviving.Count -lt 2){throw 'right-side player attack stopped after left enemy removal'}
@{status='pass';simultaneousSubmittedFrames=$Both.Count;afterLeftRemovalFrames=$Surviving.Count}|
 ConvertTo-Json|Set-Content -Encoding UTF8 "$Output/two-sided-result.json"
python tools/check_party_render.py --inspector $Inspector --output $Output
if($LASTEXITCODE -ne 0){throw 'party original body pixels differ'}
@{status='pass';follow='one tile; independent Walk0/motion';combat='Attack1, nearest companion Hurt6, Faint33/death';
 support='one room step, no corridor reposition';restore='every actual save validated';sprites='original Charmander + Kecleon submitted pixels';actualWindows11='not observed'}|
 ConvertTo-Json|Set-Content "$Output/summary.json"
Get-Content -Raw "$Output/summary.json"
