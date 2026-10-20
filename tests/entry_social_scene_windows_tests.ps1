param([string]$Exe,[string]$Tests,[string]$Output)
$ErrorActionPreference='Stop'
$Exe=(Resolve-Path $Exe).Path
$Tests=(Resolve-Path $Tests).Path
$Package=(Resolve-Path 'assets/original/exploration/dungeon-0/exploration.mdpe').Path
New-Item -ItemType Directory -Force $Output|Out-Null
$Output=[IO.Path]::GetFullPath($Output)
& $Tests --fixtures $Package $Output
if($LASTEXITCODE -ne 0){throw 'entry/social fixtures failed'}
function Inspect([string]$Name){
 $Result=& $Tests --inspect $Package "$Output/$Name.mdes"|ConvertFrom-Json
 if($LASTEXITCODE -ne 0){throw 'entry/social snapshot inspection failed'}
 return $Result
}
function Run([string]$Name,[int]$Seconds){
 $ArgsForApp=@('--solo','--explore',"`"$Package`"",'--snapshot',"`"$Output/$Name.mdes`"",'--duration',$Seconds,
  '--metrics',"`"$Output/$Name-metrics.csv`"",'--movement-trace',"`"$Output/$Name.csv`"")
 $Process=Start-Process $Exe -ArgumentList $ArgsForApp -PassThru
 if(-not $Process.WaitForExit(30000)){$Process.Kill();throw 'entry/social app timeout'}
 if($Process.ExitCode -ne 0){throw 'entry/social app failed'}
 $Rows=@(Import-Csv "$Output/$Name.csv"|Where-Object {$_.submitted -eq '1'})
 if($Rows.Count -lt 10){throw 'entry/social submitted frames not observed'}
 return $Rows
}
$EventPath=Join-Path $env:LOCALAPPDATA 'MysteryDungeonPet/events.log'
$Before=if(Test-Path $EventPath){@(Get-Content $EventPath).Count}else{0}
$EntryBefore=Inspect 'entry'
$EntryRows=@(Run 'entry' 3)
$EntryAfter=Inspect 'entry'
$Arrival=@($EntryRows|Where-Object {$_.floor -eq '2' -and $_.scene_beat -eq '14'})
if($Arrival.Count -lt 3 -or $EntryAfter.floor -ne 2 -or $EntryAfter.completed){throw 'third-floor entry immediately escaped or was not observed'}
foreach($Row in $Arrival){
 if($Row.player_tile -ne $Row.entry_tile -or $Row.entry_tile -eq $Row.stairs_tile -or
  [int]$Row.stairs_tile -eq $EntryBefore.player -or [int]$Row.entry_entities -ne 0 -or
  [double]$Row.scene_x -ne (([int]$Row.entry_tile%24)*24+12) -or
  [double]$Row.scene_y -ne ([math]::Floor([int]$Row.entry_tile/24)*24+12)){throw 'third-floor logical/display entry or underfoot placement invalid'}
}
$NpcRows=@(Run 'npc' 5)
$NpcAfter=Inspect 'npc'
if($NpcAfter.greetings -ne 1 -or @($NpcRows|Where-Object {$_.life -eq '10'}).Count -lt 3){throw 'off-path friendly NPC did not approach/greet/respond once'}
& $Tests --meal-fixture $Package $Output
if($LASTEXITCODE -ne 0){throw 'meal fixture failed'}
$MealRows=@(Run 'meal' 10)
$Dining=@($MealRows|Where-Object {$_.life -in @('2','3','4') -or $_.scene_beat -eq '2'})
if($Dining.Count -lt 10 -or @($Dining|Where-Object {$_.guild_apple_visible -ne '1'}).Count){throw 'apple was not present continuously throughout dining'}
if(@($Dining|Select-Object -ExpandProperty guild_apple_frame -Unique).Count -lt 10){throw 'apple effect stopped animating'}
& $Tests --bed-fixture $Package $Output
if($LASTEXITCODE -ne 0){throw 'bedtime fixture failed'}
$BedRows=@(Run 'bed' 4)
$BeforeGreeting=@($BedRows|Where-Object {$_.scene_beat -eq '6'})
$Greeting=@($BedRows|Where-Object {$_.scene_beat -eq '3'})
if($BeforeGreeting.Count -lt 3 -or $Greeting.Count -lt 10){throw 'bedtime facing/greeting not presented'}
foreach($Row in @($BeforeGreeting+$Greeting)) {
 if($Row.direction -ne '6' -or $Row.guild_partner_direction -ne '2'){throw 'bedtime actors did not face each other'}
}
$AfterGreeting=@($BedRows|Where-Object {$_.scene_beat -eq '4'}|Select-Object -First 1)
if($AfterGreeting.Count -ne 1 -or [long]$AfterGreeting[0].elapsed_us-[long]$Greeting[0].elapsed_us -lt 1400000){throw 'bedtime greeting still too short'}
foreach($ActorField in @('frame_index','partner_frame')) {
 foreach($Pose in @('0','1','2')) {
  if(@($Greeting|Where-Object {$_.$ActorField -eq $Pose}).Count -lt 1){throw 'slow greeting original pose missing'}
 }
}
& $Tests --wake-fixture $Package $Output
if($LASTEXITCODE -ne 0){throw 'wake fixture failed'}
$OverlapRows=@(Run 'overlap' 1)
python tools/check_scene_overlap.py --inspector (Join-Path (Split-Path $Tests) 'pet_package_inspect.exe') --trace "$Output/overlap.csv"
if($LASTEXITCODE -ne 0){throw 'overlapping player sprite did not render above partner'}
# Each wall-clock fixture must start immediately before its own process.
& $Tests --wake-fixture $Package $Output
if($LASTEXITCODE -ne 0){throw 'fresh wake fixture failed'}
$WakeRows=@(Run 'wake' 12)
$WakeAfter=Inspect 'wake'
foreach($Beat in @(18,15,19)) {
 $Alarm=@($WakeRows|Where-Object {[int]$_.scene_beat -eq $Beat})
 if($Alarm.Count -lt 3){throw "Loudred movement beat $Beat absent"}
 $ExpectedClip=if($Beat -eq 15){'3'}else{'0'}
 $ExpectedDirection=if($Beat -eq 19){'6'}else{'2'}
 if(@($Alarm|Where-Object {$_.guild_alarm_clip -ne $ExpectedClip -or $_.guild_alarm_direction -ne $ExpectedDirection -or $_.guild_alarm_y -ne '180'}).Count){throw 'Loudred clip/direction/route changed'}
 if($Beat -eq 18 -and ([int]$Alarm[-1].guild_alarm_x -le [int]$Alarm[0].guild_alarm_x)){throw 'Loudred entrance did not walk forward'}
 if($Beat -eq 19 -and ([int]$Alarm[-1].guild_alarm_x -ge [int]$Alarm[0].guild_alarm_x)){throw 'Loudred departure did not walk out'}
}
$Poses=Get-Content -Raw "$Output/poses.json"|ConvertFrom-Json
$Sleeping=@($WakeRows|Where-Object {$_.life -eq '6' -and $_.scene_beat -eq '12'})
if($Sleeping.Count -lt 3){throw 'morning sleep pose not observed'}
foreach($Row in $Sleeping){
 if($Row.clip -ne '13' -or [int]$Row.frame_index -ne $Poses.playerLast -or $Row.holding_pose -ne '1' -or
  $Row.partner_clip -ne '13' -or [int]$Row.partner_frame -ne $Poses.partnerLast){throw 'morning fade stood up or replayed sleep onset'}
}
$Exit=@($WakeRows|Where-Object {$_.scene_beat -eq '16'})
if(@($Exit|Where-Object {$_.scene_y -eq '184' -and $_.direction -eq '6'}).Count -lt 10){throw 'leader did not walk straight left'}
if(@($Exit|Where-Object {$_.guild_partner_y -eq '184' -and $_.guild_partner_direction -eq '6'}).Count -lt 3){throw 'partner did not follow straight left'}
if(@($Exit|Where-Object {$_.clip -eq '7' -and $_.scene_x -eq '80'}).Count -lt 1){throw 'arrived leader walked in place'}
if(@($Exit|Where-Object {$_.scene_y -ne '184' -or $_.guild_partner_y -ne '184'}).Count){throw 'exit route left the straight line'}
$PreviousCamera=80
foreach($Row in $Exit) {
 if([int]$Row.camera_x -gt $PreviousCamera -or $Row.camera_y -ne '84'){throw 'exit camera jumped right or moved vertically'}
 $PreviousCamera=[int]$Row.camera_x
}
$Out=@($WakeRows|Where-Object {$_.scene_beat -eq '10'})
$In=@($WakeRows|Where-Object {$_.scene_beat -eq '11'})
if($Out.Count -lt 3 -or $In.Count -lt 3 -or $WakeAfter.day -ne 2 -or $WakeAfter.hearts -ne 5){throw 'room exit fade/next day not observed'}
foreach($Row in $Out){if($Row.scene_x -ne '80' -or $Row.scene_y -ne '184' -or $Row.clip -ne '7'){throw 'exit fade teleported to bed or replayed greeting'}}
if([long]$Out[-1].rgb_sum -ge [long]$Out[0].rgb_sum -or [long]$In[0].rgb_sum -ge [long]$In[-1].rgb_sum){throw 'exit fade did not darken and brighten actual submitted pixels'}
if(@($Out+$In|Where-Object {$_.opaque_pixels -ne '40000'}).Count -ne 0){throw 'exit fade exposed desktop transparency'}
if(@($In|Where-Object {$_.action_id -ne '0' -or $_.player_tile -ne $_.entry_tile}).Count -ne 0){throw 'new day moved before fade-in completed'}
@($EntryBefore,$EntryAfter,$NpcAfter,$WakeAfter)|ConvertTo-Json -Depth 5|Set-Content "$Output/states.json"
$Events=@(Get-Content $EventPath|Select-Object -Skip $Before)
$Events|Set-Content "$Output/events.txt"
$Joined=$Events -join "`n"
if($Joined -notmatch 'life=Dialogue.*dialogueStage=0' -or $Joined -notmatch 'life=Dialogue.*dialogueStage=1'){throw 'friendly dialogue production stages missing'}
@{status='pass';entry='third-floor fade-in: logical/display feet at entry, no stairs/entity underfoot, no immediate escape';
 friendly='off-path approach, player greeting and NPC response once';wake='both original Sleep13 final frames held through daylight fade before Wake14';
 exit='corridor endpoint held during fade-out; actual submitted pixels darken/brighten; next day commits once and movement waits for fade-in';actualWindows11='not observed'}|
 ConvertTo-Json|Set-Content "$Output/summary.json"
Get-Content -Raw "$Output/summary.json"
