@echo off
REM ============================================================
REM  Build script for ESP32 Study Manager (Windows)
REM ============================================================
REM
REM  Usage:
REM    build.bat          - normal windowed build from the .spec
REM    build.bat debug    - one-off console build to see tracebacks
REM
REM  The finished app appears in the "dist" folder.
REM ============================================================

setlocal

REM --- Make sure PyInstaller is available -------------------
python -m PyInstaller --version >nul 2>&1
if errorlevel 1 (
    echo PyInstaller not found. Installing...
    pip install pyinstaller
    if errorlevel 1 (
        echo Failed to install PyInstaller. Aborting.
        exit /b 1
    )
)

REM --- Clean previous build artifacts -----------------------
if exist build rmdir /s /q build
if exist dist rmdir /s /q dist

if /i "%1"=="debug" goto debug

REM --- Normal build from the spec file ----------------------
echo.
echo Building "ESP32 Study Manager" (windowed)...
echo.
python -m PyInstaller "ESP32 Study Manager.spec" --clean --noconfirm
goto done

:debug
REM --- Debug build: console visible, prints tracebacks ------
echo.
echo Building DEBUG (console) build...
echo.
python -m PyInstaller ^
  --name "ESP32 Study Manager (debug)" ^
  --onefile ^
  --console ^
  --add-data "web;web" ^
  --collect-all webview ^
  --collect-all pdfplumber ^
  --collect-all pdfminer ^
  --clean --noconfirm ^
  app.py
goto done

:done
if errorlevel 1 (
    echo.
    echo Build FAILED.
    exit /b 1
)

echo.
echo Build complete. See the "dist" folder.
endlocal
