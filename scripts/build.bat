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
if "%BUILD_WORK_DIR%"=="" set "BUILD_WORK_DIR=%BUILD_DIR%\_work-%BUILD_CONFIG%-%RANDOM%%RANDOM%"

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
mkdir "%BUILD_WORK_DIR%"

rem Stop any running copies so the linker can overwrite the executables.
powershell -NoProfile -Command "$names = 'pic-viewer','test_image_catalog','test_slide_show_controller','test_prefetch_scheduler','test_image_decoder'; Get-CimInstance Win32_Process | Where-Object { $names -contains $_.Name -or ($_.ExecutablePath -and $_.ExecutablePath -like '*\pic-viewer\build\*') } | ForEach-Object { Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue }"
timeout /t 2 /nobreak >nul

for %%F in (
  "%BUILD_DIR%\Debug\pic-viewer.exe"
  "%BUILD_DIR%\Release\pic-viewer.exe"
  "%BUILD_DIR%\RelWithDebInfo\pic-viewer.exe"
  "%BUILD_DIR%\Debug\test_image_catalog.exe"
  "%BUILD_DIR%\Release\test_image_catalog.exe"
  "%BUILD_DIR%\Debug\test_slide_show_controller.exe"
  "%BUILD_DIR%\Release\test_slide_show_controller.exe"
  "%BUILD_DIR%\Debug\test_prefetch_scheduler.exe"
  "%BUILD_DIR%\Release\test_prefetch_scheduler.exe"
  "%BUILD_DIR%\Debug\test_image_decoder.exe"
  "%BUILD_DIR%\Release\test_image_decoder.exe"
) do (
  if exist "%%~F" del /F /Q "%%~F" >nul 2>nul
)

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
cmake -S "%ROOT_DIR%" -B "%BUILD_WORK_DIR%" -DCMAKE_TOOLCHAIN_FILE="%VCPKG_TOOLCHAIN%" -DVCPKG_MANIFEST_DIR="%ROOT_DIR%" -DVCPKG_INSTALLED_DIR="%VCPKG_ROOT%\installed" -DCMAKE_PREFIX_PATH="%QT_PREFIX%"
if errorlevel 1 exit /b 1

echo ==> Building
if "%RUN_TESTS%"=="1" (
  cmake --build "%BUILD_WORK_DIR%" --config "%BUILD_CONFIG%" -- /m:1
) else (
  cmake --build "%BUILD_WORK_DIR%" --config "%BUILD_CONFIG%" --target pic-viewer -- /m:1
)
if errorlevel 1 exit /b 1

if "%RUN_TESTS%"=="1" (
  echo ==> Running tests
  ctest --test-dir "%BUILD_WORK_DIR%" -C "%BUILD_CONFIG%" --output-on-failure
  if errorlevel 1 exit /b 1
)

if exist "%BUILD_WORK_DIR%\%BUILD_CONFIG%\pic-viewer.exe" (
  echo ==> Build complete: %BUILD_WORK_DIR%\%BUILD_CONFIG%\pic-viewer.exe
) else (
  echo ==> Build complete
)
