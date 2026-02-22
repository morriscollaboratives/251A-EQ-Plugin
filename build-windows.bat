@echo off
REM ============================================================
REM  Langevin EQ-251A VST3 — Windows Build Script
REM
REM  Prerequisites:
REM    - Git for Windows (https://git-scm.com/download/win)
REM    - CMake 3.25+ (https://cmake.org/download/)
REM    - Visual Studio 2022 with "Desktop development with C++"
REM      (Community Edition is free)
REM
REM  Just double-click this file. It handles everything.
REM ============================================================

echo.
echo ============================================================
echo   Langevin EQ-251A VST3 — Windows Build
echo   Morris Collaboratives
echo ============================================================
echo.

REM Always work from the folder this script lives in,
REM even if launched from a different directory (e.g. System32).
cd /d "%~dp0"

REM ---- Check prerequisites ----

where git >nul 2>nul
if errorlevel 1 (
    echo ERROR: Git not found.
    echo Install from https://git-scm.com/download/win
    echo Then close and reopen this window.
    echo.
    pause
    exit /b 1
)

where cmake >nul 2>nul
if errorlevel 1 (
    echo ERROR: CMake not found.
    echo Install from https://cmake.org/download/
    echo Check "Add CMake to system PATH" during install.
    echo Then close and reopen this window.
    echo.
    pause
    exit /b 1
)

REM ---- Clone VST3 SDK if not present ----

if not exist "vst3sdk" (
    echo [1/4] Downloading VST3 SDK...
    git clone --recursive --depth 1 https://github.com/steinbergmedia/vst3sdk.git
    if errorlevel 1 (
        echo ERROR: Failed to download VST3 SDK.
        pause
        exit /b 1
    )
) else (
    echo [1/4] VST3 SDK already present.
)

REM ---- Configure ----

echo [2/4] Configuring...
if exist "build" rmdir /s /q build
mkdir build
cd build
cmake .. -A x64
if errorlevel 1 (
    echo ERROR: CMake configuration failed.
    echo Make sure Visual Studio is installed with
    echo "Desktop development with C++"
    cd ..
    pause
    exit /b 1
)

REM ---- Build ----

echo [3/4] Building plugin...
cmake --build . --config Release --target LangevinEQ251A
if errorlevel 1 (
    echo ERROR: Build failed.
    cd ..
    pause
    exit /b 1
)

REM ---- Install ----

echo [4/4] Installing...

set "VST3_SYS=%CommonProgramFiles%\VST3"
set "VST3_USER=%LOCALAPPDATA%\Programs\Common\VST3"
set "BUNDLE=VST3\Release\LangevinEQ251A.vst3"
set "INSTALLED=0"

if not exist "%BUNDLE%" (
    echo ERROR: Build output not found.
    cd ..
    pause
    exit /b 1
)

REM Try system-wide install first (requires admin)
if not exist "%VST3_SYS%" mkdir "%VST3_SYS%" >nul 2>&1
xcopy /E /I /Y "%BUNDLE%" "%VST3_SYS%\LangevinEQ251A.vst3" >nul 2>&1
if not errorlevel 1 (
    set "INSTALLED=1"
    echo   Installed to: %VST3_SYS%\LangevinEQ251A.vst3
)

REM Also install to per-user location (no admin needed)
if not exist "%VST3_USER%" mkdir "%VST3_USER%" >nul 2>&1
xcopy /E /I /Y "%BUNDLE%" "%VST3_USER%\LangevinEQ251A.vst3" >nul 2>&1
if not errorlevel 1 (
    set "INSTALLED=1"
    echo   Installed to: %VST3_USER%\LangevinEQ251A.vst3
)

cd ..

echo.
echo ============================================================
if "%INSTALLED%"=="1" (
    echo   BUILD SUCCESSFUL!
    echo.
    echo   Restart your DAW and scan for new plugins.
    echo   It will appear as "Langevin EQ-251A" under Fx ^> EQ.
) else (
    echo   Plugin built but could not install automatically.
    echo   Manually copy this folder into your VST3 directory:
    echo     build\VST3\Release\LangevinEQ251A.vst3
)
echo ============================================================
echo.
pause
