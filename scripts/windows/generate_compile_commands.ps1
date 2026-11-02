# Generates compile_commands.json for clangd and VSCode / Antigravity IntelliSense

$files = @(
    "src/guikit/context.cpp",
    "src/guikit/core/main_window.cpp",
    "src/guikit/core/theme.cpp",
    "src/guikit/backend/thor_canvas.cpp",
    "src/guikit/backend/sdl_canvas.cpp",
    "src/guikit/backend/sdl_window_backend.cpp",
    "src/guikit/backend/sdl_gl_shader_engine.cpp",
    "src/guikit/graphics/icon.cpp",
    "src/guikit/layout/row.cpp",
    "src/guikit/layout/column.cpp",
    "src/guikit/layout/wrap.cpp",
    "src/guikit/layout/grid.cpp",
    "src/guikit/layout/spacer.cpp",
    "src/guikit/panels/carousel.cpp",
    "src/guikit/panels/scroll_area.cpp",
    "src/guikit/widgets/button.cpp",
    "src/guikit/widgets/checkbox.cpp",
    "src/guikit/widgets/heading.cpp",
    "src/guikit/widgets/input_field.cpp",
    "src/guikit/widgets/select.cpp",
    "src/guikit/widgets/switch.cpp",
    "src/guikit/widgets/tab_bar.cpp",
    "src/guikit/widgets/text.cpp",
    "src/guikit/panels/toolbar.cpp",
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
