<#
.SYNOPSIS
    Builds the GuiKit interactive demo for WebAssembly (Emscripten) and packages it for GitHub Pages.

.PARAMETER BuildType
    CMake build configuration (Release or Debug). Default: Release.

.PARAMETER InstallEmsdk
    Automatically clone, install, and activate the latest Emscripten SDK into .emsdk/ if emcmake is not found.

.PARAMETER Clean
    Remove build_wasm/ and dist/web/ before building.

.PARAMETER Serve
    Start a local HTTP server on port 8080 after building to preview the WebAssembly bundle.
#>
param(
    [ValidateSet("Release", "Debug")]
    [string]$BuildType = "Release",
    [switch]$InstallEmsdk,
    [switch]$Clean,
    [switch]$Serve,
    [switch]$OpenBrowser,
    [switch]$SkipBuildIfPresent
)

$ErrorActionPreference = "Stop"
$RepoRoot = Split-Path -Parent $PSScriptRoot
$BuildDir = Join-Path $RepoRoot "build_wasm"
$OutDir   = Join-Path $RepoRoot "dist\web"

function Find-Or-Activate-Emsdk {
    if (Get-Command "emcmake" -ErrorAction SilentlyContinue) {
        return
    }

    $candidates = @(
        @(
            $env:EMSDK,
            (Join-Path $RepoRoot ".emsdk"),
            (Join-Path $HOME "emsdk"),
            "C:\emsdk"
        ) | Where-Object { $_ -and (Test-Path (Join-Path $_ "emsdk_env.ps1")) }
    )

    if ($candidates.Count -gt 0) {
        $emsdkDir = $candidates[0]
        Write-Host "[WASM] Activating Emscripten SDK from $emsdkDir ..." -ForegroundColor Cyan
        & (Join-Path $emsdkDir "emsdk_env.ps1")
        if (Get-Command "emcmake" -ErrorAction SilentlyContinue) {
            return
        }
        Write-Host "[WASM] 'emcmake' still not in PATH after emsdk_env.ps1 (SDK toolchain not installed/activated yet)." -ForegroundColor Yellow
    }

    if ($InstallEmsdk) {
        $localEmsdk = if ($candidates.Count -gt 0) { $candidates[0] } else { Join-Path $RepoRoot ".emsdk" }
        if (-not (Test-Path $localEmsdk)) {
            Write-Host "[WASM] Cloning emsdk into $localEmsdk ..." -ForegroundColor Cyan
            git clone --depth 1 https://github.com/emscripten-core/emsdk.git $localEmsdk
        }
        Push-Location $localEmsdk
        try {
            & .\emsdk.ps1 install latest
            if ($LASTEXITCODE -ne 0) { throw "emsdk install latest failed." }
            & .\emsdk.ps1 activate latest
            if ($LASTEXITCODE -ne 0) { throw "emsdk activate latest failed." }
            & .\emsdk_env.ps1
        } finally {
            Pop-Location
        }
        if (-not (Get-Command "emcmake" -ErrorAction SilentlyContinue)) {
            throw "Failed to activate 'emcmake' even after installing emsdk."
        }
        return
    }

    throw "Emscripten ('emcmake') not found in PATH. Run with -InstallEmsdk to install it automatically, or activate your emsdk environment first."
}

function Ensure-Ninja {
    if (Get-Command "ninja" -ErrorAction SilentlyContinue) {
        return
    }
    $vsNinja = Get-ChildItem -Path "C:\Program Files\Microsoft Visual Studio", "C:\Program Files (x86)\Microsoft Visual Studio" `
        -Filter "ninja.exe" -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($vsNinja) {
        $ninjaDir = Split-Path -Parent $vsNinja.FullName
        Write-Host "[WASM] Adding Visual Studio Ninja to PATH: $ninjaDir" -ForegroundColor Cyan
        $env:PATH = "$ninjaDir;$env:PATH"
    }
}

if ($Clean) {
    if (Test-Path $BuildDir) { Remove-Item -Recurse -Force $BuildDir }
    if (Test-Path $OutDir)   { Remove-Item -Recurse -Force $OutDir }
}

Ensure-Ninja
Find-Or-Activate-Emsdk

$IndexHtml = Join-Path $OutDir "index.html"
$WasmFile  = Join-Path $OutDir "guikit_demo.wasm"
$NeedBuild = (-not $SkipBuildIfPresent) -or (-not (Test-Path $IndexHtml)) -or (-not (Test-Path $WasmFile))

if ($NeedBuild) {
    $CacheFile = Join-Path $BuildDir "CMakeCache.txt"
    if ((Test-Path $CacheFile) -and (Select-String -Path $CacheFile -Pattern '\$BuildType' -Quiet)) {
        Remove-Item -Recurse -Force $BuildDir
    }

    if (-not (Test-Path $CacheFile)) {
        Write-Host "[WASM] Configuring CMake ($BuildType) ..." -ForegroundColor Cyan
        & emcmake cmake -G Ninja -S $RepoRoot -B $BuildDir `
            "-DCMAKE_BUILD_TYPE=$BuildType" `
            "-DCMAKE_CXX_SCAN_FOR_MODULES=OFF" `
            "-DGUIKIT_BUILD_DEMO=ON" `
            "-DGUIKIT_BUILD_TESTS=OFF"
        if ($LASTEXITCODE -ne 0) { throw "CMake configuration failed." }
    }

    Write-Host "[WASM] Building guikit_demo ..." -ForegroundColor Cyan
    & cmake --build $BuildDir --target guikit_demo --parallel
    if ($LASTEXITCODE -ne 0) { throw "WebAssembly build failed." }

    New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

    Copy-Item (Join-Path $BuildDir "guikit_demo.html") (Join-Path $OutDir "index.html") -Force
    Copy-Item (Join-Path $BuildDir "guikit_demo.js")   (Join-Path $OutDir "guikit_demo.js") -Force
    Copy-Item (Join-Path $BuildDir "guikit_demo.wasm") (Join-Path $OutDir "guikit_demo.wasm") -Force
    if (Test-Path (Join-Path $BuildDir "guikit_demo.data")) {
        Copy-Item (Join-Path $BuildDir "guikit_demo.data") (Join-Path $OutDir "guikit_demo.data") -Force
    }

    # Create .nojekyll so GitHub Pages serves .wasm and .data files without Jekyll processing
    Set-Content -Path (Join-Path $OutDir ".nojekyll") -Value "" -NoNewline

    Write-Host ""
    Write-Host "[WASM] WebAssembly bundle ready in: $OutDir" -ForegroundColor Green
    Get-ChildItem $OutDir | Select-Object Name, @{Name="SizeKB";Expression={[math]::Round($_.Length / 1KB, 1)}} | Format-Table -AutoSize
} else {
    Write-Host "[WASM] Using existing WebAssembly bundle in: $OutDir" -ForegroundColor Green
}

if ($Serve) {
    $Port = 8080
    $Url = "http://localhost:$Port/index.html"
    Write-Host "[WASM] Serving $Url (Ctrl+C to stop) ..." -ForegroundColor Yellow
    if ($OpenBrowser) {
        Start-Process $Url
    }
    $PythonCmd = if ($env:EMSDK_PYTHON -and (Test-Path $env:EMSDK_PYTHON)) {
        $env:EMSDK_PYTHON
    } else {
        "python"
    }
    & $PythonCmd -m http.server $Port --directory $OutDir
}
