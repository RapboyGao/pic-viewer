@echo off
setlocal

set "ROOT_DIR=%~dp0.."
for %%I in ("%ROOT_DIR%") do set "ROOT_DIR=%%~fI"

if "%BUILD_DIR%"=="" set "BUILD_DIR=%ROOT_DIR%\build"
set "APP_PATH=%BUILD_DIR%\pic-viewer.exe"

if not exist "%APP_PATH%" (
  echo ==> App not found, building first
  call "%ROOT_DIR%\scripts\build.bat"
  if errorlevel 1 exit /b 1
)

"%APP_PATH%" %*
