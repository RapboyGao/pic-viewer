@echo off
setlocal

powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0install-dev-env.ps1" %*
exit /b %errorlevel%
