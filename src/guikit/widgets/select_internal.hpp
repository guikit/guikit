#pragma once

#include "guikit/widgets/select.hpp"
#include "guikit/backend/i_canvas.hpp"
#include "guikit/context.hpp"
#include "guikit/core/node.hpp"
#include "guikit/core/scrollbar.hpp"
#include "guikit/core/style_painter.hpp"
#include "guikit/core/text_util.hpp"
#include "keel/core.h"
#include "keel/format.h"
#include "keel/small_array.h"
#include "keel/string.h"

namespace guikit::detail::select_impl {

ControlSpec EffectiveSelectControl(const SelectOptions& opts, UiSize size, const UiTheme& theme) noexcept;
Vec2 MeasureSelectBox(const SelectOptions& opts, UiSize size, const UiTheme& theme) noexcept;
keel::usize SnapToUtf8CodepointBoundary(keel::StringRef text, keel::usize idx) noexcept;
bool FindCaseInsensitiveSubstring(
    keel::StringRef haystack,
    keel::StringRef needle,
    keel::usize& outBytePos,
    keel::usize& outByteLen
) noexcept;
bool ContainsCaseInsensitive(keel::StringRef haystack, keel::StringRef needle) noexcept;
bool MatchesSelectFilter(const SelectItem& item, keel::StringRef query) noexcept;

struct SelectBoxRects {
    Rect boxRect{};
    Rect textRect{};
    Rect clearRect{};
    Rect chevronRect{};
};


SelectBoxRects ResolveSelectBoxRects(
    Rect bounds,
    const UiAffix& activePrefix,
    const UiAffix& activeSuffix,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme,
    bool showClearBtn
) noexcept;

} // namespace guikit::detail::select_impl
