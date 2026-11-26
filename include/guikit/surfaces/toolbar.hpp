#pragma once

#include "guikit/actions/split_button.hpp"
#include "guikit/context.hpp"
#include "guikit/core/container_scope.hpp"
#include "guikit/core/response.hpp"
#include "guikit/style/theme.hpp"
#include "guikit/style/tokens.hpp"
#include "guikit/core/types.hpp"
#include "guikit/graphics/icon.hpp"
#include "guikit/overlays/context_menu.hpp"
#include "keel/core.h"
#include "keel/optional.h"
#include "keel/ref.h"

namespace guikit {

class Context;
class ICanvas;
struct Node;

/// Visual presentation and sizing mode for a Toolbar container.
enum class ToolbarStyle : keel::u8 {
    Surface,    ///< Adaptive shell toolbar stretching across the parent container (follows theme.shellStyle: flush in Docked, rounded panel in Floating)
    Header,     ///< Flush edge-to-edge header strip (0px corner radius, 1px subtle border)
    Floating    ///< Compact fit-to-content floating pill/island with elevated surface & border
};

/// Horizontal overflow policy for a Toolbar when tools exceed available width.
enum class ToolbarOverflow : keel::u8 {
    AutoCompact, ///< Collapse Icon+Text tools into square Icon-only tools (with tooltips), then scroll if still overflowing
    Scroll,      ///< Keep labels intact and scroll horizontally with navigation chevrons and mouse wheel
    Wrap,        ///< Wrap overflowing tools onto subsequent rows and expand toolbar height
    Clip         ///< Clip overflowing tools to the toolbar content viewport
};

/// Configuration options for a ToolButton / Toolbar item.
struct ToolButtonOptions {
    UiText text{};
    UiText tooltip{};
    Icon icon{};
    Icon trailingIcon{};
    keel::Optional<ButtonStyle> style{keel::kNullOpt}; ///< Optional per-button style override (nullopt = inherit ToolbarOptions::buttonStyle)
    UiVariant variant{UiVariant::Default};
    StateColor background{};
    Border border{};
    Placement iconPlacement{Placement::Leading};
    Align align{Align::Center};                 ///< Content horizontal alignment (Start = left, Center, End = right)
    AxisSize width{SizeMode::Fit};              ///< Horizontal sizing policy (`Fit`, `Stretch`, or explicit `dp`)
    keel::Optional<keel::f32> minWidth{keel::kNullOpt};///< Minimum capsule width in logical dp
    bool disabled{false};
    bool active{false};                         ///< Toggle active/selected highlight state
    bool square{false};                         ///< Force square (defaultHeight x defaultHeight) button geometry
    keel::Optional<Color> iconColor{keel::kNullOpt}; ///< Optional custom icon tint override
    UiText badge{};                             ///< Optional compact corner/trailing badge counter
    keel::Optional<UiSize> size{keel::kNullOpt};///< Optional per-button size override (nullopt = inherit ToolbarOptions::size)
};

/// Configuration options for Toolbar container.
struct ToolbarOptions {
    ToolbarStyle style{ToolbarStyle::Surface};              ///< Surface (default), Header (flush), or Floating (compact island)
    Orientation orientation{Orientation::Horizontal};       ///< Horizontal bar or Vertical tool palette
    ToolbarOverflow overflow{ToolbarOverflow::AutoCompact}; ///< Overflow behavior when tools exceed horizontal space
    ButtonStyle buttonStyle{ButtonStyle::Ghost};            ///< Default button presentation style inherited by toolbar items (Ghost, Default, Soft, Outline, Solid)
    keel::Optional<keel::f32> gap{keel::kNullOpt};                 ///< Gap between toolbar items (nullopt = inherit size)
    keel::Optional<Padding> padding{keel::kNullOpt};       ///< Internal toolbar padding token or explicit insets
    StateColor background{};
    Border border{};
    UiSize size{UiSize::Md};
    bool disabled{false};
};

/// RAII scope for Toolbar container.
class [[nodiscard]] ToolbarScope : public ContainerScope {
public:
    Response response{};

    ToolbarScope() noexcept : ContainerScope(nullptr, nullptr) {}
    ToolbarScope(Context* ctx, Node* node, Response res = {}, const ToolbarOptions& opts = {}) noexcept
        : ContainerScope(ctx, node), response(res), m_opts(opts) {}

    /// Adds a separator line between tool groups (vertical in Horizontal toolbars, horizontal in Vertical toolbars).
    void Separator(keel::f32 heightRatio = 0.55f, keel::SourceLocation loc = keel::SourceLocation::current());

    /// Adds an elastic spacer pushing subsequent tools to the far end of the toolbar.
    void Spacer(keel::SourceLocation loc = keel::SourceLocation::current());

    /// Adds a fixed non-stretching gap in logical dp along the toolbar's main axis.
    void Gap(keel::f32 sizeDp = 8.0f, keel::SourceLocation loc = keel::SourceLocation::current());

