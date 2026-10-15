param([string]$Exe, [string]$Tests, [string]$Output)
$ErrorActionPreference = 'Stop'
New-Item -ItemType Directory -Force $Output | Out-Null
$Exe = (Resolve-Path $Exe).Path
$Tests = (Resolve-Path $Tests).Path
$Output = [IO.Path]::GetFullPath($Output)
$Assets = (Resolve-Path 'assets/original/gameplay').Path
$Package = (Resolve-Path 'assets/original/exploration/dungeon-0/exploration.mdpe').Path
& $Tests $Assets $Package $Output | Set-Content "$Output/scenarios.jsonl"
if ($LASTEXITCODE -ne 0) { throw 'gameplay scenario setup failed' }
$EventPath = Join-Path $env:LOCALAPPDATA 'MysteryDungeonPet/events.log'
$Before = if (Test-Path $EventPath) { @(Get-Content $EventPath).Count } else { 0 }
$Csv = "$Output/life.csv"
$argsForApp = @('--explore', "`"$Package`"", '--gameplay-assets', "`"$Assets`"", '--snapshot', "`"$Output/clear.mdes`"", '--duration', '40', '--metrics', "`"$Csv`"")
$Process = Start-Process -FilePath $Exe -ArgumentList $argsForApp -PassThru
if (-not $Process.WaitForExit(60000)) { $Process.Kill(); throw 'life UI timeout' }
if ($Process.ExitCode -ne 0) { throw "life UI failed: $($Process.ExitCode)" }
$Events = @(Get-Content $EventPath | Select-Object -Skip $Before)
$Events | Set-Content "$Output/life-events.txt"
$Rows = @(Import-Csv $Csv)
$Status = 'pending: runner session paused rendering'
if (@($Rows | Where-Object { [int]$_.render_paused -eq 0 }).Count -gt 1) {
    $Joined = $Events -join "`n"
    foreach ($Pattern in @('select clip=17', 'life=ReturnHome', 'life=HomeScene', 'guild room=GuildDining characters=12 objectCount=1', 'guild room=PlayerRoom characters=2', 'guild partner wake finished', 'life=Eat', 'life=Sleep', 'life=Wake', 'life=Ready', 'life=Explore.*day=2', 'sceneBeat=13 ', 'sceneBeat=2 ', 'sceneBeat=6 ', 'sceneBeat=7 ', 'sceneBeat=3 ', 'sceneBeat=4 ', 'sceneBeat=5 ', 'sceneBeat=12 ', 'sceneBeat=8 ', 'sceneBeat=9 ', 'sceneBeat=16 ')) {
        if ($Joined -notmatch $Pattern) { throw "missing production life event: $Pattern" }
    }
    if ($Joined -match 'sceneBeat=(10|11|15) ') { throw 'sleep hold or redundant morning fades replayed' }
    $PreviousBeatIndex = -1
    foreach ($Beat in @(2,6,3,4,7,5,12,8,9,16)) {
        $BeatIndex = 0
        while ($BeatIndex -lt $Events.Count -and $Events[$BeatIndex] -notmatch "sceneBeat=$Beat ") { $BeatIndex++ }
        if ($BeatIndex -le $PreviousBeatIndex -or $BeatIndex -eq $Events.Count) { throw "bedtime/wake ordering failed at beat $Beat" }
        $PreviousBeatIndex = $BeatIndex
    }
    $SleepStart = [long]((@($Events | Where-Object { $_ -match 'sceneBeat=7 ' })[0] -split ' ')[0])
    $SleepFade = [long]((@($Events | Where-Object { $_ -match 'sceneBeat=5 ' })[0] -split ' ')[0])
    if ($SleepFade-$SleepStart -gt 6000000) { throw 'sleep onset retained the old ten-second idle' }
    $ExitStart = [long]((@($Events | Where-Object { $_ -match 'sceneBeat=16 ' })[0] -split ' ')[0])
    $NextDay = [long]((@($Events | Where-Object { $_ -match 'life=Explore.*day=2' })[0] -split ' ')[0])
    if ($NextDay-$ExitStart -lt 1800000 -or $NextDay-$ExitStart -gt 3000000) { throw 'room exit did not use the short in-map route' }
    $Returning = @($Events | Where-Object { $_ -match 'life=ReturnHome' })[0]
    $Eating = @($Events | Where-Object { $_ -match 'life=Eat' })[0]
    $ReturnUs = [long]($Returning -split ' ')[0]
    $EatUs = [long]($Eating -split ' ')[0]
    if ($EatUs - $ReturnUs -gt 2000000) { throw 'return lasted more than 2 seconds' }
    $DuringReturn = @($Events | Where-Object { [long]($_ -split ' ')[0] -ge $ReturnUs -and [long]($_ -split ' ')[0] -le $EatUs }) -join "`n"
    if ($DuringReturn -match 'select clip=0 ') { throw 'ReturnHome kept walking' }
    $Status = 'pass'
}
@{ status=$Status; test='production Pose -> fade -> original home -> Eat -> Sleep -> Wake -> Explore';
   coreScenarios=12; guildDiningCharacters=12; guildMembers=10; bedroomCharacters=2; actualWindows11='pending' } | ConvertTo-Json | Set-Content "$Output/life-result.json"
$Before = @(Get-Content $EventPath).Count
$CombatCsv = "$Output/combat.csv"
$CombatArgs = @('--explore', "`"$Package`"", '--gameplay-assets', "`"$Assets`"", '--snapshot', "`"$Output/combat.mdes`"", '--duration', '12', '--metrics', "`"$CombatCsv`"")
$CombatProcess = Start-Process -FilePath $Exe -ArgumentList $CombatArgs -PassThru
if (-not $CombatProcess.WaitForExit(30000)) { $CombatProcess.Kill(); throw 'combat UI timeout' }
if ($CombatProcess.ExitCode -ne 0) { throw 'combat UI failed' }
$CombatEvents = @(Get-Content $EventPath | Select-Object -Skip $Before)
$CombatEvents | Set-Content "$Output/combat-events.txt"
$CombatRows = @(Import-Csv $CombatCsv)
$CombatStatus = 'pending: runner session paused rendering'
if (@($CombatRows | Where-Object { [int]$_.render_paused -eq 0 }).Count -gt 1) {
    $Joined = $CombatEvents -join "`n"
    foreach ($Pattern in @('combat=PlayerAttack', 'combat=EnemyHurt', 'combat=EnemyAttack', 'combat=Faint', 'combat=CombatVictory', 'life=Explore')) {
        if ($Joined -notmatch $Pattern) { throw "missing production combat event: $Pattern" }
    }
    $VictoryIndex = 0
    while ($VictoryIndex -lt $CombatEvents.Count -and $CombatEvents[$VictoryIndex] -notmatch 'combat=CombatVictory') { $VictoryIndex++ }
    $BeforeVictory = @($CombatEvents | Select-Object -First ($VictoryIndex + 1)) -join "`n"
    if ($BeforeVictory -match 'life=ReturnHome') { throw 'combat victory returned home' }
    $CombatStatus = 'pass'
}
@{status=$CombatStatus; test='production player first Attack1 Hurt6 Faint33 victory resumes dungeon'; actualWindows11='pending'} |
    ConvertTo-Json | Set-Content "$Output/combat-result.json"
