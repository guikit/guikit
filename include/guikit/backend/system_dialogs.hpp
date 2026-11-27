#pragma once

#include "keel/array.h"
#include "keel/core.h"
#include "keel/function.h"
#include "keel/ref.h"
#include "keel/string.h"

namespace guikit {

class Context;

/// Semantic hit-test region of a top-level window for native Client-Side Decorations (CSD).
/// Returned by `WindowHostCallbacks::onHitTest` / `Context::ResolveWindowHitRegion`.
enum class WindowHitRegion : keel::u8 {
    Client = 0,         ///< Normal interactive client area
    Caption,            ///< Free title-bar area: OS window drag, double-click maximize, system menu on right-click
    ResizeLeft,
    ResizeRight,
    ResizeTop,
    ResizeBottom,
    ResizeTopLeft,
    ResizeTopRight,
    ResizeBottomLeft,
    ResizeBottomRight,
    MinimizeButton,
    MaximizeButton,
    CloseButton
};

/// Single filename filter entry for OS file open/save dialogs.
struct FileDialogFilter {
    keel::StringRef name{};     ///< Human-readable filter label (e.g. "C++ Sources")
    keel::StringRef pattern{};  ///< Semicolon-separated wildcard patterns (e.g. "*.cpp;*.hpp;*.h")
};

/// Kind of native OS file/folder picker dialog.
enum class FileDialogKind : keel::u8 {
    OpenFile = 0,
    OpenFiles,
    SaveFile,
    SelectFolder
};

/// Configuration options for native OS file and folder picker dialogs.
struct FileDialogOptions {
    FileDialogKind kind{FileDialogKind::OpenFile};
    keel::StringRef title{};        ///< Optional dialog title (UTF-8)
    keel::StringRef defaultPath{};  ///< Optional initial directory or file path (UTF-8)
    keel::StringRef defaultName{};  ///< Optional default filename for `SaveFile` (UTF-8)
    keel::ArrayRef<FileDialogFilter> filters{};
};

/// Result returned from a native OS file or folder dialog.
struct FileDialogResult {
    bool accepted{false};             ///< True when the user confirmed a selection
    bool supported{true};             ///< False on headless or unsupported platforms
    keel::Array<keel::String> paths;  ///< Selected UTF-8 paths (1+ for OpenFiles, 1 for OpenFile/SaveFile/SelectFolder)
};

/// Standard button combinations for native OS message boxes.
enum class MessageBoxButtons : keel::u8 {
    Ok = 0,
    OkCancel,
    YesNo,
    YesNoCancel
};

/// Semantic icon style for native OS message boxes.
enum class MessageBoxIcon : keel::u8 {
    Info = 0,
    Warning,
    Error,
    Question
};

/// User choice returned from a native OS message box.
enum class MessageBoxChoice : keel::u8 {
    Cancel = 0,
    Ok,
    Yes,
    No
};

/// Configuration options for a native OS message box.
struct MessageBoxOptions {
    keel::StringRef title{};
    keel::StringRef message{};
    MessageBoxButtons buttons{MessageBoxButtons::Ok};
    MessageBoxIcon icon{MessageBoxIcon::Info};
};

// ==============================================================================
// Free-Function Facade (routed through Context's WindowHostService)
// ==============================================================================

/// Displays a blocking native OS file/folder dialog matching `opts.kind`.
[[nodiscard]] FileDialogResult ShowFileDialog(Context& ui, const FileDialogOptions& opts = {});

/// Displays a blocking native OS single-file open dialog.
[[nodiscard]] FileDialogResult ShowOpenFileDialog(Context& ui, FileDialogOptions opts = {});

/// Displays a blocking native OS multi-file open dialog.
[[nodiscard]] FileDialogResult ShowOpenFilesDialog(Context& ui, FileDialogOptions opts = {});

/// Displays a blocking native OS save-file dialog.
[[nodiscard]] FileDialogResult ShowSaveFileDialog(Context& ui, FileDialogOptions opts = {});

/// Displays a blocking native OS folder selection dialog.
[[nodiscard]] FileDialogResult ShowSelectFolderDialog(Context& ui, FileDialogOptions opts = {});

/// Displays a non-blocking native OS file/folder dialog and invokes `onDone` on the main UI thread.
/// Recommended for SDL3 and WebAssembly where native dialogs are asynchronous.
void ShowFileDialogAsync(
    Context& ui,
    FileDialogOptions opts,
    keel::Function<void(FileDialogResult)> onDone
);

/// Displays a native OS message box anchored to the active window.
[[nodiscard]] MessageBoxChoice ShowMessageBox(Context& ui, const MessageBoxOptions& opts);

} // namespace guikit
