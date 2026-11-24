#pragma once

#include "guikit/core/types.hpp"
#include "guikit/graphics/icon.hpp"
#include "guikit/style/theme.hpp"
#include "guikit/style/tokens.hpp"
#include "keel/core.h"

namespace guikit {

class ICanvas;

/// Unified frame parameters shared by all input & selection widgets (`fields/`).
struct FieldFrameParams {
    Rect rect{};
    FieldStyle style{FieldStyle::Default};
    UiVariant status{UiVariant::Default};
    StateColor background{};
    Border border{};
    keel::f32 defaultRadius{4.0f};
    keel::f32 defaultStrokeWidth{1.0f};
    Color defaultBgOverride{Color::Transparent()}; ///< Optional custom base fill (e.g. SyntaxPalette editorBg)
    bool hovered{false};
    bool focused{false};
    bool active{false};                            ///< Popup open or pressed trigger state
    bool disabled{false};
};

/// Shared rendering helper for all `fields/` controls (`InputField`, `TextArea`, `Select`,
/// `MultiSelect`, `SpinBox`, `DatePicker`, `TagInput`, `ColorPicker`).
/// Guarantees consistent `FieldStyle`, `UiVariant status`, `theme.disabledOpacity`,
/// flat `.background`/`.border` overrides, and 100% `IStylePainter` routing.
class FieldFrame {
public:
    /// Resolves semantic accent color for a field's `status` (falls back to `theme.palette.accent`).
    [[nodiscard]] static Color ResolveAccent(
        const UiTheme& theme,
        UiVariant status,
        bool disabled = false
    ) noexcept;

    /// Draws the input well background, border/bevel/bottom-accent, and focus indicator via `theme.Painter()`.
    static void Draw(
        ICanvas& canvas,
        const UiTheme& theme,
        const FieldFrameParams& p
    );

    /// Draws an embedded right-side field trigger button (chevron/calendar) via `theme.Painter().DrawFieldButton(...)`
    /// and renders `icon`, returning the remaining `rightX` coordinate for field content.
    [[nodiscard]] static keel::f32 DrawTrigger(
        ICanvas& canvas,
        const UiTheme& theme,
        Rect fieldRect,
        const Icon& icon,
        keel::f32 paddingX,
        keel::f32 iconSize,
        keel::f32 cornerRadius,
        bool explicitRadius,
        bool hovered,
        bool pressed,
        bool active,
        bool disabled,
        bool showHoverPill,
        Color fallbackIconCol
    );
};

} // namespace guikit
