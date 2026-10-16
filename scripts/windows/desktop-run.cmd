@echo off
setlocal
cd /d "%~dp0..\.."

set "ROOT_DIR=%CD%"
set "CONFIG=%~1"
if "%CONFIG%"=="" set "CONFIG=Release"
set "EXE_PATH=%ROOT_DIR%\build\%CONFIG%\guikit_demo.exe"

if not exist "%EXE_PATH%" (
    echo [guikit] "%EXE_PATH%" not found. Building %CONFIG% first...
    call "%~dp0desktop-build.cmd" "%CONFIG%"
    if errorlevel 1 exit /b 1
)

echo [guikit] Launching Desktop Demo: "%EXE_PATH%"
start "" /D "%ROOT_DIR%" "%EXE_PATH%"
endlocal
