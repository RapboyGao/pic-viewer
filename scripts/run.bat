@echo off
setlocal DisableDelayedExpansion

set "ROOT_DIR=%~dp0.."
for %%I in ("%ROOT_DIR%") do set "ROOT_DIR=%%~fI"
set "VSWHERE_EXE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"

if "%BUILD_DIR%"=="" set "BUILD_DIR=%ROOT_DIR%\build"
if "%BUILD_CONFIG%"=="" set "BUILD_CONFIG=Release"
set "APP_PATH=%BUILD_DIR%\%BUILD_CONFIG%\pic-viewer.exe"
set "VCPKG_ROOT=%ROOT_DIR%\.deps\vcpkg"
set "VS_INSTALL_DIR="
if exist "%VSWHERE_EXE%" (
  for /f "usebackq delims=" %%I in (`"%VSWHERE_EXE%" -latest -products * -property installationPath`) do (
    set "VS_INSTALL_DIR=%%I"
  )
)

if not exist "%APP_PATH%" (
  echo ==> App not found, building first
  set "RUN_TESTS=0"
  set "BUILD_CONFIG=%BUILD_CONFIG%"
  call "%ROOT_DIR%\scripts\build.bat"
  if errorlevel 1 exit /b 1
  set "APP_PATH=%BUILD_DIR%\%BUILD_CONFIG%\pic-viewer.exe"
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
if not "%VS_INSTALL_DIR%"=="" (
  if exist "%VS_INSTALL_DIR%\Common7\IDE" set "PATH=%VS_INSTALL_DIR%\Common7\IDE;%PATH%"
  for /f "delims=" %%I in ('dir /b /ad /o-d "%VS_INSTALL_DIR%\VC\Redist\MSVC" 2^>nul') do (
    if exist "%VS_INSTALL_DIR%\VC\Redist\MSVC\%%I\x64" (
      set "PATH=%VS_INSTALL_DIR%\VC\Redist\MSVC\%%I\x64;%PATH%"
      goto :redist_found
    )
  )
)
:redist_found

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
