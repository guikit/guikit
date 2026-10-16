@echo off
setlocal
cd /d "%~dp0..\.."

echo [guikit] Starting WebAssembly (Emscripten) build...
powershell -NoProfile -ExecutionPolicy Bypass -File "%CD%\scripts\build_wasm.ps1" -InstallEmsdk %*
if errorlevel 1 (
    echo [guikit] Web build failed!
    exit /b 1
)

endlocal
