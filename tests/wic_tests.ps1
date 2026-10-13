param([Parameter(Mandatory=$true)][string]$Exe, [string]$Output = "build/wic-results")
$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
$Exe = (Resolve-Path $Exe).Path
New-Item -ItemType Directory -Force $Output | Out-Null
$Output = (Resolve-Path $Output).Path
python "$Root/tests/make_invalid_packages.py" "$Output/negative"
if ($LASTEXITCODE -ne 0) { throw "negative fixture generation failed" }
function Run-Validation([string]$Package, [string]$Report, [bool]$ExpectedSuccess) {
    $arguments = @('--validate-package', '--package', "`"$Package`"", '--report', "`"$Report`"")
    $process = Start-Process -FilePath $Exe -ArgumentList $arguments -PassThru
    if (-not $process.WaitForExit(30000)) { $process.Kill(); throw "validation timeout: $Package" }
    if (($process.ExitCode -eq 0) -ne $ExpectedSuccess) { throw "unexpected exit code $($process.ExitCode): $Package" }
    $reportText = Get-Content -Raw $Report
    $expectedStatus = if ($ExpectedSuccess) { 'status=pass' } else { 'status=fail' }
    if (-not $reportText.StartsWith($expectedStatus)) { throw "unexpected report: $Package" }
}
Run-Validation "$Root/assets/fixture/fixture.mdpa" "$Output/valid.txt" $true
Get-ChildItem -Directory "$Output/negative" | ForEach-Object {
    Run-Validation "$($_.FullName)/fixture.mdpa" "$Output/$($_.Name).txt" $false
}
Write-Output 'PASS native WIC fixture/cache/alpha checks and 8 negative packages. Windows UI/power checks remain separate.'
