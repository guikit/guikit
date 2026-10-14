#pragma once

#include <optional>
#include "guikit/core/Theme.hpp"
#include "guikit/core/Tokens.hpp"
#include "guikit/core/Types.hpp"

namespace guikit {

class ICanvas;

/// Common layout and text specification for a composite UI element.
struct ElementSpec {
    UiText label{};
    UiText description{};
    ElementLayout layout{ElementLayout::Stacked};
    InlineOrder inlineOrder{InlineOrder::LabelFirst};
    Align elementAlign{Align::Start};
    StretchMode stretch{StretchMode::Compact};
    std::optional<float> width{std::nullopt};         ///< Control width: in points for Compact/Space, or % (0..100) for Fill
    UiSize widgetSize{UiSize::Md};
    float labelGap{6.0f};
    float descGap{4.0f};
    float inlineGap{10.0f};
    bool stretchableControl{true};                    ///< True for Button/InputField/Select/Slider; false for fixed-size controls (Switch/Checkbox)
    bool stretchHeightWhenStandalone{false};          ///< True for Button capsule when standalone; false to center control vertically
    bool disabled{false};
};

/// Resolved bounding rectangles for a composite UI element.
struct ElementGeometry {
    Rect controlRect{};
    Rect labelRect{};
    Rect descRect{};
    float measuredLabelW{0.0f};
    float measuredDescW{0.0f};
    float labelFontSize{14.0f};
    float descFontSize{11.0f};
    bool hasLabel{false};
    bool hasDesc{false};
    bool hasExternal{false};
};

/// Shared layout and text rendering engine for composite UI elements
/// (Button, InputField, Select, Slider, StepSlider, Switch, Checkbox).
class ElementComposer {
public:
    /// Measures total intrinsic dimensions of the composite element given the intrinsic control dimensions.
    static Vec2 Measure(
        Vec2 intrinsicControlSize,
        const ElementSpec& spec,
        ICanvas* canvas,
        const UiTheme& theme
    ) noexcept;

    /// Resolves bounding boxes for the control, external label, and description within `bounds`.
    static ElementGeometry Resolve(
        Rect bounds,
        Vec2 intrinsicControlSize,
        const ElementSpec& spec,
        ICanvas* canvas,
        const UiTheme& theme
    ) noexcept;

    /// Renders the external label and description with automatic ellipsis truncation.
    static void RenderText(
        ICanvas& canvas,
        const ElementGeometry& geom,
        const ElementSpec& spec,
        const UiTheme& theme
    ) noexcept;
};

} // namespace guikit
