#pragma once

#include "guikit/context.hpp"
#include "guikit/core/control.hpp"
#include "guikit/core/response.hpp"
#include "guikit/style/theme.hpp"
#include "guikit/core/types.hpp"
#include "guikit/graphics/icon.hpp"
#include "keel/core.h"
#include "keel/optional.h"
#include "keel/ref.h"
#include "keel/static_array.h"
#include "keel/string.h"

namespace guikit {

class Context;
class ICanvas;

/// Individual item definition for Select dropdown widget.
struct SelectItem {
    UiText text{};           ///< Primary item label text (default role: LabelMedium)
    UiAffix prefix{};        ///< Optional leading icon and/or text on the left of this item
    UiAffix suffix{};        ///< Optional trailing text and/or icon on the right of this item
    bool disabled{false};    ///< Whether this item is non-selectable and dimmed
};

/// Clean configuration options for Select, ComboBox, and Autocomplete control widgets.
struct SelectOptions {
    UiText placeholder{"Select..."};                           ///< Placeholder text when no item is selected or input is empty
    UiText emptyText{"No matching items"};                     ///< Text displayed inside popup when search filter yields no matches
    UiAffix prefix{};                                          ///< Optional default leading icon/text inside closed field
    UiAffix suffix{};                                          ///< Optional default trailing text/icon inside closed field
    keel::Optional<keel::u32> maxVisibleItems{keel::kNullOpt}; ///< Optional limit on visible items count before scrolling
    Align align{Align::Start};                                 ///< Content horizontal alignment inside closed select box (Start, Center, End)
    bool searchable{false};                                    ///< Enable live keyboard search/filtering of dropdown items (ComboBox mode)
    bool editable{false};                                      ///< Enable free-form text input with dropdown suggestions (Autocomplete mode)
    bool clearable{false};                                     ///< Show interactive clear (X) button when a selection or text is present
    keel::Optional<CursorBlinkMode> cursorMode{keel::kNullOpt}; ///< Optional override for text cursor blink mode in searchable/editable modes
    UiText tooltip{};                                          ///< Optional floating tooltip text on hover
    bool disabled{false};                                      ///< Interaction suppression
    UiSize size{UiSize::Md};                                   ///< Control size token (`Xs`..`Xl`, `Dp(...)`, `Factor(...)`)
    AxisSize width{};                                          ///< Horizontal sizing policy (`Auto` -> `Stretch`, `Fit`, or explicit `dp`)
    AxisSize height{};                                         ///< Vertical sizing policy (`Auto` -> `Fit`, `Stretch`, or explicit `dp`)
    keel::Optional<keel::f32> minWidth{keel::kNullOpt};            ///< Optional minimum width in logical dp
    keel::Optional<keel::f32> maxWidth{keel::kNullOpt};            ///< Optional maximum width in logical dp
    keel::Optional<keel::f32> minHeight{keel::kNullOpt};           ///< Optional minimum height in logical dp
    keel::Optional<keel::f32> maxHeight{keel::kNullOpt};           ///< Optional maximum height in logical dp
    FieldStyle style{FieldStyle::Default};                     ///< Visual field presentation (Default, Filled, Borderless)
    UiVariant status{UiVariant::Default};                      ///< Validation / semantic accent status (Default, Success, Warning, Danger, Info)
    StateColor background{};                                   ///< Optional background fill override (`{}` = theme default, `StateColor::None()` = transparent)
    Border border{};                                           ///< Optional border width, color, and radius override
};

/// Visual chip data stored inside frame arena for MultiSelect nodes.
struct SelectChipVisual {
    UiText text{};
    UiAffix prefix{};
    keel::usize itemIndex{0};
    bool removeHovered{false};
};

/// Internal runtime state allocated in frame arena for Select / ComboBox / Autocomplete / MultiSelect nodes.
struct SelectData {
    static constexpr keel::usize kMaxChips = 16;

