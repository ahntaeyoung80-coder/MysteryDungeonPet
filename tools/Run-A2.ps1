param([switch]$Measure)
$ErrorActionPreference = 'Stop'
$Exe = Join-Path $PSScriptRoot 'MysteryDungeonPet.exe'
if (-not (Test-Path $Exe)) { throw 'Extract the complete A-2 package before running this script.' }
$Results = Join-Path $env:LOCALAPPDATA ('MysteryDungeonPet/A2-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Force $Results | Out-Null
$environment = [ordered]@{
    os = (Get-CimInstance Win32_OperatingSystem | Select-Object Caption, Version, BuildNumber)
    cpu = (Get-CimInstance Win32_Processor | Select-Object Name, NumberOfCores, NumberOfLogicalProcessors)
    installedBytes = (Get-ChildItem $PSScriptRoot -Recurse -File | Measure-Object Length -Sum).Sum
    binarySHA256 = (Get-FileHash $Exe -Algorithm SHA256).Hash
    time = (Get-Date -Format o)
    scope = 'A-2 synthetic fixture; no ROM or original timing validation'
}
$environment | ConvertTo-Json -Depth 4 | Set-Content -Encoding UTF8 "$Results/environment.json"
function Run-Pet([string[]]$PetArguments, [int]$TimeoutSeconds) {
    $process = Start-Process -FilePath $Exe -ArgumentList $PetArguments -PassThru
    if (-not $process.WaitForExit($TimeoutSeconds * 1000)) { $process.Kill(); throw 'A-2 process timeout' }
    if ($process.ExitCode -ne 0) { throw "A-2 exit code $($process.ExitCode); inspect the report or events.log" }
}
Run-Pet @('--validate-package','--report',"`"$Results/wic.txt`"") 30
if ($Measure) {
    foreach ($scenario in @('empty','idle','walk','hide','switch')) {
        $seconds = if ($scenario -eq 'switch') { 165 } else { 90 }
        Write-Output "Measuring $scenario for $seconds seconds"
        Run-Pet @('--scenario',$scenario,'--duration',"$seconds",'--metrics',"`"$Results/$scenario.csv`"") ($seconds + 30)
    }
    Write-Output "Measurements saved: $Results"
    Write-Output 'Actual drag/DPI/Win+L/display-off/suspend checks still require manual interaction.'
} else {
    Start-Process -FilePath $Exe -ArgumentList @('--metrics',"`"$Results/manual.csv`"") | Out-Null
    Write-Output "A-2 running. Right-click the pet or its tray icon for controls. Results: $Results"
    Write-Output 'Double-click the tray icon to restore a hidden window. Choose Exit to flush final metrics.'
}
