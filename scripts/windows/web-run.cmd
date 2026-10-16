@echo off
setlocal
cd /d "%~dp0..\.."

echo [guikit] Building WebAssembly bundle and launching in browser...
powershell -NoProfile -ExecutionPolicy Bypass -File "%CD%\scripts\build_wasm.ps1" -InstallEmsdk -Serve -OpenBrowser %*
if errorlevel 1 (
    echo [guikit] Web run failed!
    exit /b 1
)

endlocal
