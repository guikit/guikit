@echo off
setlocal
cd /d "%~dp0\..\.."

set "GUIKIT_BIN="
if exist "build\Release\guikit_demo.exe" set "GUIKIT_BIN=build\Release\guikit_demo.exe"
if exist "build\Debug\guikit_demo.exe" (
    if not defined GUIKIT_BIN set "GUIKIT_BIN=build\Debug\guikit_demo.exe"
)

if not defined GUIKIT_BIN (
    echo [WARN] guikit_demo.exe not found. Building target first...
    call "%~dp0build.cmd" Debug
    if errorlevel 1 (
        echo [ERROR] Build failed, cannot launch guikit_demo.
        pause
        exit /b 1
    )
    if exist "build\Debug\guikit_demo.exe" set "GUIKIT_BIN=build\Debug\guikit_demo.exe"
    if exist "build\Release\guikit_demo.exe" set "GUIKIT_BIN=build\Release\guikit_demo.exe"
)

if not defined GUIKIT_BIN (
    echo [ERROR] guikit_demo.exe binary still not found.
    pause
    exit /b 1
)

echo ========================================================
echo  Launching GuiKit Vector GUI Demo
echo  Executable: %GUIKIT_BIN%
echo ========================================================
start "" "%GUIKIT_BIN%" %*
endlocal
