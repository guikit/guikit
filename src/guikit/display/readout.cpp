#include "guikit/display/readout.hpp"
#include "guikit/backend/i_canvas.hpp"
#include "guikit/context.hpp"
#include "guikit/core/node.hpp"
#include "guikit/core/text_util.hpp"
#include "guikit/overlays/context_menu.hpp"
#include "guikit/style/style_painter.hpp"
#include "keel/format.h"
#include "keel/math.h"
#include "keel/small_array.h"
#include "keel/static_array.h"

namespace guikit {

namespace {

enum class ReadoutOpenMenu : keel::u8 {
    None = 0,
    Channel,
    Precision,
    Unit
};

struct ReadoutPersistState {
    ReadoutOpenMenu openMenu{ReadoutOpenMenu::None};
};

struct ReadoutMetrics {
    keel::f32 headerFont{12.0f};
    keel::f32 valueFont{28.0f};
    keel::f32 unitFont{16.0f};
    keel::f32 headerH{16.0f};
    keel::f32 valueRowH{34.0f};
    keel::f32 rowGap{4.0f};
    keel::f32 unitGap{6.0f};
    keel::f32 actionGap{10.0f};
    keel::f32 actionBtnSize{32.0f};
    keel::f32 actionIconSize{16.0f};
    keel::f32 radius{8.0f};
    Padding defaultPadding{10.0f, 12.0f, 10.0f, 12.0f};
};

ReadoutMetrics ComputeReadoutMetrics(UiSize size, const UiTheme& theme) noexcept {
    ReadoutMetrics m{};
    switch (size.GetPreset()) {
        case UiSize::Preset::Xs:
            m.headerFont = 10.0f;
            m.valueFont = 20.0f;
            m.unitFont = 12.0f;
            m.headerH = 14.0f;
            m.valueRowH = 24.0f;
            m.rowGap = 2.0f;
            m.unitGap = 4.0f;
            m.actionGap = 8.0f;
            m.actionBtnSize = 24.0f;
            m.actionIconSize = 13.0f;
            m.radius = 6.0f;
            m.defaultPadding = Padding(6.0f, 8.0f, 6.0f, 8.0f);
            break;
        case UiSize::Preset::Sm:
            m.headerFont = 11.0f;
            m.valueFont = 24.0f;
            m.unitFont = 14.0f;
            m.headerH = 15.0f;
            m.valueRowH = 28.0f;
            m.rowGap = 3.0f;
            m.unitGap = 5.0f;
            m.actionGap = 8.0f;
            m.actionBtnSize = 28.0f;
            m.actionIconSize = 14.0f;
            m.radius = 7.0f;
            m.defaultPadding = Padding(8.0f, 10.0f, 8.0f, 10.0f);
            break;
        case UiSize::Preset::Lg:
            m.headerFont = 13.0f;
            m.valueFont = 34.0f;
            m.unitFont = 18.0f;
            m.headerH = 18.0f;
            m.valueRowH = 40.0f;
            m.rowGap = 5.0f;
            m.unitGap = 7.0f;
            m.actionGap = 12.0f;
            m.actionBtnSize = 36.0f;
            m.actionIconSize = 18.0f;
            m.radius = 9.0f;
            m.defaultPadding = Padding(12.0f, 14.0f, 12.0f, 14.0f);
            break;
        case UiSize::Preset::Xl:
            m.headerFont = 14.0f;
            m.valueFont = 40.0f;
            m.unitFont = 20.0f;
            m.headerH = 20.0f;
            m.valueRowH = 46.0f;
            m.rowGap = 6.0f;
            m.unitGap = 8.0f;
            m.actionGap = 14.0f;
            m.actionBtnSize = 40.0f;
            m.actionIconSize = 20.0f;
            m.radius = 10.0f;
            m.defaultPadding = Padding(14.0f, 16.0f, 14.0f, 16.0f);
            break;
        case UiSize::Preset::Md:
            m.headerFont = 12.0f;
            m.valueFont = 28.0f;
            m.unitFont = 16.0f;
            m.headerH = 16.0f;
            m.valueRowH = 34.0f;
            m.rowGap = 4.0f;
            m.unitGap = 6.0f;
            m.actionGap = 10.0f;
            m.actionBtnSize = 32.0f;
            m.actionIconSize = 16.0f;
            m.radius = 8.0f;
            m.defaultPadding = Padding(10.0f, 12.0f, 10.0f, 12.0f);
            break;
        default: {
            const keel::f32 f = theme.SizeFactor(size);
            m.headerFont = keel::round(12.0f * f);
            m.valueFont = keel::round(28.0f * f);
            m.unitFont = keel::round(16.0f * f);
            m.headerH = keel::round(16.0f * f);
            m.valueRowH = keel::round(34.0f * f);
            m.rowGap = keel::round(4.0f * f);
            m.unitGap = keel::round(6.0f * f);
            m.actionGap = keel::round(10.0f * f);
            m.actionBtnSize = keel::round(32.0f * f);
            m.actionIconSize = keel::round(16.0f * f);
            m.radius = keel::round(8.0f * f);
            m.defaultPadding = Padding(
                keel::round(10.0f * f),
                keel::round(12.0f * f),
                keel::round(10.0f * f),
                keel::round(12.0f * f)
            );
            break;
        }
    }
    return m;
}

keel::f32 MeasureStyledWidth(keel::StringRef text, keel::f32 fontSize, FontStyle style, ICanvas* canvas) noexcept {
    if (text.empty()) return 0.0f;
    if (canvas != nullptr) {
        return canvas->MeasureTextStyled(text, fontSize, style).x;
    }
    return TextUtil::MeasureUiTextWidth(UiText(text), fontSize, nullptr);
}

struct ReadoutGeometry {
    Rect headerRect{};
    Rect valueRect{};
    Rect unitRect{};
    Rect actionRect{};
};

ReadoutGeometry ComputeReadoutGeometry(
    const Rect& bounds,
    keel::StringRef headerText,
    keel::StringRef valueText,
    keel::StringRef unitText,
    bool showAction,
    const ReadoutOptions& opts,
    const ReadoutMetrics& m,
    ICanvas* canvas
) noexcept {
    ReadoutGeometry g{};
    const Padding pad = opts.padding.value_or(m.defaultPadding);
    const keel::f32 innerX = bounds.x + pad.left;
    const keel::f32 innerY = bounds.y + pad.top;
    const keel::f32 innerW = keel::max(8.0f, bounds.w - pad.left - pad.right);
    const keel::f32 innerH = keel::max(8.0f, bounds.h - pad.top - pad.bottom);

    const keel::f32 headerTextW = MeasureStyledWidth(headerText, m.headerFont, FontStyle::Bold, canvas);
    const keel::f32 valueTextW = MeasureStyledWidth(valueText, m.valueFont, FontStyle::Bold, canvas);
    const keel::f32 unitTextW = MeasureStyledWidth(unitText, m.unitFont, FontStyle::Regular, canvas);

    const keel::f32 headerH = headerText.empty() ? 0.0f : m.headerH;
    const keel::f32 rowGap = headerText.empty() ? 0.0f : m.rowGap;
    const keel::f32 totalContentH = headerH + rowGap + m.valueRowH;
    const keel::f32 topOffset = keel::max(0.0f, (innerH - totalContentH) * 0.5f);

    const keel::f32 headerY = keel::round(innerY + topOffset);
    const keel::f32 valueRowY = keel::round(headerY + headerH + rowGap);

    if (!headerText.empty()) {
        g.headerRect = Rect(
            innerX,
            headerY,
            keel::min(innerW, keel::ceil(headerTextW) + 2.0f),
            headerH
        );
    }

    keel::f32 rightLimit = innerX + innerW;
    if (showAction) {
        const keel::f32 btnSz = keel::min(m.actionBtnSize, innerH);
        const keel::f32 btnX = keel::round(innerX + innerW - btnSz);
        const keel::f32 btnY = keel::round(valueRowY + (m.valueRowH - btnSz) * 0.5f);
        g.actionRect = Rect(btnX, btnY, btnSz, btnSz);
        rightLimit = keel::max(innerX, btnX - m.actionGap);
    }

    const keel::f32 availValueAndUnitW = keel::max(8.0f, rightLimit - innerX);
    const keel::f32 unitReserve = unitText.empty() ? 0.0f : (m.unitGap + keel::ceil(unitTextW) + 2.0f);
    const keel::f32 maxValW = keel::max(12.0f, availValueAndUnitW - unitReserve);
    const keel::f32 actualValW = keel::min(maxValW, keel::ceil(valueTextW) + 2.0f);

    g.valueRect = Rect(innerX, valueRowY, actualValW, m.valueRowH);

    if (!unitText.empty()) {
        const keel::f32 unitH = keel::round(m.unitFont + 6.0f);
        // Align unit text near the bottom baseline of the large numeric readout
        const keel::f32 unitY = keel::round(valueRowY + m.valueRowH - unitH - 1.0f);
        const keel::f32 unitX = keel::round(g.valueRect.Right() + m.unitGap);
        const keel::f32 availUnitW = keel::max(0.0f, rightLimit - unitX);
        g.unitRect = Rect(unitX, unitY, keel::min(availUnitW, keel::ceil(unitTextW) + 2.0f), unitH);
    }

    return g;
}

void DrawDashedUnderline(ICanvas& canvas, keel::f32 x0, keel::f32 x1, keel::f32 y, Color color, keel::f32 strokeW = 1.5f) {
    if (x1 <= x0 || color.a == 0) return;
    const keel::f32 dashLen = 3.0f;
    const keel::f32 gapLen = 2.0f;
    const keel::f32 py = keel::round(y);
    for (keel::f32 x = x0; x < x1; x += dashLen + gapLen) {
        const keel::f32 segEnd = keel::min(x + dashLen, x1);
        canvas.DrawLine(Vec2(x, py), Vec2(segEnd, py), color, strokeW);
    }
}

keel::StringRef FormatNumericValue(
    Context& ctx,
    keel::f64 val,
    keel::i32 precision,
    bool showPlusSign,
    keel::StringRef invalidText
) {
    if (!keel::is_finite(val)) {
        return ctx.AllocString(invalidText.empty() ? keel::StringRef{"---"} : invalidText);
    }

    const keel::i32 p = keel::clamp(precision, 0, 8);
    static constexpr keel::f64 kPow10[9] = {
        1.0, 10.0, 100.0, 1000.0, 10000.0, 100000.0, 1000000.0, 10000000.0, 100000000.0
    };
    // Suppress flickering "-0.00" when noisy sensor signal hovers near zero
    if (keel::abs(val) < 0.5 / kPow10[p]) {
        val = 0.0;
    }

    static constexpr keel::StaticArray<keel::StringRef, 9> kFmt = {{
        "{:.0f}", "{:.1f}", "{:.2f}", "{:.3f}", "{:.4f}",
        "{:.5f}", "{:.6f}", "{:.7f}", "{:.8f}"
    }};

    char buf[96]{};
    keel::usize offset = 0;
    if (showPlusSign && val > 0.0) {
        buf[0] = '+';
        offset = 1;
    }
    const keel::usize written = keel::format_buf(buf + offset, sizeof(buf) - offset, kFmt[keel::as<keel::usize>(p)], val);
    return ctx.AllocString(keel::StringRef(buf, offset + written));
}

keel::StringRef FormatDefaultPrecisionLabel(Context& ctx, keel::i32 precision) {
    static constexpr keel::StaticArray<keel::StringRef, 9> kExamples = {{
        "0", "0.0", "0.00", "0.000", "0.0000", "0.00000", "0.000000", "0.0000000", "0.00000000"
    }};
    const keel::i32 p = keel::clamp(precision, 0, 8);
    char buf[64]{};
    const keel::StringRef fmt = (p == 1) ? "{} decimal ({})" : "{} decimals ({})";
    const keel::usize written = keel::format_buf(buf, sizeof(buf), fmt, p, kExamples[keel::as<keel::usize>(p)]);
    return ctx.AllocString(keel::StringRef(buf, written));
}

struct ReadoutData {
    ReadoutOptions opts{};
    keel::StringRef headerText{};
    keel::StringRef valueText{};
    keel::StringRef unitText{};
    UiVariant valueVariant{UiVariant::Primary};
    bool showAction{false};
    bool actionDisabled{false};
    bool canOpenChannel{false};
    bool canOpenPrecision{false};
    bool canOpenUnit{false};
    bool cardHovered{false};
    bool headerHovered{false};
    bool valueHovered{false};
    bool unitHovered{false};
    bool actionHovered{false};
    bool actionPressed{false};
    ReadoutOpenMenu openMenu{ReadoutOpenMenu::None};
};

} // namespace

Vec2 detail::Readout::Measure(
    keel::StringRef headerText,
    keel::StringRef valueText,
    keel::StringRef unitText,
    bool showAction,
    const ReadoutOptions& opts,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme
) {
    const ReadoutMetrics m = ComputeReadoutMetrics(size, theme);
    const Padding pad = opts.padding.value_or(m.defaultPadding);

    const keel::f32 headerW = MeasureStyledWidth(headerText, m.headerFont, FontStyle::Bold, canvas);
    const keel::f32 valueW = MeasureStyledWidth(valueText, m.valueFont, FontStyle::Bold, canvas);
    const keel::f32 unitW = MeasureStyledWidth(unitText, m.unitFont, FontStyle::Regular, canvas);

    keel::f32 bottomRowW = valueW;
    if (!unitText.empty()) {
        bottomRowW += m.unitGap + unitW;
    }
    if (showAction) {
        bottomRowW += m.actionGap + m.actionBtnSize;
    }

    const keel::f32 contentW = keel::max(headerW, bottomRowW);
    const keel::f32 headerH = headerText.empty() ? 0.0f : (m.headerH + m.rowGap);
    const keel::f32 contentH = headerH + m.valueRowH;

    const keel::f32 totalW = keel::ceil(contentW + pad.left + pad.right + 4.0f);
    const keel::f32 totalH = keel::ceil(contentH + pad.top + pad.bottom);
    return Vec2(totalW, totalH);
}

void detail::Readout::Render(const Node* node, ICanvas& canvas, const UiTheme& theme) {
    const auto* data = node ? node->As<ReadoutData>() : nullptr;
    if (!data) return;

    const Rect& rect = node->computedRect;
    if (rect.w <= 0.0f || rect.h <= 0.0f) return;

    const ReadoutOptions& opts = data->opts;
    const ReadoutMetrics m = ComputeReadoutMetrics(node->uiSize, theme);
    const auto& pal = theme.palette;
    const bool isDark = (theme.mode == ThemeMode::Dark);
    const bool disabled = node->disabled;
    const keel::f32 radius = opts.border.radius.has_value() ? *opts.border.radius : m.radius;

    // 1 & 2. Card surface, shadow & border routed through IStylePainter
    const bool isRaised = (opts.surface == SurfaceRole::Raised || opts.surface == SurfaceRole::Popup);
    const Color defaultBg = isRaised ? pal.surfaceElevated : pal.surface;
    const Color defaultBorder = (data->openMenu != ReadoutOpenMenu::None)
        ? pal.accent.WithAlphaF(0.65f)
        : (data->cardHovered && !disabled ? pal.borderHover : pal.border);
    const Color bg = opts.background.IsBackgroundEnabled(true)
        ? theme.ResolveColor(opts.background, data->cardHovered, false, disabled, defaultBg)
        : Color::Transparent();
    const Color borderCol = (opts.border.width > 0.0f)
        ? theme.ResolveColor(opts.border.color, data->cardHovered, data->openMenu != ReadoutOpenMenu::None, disabled, defaultBorder)
        : Color::Transparent();

    theme.Painter().DrawSurface(canvas, theme, SurfacePaintParams{
        .rect = rect,
        .role = opts.surface,
        .radius = radius,
        .explicitRadius = opts.border.radius.has_value(),
        .bg = bg,
        .border = borderCol,
        .borderWidth = opts.border.width,
        .explicitBorder = opts.border.HasExplicitStroke(),
        .elevation = isRaised ? Elevation::Sm : Elevation::None
    });

    // 3. Deterministic sub-element geometry
    const ReadoutGeometry g = ComputeReadoutGeometry(
        rect,
        data->headerText,
        data->valueText,
        data->unitText,
        data->showAction,
        opts,
        m,
        &canvas
    );

    canvas.PushClip(rect);

    // 4. Channel header
    if (!data->headerText.empty()) {
        const bool headerOpen = (data->openMenu == ReadoutOpenMenu::Channel);
        Color headerCol = headerOpen ? pal.accent : pal.textSecondary;
        if (data->canOpenChannel && data->headerHovered && !headerOpen) {
            headerCol = pal.textPrimary;
        }
        if (disabled) {
            headerCol = headerCol.WithAlphaF(theme.disabledOpacity);
        }
        canvas.DrawTextBoxStyled(
            data->headerText,
            g.headerRect,
            m.headerFont,
            headerCol,
            FontStyle::Bold,
            0.0f,
            0.5f
        );
        if (data->canOpenChannel && (data->headerHovered || headerOpen) && !disabled) {
            const Color ulCol = headerOpen ? pal.accent : pal.textSecondary.WithAlphaF(0.85f);
            DrawDashedUnderline(canvas, g.headerRect.x, g.headerRect.Right() - 1.0f, g.headerRect.Bottom(), ulCol, 1.4f);
        }
    }

    // 5. Large numeric value
    if (!data->valueText.empty()) {
        const bool valueOpen = (data->openMenu == ReadoutOpenMenu::Precision);
        Color valCol = (data->valueVariant == UiVariant::Default)
            ? pal.textPrimary
            : theme.GetSemanticAccent(data->valueVariant);
        if (data->valueVariant == UiVariant::Primary) {
            valCol = isDark
                ? Color::Lerp(pal.accent, Color(255, 255, 255, pal.accent.a), 0.10f)
                : Color::Lerp(pal.accent, Color(0, 0, 0, pal.accent.a), 0.10f);
        } else if (data->valueVariant == UiVariant::Muted) {
            valCol = pal.textSecondary;
        }
        if (disabled) {
            valCol = valCol.WithAlphaF(theme.disabledOpacity);
        }

        canvas.DrawTextBoxStyled(
            data->valueText,
            g.valueRect,
            m.valueFont,
            valCol,
            FontStyle::Bold,
            0.0f,
            0.5f
        );
        if (data->canOpenPrecision && (data->valueHovered || valueOpen) && !disabled) {
            const Color ulCol = valueOpen ? valCol : valCol.WithAlphaF(0.75f);
            DrawDashedUnderline(canvas, g.valueRect.x, g.valueRect.Right() - 1.0f, g.valueRect.Bottom() - 1.0f, ulCol, 1.8f);
        }
    }

    // 6. Display unit
    if (!data->unitText.empty() && g.unitRect.w > 2.0f) {
        const bool unitOpen = (data->openMenu == ReadoutOpenMenu::Unit);
        Color unitCol = unitOpen ? pal.accent : pal.textSecondary;
        if (data->canOpenUnit && data->unitHovered && !unitOpen) {
            unitCol = pal.textPrimary;
        }
        if (disabled) {
            unitCol = unitCol.WithAlphaF(theme.disabledOpacity);
        }

        canvas.DrawTextBoxStyled(
            data->unitText,
            g.unitRect,
            m.unitFont,
            unitCol,
            FontStyle::Regular,
            0.0f,
            0.5f
        );
        if (data->canOpenUnit && (data->unitHovered || unitOpen) && !disabled) {
            const Color ulCol = unitOpen ? pal.accent : pal.textSecondary.WithAlphaF(0.85f);
            DrawDashedUnderline(canvas, g.unitRect.x, g.unitRect.Right() - 1.0f, g.unitRect.Bottom() - 1.0f, ulCol, 1.5f);
        }
    }

    // 7. Trailing momentary action button (◎ Target / Tare)
    if (data->showAction && g.actionRect.w > 0.0f && g.actionRect.h > 0.0f) {
        const bool actDis = disabled || data->actionDisabled;
        Color btnBg = isDark ? pal.surfaceElevated : pal.surfaceHover;
        Color btnBorder = pal.border;
        Color iconCol = pal.textSecondary;

        if (!actDis) {
            if (data->actionPressed) {
                btnBg = pal.surfaceActive;
                btnBorder = pal.accent.WithAlphaF(0.75f);
                iconCol = pal.textPrimary;
            } else if (data->actionHovered) {
                btnBg = isDark ? pal.surfaceActive : Color::Lerp(pal.surfaceHover, pal.border, 0.25f);
                btnBorder = pal.borderHover;
                iconCol = pal.textPrimary;
            }
        } else {
            btnBg = btnBg.WithAlphaF(theme.disabledOpacity);
            btnBorder = btnBorder.WithAlphaF(theme.disabledOpacity);
            iconCol = iconCol.WithAlphaF(theme.disabledOpacity);
        }

        const keel::f32 btnRadius = keel::max(4.0f, radius - 2.0f);
        theme.Painter().DrawFrame(canvas, theme, FrameKind::Button, FramePaintParams{
            .rect = g.actionRect,
            .radius = btnRadius,
            .bg = btnBg,
            .border = btnBorder,
            .borderWidth = 1.0f,
            .hovered = data->actionHovered && !actDis,
            .pressed = data->actionPressed && !actDis,
            .active = false,
            .disabled = actDis,
            .accent = pal.accent
        });

        if (opts.actionIcon.IsValid()) {
            const keel::f32 shift = theme.Painter().GetButtonContentOffset(theme, data->actionPressed, false, actDis);
            const keel::f32 icSz = keel::min(m.actionIconSize, g.actionRect.w * 0.65f);
            const Rect icRect(
                g.actionRect.x + (g.actionRect.w - icSz) * 0.5f + shift,
                g.actionRect.y + (g.actionRect.h - icSz) * 0.5f + shift,
                icSz,
                icSz
            );
            opts.actionIcon.Draw(canvas, icRect, iconCol);
        }
    }

    canvas.PopClip();
}

ReadoutResponse detail::Readout::Run(
    Context& ctx,
    ReadoutState& state,
    keel::ArrayRef<ReadoutChannel> channels,
    const ReadoutOptions& opts,
    keel::SourceLocation loc
) {
    const Id id = ctx.MakeChildId(loc);
    const UiSize activeSize = opts.size;
    const bool effectiveDisabled = opts.disabled || ctx.IsDisabled();
    const UiTheme& theme = ctx.GetTheme();
    const ReadoutMetrics m = ComputeReadoutMetrics(activeSize, theme);

    auto& persist = ctx.GetOrCreateState<ReadoutPersistState>(id);

    ReadoutResponse res{};
    res.id = id;
    res.ctx = &ctx;

    // Clamp state indices safely
    const keel::i32 minP = keel::clamp(opts.minPrecision, 0, 8);
    const keel::i32 maxP = keel::clamp(opts.maxPrecision, minP, 8);
    state.precision = keel::clamp(state.precision, minP, maxP);

    ReadoutChannel fallbackCh{};
    if (!channels.empty()) {
        state.channelIndex = keel::clamp(state.channelIndex, 0, keel::as<keel::i32>(channels.size()) - 1);
    } else {
        state.channelIndex = 0;
    }
    const ReadoutChannel& activeCh = channels.empty()
        ? fallbackCh
        : channels[keel::as<keel::usize>(state.channelIndex)];

    if (!activeCh.units.empty()) {
        state.unitIndex = keel::clamp(state.unitIndex, 0, keel::as<keel::i32>(activeCh.units.size()) - 1);
    } else {
        state.unitIndex = 0;
    }

    const bool hasUnit = !activeCh.units.empty();
    const keel::f64 displayedVal = hasUnit
        ? activeCh.units[keel::as<keel::usize>(state.unitIndex)].Convert(activeCh.value)
        : activeCh.value;
    res.displayedValue = displayedVal;

    const bool hasCustomFormatted = !activeCh.formattedValue.empty();
    const keel::StringRef headerText = !activeCh.headerLabel.empty()
        ? ctx.AllocString(activeCh.headerLabel.text)
        : ctx.AllocString(activeCh.label.text);
    const keel::StringRef valueText = hasCustomFormatted
        ? ctx.AllocString(activeCh.formattedValue.text)
        : FormatNumericValue(ctx, displayedVal, state.precision, opts.showPlusSign, opts.invalidText.text);
    const keel::StringRef unitText = hasUnit
        ? ctx.AllocString(activeCh.units[keel::as<keel::usize>(state.unitIndex)].label.text)
        : keel::StringRef{};

    const bool canOpenChannel = !effectiveDisabled && opts.allowChannelMenu && channels.size() > 1;
    const bool canOpenPrecision = !effectiveDisabled && opts.allowPrecisionMenu && !hasCustomFormatted && (maxP > minP);
    const bool canOpenUnit = !effectiveDisabled && opts.allowUnitMenu && activeCh.units.size() > 1;

    if (effectiveDisabled) {
        persist.openMenu = ReadoutOpenMenu::None;
    }
    if ((persist.openMenu == ReadoutOpenMenu::Channel && !canOpenChannel) ||
        (persist.openMenu == ReadoutOpenMenu::Precision && !canOpenPrecision) ||
        (persist.openMenu == ReadoutOpenMenu::Unit && !canOpenUnit)) {
        persist.openMenu = ReadoutOpenMenu::None;
    }

    const Rect prevRect = ctx.GetPrevFrameBounds(id);
    res.rect = prevRect;

    const ReadoutGeometry prevGeom = (prevRect.w > 0.0f && prevRect.h > 0.0f)
        ? ComputeReadoutGeometry(prevRect, headerText, valueText, unitText, activeCh.showAction, opts, m, ctx.GetCanvas())
        : ReadoutGeometry{};

    bool cardHovered = false;
    bool headerHovered = false;
    bool valueHovered = false;
    bool unitHovered = false;
    bool actionHovered = false;
    bool actionPressed = false;

    const bool menuWasOpen = (persist.openMenu != ReadoutOpenMenu::None);

    // Hit-test sub-zones (note: allow clicking card zones even while our own menu is open so 1-click toggle/switch works!)
    if (!effectiveDisabled && prevRect.w > 0.0f && prevRect.h > 0.0f) {
        const Vec2 mouse = ctx.GetMousePos();
        const bool overCard = ctx.IsRectHovered(id, prevRect) ||
                              (menuWasOpen && prevRect.Contains(mouse) && !ctx.GetOpenPopupRect().Contains(mouse));
        if (overCard) {
            cardHovered = true;
            if (activeCh.showAction && prevGeom.actionRect.Contains(mouse)) {
                actionHovered = true;
                const bool actDis = activeCh.actionDisabled;
                if (!actDis) {
                    ctx.SetCursor(CursorType::Pointer);
                    actionPressed = ctx.IsMouseDown();
                    if (ctx.IsMouseClicked()) {
                        res.actionClicked = true;
                        res.changed = true;
                        persist.openMenu = ReadoutOpenMenu::None;
                        ctx.ConsumeMouseClick();
                        ctx.InvalidateAll();
                    }
                }
                const UiText& tip = !activeCh.actionTooltip.empty() ? activeCh.actionTooltip : opts.actionTooltip;
                if (!tip.empty()) {
                    ctx.SetTooltip(tip, prevGeom.actionRect);
                }
            } else if (canOpenChannel && prevGeom.headerRect.Contains(mouse)) {
                headerHovered = true;
                ctx.SetCursor(CursorType::Pointer);
                if (ctx.IsMouseClicked()) {
                    persist.openMenu = (persist.openMenu == ReadoutOpenMenu::Channel)
                        ? ReadoutOpenMenu::None
                        : ReadoutOpenMenu::Channel;
                    ctx.ConsumeMouseClick();
                    ctx.InvalidateAll();
                }
            } else if (canOpenPrecision && prevGeom.valueRect.Contains(mouse)) {
                valueHovered = true;
                ctx.SetCursor(CursorType::Pointer);
                if (ctx.IsMouseClicked()) {
                    persist.openMenu = (persist.openMenu == ReadoutOpenMenu::Precision)
                        ? ReadoutOpenMenu::None
                        : ReadoutOpenMenu::Precision;
                    ctx.ConsumeMouseClick();
                    ctx.InvalidateAll();
                }
            } else if (canOpenUnit && prevGeom.unitRect.Contains(mouse)) {
                unitHovered = true;
                ctx.SetCursor(CursorType::Pointer);
                if (ctx.IsMouseClicked()) {
                    persist.openMenu = (persist.openMenu == ReadoutOpenMenu::Unit)
                        ? ReadoutOpenMenu::None
                        : ReadoutOpenMenu::Unit;
                    ctx.ConsumeMouseClick();
                    ctx.InvalidateAll();
                }
            }
        }
    }

    // Run active dropdown menu via native ContextMenu
    {
        bool menuOpen = (persist.openMenu != ReadoutOpenMenu::None);

        keel::SmallArray<ContextMenuItem, 16> menuItems;
        ContextMenuOptions menuOpts{};
        menuOpts.anchorRect = prevRect;
        menuOpts.size = activeSize;
        menuOpts.minWidth = 176.0f;

        if (persist.openMenu == ReadoutOpenMenu::Channel) {
            menuOpts.title = opts.channelMenuTitle;
            menuOpts.anchorPos = Vec2(prevGeom.headerRect.x, prevGeom.headerRect.Bottom() + 6.0f);
            menuItems.reserve(channels.size());
            for (keel::usize i = 0; i < channels.size(); ++i) {
                ContextMenuItem item{};
                item.label = channels[i].label;
                item.hasCheck = true;
                item.checked = (keel::as<keel::i32>(i) == state.channelIndex);
                menuItems.push_back(item);
            }
        } else if (persist.openMenu == ReadoutOpenMenu::Precision) {
            menuOpts.title = opts.precisionMenuTitle;
            menuOpts.anchorPos = Vec2(prevGeom.valueRect.x, prevGeom.valueRect.Bottom() + 6.0f);
            const keel::i32 count = maxP - minP + 1;
            menuItems.reserve(keel::as<keel::usize>(count));
            for (keel::i32 p = minP; p <= maxP; ++p) {
                const keel::usize labelIdx = keel::as<keel::usize>(p - minP);
                ContextMenuItem item{};
                item.label = (labelIdx < opts.precisionLabels.size() && !opts.precisionLabels[labelIdx].empty())
                    ? opts.precisionLabels[labelIdx]
                    : UiText(FormatDefaultPrecisionLabel(ctx, p));
                item.hasCheck = true;
                item.checked = (p == state.precision);
                menuItems.push_back(item);
            }
        } else if (persist.openMenu == ReadoutOpenMenu::Unit) {
            menuOpts.title = opts.unitMenuTitle;
            menuOpts.anchorPos = Vec2(prevGeom.unitRect.x, prevGeom.unitRect.Bottom() + 6.0f);
            menuItems.reserve(activeCh.units.size());
            for (keel::usize i = 0; i < activeCh.units.size(); ++i) {
                ContextMenuItem item{};
                item.label = activeCh.units[i].label;
                item.hasCheck = true;
                item.checked = (keel::as<keel::i32>(i) == state.unitIndex);
                menuItems.push_back(item);
            }
        }

        const ContextMenuResponse menuRes = detail::ContextMenu::Run(ctx, menuOpen, menuItems, menuOpts, loc);
        if (menuRes.clickedIndex >= 0) {
            if (persist.openMenu == ReadoutOpenMenu::Channel) {
                const keel::i32 nextChIdx = keel::clamp(menuRes.clickedIndex, 0, keel::as<keel::i32>(channels.size()) - 1);
                if (nextChIdx != state.channelIndex) {
                    state.channelIndex = nextChIdx;
                    const ReadoutChannel& nextCh = channels[keel::as<keel::usize>(nextChIdx)];
                    state.unitIndex = nextCh.units.empty()
                        ? 0
                        : keel::clamp(nextCh.defaultUnitIndex, 0, keel::as<keel::i32>(nextCh.units.size()) - 1);
                    state.precision = keel::clamp(nextCh.defaultPrecision, minP, maxP);
                    res.channelChanged = true;
                    res.changed = true;
                }
            } else if (persist.openMenu == ReadoutOpenMenu::Precision) {
                const keel::i32 nextP = keel::clamp(minP + menuRes.clickedIndex, minP, maxP);
                if (nextP != state.precision) {
                    state.precision = nextP;
                    res.precisionChanged = true;
                    res.changed = true;
                }
            } else if (persist.openMenu == ReadoutOpenMenu::Unit) {
                const keel::i32 nextUnit = activeCh.units.empty()
                    ? 0
                    : keel::clamp(menuRes.clickedIndex, 0, keel::as<keel::i32>(activeCh.units.size()) - 1);
                if (nextUnit != state.unitIndex) {
                    state.unitIndex = nextUnit;
                    res.unitChanged = true;
                    res.changed = true;
                }
            }
            persist.openMenu = ReadoutOpenMenu::None;
        } else if (!menuOpen) {
            persist.openMenu = ReadoutOpenMenu::None;
        }
    }

    res.hovered = cardHovered;
    res.pressed = actionPressed;

    const ControlSpec spec = Control::FromOptions(
        opts,
        SizeMode::Fit,
        SizeMode::Fit,
        ElementLayout::Stacked,
        6.0f,
        4.0f,
        12.0f
    );
    Node* node = ctx.CreateNode(NodeType::Readout, id);
    node->uiSize = activeSize;
    node->disabled = effectiveDisabled;
    node->hovered = cardHovered;
    node->pressed = actionPressed;

    auto* data = ctx.AllocPayload<ReadoutData>();
    data->opts = opts;
    data->opts.actionIcon = ctx.AllocIcon(opts.actionIcon);
    data->opts.actionTooltip = ctx.AllocText(opts.actionTooltip);
    data->opts.invalidText = ctx.AllocText(opts.invalidText);
    data->opts.channelMenuTitle = ctx.AllocText(opts.channelMenuTitle);
    data->opts.precisionMenuTitle = ctx.AllocText(opts.precisionMenuTitle);
    data->opts.unitMenuTitle = ctx.AllocText(opts.unitMenuTitle);
    data->headerText = headerText;
    data->valueText = valueText;
    data->unitText = unitText;
    data->valueVariant = activeCh.variant;
    data->showAction = activeCh.showAction;
    data->actionDisabled = activeCh.actionDisabled;
    data->canOpenChannel = canOpenChannel;
    data->canOpenPrecision = canOpenPrecision;
    data->canOpenUnit = canOpenUnit;
    data->cardHovered = cardHovered;
    data->headerHovered = headerHovered;
    data->valueHovered = valueHovered;
    data->unitHovered = unitHovered;
    data->actionHovered = actionHovered;
    data->actionPressed = actionPressed;
    data->openMenu = persist.openMenu;

    node->text = valueText;
    node->SetPayload(data);
    node->measureFn = [](const Node* n, keel::f32 /*availW*/, ICanvas* canvas, const UiTheme& t) -> Vec2 {
        const auto* d = n->As<ReadoutData>();
        if (!d) return Vec2(188.0f, 72.0f);
        return detail::Readout::Measure(d->headerText, d->valueText, d->unitText, d->showAction, d->opts, n->uiSize, canvas, t);
    };
    node->renderFn = &detail::Readout::Render;

    node->visualHash = detail::HashArgs(
        headerText,
        valueText,
        unitText,
        activeCh.variant,
        activeCh.showAction,
        activeCh.actionDisabled,
        opts.actionIcon,
        opts.background,
        opts.border,
        keel::as<keel::u8>(opts.surface),
        activeSize,
        effectiveDisabled,
        cardHovered,
        headerHovered,
        valueHovered,
        unitHovered,
        actionHovered,
        actionPressed,
        persist.openMenu
    );
    Control::SetupNode(ctx, node, spec);
    return res;
}

ReadoutResponse detail::Readout::Run(
    Context& ctx,
    UiText label,
    keel::f64 value,
    keel::i32& unitIndex,
    keel::i32& precision,
    keel::ArrayRef<ReadoutUnit> units,
    const ReadoutOptions& opts,
    keel::SourceLocation loc
) {
    ReadoutChannel ch{};
    ch.label = label;
    ch.value = value;
    ch.units = units;
    ch.defaultUnitIndex = unitIndex;
    ch.defaultPrecision = precision;

    ReadoutState st{};
    st.channelIndex = 0;
    st.unitIndex = unitIndex;
    st.precision = precision;

    const ReadoutChannel single[1] = {ch};
    const ReadoutResponse res = Run(ctx, st, keel::ArrayRef<ReadoutChannel>(single, 1), opts, loc);
    unitIndex = st.unitIndex;
    precision = st.precision;
    return res;
}

} // namespace guikit