    SelectOptions opts{};
    UiText displayText{};
    keel::usize cursor{0};
    keel::usize selStart{0};
    keel::usize selEnd{0};
    keel::f32 scrollX{0.0f};
    keel::f32 cursorWidth{1.5f};
    keel::f32 naturalWidth{0.0f};
    bool isEditing{false};
    bool cursorVisible{false};
    bool showClearBtn{false};
    bool clearHovered{false};
    bool chevronHovered{false};
    bool isMulti{false};
    keel::StaticArray<SelectChipVisual, kMaxChips> chips{};
    keel::usize chipCount{0};
    keel::usize totalSelectedCount{0};
};

/// Autonomous Select (dropdown combo box) widget logic.
namespace detail {

class Select {
public:
    /// Primary immediate mode entry point for Select with rich SelectItem elements.
    static Response Run(
        Context& ctx,
        keel::usize* selectedIndex,
        keel::i32* selectedIntIndex,
        keel::Optional<keel::StringSink> selectedString,
        keel::StringRef* selectedStringView,
        keel::ArrayRef<SelectItem> items,
        const SelectOptions& opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    /// Convenience immediate mode entry point for Select with plain StringRef items.
    static Response Run(
        Context& ctx,
        keel::usize* selectedIndex,
        keel::i32* selectedIntIndex,
        keel::Optional<keel::StringSink> selectedString,
        keel::StringRef* selectedStringView,
        keel::ArrayRef<keel::StringRef> items,
        const SelectOptions& opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    /// Multi-selection immediate mode entry point with plain StringRef items.
    [[nodiscard]] static Response RunMulti(
        Context& ctx,
        IndexListSink selectedIndices,
        keel::ArrayRef<keel::StringRef> items,
        const SelectOptions& opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    /// Multi-selection immediate mode entry point with rich SelectItem elements.
    [[nodiscard]] static Response RunMulti(
        Context& ctx,
        IndexListSink selectedIndices,
        keel::ArrayRef<SelectItem> items,
        const SelectOptions& opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    /// Measures intrinsic dimensions based on content, label, and placement.
    static Vec2 Measure(
        const SelectOptions& opts,
        UiSize size,
        ICanvas* canvas,
        const UiTheme& theme,
        keel::f32 availableWidth = 0.0f
    );
    static Vec2 Measure(
        const SelectOptions& opts,
        UiSize size,
        ICanvas* canvas = nullptr,
        keel::f32 availableWidth = 0.0f
    );

    /// Computes the exact bounding box for the closed select field within overall bounds.
    static Rect GetBoxRect(
        Rect bounds,
        const SelectOptions& opts,
        UiSize size,
        ICanvas* canvas = nullptr,
        const UiTheme& theme = UiTheme::Dark()
    );

    /// Renders the closed select frame with active text, prefix/suffix affixes, clear button, and chevron.
    static void Render(
        ICanvas& canvas,
        Rect bounds,
        UiText currentText,
        const SelectOptions& opts,
        UiSize size,
        bool hovered = false,
        bool open = false,
        bool disabled = false,
        const UiTheme& theme = UiTheme::Dark(),
        bool focused = false,
        bool isEditing = false,
        keel::usize cursor = 0,
        bool cursorVisible = false,
        keel::f32 cursorWidth = 1.5f,
        keel::usize selStart = 0,
        keel::usize selEnd = 0,
        keel::f32 scrollX = 0.0f,
        bool showClearBtn = false,
        bool clearHovered = false,
        bool chevronHovered = false
    );

    /// Renders the floating popup menu containing rich SelectItem dropdown items.
    static void RenderPopup(
        ICanvas& canvas,
        Rect popupRect,
        keel::ArrayRef<SelectItem> items,
        keel::usize selectedIndex,
        keel::i32 hoveredIndex,
        const SelectOptions& opts,
        UiSize size,
        const UiTheme& theme = UiTheme::Dark(),
        keel::f32 scrollY = 0.0f,
        bool scrollbarHovered = false,
        bool scrollbarActive = false,
        keel::StringRef highlightQuery = {}
    );

    /// Convenience overload rendering popup menu from plain StringRef items.
    static void RenderPopup(
        ICanvas& canvas,
        Rect popupRect,
        keel::ArrayRef<keel::StringRef> items,
        keel::usize selectedIndex,
        keel::i32 hoveredIndex,
        const SelectOptions& opts,
        UiSize size,
        const UiTheme& theme = UiTheme::Dark(),
        keel::f32 scrollY = 0.0f,
        bool scrollbarHovered = false,
        bool scrollbarActive = false,
        keel::StringRef highlightQuery = {}
    );
};

} // namespace detail

template <SelectTarget T>
Response Select(
    Context& ui,
    T& selected,
    keel::ArrayRef<SelectItem> items,
    SelectOptions opts = {},
    keel::SourceLocation loc = keel::SourceLocation::current()
) {
    WidgetExecGuard guard(ui);
    if constexpr (keel::IsSame<keel::Decay<T>, keel::usize>) {
        return detail::Select::Run(ui, &selected, nullptr, keel::Optional<keel::StringSink>{}, nullptr, items, opts, loc);
    } else if constexpr (keel::IsSame<keel::Decay<T>, keel::i32>) {
        return detail::Select::Run(ui, nullptr, &selected, keel::Optional<keel::StringSink>{}, nullptr, items, opts, loc);
    } else if constexpr (IndexLike<T>) {
        constexpr bool isSignedIndex = []() constexpr {
            if constexpr (keel::IsEnum<keel::Decay<T>>) {
                return keel::IsSigned<keel::UnderlyingType<keel::Decay<T>>>;
            } else {
                return keel::IsSigned<keel::Decay<T>>;
            }
        }();
        if constexpr (isSignedIndex) {
            keel::i32 idx = keel::as<keel::i32>(selected);
            Response r = detail::Select::Run(ui, nullptr, &idx, keel::Optional<keel::StringSink>{}, nullptr, items, opts, loc);
            if (r.changed) {
                selected = detail::FromIndexLike<T>(idx);
            }
            return r;
        } else {
            keel::usize idx = keel::as<keel::usize>(selected);
            Response r = detail::Select::Run(ui, &idx, nullptr, keel::Optional<keel::StringSink>{}, nullptr, items, opts, loc);
            if (r.changed) {
                selected = detail::FromIndexLike<T>(idx);
            }
            return r;
        }
    } else if constexpr (keel::WritableString<T>) {
        return detail::Select::Run(ui, nullptr, nullptr, keel::Optional<keel::StringSink>(keel::StringSink(selected)), nullptr, items, opts, loc);
    } else if constexpr (keel::IsSame<keel::RemoveCvRef<T>, keel::StringRef>) {
        return detail::Select::Run(ui, nullptr, nullptr, keel::Optional<keel::StringSink>{}, &selected, items, opts, loc);
    } else if constexpr (MutableForeignStringView<T>) {
        keel::StringRef sr(selected.data(), keel::as<keel::usize>(selected.size()));
        Response r = detail::Select::Run(ui, nullptr, nullptr, keel::Optional<keel::StringSink>{}, &sr, items, opts, loc);
        if (r.changed) {
            selected = keel::RemoveCvRef<T>(sr.data(), sr.size());
        }
        return r;
    }
}

template <SelectTarget T>
Response Select(
    Context& ui,
    T& selected,
    keel::ArrayRef<keel::StringRef> items,
    SelectOptions opts = {},
    keel::SourceLocation loc = keel::SourceLocation::current()
) {
    WidgetExecGuard guard(ui);
    if constexpr (keel::IsSame<keel::Decay<T>, keel::usize>) {
        return detail::Select::Run(ui, &selected, nullptr, keel::Optional<keel::StringSink>{}, nullptr, items, opts, loc);
    } else if constexpr (keel::IsSame<keel::Decay<T>, keel::i32>) {
        return detail::Select::Run(ui, nullptr, &selected, keel::Optional<keel::StringSink>{}, nullptr, items, opts, loc);
    } else if constexpr (IndexLike<T>) {
        constexpr bool isSignedIndex = []() constexpr {
            if constexpr (keel::IsEnum<keel::Decay<T>>) {
                return keel::IsSigned<keel::UnderlyingType<keel::Decay<T>>>;
            } else {
                return keel::IsSigned<keel::Decay<T>>;
            }
        }();
        if constexpr (isSignedIndex) {
            keel::i32 idx = keel::as<keel::i32>(selected);
            Response r = detail::Select::Run(ui, nullptr, &idx, keel::Optional<keel::StringSink>{}, nullptr, items, opts, loc);
            if (r.changed) {
                selected = detail::FromIndexLike<T>(idx);
            }
            return r;
        } else {
            keel::usize idx = keel::as<keel::usize>(selected);
            Response r = detail::Select::Run(ui, &idx, nullptr, keel::Optional<keel::StringSink>{}, nullptr, items, opts, loc);
            if (r.changed) {
                selected = detail::FromIndexLike<T>(idx);
            }
            return r;
        }
    } else if constexpr (keel::WritableString<T>) {
        return detail::Select::Run(ui, nullptr, nullptr, keel::Optional<keel::StringSink>(keel::StringSink(selected)), nullptr, items, opts, loc);
    } else if constexpr (keel::IsSame<keel::RemoveCvRef<T>, keel::StringRef>) {
        return detail::Select::Run(ui, nullptr, nullptr, keel::Optional<keel::StringSink>{}, &selected, items, opts, loc);
    } else if constexpr (MutableForeignStringView<T>) {
        keel::StringRef sr(selected.data(), keel::as<keel::usize>(selected.size()));
        Response r = detail::Select::Run(ui, nullptr, nullptr, keel::Optional<keel::StringSink>{}, &sr, items, opts, loc);
        if (r.changed) {
            selected = keel::RemoveCvRef<T>(sr.data(), sr.size());
        }
        return r;
    }
}

template <SelectTarget T>
Response ComboBox(
    Context& ui,
    T& selected,
    keel::ArrayRef<SelectItem> items,
    SelectOptions opts = {},
    keel::SourceLocation loc = keel::SourceLocation::current()
) {
    opts.searchable = true;
    return guikit::Select(ui, selected, items, keel::move(opts), loc);
}

template <SelectTarget T>
Response ComboBox(
    Context& ui,
    T& selected,
    keel::ArrayRef<keel::StringRef> items,
    SelectOptions opts = {},
    keel::SourceLocation loc = keel::SourceLocation::current()
) {
    opts.searchable = true;
    return guikit::Select(ui, selected, items, keel::move(opts), loc);
}

template <keel::WritableString S>
Response Autocomplete(
    Context& ui,
    S& value,
    keel::ArrayRef<SelectItem> items,
    SelectOptions opts = {},
    keel::SourceLocation loc = keel::SourceLocation::current()
) {
    opts.searchable = true;
    opts.editable = true;
    return guikit::Select(ui, value, items, keel::move(opts), loc);
}

template <keel::WritableString S>
Response Autocomplete(
    Context& ui,
    S& value,
    keel::ArrayRef<keel::StringRef> items,
    SelectOptions opts = {},
    keel::SourceLocation loc = keel::SourceLocation::current()
) {
    opts.searchable = true;
    opts.editable = true;
    return guikit::Select(ui, value, items, keel::move(opts), loc);
}

template <WritableIndexList Arr>
Response MultiSelect(
    Context& ui,
    Arr& selectedIndices,
    keel::ArrayRef<SelectItem> items,
    SelectOptions opts = {},
    keel::SourceLocation loc = keel::SourceLocation::current()
) {
    WidgetExecGuard guard(ui);
    return detail::Select::RunMulti(ui, IndexListSink(selectedIndices), items, opts, loc);
}

template <WritableIndexList Arr>
Response MultiSelect(
    Context& ui,
    Arr& selectedIndices,
    keel::ArrayRef<keel::StringRef> items,
    SelectOptions opts = {},
    keel::SourceLocation loc = keel::SourceLocation::current()
) {
    WidgetExecGuard guard(ui);
    return detail::Select::RunMulti(ui, IndexListSink(selectedIndices), items, opts, loc);
}

} // namespace guikit
