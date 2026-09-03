@echo off
chcp 65001 >nul
rem ============================================================
rem  One-click flash: Motion snake
rem  Merged image, written from 0x0.
rem  Board connected via USB. No need to press BOOT manually.
rem ============================================================
set "PORT=COM5"
set "PY=%~dp0..\.venv\Scripts\python.exe"
set "ESPTOOL=%USERPROFILE%\.platformio\packages\tool-esptoolpy\esptool.py"
set "BIN=%~dp0motion-snake-firmware.bin"

if not exist "%BIN%" (
  echo [ERROR] firmware not found: %BIN%
  pause
  exit /b 1
)
if not exist "%PY%" (
  echo [ERROR] venv not found: %PY%
  pause
  exit /b 1
)

echo Flashing %BIN% to %PORT% ...
"%PY%" "%ESPTOOL%" --chip esp32s3 --port %PORT% --baud 460800 --before default_reset --after hard_reset write_flash 0x0 "%BIN%"
if errorlevel 1 (
  echo [FAILED] check wiring / serial port / other program holding it
  echo If it keeps failing: hold BOOT then re-plug USB, then retry
) else (
  echo [OK] flash done, board reset
)
pause
