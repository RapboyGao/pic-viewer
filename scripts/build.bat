@echo off
setlocal enabledelayedexpansion

set "ROOT_DIR=%~dp0.."
for %%I in ("%ROOT_DIR%") do set "ROOT_DIR=%%~fI"
set "VCPKG_ROOT=%ROOT_DIR%\.deps\vcpkg"
set "VCPKG_TOOLCHAIN=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake"

if "%BUILD_DIR%"=="" set "BUILD_DIR=%ROOT_DIR%\build"
if "%RUN_TESTS%"=="" set "RUN_TESTS=1"

if "%QT_PREFIX%"=="" (
  if not "%Qt6_DIR%"=="" (
    for %%I in ("%Qt6_DIR%\..\..") do set "QT_PREFIX=%%~fI"
  )
)

if not exist "%VCPKG_TOOLCHAIN%" (
  echo error: vcpkg toolchain not found at "%VCPKG_TOOLCHAIN%".
  exit /b 1
)

if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

if "%QT_PREFIX%"=="" (
  if exist "%VCPKG_ROOT%\installed\x64-windows" set "QT_PREFIX=%VCPKG_ROOT%\installed\x64-windows"
)

echo ==> Configuring with vcpkg toolchain: %VCPKG_TOOLCHAIN%
cmake -S "%ROOT_DIR%" -B "%BUILD_DIR%" -DCMAKE_TOOLCHAIN_FILE="%VCPKG_TOOLCHAIN%" -DVCPKG_MANIFEST_DIR="%ROOT_DIR%" -DVCPKG_INSTALLED_DIR="%VCPKG_ROOT%\installed" -DCMAKE_PREFIX_PATH="%QT_PREFIX%"
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
