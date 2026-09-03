@echo off
chcp 65001 >nul
rem GPU telemetry broadcaster. Runs minimized.
rem Double-click to start; shell:startup + shortcut for auto-run.
rem Uses relative venv path so the whole project folder stays portable.
set "PY=%~dp0..\.venv\Scripts\python.exe"
set "SCRIPT=%~dp0pcmon_host.py"

if not exist "%PY%" (
  echo [ERROR] venv not found: %PY%
  echo Run: py -3.12 -m venv ..\\.venv  then  ..\\.venv\Scripts\python.exe -m pip install nvidia-ml-py
  pause
  exit /b 1
)

echo Starting GPU telemetry broadcaster ...
"%PY%" "%SCRIPT%"
pause
