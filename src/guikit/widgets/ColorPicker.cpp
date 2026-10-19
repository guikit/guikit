#include "guikit/widgets/ColorPicker.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <vector>
#include "guikit/Context.hpp"
#include "guikit/backend/ICanvas.hpp"
#include "guikit/core/Node.hpp"
#include "guikit/core/TextUtil.hpp"

namespace guikit {

namespace {

constexpr std::array<Color, 10> kDefaultPresets = {{
    Color(59, 130, 246, 255),   // Primary Blue
    Color(14, 165, 233, 255),   // Sky / Cyan
    Color(16, 185, 129, 255),   // Emerald / Success
    Color(132, 204, 22, 255),   // Lime
    Color(245, 158, 11, 255),   // Amber / Warning
    Color(249, 115, 22, 255),   // Orange
    Color(239, 68, 68, 255),    // Red / Danger
    Color(236, 72, 153, 255),   // Pink
    Color(168, 85, 247, 255),   // Purple
    Color(226, 232, 240, 255)   // Slate White
}};

struct PanelLayout {
    Rect panelRect{};
    Rect svRect{};
    Rect hueRect{};
    Rect alphaRect{};
    Rect infoRowRect{};
    Rect previewSwatchRect{};
    Rect hexTextRect{};
    Rect rgbaTextRect{};
    Rect swatchesRowRect{};
};

PanelLayout ComputePanelLayout(Rect rect, const ColorPickerOptions& opts, const UiTheme& theme) noexcept {
    PanelLayout L{};
    L.panelRect = rect;
    const float pad = theme.Scale(10.0f);
    const float gap = theme.Scale(8.0f);
    const float barH = theme.Scale(12.0f);
    const float infoH = theme.Scale(24.0f);
    const float swatchRowH = opts.showSwatches ? theme.Scale(22.0f) : 0.0f;

    float bottomSpace = barH + gap + infoH;
    if (opts.showAlpha) {
        bottomSpace += barH + gap;
    }
    if (opts.showSwatches) {
        bottomSpace += swatchRowH + gap;
    }

    const float innerX = rect.x + pad;
    const float innerW = std::max(20.0f, rect.w - pad * 2.0f);
    const float svH = std::max(48.0f, rect.h - pad * 2.0f - bottomSpace);

    float curY = rect.y + pad;
    L.svRect = Rect(innerX, curY, innerW, svH);
    curY += svH + gap;

    L.hueRect = Rect(innerX, curY, innerW, barH);
    curY += barH + gap;

    if (opts.showAlpha) {
        L.alphaRect = Rect(innerX, curY, innerW, barH);
        curY += barH + gap;
    }

    L.infoRowRect = Rect(innerX, curY, innerW, infoH);
    const float prevW = theme.Scale(32.0f);
    L.previewSwatchRect = Rect(innerX, curY, prevW, infoH);
    const float textStartX = innerX + prevW + gap;
    const float remW = std::max(0.0f, innerW - prevW - gap);
    L.hexTextRect = Rect(textStartX, curY, remW * 0.46f, infoH);
    L.rgbaTextRect = Rect(textStartX + remW * 0.46f, curY, remW * 0.54f, infoH);
    curY += infoH;

    if (opts.showSwatches) {
        curY += gap;
        L.swatchesRowRect = Rect(innerX, curY, innerW, swatchRowH);
    }

    return L;
}

Vec2 DefaultPanelDimensions(const ColorPickerOptions& opts, const UiTheme& theme) noexcept {
    float h = 196.0f;
    if (opts.showAlpha) h += 20.0f;
    if (opts.showSwatches) h += 30.0f;
    return {theme.Scale(236.0f), theme.Scale(h)};
}

ControlSpec EffectiveColorPickerControl(const ColorPickerOptions& opts, UiSize size, const UiTheme& theme) noexcept {
    const SelectMetrics& m = theme.GetSelect(size);
    ControlSpec c{};
    c.width = opts.width;
    c.height = opts.height;
    c.minWidth = opts.minWidth;
    c.align = opts.align;
    c.background = opts.background;
    c.border = opts.border;
    c.defaultLayout = ElementLayout::Stacked;
    c.defaultInlineOrder = InlineOrder::LabelFirst;
    c.labelGap = m.labelGap;
    c.descGap = m.descGap;
    c.inlineGap = m.paddingX;
    c.stretchable = true;
    c.stretchHeightWhenStandalone = false;
    c.hasIntrinsicBox = true;
    return c;
}

Vec2 MeasureColorPickerBox(const ColorPickerOptions& opts, UiSize size, ICanvas* canvas, const UiTheme& theme) noexcept {
    if (opts.inlinePanel) {
        const Vec2 defPanel = DefaultPanelDimensions(opts, theme);
        float w = (opts.stretch != StretchMode::Fill && opts.width.has_value()) ? theme.Scale(*opts.width) : defPanel.x;
        if (opts.minWidth.has_value()) {
            w = std::max(theme.Scale(*opts.minWidth), w);
        }
        const float h = opts.height.has_value() ? theme.Scale(*opts.height) : defPanel.y;
        return {w, h};
    }

    const SelectMetrics& m = theme.GetSelect(size);
    const float boxH = opts.height.has_value() ? theme.Scale(*opts.height) : m.defaultHeight;
    float naturalW = boxH * 1.45f;
    if (opts.showHex) {
        const float fontSize = theme.GetFontSize(TextRole::LabelMedium, size);
        const std::string_view sample = opts.showAlpha ? "#FF8800FF" : "#FF8800";
        const float textW = (canvas != nullptr)
            ? canvas->MeasureText(sample, fontSize).x
            : (static_cast<float>(sample.size()) * fontSize * 0.62f);
        naturalW = boxH + textW + m.paddingX * 2.0f + m.iconSize + 12.0f;
    }
    if (opts.minWidth.has_value()) {
        naturalW = std::max(theme.Scale(*opts.minWidth), naturalW);
    }
    if (opts.stretch != StretchMode::Fill && opts.width.has_value()) {
        naturalW = theme.Scale(*opts.width);
    }
    return {naturalW, boxH};
}

void DrawCheckerboardSwatch(ICanvas& canvas, Rect rect, Color color, float radius, Color borderCol) {
    if (rect.w <= 0.0f || rect.h <= 0.0f) return;

    if (color.a < 255) {
        const Color cLight(210, 214, 224, 255);
        const Color cDark(132, 138, 152, 255);
        canvas.DrawRoundRect(rect, radius, cLight);
        const Rect inner = rect.Inset(1.5f);
        const float midX = inner.x + inner.w * 0.5f;
        const float midY = inner.y + inner.h * 0.5f;
        canvas.DrawRect(Rect(inner.x, inner.y, inner.w * 0.5f, inner.h * 0.5f), cDark);
        canvas.DrawRect(Rect(midX, midY, inner.w * 0.5f, inner.h * 0.5f), cDark);
    }
    canvas.DrawRoundRect(rect, radius, color, borderCol, 1.0f);
}

void FormatHexColor(Color c, bool showAlpha, char* buf, size_t bufSize) noexcept {
    if (showAlpha) {
        std::snprintf(buf, bufSize, "#%02X%02X%02X%02X", c.r, c.g, c.b, c.a);
    } else {
        std::snprintf(buf, bufSize, "#%02X%02X%02X", c.r, c.g, c.b);
    }
}

} // namespace

void ColorPicker::RgbToHsv(Color c, float& outH, float& outS, float& outV) noexcept {
    const float r = static_cast<float>(c.r) / 255.0f;
    const float g = static_cast<float>(c.g) / 255.0f;
    const float b = static_cast<float>(c.b) / 255.0f;

    const float maxC = std::max({r, g, b});
    const float minC = std::min({r, g, b});
    const float delta = maxC - minC;

    outV = maxC;
    outS = (maxC > 1e-5f) ? (delta / maxC) : 0.0f;

    if (delta <= 1e-5f) {
        return;
    }

    float h = 0.0f;
    if (maxC == r) {
        h = 60.0f * std::fmod(((g - b) / delta) + 6.0f, 6.0f);
    } else if (maxC == g) {
        h = 60.0f * (((b - r) / delta) + 2.0f);
    } else {
        h = 60.0f * (((r - g) / delta) + 4.0f);
    }
    if (h < 0.0f) h += 360.0f;
    outH = std::clamp(h, 0.0f, 360.0f);
}

Color ColorPicker::HsvToRgb(float h, float s, float v, uint8_t a) noexcept {
    const float clampedH = std::fmod(std::max(0.0f, h), 360.0f);
    const float clampedS = std::clamp(s, 0.0f, 1.0f);
    const float clampedV = std::clamp(v, 0.0f, 1.0f);

    const float c = clampedV * clampedS;
    const float x = c * (1.0f - std::abs(std::fmod(clampedH / 60.0f, 2.0f) - 1.0f));
    const float m = clampedV - c;

    float r1 = 0.0f, g1 = 0.0f, b1 = 0.0f;
    if (clampedH < 60.0f) {
        r1 = c; g1 = x; b1 = 0.0f;
    } else if (clampedH < 120.0f) {
        r1 = x; g1 = c; b1 = 0.0f;
    } else if (clampedH < 180.0f) {
        r1 = 0.0f; g1 = c; b1 = x;
    } else if (clampedH < 240.0f) {
        r1 = 0.0f; g1 = x; b1 = c;
    } else if (clampedH < 300.0f) {
        r1 = x; g1 = 0.0f; b1 = c;
    } else {
        r1 = c; g1 = 0.0f; b1 = x;
    }

    return Color(
        static_cast<uint8_t>(std::clamp(std::round((r1 + m) * 255.0f), 0.0f, 255.0f)),
        static_cast<uint8_t>(std::clamp(std::round((g1 + m) * 255.0f), 0.0f, 255.0f)),
        static_cast<uint8_t>(std::clamp(std::round((b1 + m) * 255.0f), 0.0f, 255.0f)),
        a
    );
}

Vec2 ColorPicker::Measure(
    const ColorPickerOptions& opts,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme,
    float /*availableWidth*/
) {
    const Vec2 boxDims = MeasureColorPickerBox(opts, size, canvas, theme);
    const ControlSpec ctrl = EffectiveColorPickerControl(opts, size, theme);
    return Control::Measure(boxDims, ctrl, opts.stretch, size, theme);
}

Vec2 ColorPicker::Measure(
    const ColorPickerOptions& opts,
    UiSize size,
    ICanvas* canvas,
    float availableWidth
) {
    static const UiTheme defaultTheme = UiTheme::Dark();
    return Measure(opts, size, canvas, defaultTheme, availableWidth);
}

Response ColorPicker::Run(
    Context& ctx,
    Color& value,
    const ColorPickerOptions& opts,
    std::source_location loc
) {
    const Id id = ctx.MakeChildId(loc);
    const UiSize activeSize = ctx.GetCurrentSize();
    const bool effectiveDisabled = opts.disabled || ctx.IsDisabled();
    const UiTheme theme = ctx.GetTheme();
    ICanvas* canvas = ctx.GetCanvas();

    ColorPickerOptions optsCopy = opts;
    Control::InheritElementSizing(ctx, optsCopy.stretch, optsCopy.width, optsCopy.height);

    const Vec2 naturalDims = MeasureColorPickerBox(optsCopy, activeSize, canvas, theme);
    const float unscaledW = (theme.scale > 0.0f) ? (naturalDims.x / theme.scale) : naturalDims.x;
    if (optsCopy.stretch != StretchMode::Fill && !optsCopy.width.has_value()) {
        optsCopy.width = unscaledW;
    }

    auto& state = ctx.GetOrCreateState<ColorPickerState>(id);
    if (!state.initialized || state.lastSyncedColor != value) {
        RgbToHsv(value, state.h, state.s, state.v);
        state.lastSyncedColor = value;
        state.initialized = true;
    }

    const bool wasOpen = !optsCopy.inlinePanel && ctx.IsPopupOpen(id);
    ctx.RegisterFocusable(id, effectiveDisabled);

    Response res = ctx.Interact(id);
    if (effectiveDisabled) {
        res.clicked = false;
        res.hovered = false;
        res.pressed = false;
        res.focused = false;
        state.dragTarget = 0;
        state.hoveredSwatch = -1;
    } else if (res.clicked && !optsCopy.inlinePanel) {
        ctx.RequestFocus(id);
    }

    const Rect prevBounds = ctx.GetPrevFrameBounds(id);
    const Rect ctrlRect = prevBounds;

    bool isOpen = wasOpen;

    // Toggle popup when closed swatch button is clicked
    if (!optsCopy.inlinePanel && !effectiveDisabled) {
        bool toggleRequested = res.clicked;
        if (!wasOpen && ctx.IsFocused(id) && (ctx.ConsumeKey(Key::Space) || ctx.ConsumeKey(Key::Enter))) {
            toggleRequested = true;
        }
        if (toggleRequested) {
            if (wasOpen) {
                ctx.ClosePopup();
                isOpen = false;
                state.dragTarget = 0;
            } else {
                const Vec2 popDims = DefaultPanelDimensions(optsCopy, theme);
                float popupX = ctrlRect.x;
                float popupY = ctrlRect.Bottom() + 4.0f;
                const Vec2 vp = ctx.GetViewportSize();
                if (vp.y > 0.0f && popupY + popDims.y > vp.y - 6.0f) {
                    const float upY = ctrlRect.y - 4.0f - popDims.y;
                    if (upY >= 6.0f) {
                        popupY = upY;
                    }
                }
                if (vp.x > 0.0f) {
                    popupX = std::clamp(popupX, 6.0f, std::max(6.0f, vp.x - popDims.x - 6.0f));
                }
                ctx.OpenPopup(id, Rect(popupX, popupY, popDims.x, popDims.y));
                isOpen = true;
                state.dragTarget = 0;
            }
        }
    }

    // Interactive handling for either the open floating popup OR the inline panel
    const bool panelActive = !effectiveDisabled && (optsCopy.inlinePanel || (isOpen && wasOpen));
    state.hoveredSwatch = -1;

    if (panelActive) {
        const Rect activePanelRect = optsCopy.inlinePanel ? ctrlRect : ctx.GetOpenPopupRect();
        const PanelLayout L = ComputePanelLayout(activePanelRect, optsCopy, theme);
        const Vec2 mouse = ctx.GetMousePos();
        const std::span<const Color> swatches = optsCopy.presets.empty()
            ? std::span<const Color>(kDefaultPresets)
            : optsCopy.presets;

        // Check swatch hover & click
        if (optsCopy.showSwatches && !swatches.empty() && state.dragTarget == 0 && L.swatchesRowRect.Contains(mouse)) {
            const float gap = theme.Scale(5.0f);
            const float countF = static_cast<float>(swatches.size());
            const float swW = std::max(10.0f, (L.swatchesRowRect.w - gap * (countF - 1.0f)) / countF);
            for (size_t i = 0; i < swatches.size(); ++i) {
                const Rect sr(L.swatchesRowRect.x + static_cast<float>(i) * (swW + gap), L.swatchesRowRect.y, swW, L.swatchesRowRect.h);
                if (sr.Contains(mouse)) {
                    state.hoveredSwatch = static_cast<int>(i);
                    if (ctx.IsMouseClicked()) {
                        const uint8_t newAlpha = optsCopy.showAlpha ? swatches[i].a : value.a;
                        const Color nextCol(swatches[i].r, swatches[i].g, swatches[i].b, newAlpha);
                        if (nextCol != value) {
                            value = nextCol;
                            RgbToHsv(value, state.h, state.s, state.v);
                            state.lastSyncedColor = value;
                            res.changed = true;
                        }
                        if (!optsCopy.inlinePanel) {
                            ctx.ConsumeMouseClick();
                        }
                    }
                    break;
                }
            }
        }

        // Start drag on SV square, Hue bar, or Alpha bar
        if (ctx.IsMouseClicked()) {
            if (L.svRect.Contains(mouse)) {
                state.dragTarget = 1;
                ctx.SetActiveId(id);
                if (!optsCopy.inlinePanel) ctx.ConsumeMouseClick();
            } else if (L.hueRect.Contains(mouse)) {
                state.dragTarget = 2;
                ctx.SetActiveId(id);
                if (!optsCopy.inlinePanel) ctx.ConsumeMouseClick();
            } else if (optsCopy.showAlpha && L.alphaRect.Contains(mouse)) {
                state.dragTarget = 3;
                ctx.SetActiveId(id);
                if (!optsCopy.inlinePanel) ctx.ConsumeMouseClick();
            }
        }

        if (state.dragTarget != 0) {
            if (ctx.IsMouseDown()) {
                res.pressed = true;
                if (state.dragTarget == 1 && L.svRect.w > 1.0f && L.svRect.h > 1.0f) {
                    const float newS = std::clamp((mouse.x - L.svRect.x) / L.svRect.w, 0.0f, 1.0f);
                    const float newV = std::clamp(1.0f - (mouse.y - L.svRect.y) / L.svRect.h, 0.0f, 1.0f);
                    state.s = newS;
                    state.v = newV;
                    const Color nextCol = HsvToRgb(state.h, state.s, state.v, value.a);
                    if (nextCol != value) {
                        value = nextCol;
                        res.changed = true;
                    }
                    state.lastSyncedColor = value;
                } else if (state.dragTarget == 2 && L.hueRect.w > 1.0f) {
                    const float newH = std::clamp((mouse.x - L.hueRect.x) / L.hueRect.w, 0.0f, 1.0f) * 359.9f;
                    state.h = newH;
                    const Color nextCol = HsvToRgb(state.h, state.s, state.v, value.a);
                    if (nextCol != value) {
                        value = nextCol;
                        res.changed = true;
                    }
                    state.lastSyncedColor = value;
                } else if (state.dragTarget == 3 && L.alphaRect.w > 1.0f) {
                    const float alphaNorm = std::clamp((mouse.x - L.alphaRect.x) / L.alphaRect.w, 0.0f, 1.0f);
                    const uint8_t newA = static_cast<uint8_t>(std::round(alphaNorm * 255.0f));
                    if (value.a != newA) {
                        value.a = newA;
                        state.lastSyncedColor = value;
                        res.changed = true;
                    }
                }
            } else {
                state.dragTarget = 0;
                if (ctx.GetActiveId() == id) {
                    ctx.SetActiveId(Id::Null());
                }
                if (!optsCopy.inlinePanel) {
                    ctx.ConsumeMouseClick();
                }
            }
        }

        if (!optsCopy.inlinePanel && isOpen && (ctx.ConsumeKey(Key::Escape) || ctx.ConsumeKey(Key::Enter))) {
            ctx.ClosePopup();
            isOpen = false;
            state.dragTarget = 0;
        }
    } else {
        state.dragTarget = 0;
    }

    char hexBuf[16]{};
    FormatHexColor(value, optsCopy.showAlpha, hexBuf, sizeof(hexBuf));
    const std::string_view hexView = ctx.AllocString(hexBuf);

    // Register floating popover overlay if open
    if (!optsCopy.inlinePanel && isOpen) {
        const Rect popRect = ctx.GetOpenPopupRect();
        const Color curVal = value;
        const float curH = state.h;
        const float curS = state.s;
        const float curV = state.v;
        const int hovSwatch = state.hoveredSwatch;
        std::vector<Color> presetsVec(optsCopy.presets.begin(), optsCopy.presets.end());
        ctx.RegisterOverlay([popRect, curVal, curH, curS, curV, hexView, hovSwatch, optsCopy, presetsVec = std::move(presetsVec), activeSize, effectiveDisabled, theme](ICanvas& c) {
            ColorPickerOptions popOpts = optsCopy;
            if (!presetsVec.empty()) {
                popOpts.presets = std::span<const Color>(presetsVec);
            }
            ColorPicker::RenderPanel(c, popRect, curVal, curH, curS, curV, hexView, hovSwatch, popOpts, activeSize, true, effectiveDisabled, theme);
        });
    }

    if (res.hovered && !isOpen && !optsCopy.tooltip.empty()) {
        ctx.SetTooltip(optsCopy.tooltip, prevBounds);
    }

    Node* node = ctx.CreateNode(NodeType::ColorPicker, id);
    optsCopy.tooltip = ctx.AllocText(optsCopy.tooltip);
    if (!optsCopy.presets.empty()) {
        Color* arenaPresets = static_cast<Color*>(
            ctx.GetArena().AllocateRaw(sizeof(Color) * optsCopy.presets.size(), alignof(Color))
        );
        for (size_t i = 0; i < optsCopy.presets.size(); ++i) {
            arenaPresets[i] = optsCopy.presets[i];
        }
        optsCopy.presets = std::span<const Color>(arenaPresets, optsCopy.presets.size());
    }

    auto* data = ctx.AllocPayload<ColorPickerData>();
    data->opts = optsCopy;
    data->value = value;
    data->h = state.h;
    data->s = state.s;
    data->v = state.v;
    data->hexText = hexView;
    data->hoveredSwatch = state.hoveredSwatch;
    data->isOpen = isOpen;

    node->text = hexView;
    node->SetPayload(data);
    node->visualHash = detail::HashArgs(
        value,
        state.h,
        state.s,
        state.v,
        state.hoveredSwatch,
        isOpen,
        static_cast<uint8_t>(optsCopy.stretch),
        static_cast<uint8_t>(optsCopy.align),
        optsCopy.width.value_or(0.0f),
        optsCopy.height.value_or(0.0f),
        optsCopy.minWidth.value_or(0.0f),
        optsCopy.background,
        optsCopy.border,
        optsCopy.showHex,
        optsCopy.showAlpha,
        optsCopy.showSwatches,
        optsCopy.inlinePanel
    );

    node->measureFn = [](const Node* n, float availableWidth, ICanvas* c, const UiTheme& t) noexcept -> Vec2 {
        const auto* d = n->As<ColorPickerData>();
        return ColorPicker::Measure(d ? d->opts : ColorPickerOptions{}, n->uiSize, c, t, availableWidth);
    };
    node->renderFn = [](const Node* n, ICanvas& c, const UiTheme& t) noexcept {
        const auto* d = n->As<ColorPickerData>();
        if (!d) return;
        ColorPicker::Render(
            c,
            n->computedRect,
            d->value,
            d->h,
            d->s,
            d->v,
            d->hexText,
            d->hoveredSwatch,
            d->opts,
            n->uiSize,
            n->hovered,
            d->isOpen,
            n->disabled,
            t,
            n->focused
        );
    };
    node->uiSize = activeSize;
    node->disabled = effectiveDisabled;
    node->hovered = res.hovered;
    node->active = isOpen || (state.dragTarget != 0);
    node->focused = isOpen || ctx.IsFocused(id);

    const ControlSpec ctrl = EffectiveColorPickerControl(optsCopy, activeSize, theme);
    Control::SetupNode(ctx, node, ctrl, optsCopy.stretch, optsCopy.align);
    ctx.AdaptChildToContainer(node);

    return res;
}

void ColorPicker::Render(
    ICanvas& canvas,
    Rect bounds,
    Color value,
    float h,
    float s,
    float v,
    std::string_view hexText,
    int hoveredSwatch,
    const ColorPickerOptions& opts,
    UiSize size,
    bool hovered,
    bool open,
    bool disabled,
    const UiTheme& theme,
    bool focused
) {
    if (bounds.w <= 0.0f || bounds.h <= 0.0f) return;

    const Rect ctrlRect = bounds;
    if (opts.inlinePanel) {
        RenderPanel(canvas, ctrlRect, value, h, s, v, hexText, hoveredSwatch, opts, size, false, disabled, theme);
        return;
    }

    const SelectMetrics& m = theme.GetSelect(size);
    const auto& pal = theme.palette;
    const ControlSpec ctrl = EffectiveColorPickerControl(opts, size, theme);
    const float radius = ctrl.border.radius.has_value() ? theme.Scale(*ctrl.border.radius) : m.cornerRadius;

    Color boxBg = ctrl.HasBackground(true)
        ? ((hovered && !disabled) ? pal.surface : pal.inputBackground)
        : Color::Transparent();
    Color boxBorder = ((open || focused) && !disabled)
        ? pal.focusRing
        : ((hovered && !disabled) ? pal.borderHover : pal.inputBorder);
    float strokeW = (ctrl.border.width > 0.0f) ? theme.Scale(ctrl.border.width) : m.strokeWidth;
    if ((open || focused) && !disabled && (ctrl.border.width > 0.0f || m.strokeWidth > 0.0f)) {
        strokeW += 0.5f;
    }
    if (disabled) {
        boxBg = boxBg.WithAlphaF(0.40f);
        boxBorder = boxBorder.WithAlphaF(0.40f);
    }
    if (ctrl.HasBackground(true) && ctrl.background.HasAny() && !ctrl.background.IsSurface()) {
        boxBg = ctrl.background.Resolve(hovered, open || focused, disabled, boxBg);
    }
    if (ctrl.border.color.HasAny()) {
        boxBorder = ctrl.border.color.Resolve(hovered, open || focused, disabled, boxBorder);
    }

    canvas.DrawRoundRect(ctrlRect, radius, boxBg, boxBorder, strokeW);

    const float pad = std::max(4.0f, ctrlRect.h * 0.16f);
    const float swatchH = std::max(8.0f, ctrlRect.h - pad * 2.0f);
    const float swatchRadius = std::max(2.0f, radius - pad * 0.6f);

    if (!opts.showHex) {
        const Rect fullSwatch = ctrlRect.Inset(pad);
        DrawCheckerboardSwatch(canvas, fullSwatch, disabled ? value.WithAlphaF(0.45f) : value, swatchRadius, pal.borderHover.WithAlpha(150));
        return;
    }

    const float swatchW = std::min(ctrlRect.w - pad * 2.0f, std::max(swatchH * 1.25f, 22.0f));
    const Rect swatchRect(ctrlRect.x + pad, ctrlRect.y + pad, swatchW, swatchH);
    DrawCheckerboardSwatch(canvas, swatchRect, disabled ? value.WithAlphaF(0.45f) : value, swatchRadius, pal.borderHover.WithAlpha(150));

    // Draw #HEX code and optional alpha percentage + chevron
    const float fontSize = theme.GetFontSize(TextRole::LabelMedium, size);
    const Color textCol = disabled ? pal.textMuted : pal.textPrimary;
    const float chevronSpace = m.iconSize + m.paddingX;
    const float textStartX = swatchRect.Right() + 8.0f;
    const float textW = std::max(0.0f, ctrlRect.Right() - chevronSpace - textStartX);
    if (textW > 8.0f) {
        const Rect textRect(textStartX, ctrlRect.y, textW, ctrlRect.h);
        canvas.DrawTextBox(hexText, textRect, fontSize, textCol, 0.0f, 0.5f);

        if (opts.showAlpha && textW > 110.0f) {
            char alphaBuf[16]{};
            const int alphaPct = static_cast<int>(std::round((static_cast<float>(value.a) / 255.0f) * 100.0f));
            std::snprintf(alphaBuf, sizeof(alphaBuf), "%d%%", alphaPct);
            canvas.DrawTextBox(alphaBuf, textRect.Inset(4.0f, 0.0f), fontSize * 0.9f, pal.textSecondary, 1.0f, 0.5f);
        }
    }

    // Chevron indicator on right
    const Icon chevron = open ? Icon::ChevronUp() : Icon::ChevronDown();
    const Rect iconRect(
        ctrlRect.Right() - m.paddingX * 0.75f - m.iconSize,
        ctrlRect.y + (ctrlRect.h - m.iconSize) * 0.5f,
        m.iconSize,
        m.iconSize
    );
    chevron.Draw(canvas, iconRect, disabled ? pal.textMuted : (open ? pal.accent : pal.textSecondary));
}

void ColorPicker::RenderPanel(
    ICanvas& canvas,
    Rect panelRect,
    Color value,
    float h,
    float s,
    float v,
    std::string_view hexText,
    int hoveredSwatch,
    const ColorPickerOptions& opts,
    UiSize size,
    bool isFloatingPopup,
    bool disabled,
    const UiTheme& theme
) {
    if (panelRect.w <= 10.0f || panelRect.h <= 10.0f) return;

    const auto& pal = theme.palette;
    const float radius = opts.border.radius.has_value() ? theme.Scale(*opts.border.radius) : theme.Scale(8.0f);
    if (isFloatingPopup) {
        canvas.DrawRoundRect(Rect(panelRect.x, panelRect.y + 3.0f, panelRect.w, panelRect.h), radius + 1.0f, Color(0, 0, 0, 95));
        canvas.DrawRoundRect(panelRect, radius, pal.surface, pal.borderHover, 1.0f);
    } else {
        const ControlSpec ctrl = EffectiveColorPickerControl(opts, size, theme);
        Color panelBg = ctrl.HasBackground(true) ? pal.surface : Color::Transparent();
        if (ctrl.HasBackground(true) && ctrl.background.HasAny() && !ctrl.background.IsSurface()) {
            panelBg = ctrl.background.Resolve(false, false, disabled, panelBg);
        }
        Color panelBorder = ctrl.border.color.HasAny()
            ? ctrl.border.color.Resolve(false, false, disabled, pal.inputBorder)
            : pal.inputBorder;
        const float strokeW = (ctrl.border.width > 0.0f) ? theme.Scale(ctrl.border.width) : 1.0f;
        canvas.DrawRoundRect(panelRect, radius, panelBg, panelBorder, strokeW);
    }

    const PanelLayout L = ComputePanelLayout(panelRect, opts, theme);

    // 1. Render 2D Saturation-Value (SV) gradient square via DrawPixels (48x36)
    constexpr uint32_t kSvW = 48;
    constexpr uint32_t kSvH = 36;
    std::array<uint32_t, kSvW * kSvH> svPixels{};
    for (uint32_t py = 0; py < kSvH; ++py) {
        const float rowV = 1.0f - static_cast<float>(py) / static_cast<float>(kSvH - 1);
        for (uint32_t px = 0; px < kSvW; ++px) {
            const float colS = static_cast<float>(px) / static_cast<float>(kSvW - 1);
            const Color c = HsvToRgb(h, colS, rowV, 255);
            svPixels[py * kSvW + px] = c.ToUint32();
        }
    }
    canvas.DrawPixels(svPixels.data(), kSvW, kSvH, L.svRect, 4.0f, disabled ? 130 : 255);
    canvas.DrawRoundRect(L.svRect, 4.0f, Color::Transparent(), pal.borderSubtle, 1.0f);

    // SV thumb ring
    const float svThumbX = L.svRect.x + std::clamp(s, 0.0f, 1.0f) * L.svRect.w;
    const float svThumbY = L.svRect.y + (1.0f - std::clamp(v, 0.0f, 1.0f)) * L.svRect.h;
    const float svR = theme.Scale(5.5f);
    canvas.DrawRoundRect(Rect(svThumbX - svR - 1.0f, svThumbY - svR - 1.0f, (svR + 1.0f) * 2.0f, (svR + 1.0f) * 2.0f), svR + 1.0f, Color::Transparent(), Color(0, 0, 0, 190), 1.5f);
    canvas.DrawRoundRect(Rect(svThumbX - svR, svThumbY - svR, svR * 2.0f, svR * 2.0f), svR, HsvToRgb(h, s, v, 255), Color(255, 255, 255, 245), 2.0f);

    // 2. Render Hue rainbow slider (96x1)
    constexpr uint32_t kHueW = 96;
    std::array<uint32_t, kHueW> huePixels{};
    for (uint32_t px = 0; px < kHueW; ++px) {
        const float deg = (static_cast<float>(px) / static_cast<float>(kHueW - 1)) * 360.0f;
        huePixels[px] = HsvToRgb(deg, 1.0f, 1.0f, 255).ToUint32();
    }
    canvas.DrawPixels(huePixels.data(), kHueW, 1, L.hueRect, L.hueRect.h * 0.5f, disabled ? 130 : 255);
    canvas.DrawRoundRect(L.hueRect, L.hueRect.h * 0.5f, Color::Transparent(), pal.borderSubtle, 1.0f);

    const float hueNorm = std::clamp(h / 360.0f, 0.0f, 1.0f);
    const float hueThumbX = L.hueRect.x + hueNorm * L.hueRect.w;
    const float barThumbR = L.hueRect.h * 0.58f;
    canvas.DrawRoundRect(
        Rect(hueThumbX - barThumbR, L.hueRect.Center().y - barThumbR, barThumbR * 2.0f, barThumbR * 2.0f),
        barThumbR,
        HsvToRgb(h, 1.0f, 1.0f, 255),
        Color(255, 255, 255, 240),
        2.0f
    );

    // 3. Render Alpha slider if enabled
    if (opts.showAlpha) {
        constexpr uint32_t kAlphaW = 64;
        constexpr uint32_t kAlphaH = 8;
        std::array<uint32_t, kAlphaW * kAlphaH> alphaPixels{};
        const Color solidRgb = HsvToRgb(h, s, v, 255);
        for (uint32_t py = 0; py < kAlphaH; ++py) {
            for (uint32_t px = 0; px < kAlphaW; ++px) {
                const bool darkCell = (((px / 4) + (py / 4)) & 1u) != 0u;
                const Color checker = darkCell ? Color(132, 138, 152, 255) : Color(210, 214, 224, 255);
                const float aFactor = static_cast<float>(px) / static_cast<float>(kAlphaW - 1);
                const Color blended = Color::Lerp(checker, solidRgb, aFactor);
                alphaPixels[py * kAlphaW + px] = blended.ToUint32();
            }
        }
        canvas.DrawPixels(alphaPixels.data(), kAlphaW, kAlphaH, L.alphaRect, L.alphaRect.h * 0.5f, disabled ? 130 : 255);
        canvas.DrawRoundRect(L.alphaRect, L.alphaRect.h * 0.5f, Color::Transparent(), pal.borderSubtle, 1.0f);

        const float alphaNorm = std::clamp(static_cast<float>(value.a) / 255.0f, 0.0f, 1.0f);
        const float alphaThumbX = L.alphaRect.x + alphaNorm * L.alphaRect.w;
        canvas.DrawRoundRect(
            Rect(alphaThumbX - barThumbR, L.alphaRect.Center().y - barThumbR, barThumbR * 2.0f, barThumbR * 2.0f),
            barThumbR,
            value,
            Color(255, 255, 255, 240),
            2.0f
        );
    }

    // 4. Render Live Preview Swatch + HEX pill + RGBA summary
    DrawCheckerboardSwatch(canvas, L.previewSwatchRect, value, 4.0f, pal.borderHover);
    canvas.DrawRoundRect(L.hexTextRect, 4.0f, pal.inputBackground, pal.borderSubtle, 1.0f);
    const float fontSm = theme.GetFontSize(TextRole::LabelSmall, size);
    canvas.DrawTextBox(hexText, L.hexTextRect.Inset(4.0f, 0.0f), fontSm, disabled ? pal.textMuted : pal.textPrimary, 0.5f, 0.5f);

    char rgbaBuf[32]{};
    if (opts.showAlpha) {
        std::snprintf(rgbaBuf, sizeof(rgbaBuf), "%u, %u, %u, %u", value.r, value.g, value.b, value.a);
    } else {
        std::snprintf(rgbaBuf, sizeof(rgbaBuf), "RGB(%u, %u, %u)", value.r, value.g, value.b);
    }
    canvas.DrawTextBox(rgbaBuf, L.rgbaTextRect, fontSm * 0.92f, pal.textSecondary, 1.0f, 0.5f);

    // 5. Render Quick Preset Swatches row
    if (opts.showSwatches) {
        const std::span<const Color> swatches = opts.presets.empty()
            ? std::span<const Color>(kDefaultPresets)
            : opts.presets;
        if (!swatches.empty()) {
            const float gap = theme.Scale(5.0f);
            const float countF = static_cast<float>(swatches.size());
            const float swW = std::max(10.0f, (L.swatchesRowRect.w - gap * (countF - 1.0f)) / countF);
            for (size_t i = 0; i < swatches.size(); ++i) {
                Rect sr(L.swatchesRowRect.x + static_cast<float>(i) * (swW + gap), L.swatchesRowRect.y, swW, L.swatchesRowRect.h);
                const bool isHovered = !disabled && (hoveredSwatch == static_cast<int>(i));
                const bool isSelected = (swatches[i].r == value.r && swatches[i].g == value.g && swatches[i].b == value.b);
                if (isHovered) {
                    sr = sr.Inset(-1.0f);
                }
                const Color borderCol = isSelected ? Color(255, 255, 255, 240) : (isHovered ? pal.textPrimary : pal.borderSubtle);
                DrawCheckerboardSwatch(canvas, sr, swatches[i], 4.0f, borderCol);
                if (isSelected) {
                    canvas.DrawRoundRect(sr.Inset(2.0f), 2.5f, Color::Transparent(), Color(0, 0, 0, 140), 1.0f);
                }
            }
        }
    }
}

} // namespace guikit
