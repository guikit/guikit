@echo off
setlocal
cd /d "%~dp0..\.."

set CONFIG=Release
if /I "%~1"=="Debug" set CONFIG=Debug

echo ========================================================
echo  GuiKit Desktop Build (%CONFIG%)
echo ========================================================

:: Stop running guikit_demo.exe if open so linker can overwrite it cleanly
taskkill /F /IM guikit_demo.exe >nul 2>&1

if not exist "build\CMakeCache.txt" (
    echo [INFO] Configuring CMake build directory...
    cmake -B build -S .
    if errorlevel 1 (
        echo [ERROR] CMake configuration failed!
        exit /b 1
    )
)

echo [INFO] Building guikit_demo (%CONFIG%)...
cmake --build build --config %CONFIG% --target guikit_demo --parallel
if errorlevel 1 (
    echo [ERROR] Desktop build failed!
    exit /b 1
)

echo.
echo ========================================================
echo  Desktop build succeeded!
echo  Binary: build\%CONFIG%\guikit_demo.exe
echo ========================================================
endlocal
