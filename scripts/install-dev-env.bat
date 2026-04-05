@echo off
setlocal

set "SCRIPT_DIR=%~dp0"
set "PS_SCRIPT=%SCRIPT_DIR%install-dev-env.ps1"

if not exist "%PS_SCRIPT%" (
  echo error: cannot find PowerShell installer script: "%PS_SCRIPT%"
  exit /b 1
)

set "POWERSHELL_EXE=powershell"
where pwsh >nul 2>nul
if %errorlevel%==0 set "POWERSHELL_EXE=pwsh"

"%POWERSHELL_EXE%" -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "%PS_SCRIPT%" %*
exit /b %errorlevel%
