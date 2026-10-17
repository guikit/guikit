@echo off
setlocal
cd /d "%~dp0..\.."

echo [guikit] Building WebAssembly bundle and publishing to GitHub Pages (guikit/demo)...
powershell -NoProfile -ExecutionPolicy Bypass -File "%CD%\scripts\publish_demo.ps1" %*
if errorlevel 1 (
    echo [guikit] Web publish failed!
    exit /b 1
)

endlocal
