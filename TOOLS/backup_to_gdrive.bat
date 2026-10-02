@echo off
setlocal
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0backup_to_gdrive.ps1"
exit /b %ERRORLEVEL%
