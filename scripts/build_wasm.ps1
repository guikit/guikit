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
    [switch]$Serve
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
        $env:EMSDK,
        (Join-Path $RepoRoot ".emsdk"),
        (Join-Path $HOME "emsdk"),
        "C:\emsdk"
    ) | Where-Object { $_ -and (Test-Path (Join-Path $_ "emsdk_env.ps1")) }

    if ($candidates.Count -gt 0) {
        $emsdkDir = $candidates[0]
        Write-Host "[WASM] Activating Emscripten SDK from $emsdkDir ..." -ForegroundColor Cyan
        & (Join-Path $emsdkDir "emsdk_env.ps1")
        return
    }

    if ($InstallEmsdk) {
        $localEmsdk = Join-Path $RepoRoot ".emsdk"
        if (-not (Test-Path $localEmsdk)) {
            Write-Host "[WASM] Cloning emsdk into $localEmsdk ..." -ForegroundColor Cyan
            git clone --depth 1 https://github.com/emscripten-core/emsdk.git $localEmsdk
        }
        Push-Location $localEmsdk
        try {
            & .\emsdk.ps1 install latest
            & .\emsdk.ps1 activate latest
            & .\emsdk_env.ps1
        } finally {
            Pop-Location
        }
        return
    }

    throw "Emscripten ('emcmake') not found in PATH. Run with -InstallEmsdk to install it automatically, or activate your emsdk environment first."
}

if ($Clean) {
    if (Test-Path $BuildDir) { Remove-Item -Recurse -Force $BuildDir }
    if (Test-Path $OutDir)   { Remove-Item -Recurse -Force $OutDir }
}

Find-Or-Activate-Emsdk

Write-Host "[WASM] Configuring CMake ($BuildType) ..." -ForegroundColor Cyan
& emcmake cmake -S $RepoRoot -B $BuildDir `
    -DCMAKE_BUILD_TYPE=$BuildType `
    -DGUIKIT_BUILD_DEMO=ON `
    -DGUIKIT_BUILD_TESTS=OFF
if ($LASTEXITCODE -ne 0) { throw "CMake configuration failed." }

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
Write-Host "[WASM] GitHub Pages bundle ready in: $OutDir" -ForegroundColor Green
Get-ChildItem $OutDir | Select-Object Name, @{Name="SizeKB";Expression={[math]::Round($_.Length / 1KB, 1)}} | Format-Table -AutoSize

if ($Serve) {
    Write-Host "[WASM] Serving http://localhost:8080 (Ctrl+C to stop) ..." -ForegroundColor Yellow
    python -m http.server 8080 --directory $OutDir
}
