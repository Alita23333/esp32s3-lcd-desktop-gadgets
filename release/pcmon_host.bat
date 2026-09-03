@echo off
chcp 65001 >nul
rem ============================================================
rem  GPU monitor host launcher (manual run, no auto-start)
rem  Double-click to start broadcasting to the secondary screen;
rem  close the window to stop.
rem ============================================================
rem  Relative path to project root venv and script (portable folder).
set "ROOT=%~dp0.."
set "PY=%ROOT%\.venv\Scripts\python.exe"
set "SCRIPT=%ROOT%\pcmon\pcmon_host.py"

if not exist "%PY%" (
  echo [ERROR] venv not found: %PY%
  echo Run in project root: py -3.12 -m venv .venv ^&^& .venv\Scripts\python.exe -m pip install nvidia-ml-py
  pause
  exit /b 1
)

echo Starting GPU data broadcaster ...  (stop: close this window)
"%PY%" "%SCRIPT%"
pause
