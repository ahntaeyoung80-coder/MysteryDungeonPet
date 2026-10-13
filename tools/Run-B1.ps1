param([int]$Seed = 42, [switch]$NewRun)
$ErrorActionPreference = 'Stop'
$Exe = Join-Path $PSScriptRoot 'MysteryDungeonPet.exe'
$Package = Join-Path $PSScriptRoot 'assets/exploration/dungeon-0/exploration.mdpe'
$Actor = Join-Path $PSScriptRoot 'assets/original/asset-0/asset.mdpa'
$Arguments = @('--explore', "`"$Package`"", '--package', "`"$Actor`"", '--seed', "$Seed")
if ($NewRun) { $Arguments += '--new-run' }
Start-Process -FilePath $Exe -ArgumentList $Arguments
