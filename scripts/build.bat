@echo off
setlocal enabledelayedexpansion

set "ROOT_DIR=%~dp0.."
for %%I in ("%ROOT_DIR%") do set "ROOT_DIR=%%~fI"

if "%BUILD_DIR%"=="" set "BUILD_DIR=%ROOT_DIR%\build"
if "%RUN_TESTS%"=="" set "RUN_TESTS=1"

if "%QT_PREFIX%"=="" (
  if not "%Qt6_DIR%"=="" (
    for %%I in ("%Qt6_DIR%\..\..") do set "QT_PREFIX=%%~fI"
  )
)

if "%QT_PREFIX%"=="" (
  echo error: Qt prefix not found. Set QT_PREFIX or Qt6_DIR before running this script.
  exit /b 1
)

if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

echo ==> Configuring with Qt at: %QT_PREFIX%
cmake -S "%ROOT_DIR%" -B "%BUILD_DIR%" -DCMAKE_PREFIX_PATH="%QT_PREFIX%"
if errorlevel 1 exit /b 1

echo ==> Building
cmake --build "%BUILD_DIR%" --parallel
if errorlevel 1 exit /b 1

if "%RUN_TESTS%"=="1" (
  echo ==> Running tests
  ctest --test-dir "%BUILD_DIR%" --output-on-failure
  if errorlevel 1 exit /b 1
)

echo ==> Build complete: %BUILD_DIR%\pic-viewer.exe
