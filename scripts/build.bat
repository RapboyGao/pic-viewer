@echo off
setlocal enabledelayedexpansion

set "ROOT_DIR=%~dp0.."
for %%I in ("%ROOT_DIR%") do set "ROOT_DIR=%%~fI"
set "VCPKG_ROOT=%ROOT_DIR%\.deps\vcpkg"
set "VCPKG_TOOLCHAIN=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake"
set "VCPKG_DEFAULT_PREFIX=%ROOT_DIR%\vcpkg_installed\x64-windows"

if "%BUILD_DIR%"=="" set "BUILD_DIR=%ROOT_DIR%\build"
if "%RUN_TESTS%"=="" set "RUN_TESTS=1"
if "%BUILD_CONFIG%"=="" set "BUILD_CONFIG=Debug"

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
  if exist "%VCPKG_DEFAULT_PREFIX%" set "QT_PREFIX=%VCPKG_DEFAULT_PREFIX%"
)

if not exist "%QT_PREFIX%\share\Qt6\Qt6Config.cmake" if not exist "%QT_PREFIX%\share\qt6\Qt6Config.cmake" (
  if exist "%VCPKG_DEFAULT_PREFIX%" set "QT_PREFIX=%VCPKG_DEFAULT_PREFIX%"
)

if exist "%QT_PREFIX%\x64-windows\share\Qt6\Qt6Config.cmake" (
  set "QT_PREFIX=%QT_PREFIX%\x64-windows"
) else if exist "%QT_PREFIX%\x64-windows\share\qt6\Qt6Config.cmake" (
  set "QT_PREFIX=%QT_PREFIX%\x64-windows"
)

if not exist "%QT_PREFIX%\share\Qt6\Qt6Config.cmake" if not exist "%QT_PREFIX%\share\qt6\Qt6Config.cmake" (
  echo error: Qt6Config.cmake not found under "%QT_PREFIX%".
  exit /b 1
)

echo ==> Configuring with vcpkg toolchain: %VCPKG_TOOLCHAIN%
cmake -S "%ROOT_DIR%" -B "%BUILD_DIR%" -DCMAKE_TOOLCHAIN_FILE="%VCPKG_TOOLCHAIN%" -DVCPKG_MANIFEST_DIR="%ROOT_DIR%" -DVCPKG_INSTALLED_DIR="%VCPKG_ROOT%\installed" -DCMAKE_PREFIX_PATH="%QT_PREFIX%"
if errorlevel 1 exit /b 1

echo ==> Building
cmake --build "%BUILD_DIR%" --config "%BUILD_CONFIG%" --parallel
if errorlevel 1 exit /b 1

set "APP_DIR=%BUILD_DIR%\%BUILD_CONFIG%"
if exist "%APP_DIR%" (
  set "WINDEPLOYQT=%VCPKG_ROOT%\installed\x64-windows\tools\Qt6\bin\windeployqt.exe"
  set "APP_EXE=%APP_DIR%\pic-viewer.exe"
  if exist "%WINDEPLOYQT%" if exist "%APP_EXE%" (
    if /I "%BUILD_CONFIG%"=="Debug" (
      "%WINDEPLOYQT%" --debug --compiler-runtime --dir "%APP_DIR%" "%APP_EXE%"
    ) else (
      "%WINDEPLOYQT%" --release --compiler-runtime --dir "%APP_DIR%" "%APP_EXE%"
    )
  )
)

if "%RUN_TESTS%"=="1" (
  echo ==> Running tests
  ctest --test-dir "%BUILD_DIR%" -C "%BUILD_CONFIG%" --output-on-failure
  if errorlevel 1 exit /b 1
)

if exist "%BUILD_DIR%\%BUILD_CONFIG%\pic-viewer.exe" (
  echo ==> Build complete: %BUILD_DIR%\%BUILD_CONFIG%\pic-viewer.exe
) else (
  echo ==> Build complete
)