    /// Adds a compact tool button with icon and optional label.
    Response ToolButton(
        ToolButtonOptions opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    /// Alias for ToolButton representing a stateless command action.
    Response Action(
        ToolButtonOptions opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    ) {
        return ToolButton(keel::move(opts), loc);
    }

    /// Adds a stateful toggle button bound to `active`, highlighting when active and flipping `active` on click.
    Response Toggle(
        bool& active,
        ToolButtonOptions opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    /// Adds a mutually exclusive tool selector bound to `selectedValue` (enum, int, size_t).
    template <typename T>
    Response Item(
        T& selectedValue,
        T itemValue,
        ToolButtonOptions opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    ) {
        const bool isSelected = (selectedValue == itemValue);
        ApplyToggleStyle(opts, m_opts.buttonStyle, isSelected);
        Response res = ToolButton(keel::move(opts), loc);
        if (res.clicked && !isSelected) {
            selectedValue = itemValue;
            res.changed = true;
            InvalidateContext();
        }
        return res;
    }

    /// Adds a dropdown tool button that opens a floating ContextMenu on click.
    ContextMenuResponse Dropdown(
        ToolButtonOptions opts,
        keel::ArrayRef<ContextMenuItem> menuItems,
        ContextMenuOptions menuOpts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    /// Adds a split tool button: left zone triggers the primary action, right chevron opens a ContextMenu.
    SplitButtonResponse SplitButton(
        ToolButtonOptions opts,
        keel::ArrayRef<ContextMenuItem> menuItems,
        ContextMenuOptions menuOpts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    /// Adds a split toggle button: left zone toggles `active`, right chevron selects `selectedIndex` from `items`.
    SplitButtonResponse SplitToggle(
        bool& active,
        keel::i32& selectedIndex,
        keel::ArrayRef<keel::StringRef> items,
        ToolButtonOptions opts = {},
        ContextMenuOptions menuOpts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    template <IndexLike IndexT>
        requires(!keel::IsSame<keel::RemoveCv<IndexT>, keel::i32>)
    SplitButtonResponse SplitToggle(
        bool& active,
        IndexT& selectedIndex,
        keel::ArrayRef<keel::StringRef> items,
        ToolButtonOptions opts = {},
        ContextMenuOptions menuOpts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    ) {
        keel::i32 raw = keel::as<keel::i32>(selectedIndex);
        const SplitButtonResponse r = SplitToggle(active, raw, items, keel::move(opts), keel::move(menuOpts), loc);
        if (r.selectionChanged && raw >= 0) {
            selectedIndex = detail::FromIndexLike<IndexT>(raw);
        }
        return r;
    }

    /// Adds a split toggle button with rich ContextMenuItem entries (supporting icons, shortcuts, disabled items).
    SplitButtonResponse SplitToggle(
        bool& active,
        keel::i32& selectedIndex,
        keel::ArrayRef<ContextMenuItem> items,
        ToolButtonOptions opts = {},
        ContextMenuOptions menuOpts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    template <IndexLike IndexT>
        requires(!keel::IsSame<keel::RemoveCv<IndexT>, keel::i32>)
    SplitButtonResponse SplitToggle(
        bool& active,
        IndexT& selectedIndex,
        keel::ArrayRef<ContextMenuItem> items,
        ToolButtonOptions opts = {},
        ContextMenuOptions menuOpts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    ) {
        keel::i32 raw = keel::as<keel::i32>(selectedIndex);
        const SplitButtonResponse r = SplitToggle(active, raw, items, keel::move(opts), keel::move(menuOpts), loc);
        if (r.selectionChanged && raw >= 0) {
            selectedIndex = detail::FromIndexLike<IndexT>(raw);
        }
        return r;
    }

    /// Direct access to underlying Context.
    [[nodiscard]] Context& Ctx() noexcept { return *m_ctx; }
    [[nodiscard]] const Response* operator->() const noexcept { return &response; }

private:
    static void ApplyToggleStyle(ToolButtonOptions& opts, ButtonStyle defaultStyle, bool isActive) noexcept {
        const ButtonStyle effStyle = opts.style.value_or(defaultStyle);
        opts.active = isActive;
        if (isActive) {
            if (effStyle == ButtonStyle::Ghost && !opts.background.HasAny()) {
                opts.style = ButtonStyle::Default;
                if (opts.variant == UiVariant::Default) {
                    opts.variant = UiVariant::Primary;
                }
            } else {
                opts.style = effStyle;
            }
        } else {
            opts.style = effStyle;
            if (!opts.background.HasAny()) {
                opts.variant = UiVariant::Default;
            }
        }
    }

    void InvalidateContext() noexcept;
    ToolbarOptions m_opts{};
};

/// Autonomous Toolbar widget logic (measuring, rendering, and tool button utilities).
namespace detail {

class Toolbar {
public:
    [[nodiscard]] static ToolbarScope Open(
        Context& ctx,
        ToolbarOptions opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    static Response ToolButton(
        Context& ctx,
        const ToolButtonOptions& opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    static SplitButtonResponse SplitButton(
        Context& ctx,
        const ToolButtonOptions& opts,
        keel::ArrayRef<ContextMenuItem> menuItems,
        ContextMenuOptions menuOpts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    static void RenderSeparator(
        ICanvas& canvas,
        Rect bounds,
        Color color = Color::Transparent(),
        const UiTheme& theme = UiTheme::Dark(),
        Orientation orientation = Orientation::Horizontal
    );

    static void Arrange(
        Node* node,
        const Rect& availableRect,
        const UiTheme& theme = UiTheme::Dark()
    );

    [[nodiscard]] static Rect GetContentClipRect(const Node* node) noexcept;
};

} // namespace detail

[[nodiscard]] inline ToolbarScope Toolbar(
    Context& ui,
    ToolbarOptions opts = {},
    keel::SourceLocation loc = keel::SourceLocation::current()
) {
    return detail::Toolbar::Open(ui, opts, loc);
}

inline Response ToolButton(
    Context& ui,
    ToolButtonOptions opts = {},
    keel::SourceLocation loc = keel::SourceLocation::current()
) {
    return detail::Toolbar::ToolButton(ui, opts, loc);
}

} // namespace guikit
