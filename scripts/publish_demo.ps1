<#
.SYNOPSIS
    Builds the GuiKit WebAssembly showcase demo and publishes it to GitHub Pages (guikit/demo).

.DESCRIPTION
    1. Configures (if needed) and builds `guikit_demo` in `build_wasm` using Emscripten.
    2. Stages `index.html`, `guikit_demo.html`, `guikit_demo.js`, `guikit_demo.wasm`, `.nojekyll`,
       and `README.md` into `dist/gh-pages`.
    3. Commits and force-pushes the static bundle to `git@github.com:guikit/demo.git`
       on both `main` and `gh-pages` branches.

.PARAMETER RemoteUrl
    Target Git repository URL for GitHub Pages deployment.
    Default: git@github.com:guikit/demo.git

.PARAMETER SkipBuild
    If set, skips running CMake build and deploys existing artifacts from `build_wasm/`.
#>
param(
    [string]$RemoteUrl = "git@github.com:guikit/demo.git",
    [switch]$SkipBuild = $false
)

$ErrorActionPreference = "Stop"

$RepoRoot  = Split-Path -Parent $PSScriptRoot
$BuildDir  = Join-Path $RepoRoot "build_wasm"
$StageDir  = Join-Path $RepoRoot "dist\gh-pages"
$EmsdkDir  = Join-Path $RepoRoot ".emsdk"

Write-Host "==> [1/4] Preparing WebAssembly build..." -ForegroundColor Cyan

if (-not $SkipBuild) {
    # Activate local .emsdk environment if emcmake is not yet in PATH
    if (-not (Get-Command "emcmake" -ErrorAction SilentlyContinue)) {
        $EmsdkEnv = Join-Path $EmsdkDir "emsdk_env.ps1"
        if (Test-Path $EmsdkEnv) {
            Write-Host "    Activating local Emscripten SDK ($EmsdkDir)..." -ForegroundColor Gray
            & $EmsdkEnv | Out-Null
        }
    }

    # Configure build_wasm if Ninja build file does not exist
    $NinjaFile = Join-Path $BuildDir "build.ninja"
    if (-not (Test-Path $NinjaFile)) {
        Write-Host "    Configuring CMake in $BuildDir..." -ForegroundColor Gray
        $EmcmakeBat = Join-Path $EmsdkDir "upstream\emscripten\emcmake.bat"
        if (Test-Path $EmcmakeBat) {
            & $EmcmakeBat cmake -S $RepoRoot -B $BuildDir -G Ninja -DCMAKE_BUILD_TYPE=Release -DGUIKIT_BUILD_TESTS=OFF
        } else {
            emcmake cmake -S $RepoRoot -B $BuildDir -G Ninja -DCMAKE_BUILD_TYPE=Release -DGUIKIT_BUILD_TESTS=OFF
        }
        if ($LASTEXITCODE -ne 0) { throw "CMake configuration failed with exit code $LASTEXITCODE" }
    }

    Write-Host "    Building target guikit_demo (Release)..." -ForegroundColor Gray
    cmake --build $BuildDir --config Release --target guikit_demo
    if ($LASTEXITCODE -ne 0) { throw "WASM build failed with exit code $LASTEXITCODE" }
} else {
    Write-Host "    Skipping build (-SkipBuild specified)." -ForegroundColor Yellow
}

# Verify required artifacts exist
$HtmlFile = Join-Path $BuildDir "guikit_demo.html"
$JsFile   = Join-Path $BuildDir "guikit_demo.js"
$WasmFile = Join-Path $BuildDir "guikit_demo.wasm"
$DataFile = Join-Path $BuildDir "guikit_demo.data"

foreach ($f in @($HtmlFile, $JsFile, $WasmFile)) {
    if (-not (Test-Path $f)) {
        throw "Required WebAssembly artifact not found: $f"
    }
}

Write-Host "==> [2/4] Staging static bundle in $StageDir..." -ForegroundColor Cyan

if (Test-Path $StageDir) {
    Remove-Item -Path $StageDir -Recurse -Force
}
New-Item -ItemType Directory -Path $StageDir -Force | Out-Null

# Copy index.html (for root URL) and guikit_demo.*
Copy-Item $HtmlFile (Join-Path $StageDir "index.html") -Force
Copy-Item $HtmlFile (Join-Path $StageDir "guikit_demo.html") -Force
Copy-Item $JsFile   (Join-Path $StageDir "guikit_demo.js") -Force
Copy-Item $WasmFile (Join-Path $StageDir "guikit_demo.wasm") -Force
if (Test-Path $DataFile) {
    Copy-Item $DataFile (Join-Path $StageDir "guikit_demo.data") -Force
}

# Create .nojekyll so GitHub Pages serves .wasm and .js without Jekyll filtering
New-Item -ItemType File -Path (Join-Path $StageDir ".nojekyll") -Force | Out-Null

# Resolve source repository commit hash for traceability
$SourceCommit = "unknown"
try {
    $SourceCommit = (git -C $RepoRoot rev-parse --short HEAD 2>$null).Trim()
} catch {}

$WasmSizeMb = [math]::Round((Get-Item $WasmFile).Length / 1MB, 2)
$UtcNow     = (Get-Date).ToUniversalTime().ToString("yyyy-MM-dd HH:mm:ss 'UTC'")

$ReadmeContent = @"
# GuiKit — WebAssembly Interactive Demo

Live WebAssembly build of **GuiKit** (`guikit_demo`), compiled with Emscripten + SDL3 + ThorVG + WebGL2.

- **Live Demo (GitHub Pages)**: [https://guikit.github.io/demo/](https://guikit.github.io/demo/)
- **Source Repository**: `guikit/guikit` (`@$SourceCommit`)
- **Bundle Size**: `guikit_demo.wasm` ($WasmSizeMb MB uncompressed, embedded fonts + images + S-57/ENC GeoJSON chart)
- **Last Published**: $UtcNow
"@
Set-Content -Path (Join-Path $StageDir "README.md") -Value $ReadmeContent -Encoding UTF8

Write-Host "==> [3/4] Creating deployment commit (source @$SourceCommit)..." -ForegroundColor Cyan

Push-Location $StageDir
try {
    git init -b main | Out-Null
    git add -A
    git commit -m "deploy(wasm): publish guikit_demo @$SourceCommit ($UtcNow)" | Out-Null

    Write-Host "==> [4/4] Pushing to $RemoteUrl (branches: main, gh-pages)..." -ForegroundColor Cyan
    git remote add origin $RemoteUrl

    # Retry push up to 3 times in case of transient SSH port 22 timeouts
    $MaxAttempts = 3
    $Pushed = $false
    for ($Attempt = 1; $Attempt -le $MaxAttempts; $Attempt++) {
        git push --force origin HEAD:main HEAD:gh-pages
        if ($LASTEXITCODE -eq 0) {
            $Pushed = $true
            break
        }
        if ($Attempt -lt $MaxAttempts) {
            Write-Host "    Push attempt $Attempt failed, retrying in 2s..." -ForegroundColor Yellow
            Start-Sleep -Seconds 2
        }
    }

    if (-not $Pushed) {
        throw "Failed to push to $RemoteUrl after $MaxAttempts attempts."
    }
} finally {
    Pop-Location
}

Write-Host ""
Write-Host "[OK] Published successfully to $RemoteUrl!" -ForegroundColor Green
Write-Host "     URL: https://guikit.github.io/demo/" -ForegroundColor Green
