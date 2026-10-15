#pragma once

#include <source_location>
#include "guikit/core/Element.hpp"

namespace guikit {

/// Heading is a standalone `Element` configured with Heading typography (`headingTypography = true`).
class Heading : public Element {
public:
    [[nodiscard]] static ElementOptions WithHeadingStyle(ElementOptions opts) noexcept {
        opts.headingTypography = true;
        return opts;
    }

    static Response Run(
        Context& ctx,
        const HeadingOptions& opts,
        std::source_location loc = std::source_location::current()
    ) {
        return Element::Run(ctx, WithHeadingStyle(opts), loc);
    }

    static Vec2 Measure(
        const HeadingOptions& opts,
        UiSize size,
        ICanvas* canvas,
        const UiTheme& theme,
        float availableWidth = 0.0f
    ) noexcept {
        return Element::Measure(WithHeadingStyle(opts), size, canvas, theme, availableWidth);
    }

    static Vec2 Measure(
        const HeadingOptions& opts,
        UiSize size,
        float availableWidth,
        ICanvas* canvas,
        const UiTheme& theme
    ) noexcept {
        return Element::Measure(WithHeadingStyle(opts), size, canvas, theme, availableWidth);
    }

    static Vec2 Measure(
        const HeadingOptions& opts,
        UiSize size,
        float availableWidth = 0.0f,
        ICanvas* canvas = nullptr
    ) noexcept {
        return Element::Measure(WithHeadingStyle(opts), size, canvas, UiTheme::Dark(), availableWidth);
    }

    static float ResolveHeight(
        const HeadingOptions& opts,
        UiSize size,
        float availableWidth,
        ICanvas* canvas,
        const UiTheme& theme
    ) noexcept {
        return Measure(opts, size, canvas, theme, availableWidth).y;
    }

    static void Render(
        ICanvas& canvas,
        Rect bounds,
        const HeadingOptions& opts,
        UiSize size,
        const UiTheme& theme,
        bool disabled = false
    ) noexcept {
        Element::Render(canvas, bounds, WithHeadingStyle(opts), size, theme, disabled);
    }
};

} // namespace guikit
