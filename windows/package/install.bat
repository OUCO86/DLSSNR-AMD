@echo off
rem DLSSNR-AMD: double-click, or drag the game's exe onto this file.
powershell -NoProfile -ExecutionPolicy Bypass -STA -File "%~dp0install.ps1" %*
pause
