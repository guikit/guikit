#pragma once

#include <cstdint>
#include <initializer_list>
#include <optional>
#include <span>
#include <source_location>
#include <string_view>
#include "guikit/core/Response.hpp"
#include "guikit/core/Theme.hpp"
#include "guikit/core/Types.hpp"
#include "guikit/graphics/Icon.hpp"

namespace guikit {

class Context;
class ICanvas;

/// Visual presentation style for TabBar.
enum class TabBarStyle : uint8_t {
    Underline,  ///< Classic desktop/IDE style: transparent tab background with an accent bottom indicator line
    Pill        ///< Segmented capsule style: active tab has a rounded background fill
};

/// Definition of an individual tab item.
struct TabItem {
    std::string_view label{};
    Icon icon{};
    std::string_view badge{};
    UiVariant variant{UiVariant::Default};
    bool disabled{false};
};

/// Configuration options for TabBar widget.
struct TabBarOptions {
    TabBarStyle style{TabBarStyle::Underline};
    bool showSeparator{true};   ///< In Underline style: draws a 1px baseline line under tabs
    bool fullWidth{false};      ///< Stretch tabs equally to fill available container width
    bool scrollable{true};      ///< Enable horizontal scrolling with arrows when tabs overflow
    Color activeColor{Color::Transparent()}; ///< Optional custom accent color override
    std::optional<UiSize> size{std::nullopt}; ///< Local size override (nullopt = inherit container size)
};

/// Internal runtime state allocated in frame arena for TabItem nodes.
struct TabItemData {
    TabItem item{};
    TabBarOptions barOpts{};
};

/// Autonomous TabBar widget logic (execution, measuring, and rendering).
class TabBar {
public:
    /// Primary immediate mode entry point for TabBar widget.
    static Response Run(
        Context& ctx,
        int& activeIndex,
        std::span<const TabItem> tabs,
        const TabBarOptions& opts,
        std::source_location loc = std::source_location::current()
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
        const UiTheme& theme = UiTheme::Dark()
    );
};

} // namespace guikit
