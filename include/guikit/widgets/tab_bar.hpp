#pragma once

#include <initializer_list>
#include "guikit/core/response.hpp"
#include "guikit/core/theme.hpp"
#include "guikit/core/types.hpp"
#include "guikit/graphics/icon.hpp"
#include "keel/core.h"
#include "keel/optional.h"
#include "keel/ref.h"

namespace guikit {

class Context;
class ICanvas;

/// Visual presentation style for TabBar.
enum class TabBarStyle : keel::u8 {
    Underline,  ///< Classic desktop/IDE style: transparent tab background with an accent bottom indicator line
    Pill,       ///< Segmented capsule style: active tab has a rounded background fill
    Card,       ///< Outlined card tabs docked to baseline (open bottom on active tab)
    Folder      ///< Browser/editor folder tabs with angled/tapered shoulders and top accent line
};

/// Definition of an individual tab item.
struct TabItem {
    UiText label{};
    Icon icon{};
    UiText badge{};
    UiText tooltip{};
    UiVariant variant{UiVariant::Default};
    bool closable{false};   ///< Show an interactive close ('X') button on the trailing side of the tab
    bool disabled{false};
};

/// Response returned by TabBar::Run, extending Response with closed-tab notification.
struct TabBarResponse : Response {
    keel::Optional<keel::usize> closedIndex{keel::kNullOpt}; ///< Index of the tab whose close ('X') button was clicked this frame
};

/// Configuration options for TabBar widget.
struct TabBarOptions {
    TabBarStyle style{TabBarStyle::Underline};
    bool showSeparator{true};   ///< In Underline/Card/Folder styles: draws a 1px baseline line under tabs
    bool fullWidth{false};      ///< Stretch tabs equally to fill available container width
    bool scrollable{true};      ///< Enable horizontal scrolling with arrows when tabs overflow
    bool disabled{false};       ///< Disable interaction across the entire TabBar
    keel::Optional<keel::f32> gap{keel::kNullOpt};   ///< Gap between consecutive tab items (nullopt = from theme)
    keel::Optional<Padding> padding{keel::kNullOpt};   ///< Container padding around tabs (nullopt = auto based on style)
    UiSize size{UiSize::Md};                     ///< Component size scale
    StateColor color{};                          ///< Optional semantic/custom accent color for active tab indicator / pill
    StateColor background{};                     ///< Optional background fill color for the tab bar
    Border border{};                             ///< Optional border configuration (color, width, radius)
};

/// Internal runtime state allocated in frame arena for TabItem nodes.
struct TabItemData {
    TabItem item{};
    TabBarOptions barOpts{};
    bool closeHovered{false};
};

/// Autonomous TabBar widget logic (execution, measuring, and rendering).
class TabBar {
public:
    /// Primary immediate mode entry point for TabBar widget.
    static TabBarResponse Run(
        Context& ctx,
        keel::i32& activeIndex,
        keel::ArrayRef<TabItem> tabs,
        const TabBarOptions& opts,
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    /// Measures intrinsic dimensions of a single tab item.
    static Vec2 MeasureTab(
        const TabItem& item,
        const TabBarOptions& opts,
        UiSize size,
        ICanvas* canvas,
        const UiTheme& theme
    );
    static Vec2 MeasureTab(
        const TabItem& item,
        const TabBarOptions& opts,
        UiSize size,
        ICanvas* canvas = nullptr
    );

    /// Renders a single tab item according to style, selection, hover, and disabled state.
    static void RenderTab(
        ICanvas& canvas,
        Rect bounds,
        const TabItem& item,
        const TabBarOptions& opts,
        UiSize size,
        bool active,
        bool hovered,
        bool pressed,
        bool disabled,
        const UiTheme& theme = UiTheme::Dark(),
        bool focused = false,
        bool closeHovered = false
    );
};

} // namespace guikit
