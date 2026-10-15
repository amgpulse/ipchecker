@echo off
REM ==========================================================
REM  IP Inspector Pro - Windows Build Script
REM  Requires: Qt6 (with mingw or MSVC) installed
REM  Run this script from Qt command prompt or set QT_PATH
REM ==========================================================

set QT_PATH=C:\Qt
set QT_VERSION=6.7.0
set COMPILER=mingw_64

echo ========================================
echo  IP Inspector Pro - Windows Build
echo ========================================

REM Always run from the repository root (this script lives in packaging\windows)
cd /d "%~dp0\..\.."

REM Find qmake (Qt for Windows ships "qmake.exe", not "qmake6.exe")
where qmake6 >nul 2>nul
if %errorlevel% neq 0 (
    where qmake >nul 2>nul
)

if exist "%QT_PATH%\%QT_VERSION%\%COMPILER%\bin\qmake6.exe" (
    set PATH=%QT_PATH%\%QT_VERSION%\%COMPILER%\bin;%PATH%
    echo Using Qt from: %QT_PATH%\%QT_VERSION%\%COMPILER%\bin
) else if exist "%QT_PATH%\%QT_VERSION%\%COMPILER%\bin\qmake.exe" (
    set PATH=%QT_PATH%\%QT_VERSION%\%COMPILER%\bin;%PATH%
    echo Using Qt from: %QT_PATH%\%QT_VERSION%\%COMPILER%\bin
) else (
    echo WARNING: Qt path not found at default location.
    echo Trying system PATH qmake...
)

echo.
echo [1/3] Running qmake...
qmake ipchecker.pro -spec win32-g++ CONFIG+=release 2>nul || qmake6 ipchecker.pro -spec win32-g++ CONFIG+=release
if %errorlevel% neq 0 (
    echo ERROR: qmake failed!
    pause
    exit /b 1
)

echo [2/3] Building...
mingw32-make -j4 2>nul || make -j4 2>nul || nmake 2>nul
if %errorlevel% neq 0 (
    echo ERROR: Build failed!
    pause
    exit /b 1
)

echo [3/3] Deploying Qt libraries...
echo Copying required Qt DLLs next to executable...
windeployqt6.exe --release --no-translations --no-opengl-sw ./release/ipchecker.exe 2>nul || windeployqt.exe --release --no-translations --no-opengl-sw ./release/ipchecker.exe 2>nul || windeployqt6.exe --release ./release/ipchecker.exe 2>nul || windeployqt.exe --release ./release/ipchecker.exe 2>nul

echo.
echo ========================================
echo  Build complete!
echo  Output: release\ipchecker.exe
echo ========================================
pause
