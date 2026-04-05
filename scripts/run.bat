@echo off
setlocal DisableDelayedExpansion

set "ROOT_DIR=%~dp0.."
for %%I in ("%ROOT_DIR%") do set "ROOT_DIR=%%~fI"

if "%BUILD_DIR%"=="" set "BUILD_DIR=%ROOT_DIR%\build"
if "%BUILD_CONFIG%"=="" set "BUILD_CONFIG=Release"
set "BUILD_WORK_DIR=%BUILD_DIR%\_work-%BUILD_CONFIG%"
for /f "delims=" %%I in ('dir /b /ad /o-d "%BUILD_DIR%\_work-%BUILD_CONFIG%-*" 2^>nul') do (
  if exist "%BUILD_DIR%\%%I\%BUILD_CONFIG%\pic-viewer.exe" (
    set "BUILD_WORK_DIR=%BUILD_DIR%\%%I"
    goto :workdir_found
  )
)
:workdir_found
set "VCPKG_ROOT=%ROOT_DIR%\.deps\vcpkg"
set "APP_PATH=%BUILD_WORK_DIR%\%BUILD_CONFIG%\pic-viewer.exe"
if not exist "%APP_PATH%" set "APP_PATH=%BUILD_DIR%\%BUILD_CONFIG%\pic-viewer.exe"
if not exist "%APP_PATH%" set "APP_PATH=%BUILD_DIR%\Release\pic-viewer.exe"
if not exist "%APP_PATH%" set "APP_PATH=%BUILD_DIR%\RelWithDebInfo\pic-viewer.exe"
if not exist "%APP_PATH%" set "APP_PATH=%BUILD_DIR%\pic-viewer.exe"

if not exist "%APP_PATH%" (
  echo ==> App not found, building first
  set "RUN_TESTS=0"
  set "BUILD_CONFIG=%BUILD_CONFIG%"
  call "%ROOT_DIR%\scripts\build.bat"
  if errorlevel 1 exit /b 1
  set "APP_PATH=%BUILD_WORK_DIR%\%BUILD_CONFIG%\pic-viewer.exe"
  if not exist "%APP_PATH%" set "APP_PATH=%BUILD_DIR%\%BUILD_CONFIG%\pic-viewer.exe"
  if not exist "%APP_PATH%" set "APP_PATH=%BUILD_DIR%\Release\pic-viewer.exe"
  if not exist "%APP_PATH%" set "APP_PATH=%BUILD_DIR%\RelWithDebInfo\pic-viewer.exe"
  if not exist "%APP_PATH%" set "APP_PATH=%BUILD_DIR%\pic-viewer.exe"
)

if exist "%VCPKG_ROOT%\installed\x64-windows\bin" (
  set "PATH=%VCPKG_ROOT%\installed\x64-windows\bin;%PATH%"
)
if exist "%VCPKG_ROOT%\installed\x64-windows\debug\bin" (
  set "PATH=%VCPKG_ROOT%\installed\x64-windows\debug\bin;%PATH%"
)
if exist "%VCPKG_ROOT%\installed\x64-windows\Qt6\plugins" (
  set "QT_PLUGIN_PATH=%VCPKG_ROOT%\installed\x64-windows\Qt6\plugins"
  set "QT_QPA_PLATFORM_PLUGIN_PATH=%VCPKG_ROOT%\installed\x64-windows\Qt6\plugins\platforms"
)
set "VS_COMMON7_IDE=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE"
set "VS_VC_REDIST=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Redist\MSVC\14.44.35112\x64"
if exist "%VS_COMMON7_IDE%" set "PATH=%VS_COMMON7_IDE%;%PATH%"
if exist "%VS_VC_REDIST%" set "PATH=%VS_VC_REDIST%;%PATH%"

if "%~1"=="" (
  start "" "%APP_PATH%"
) else (
  for %%I in (%*) do (
    if not exist "%%~fI" (
      if exist "%%~I" (
        rem Path exists after expansion.
      ) else if not "%%~I"=="%%~fI" (
        echo warning: input path not found: %%~I
      )
    )
  )
  start "" "%APP_PATH%" %*
)
exit /b 0
