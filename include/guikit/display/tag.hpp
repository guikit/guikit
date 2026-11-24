#pragma once

#include "guikit/context.hpp"
#include "guikit/core/control.hpp"
#include "guikit/core/response.hpp"
#include "guikit/style/theme.hpp"
#include "guikit/core/types.hpp"
#include "guikit/graphics/icon.hpp"
#include "keel/core.h"
#include "keel/optional.h"

namespace guikit {

class Context;
class ICanvas;
struct Node;

/// Visual surface style for `Tag` / badge chips.
enum class TagStyle : keel::u8 {
    Soft = 0, ///< Subtle semantic tinted fill with crisp hairline border (default)
    Outline,  ///< Transparent surface with semantic border and text
    Solid     ///< Filled semantic background with high-contrast foreground text
};

/// Configuration options for the atomic `Tag` (badge / chip) widget.
struct TagOptions {
    Icon icon{};                                     ///< Optional leading icon
    UiVariant variant{UiVariant::Default};           ///< Semantic color token (Default, Primary, Success, Warning, Danger, Info, Muted...)
    TagStyle style{TagStyle::Soft};                  ///< Surface fill style (Soft, Outline, Solid)
    UiSize size{UiSize::Sm};                         ///< Compact size token (default Sm)
    bool pill{false};                                ///< Full capsule corner radius (false = subtle rounded rectangle)
    bool clickable{false};                           ///< Enable hover/press feedback and click response
    bool closable{false};                            ///< Show trailing close ('x') icon; sets TagResponse::closed on click
    bool disabled{false};                            ///< Non-interactive dimmed state
    UiText tooltip{};                                ///< Optional hover tooltip
    StateColor color{};                              ///< Optional custom accent override
    Border border{.width = 1.0f};                    ///< Border width, radius & color override
    AxisSize width{};                                ///< Horizontal sizing (`Auto` -> `Fit`)
    AxisSize height{};                               ///< Vertical sizing (`Auto` -> `Fit`)
    keel::Optional<keel::f32> minWidth{keel::kNullOpt};
    keel::Optional<keel::f32> maxWidth{keel::kNullOpt};
    keel::Optional<keel::f32> minHeight{keel::kNullOpt};
    keel::Optional<keel::f32> maxHeight{keel::kNullOpt};
};

/// Extended response returned by `ui.Tag(...)`.
struct TagResponse : public Response {
    bool closed{false}; ///< True on the frame the trailing close ('x') button was clicked
};

/// Autonomous atomic Tag / Badge / Chip widget.
namespace detail {

class Tag {
public:
    static TagResponse Run(
        Context& ctx,
        UiText text,
        const TagOptions& opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    static Vec2 Measure(
        const UiText& text,
        const TagOptions& opts,
        UiSize size,
        ICanvas* canvas,
        const UiTheme& theme
    );

    static void Render(const Node* node, ICanvas& canvas, const UiTheme& theme);
};

} // namespace detail

inline TagResponse Tag(
    Context& ui,
    UiText text,
    TagOptions opts = {},
    keel::SourceLocation loc = keel::SourceLocation::current()
) {
    WidgetExecGuard guard(ui);
    return detail::Tag::Run(ui, text, opts, loc);
}

} // namespace guikit
