@echo off
setlocal DisableDelayedExpansion

set "ROOT_DIR=%~dp0.."
for %%I in ("%ROOT_DIR%") do set "ROOT_DIR=%%~fI"

if "%BUILD_DIR%"=="" set "BUILD_DIR=%ROOT_DIR%\build"
set "VCPKG_ROOT=%ROOT_DIR%\.deps\vcpkg"
set "APP_PATH=%BUILD_DIR%\Debug\pic-viewer.exe"
if not exist "%APP_PATH%" set "APP_PATH=%BUILD_DIR%\Release\pic-viewer.exe"
if not exist "%APP_PATH%" set "APP_PATH=%BUILD_DIR%\RelWithDebInfo\pic-viewer.exe"
if not exist "%APP_PATH%" set "APP_PATH=%BUILD_DIR%\pic-viewer.exe"

if not exist "%APP_PATH%" (
  echo ==> App not found, building first
  set "RUN_TESTS=0"
  call "%ROOT_DIR%\scripts\build.bat"
  if errorlevel 1 exit /b 1
  set "APP_PATH=%BUILD_DIR%\Debug\pic-viewer.exe"
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

if "%~1"=="" (
  "%APP_PATH%"
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
  "%APP_PATH%" %*
)
