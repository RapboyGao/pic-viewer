@echo off
setlocal enabledelayedexpansion

set "ROOT_DIR=%~dp0.."
for %%I in ("%ROOT_DIR%") do set "ROOT_DIR=%%~fI"
set "VCPKG_ROOT=%ROOT_DIR%\.deps\vcpkg"
set "VCPKG_TOOLCHAIN=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake"
set "VCPKG_DEFAULT_PREFIX=%ROOT_DIR%\vcpkg_installed\x64-windows"
set "MSBUILD_EXE="
set "VSWHERE_EXE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"

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

if not defined MSBUILD_EXE (
  if exist "%VSWHERE_EXE%" (
    for /f "usebackq delims=" %%I in (`"%VSWHERE_EXE%" -latest -products * -requires Microsoft.Component.MSBuild -find MSBuild\Current\Bin\amd64\MSBuild.exe`) do (
      set "MSBUILD_EXE=%%I"
    )
  )
)

if not defined MSBUILD_EXE (
  for /f "delims=" %%I in ('where msbuild.exe 2^>nul') do (
    set "MSBUILD_EXE=%%I"
    goto :msbuild_found
  )
)
:msbuild_found

if not exist "%MSBUILD_EXE%" (
  echo error: MSBuild not found. Set MSBUILD_EXE or install Visual Studio Build Tools.
  exit /b 1
)

if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"
mkdir "%BUILD_WORK_DIR%"

rem Stop any running copies so the linker can overwrite the executables.
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
cmake -S "%ROOT_DIR%" -B "%BUILD_WORK_DIR%" -DCMAKE_TOOLCHAIN_FILE="%VCPKG_TOOLCHAIN%" -DVCPKG_MANIFEST_DIR="%ROOT_DIR%" -DVCPKG_INSTALLED_DIR="%VCPKG_ROOT%\installed" -DVCPKG_APPLOCAL_DEPS=OFF -DCMAKE_PREFIX_PATH="%QT_PREFIX%"
if errorlevel 1 exit /b 1

echo ==> Building
if "%RUN_TESTS%"=="1" (
  "%MSBUILD_EXE%" "%BUILD_WORK_DIR%\pic-viewer.vcxproj" /t:Build /p:Configuration=%BUILD_CONFIG% /p:Platform=x64 /m:1 /nologo
) else (
  "%MSBUILD_EXE%" "%BUILD_WORK_DIR%\pic-viewer.vcxproj" /t:Build /p:Configuration=%BUILD_CONFIG% /p:Platform=x64 /m:1 /nologo
)
if errorlevel 1 exit /b 1

if "%RUN_TESTS%"=="1" (
  "%MSBUILD_EXE%" "%BUILD_WORK_DIR%\test_image_catalog.vcxproj" /t:Build /p:Configuration=%BUILD_CONFIG% /p:Platform=x64 /m:1 /nologo
  if errorlevel 1 exit /b 1
  "%MSBUILD_EXE%" "%BUILD_WORK_DIR%\test_slide_show_controller.vcxproj" /t:Build /p:Configuration=%BUILD_CONFIG% /p:Platform=x64 /m:1 /nologo
  if errorlevel 1 exit /b 1
  "%MSBUILD_EXE%" "%BUILD_WORK_DIR%\test_prefetch_scheduler.vcxproj" /t:Build /p:Configuration=%BUILD_CONFIG% /p:Platform=x64 /m:1 /nologo
  if errorlevel 1 exit /b 1
  "%MSBUILD_EXE%" "%BUILD_WORK_DIR%\test_image_decoder.vcxproj" /t:Build /p:Configuration=%BUILD_CONFIG% /p:Platform=x64 /m:1 /nologo
  if errorlevel 1 exit /b 1
)

if "%RUN_TESTS%"=="1" (
  echo ==> Running tests
  ctest --test-dir "%BUILD_WORK_DIR%" -C "%BUILD_CONFIG%" --output-on-failure
  if errorlevel 1 exit /b 1
)
set "OUTPUT_EXE=%BUILD_WORK_DIR%\%BUILD_CONFIG%\pic-viewer.exe"
if exist "%OUTPUT_EXE%" (
  echo ==> Build complete: %OUTPUT_EXE%
) else (
  echo ==> Build complete, but %OUTPUT_EXE% was not found
)
exit /b 0
