#pragma once

#include <optional>
#include <span>
#include <source_location>
#include <string>
#include <string_view>
#include <vector>
#include "guikit/core/Control.hpp"
#include "guikit/core/Response.hpp"
#include "guikit/core/Theme.hpp"
#include "guikit/core/Types.hpp"
#include "guikit/graphics/Icon.hpp"

namespace guikit {

class Context;
class ICanvas;

struct TagItem {
    std::string text{};
    UiVariant variant{UiVariant::Default};
    Icon icon{};

    constexpr bool operator==(const TagItem&) const noexcept = default;
};

struct TagInputOptions {
    UiText placeholder{"Add tag..."};              ///< Placeholder text shown when empty or in trailing inline input
    UiAffix prefix{};                              ///< Optional leading icon/text inside field on left
    UiVariant tagVariant{UiVariant::Primary};      ///< Default visual variant for tag pills
    uint32_t maxTags{0};                           ///< Maximum allowed tags (0 = unlimited)
    bool allowDuplicates{false};                   ///< Whether duplicate tag strings are permitted
    bool splitOnComma{true};                       ///< Commit tag(s) when typing ',' or ';'
    bool splitOnSpace{false};                      ///< Commit tag when typing space ' '
    bool clearable{true};                          ///< Show trailing '×' button to clear all tags
    bool showCounter{false};                       ///< Show tag count badge on the right (e.g. "3" or "3/8")
    bool readOnly{false};                          ///< Non-editable display mode (hides inline input and '×' buttons)
    StateColor color{};                            ///< Optional accent color override for active focus / chips
    StretchMode stretch{StretchMode::Fill};
    Align align{Align::Start};
    std::optional<float> width{std::nullopt};
    std::optional<float> height{std::nullopt};
    std::optional<float> minWidth{std::nullopt};
    StateColor background{StateColor::Surface()};
    Border border{};
    UiText tooltip{};
    bool disabled{false};
    const UiTheme* theme{nullptr};
    std::optional<float> scale{std::nullopt};
    std::optional<bool> debugBounds{std::nullopt};
    std::optional<bool> inspectMode{std::nullopt};
};

class TagInput {
public:
    [[nodiscard]] static Response Run(
        Context& ctx,
        std::vector<std::string>& tags,
        const TagInputOptions& opts = {},
        std::source_location loc = std::source_location::current()
    );

    [[nodiscard]] static Response Run(
        Context& ctx,
        std::vector<TagItem>& tags,
        const TagInputOptions& opts = {},
        std::source_location loc = std::source_location::current()
    );

    [[nodiscard]] static Vec2 Measure(
        std::span<const std::string> tags,
        const TagInputOptions& opts,
        UiSize size,
        ICanvas* canvas,
        const UiTheme& theme,
        float availableWidth = 0.0f
    );

    static void Render(const Node* node, ICanvas& canvas, const UiTheme& theme);
};

} // namespace guikit
