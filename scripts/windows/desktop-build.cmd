@echo off
setlocal
cd /d "%~dp0..\.."

set CONFIG=Release
if /I "%~1"=="Debug" set CONFIG=Debug

echo ========================================================
echo  GuiKit Desktop Build (%CONFIG%)
echo ========================================================

:: Stop running guikit_minimal.exe if open so linker can overwrite it cleanly
taskkill /F /IM guikit_minimal.exe >nul 2>&1

if not exist "build\guikit.sln" if not exist "build\build.ninja" if not exist "build\Makefile" (
    echo [INFO] Configuring CMake build directory...
    cmake -B build -S .
    if errorlevel 1 (
        echo [ERROR] CMake configuration failed!
        exit /b 1
    )
)

echo [INFO] Building GuiKit library, minimal example & tests (%CONFIG%)...
cmake --build build --config %CONFIG% --parallel
if errorlevel 1 (
    echo [ERROR] Desktop build failed!
    exit /b 1
)

echo.
echo ========================================================
echo  Desktop build succeeded!
echo  Minimal example: build\%CONFIG%\guikit_minimal.exe
echo ========================================================
endlocal
