@echo off
"%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe" -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\run_quality_check.ps1" %*
set "MDP_QUALITY_EXIT=%errorlevel%"
pause
exit /b %MDP_QUALITY_EXIT%
