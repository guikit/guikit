# Generates compile_commands.json for clangd and VSCode / Antigravity IntelliSense

$files = @(
    "src/guikit/Context.cpp",
    "src/guikit/core/MainWindow.cpp",
    "src/guikit/core/Theme.cpp",
    "src/guikit/backend/ThorCanvas.cpp",
    "src/guikit/backend/SdlCanvas.cpp",
    "src/guikit/backend/SdlWindowBackend.cpp",
    "src/guikit/backend/SdlGlShaderEngine.cpp",
    "src/guikit/graphics/Icon.cpp",
    "src/guikit/layout/Row.cpp",
    "src/guikit/layout/Column.cpp",
    "src/guikit/layout/Wrap.cpp",
    "src/guikit/layout/Grid.cpp",
    "src/guikit/layout/Spacer.cpp",
    "src/guikit/panels/Carousel.cpp",
    "src/guikit/panels/ScrollArea.cpp",
    "src/guikit/widgets/Button.cpp",
    "src/guikit/widgets/Checkbox.cpp",
    "src/guikit/widgets/Heading.cpp",
    "src/guikit/widgets/InputField.cpp",
    "src/guikit/widgets/Select.cpp",
    "src/guikit/widgets/Switch.cpp",
    "src/guikit/widgets/TabBar.cpp",
    "src/guikit/widgets/Text.cpp",
    "src/guikit/widgets/Toolbar.cpp",
    "examples/demo/main.cpp",
    "tests/test_core.cpp",
    "tests/test_layout.cpp",
    "tests/test_context.cpp"
)

$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$cwd = $RepoRoot.Replace("\", "/")
$commands = @()

foreach ($f in $files) {
    $commands += [PSCustomObject]@{
        directory = $cwd
        command = "clang++ -std=c++20 -Iinclude -Ithird_party/SDL/include -Ithird_party/thorvg/inc -c $f"
        file = "$cwd/$f"
    }
}

$commands | ConvertTo-Json -Depth 4 | Out-File -Encoding utf8 (Join-Path $RepoRoot "compile_commands.json")
Write-Output "Successfully generated compile_commands.json with $($files.Count) compilation units."
