#include "guikit/fields/color_picker.hpp"
#include <cstdio>
#include "guikit/backend/i_canvas.hpp"
#include "guikit/context.hpp"
#include "guikit/core/node.hpp"
#include "guikit/core/text_util.hpp"
#include "guikit/style/field_frame.hpp"
#include "guikit/style/style_painter.hpp"
#include "keel/math.h"
#include "keel/static_array.h"

namespace guikit {

namespace {

constexpr keel::StaticArray<Color, 10> kDefaultPresets = {{
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

PanelLayout ComputePanelLayout(Rect rect, const ColorPickerOptions& opts) noexcept {
    PanelLayout L{};
    L.panelRect = rect;
    const keel::f32 pad = 10.0f;
    const keel::f32 gap = 8.0f;
    const keel::f32 barH = 12.0f;
    const keel::f32 infoH = 24.0f;
    const keel::f32 swatchRowH = opts.showSwatches ? 22.0f : 0.0f;

    keel::f32 bottomSpace = barH + gap + infoH;
    if (opts.showAlpha) {
        bottomSpace += barH + gap;
    }
    if (opts.showSwatches) {
        bottomSpace += swatchRowH + gap;
    }

    const keel::f32 innerX = rect.x + pad;
    const keel::f32 innerW = keel::max(20.0f, rect.w - pad * 2.0f);
    const keel::f32 svH = keel::max(48.0f, rect.h - pad * 2.0f - bottomSpace);

    keel::f32 curY = rect.y + pad;
    L.svRect = Rect(innerX, curY, innerW, svH);
    curY += svH + gap;

    L.hueRect = Rect(innerX, curY, innerW, barH);
    curY += barH + gap;

    if (opts.showAlpha) {
        L.alphaRect = Rect(innerX, curY, innerW, barH);
        curY += barH + gap;
    }

    L.infoRowRect = Rect(innerX, curY, innerW, infoH);
    const keel::f32 prevW = infoH;
    L.previewSwatchRect = Rect(innerX, curY, prevW, infoH);
    const keel::f32 textStartX = innerX + prevW + gap;
    const keel::f32 remW = keel::max(0.0f, innerW - prevW - gap);
    L.hexTextRect = Rect(textStartX, curY, remW * 0.45f, infoH);
    L.rgbaTextRect = Rect(textStartX + remW * 0.45f, curY, remW * 0.55f, infoH);
    curY += infoH;

    if (opts.showSwatches) {
        curY += gap;
        L.swatchesRowRect = Rect(innerX, curY, innerW, swatchRowH);
    }

    return L;
}

Vec2 DefaultPanelDimensions(const ColorPickerOptions& opts, const UiTheme& /*theme*/) noexcept {
    keel::f32 h = 196.0f;
    if (opts.showAlpha) h += 20.0f;
    if (opts.showSwatches) h += 30.0f;
    return {254.0f, h};
}

ControlSpec EffectiveColorPickerControl(const ColorPickerOptions& opts, UiSize size, const UiTheme& theme) noexcept {
    const SelectMetrics& m = theme.GetSelect(size);
    return Control::FromOptions(
        opts,
        SizeMode::Stretch,
        SizeMode::Fit,
        ElementLayout::Stacked,
        m.labelGap,
        m.descGap,
        m.paddingX
    );
}

Vec2 MeasureColorPickerBox(const ColorPickerOptions& opts, UiSize size, ICanvas* canvas, const UiTheme& theme) noexcept {
    const bool hasExplicitDpW = opts.width.IsFixed();
    if (opts.inlinePanel) {
        const Vec2 defPanel = DefaultPanelDimensions(opts, theme);
        keel::f32 w = hasExplicitDpW ? opts.width.Dp() : defPanel.x;
        if (opts.minWidth.has_value()) {
            w = keel::max(*opts.minWidth, w);
        }
        const keel::f32 h = opts.height.IsFixed() ? opts.height.Dp() : defPanel.y;
        return {w, h};
    }

    const SelectMetrics& m = theme.GetSelect(size);
    const keel::f32 boxH = opts.height.IsFixed() ? opts.height.Dp() : m.defaultHeight;
    keel::f32 naturalW = boxH * 1.45f;
    if (opts.showHex) {
        const keel::f32 fontSize = theme.GetFontSize(TextRole::LabelMedium, size);
        const keel::StringRef sample = opts.showAlpha ? "#FF8800FF" : "#FF8800";
        const keel::f32 textW = (canvas != nullptr)
            ? canvas->MeasureText(sample, fontSize).x
            : (keel::as<keel::f32>(sample.size()) * fontSize * 0.62f);
        const keel::f32 alphaExtra = opts.showAlpha ? 38.0f : 0.0f;
        naturalW = boxH + textW + alphaExtra + m.paddingX * 2.0f + m.iconSize + 12.0f;
    }
    if (opts.minWidth.has_value()) {
        naturalW = keel::max(*opts.minWidth, naturalW);
    }
    if (hasExplicitDpW) {
        naturalW = opts.width.Dp();
    }
    return {naturalW, boxH};
}

void DrawCheckerboardSwatch(ICanvas& canvas, Rect rect, Color color, keel::f32 radius, Color borderCol) {
    if (rect.w <= 0.0f || rect.h <= 0.0f) return;

    if (color.a < 255) {
        const Color cLight(210, 214, 224, 255);
        const Color cDark(132, 138, 152, 255);
        canvas.DrawRoundRect(rect, radius, cLight);
        const Rect inner = rect.Inset(1.5f);
        const keel::f32 midX = inner.x + inner.w * 0.5f;
        const keel::f32 midY = inner.y + inner.h * 0.5f;
        canvas.DrawRect(Rect(inner.x, inner.y, inner.w * 0.5f, inner.h * 0.5f), cDark);
        canvas.DrawRect(Rect(midX, midY, inner.w * 0.5f, inner.h * 0.5f), cDark);
    }
    canvas.DrawRoundRect(rect, radius, color, borderCol, 1.0f);
}

void FormatHexColor(Color c, bool showAlpha, char* buf, keel::usize bufSize) noexcept {
    if (showAlpha) {
        std::snprintf(buf, bufSize, "#%02X%02X%02X%02X", c.r, c.g, c.b, c.a);
    } else {
        std::snprintf(buf, bufSize, "#%02X%02X%02X", c.r, c.g, c.b);
    }
}

} // namespace

void detail::ColorPicker::RgbToHsv(Color c, keel::f32& outH, keel::f32& outS, keel::f32& outV) noexcept {
    const keel::f32 r = keel::as<keel::f32>(c.r) / 255.0f;
    const keel::f32 g = keel::as<keel::f32>(c.g) / 255.0f;
    const keel::f32 b = keel::as<keel::f32>(c.b) / 255.0f;

    const keel::f32 maxC = keel::max(r, keel::max(g, b));
    const keel::f32 minC = keel::min(r, keel::min(g, b));
    const keel::f32 delta = maxC - minC;

    outV = maxC;
    outS = (maxC > 1e-5f) ? (delta / maxC) : 0.0f;

    if (delta <= 1e-5f) {
        return;
    }

    keel::f32 h = 0.0f;
    if (maxC == r) {
        h = 60.0f * keel::fmod(((g - b) / delta) + 6.0f, 6.0f);
    } else if (maxC == g) {
        h = 60.0f * (((b - r) / delta) + 2.0f);
    } else {
        h = 60.0f * (((r - g) / delta) + 4.0f);
    }
    if (h < 0.0f) h += 360.0f;
    outH = keel::clamp(h, 0.0f, 360.0f);
}

Color detail::ColorPicker::HsvToRgb(keel::f32 h, keel::f32 s, keel::f32 v, keel::u8 a) noexcept {
    const keel::f32 clampedH = keel::fmod(keel::max(0.0f, h), 360.0f);
    const keel::f32 clampedS = keel::clamp(s, 0.0f, 1.0f);
    const keel::f32 clampedV = keel::clamp(v, 0.0f, 1.0f);

    const keel::f32 c = clampedV * clampedS;
    const keel::f32 x = c * (1.0f - keel::abs(keel::fmod(clampedH / 60.0f, 2.0f) - 1.0f));
    const keel::f32 m = clampedV - c;

    keel::f32 r1 = 0.0f, g1 = 0.0f, b1 = 0.0f;
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
        keel::as<keel::u8>(keel::clamp(keel::round_i32((r1 + m) * 255.0f), 0, 255)),
        keel::as<keel::u8>(keel::clamp(keel::round_i32((g1 + m) * 255.0f), 0, 255)),
        keel::as<keel::u8>(keel::clamp(keel::round_i32((b1 + m) * 255.0f), 0, 255)),
        a
    );
}

Vec2 detail::ColorPicker::Measure(
    const ColorPickerOptions& opts,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme,
    keel::f32 /*availableWidth*/
) {
    const Vec2 boxDims = MeasureColorPickerBox(opts, size, canvas, theme);
    const ControlSpec ctrl = EffectiveColorPickerControl(opts, size, theme);
    return Control::Measure(boxDims, ctrl, theme);
}

Vec2 detail::ColorPicker::Measure(
    const ColorPickerOptions& opts,
    UiSize size,
    ICanvas* canvas,
    keel::f32 availableWidth
) {
    static const UiTheme defaultTheme = UiTheme::Dark();
    return Measure(opts, size, canvas, defaultTheme, availableWidth);
}

Response detail::ColorPicker::Run(
    Context& ctx,
    Color& value,
    const ColorPickerOptions& opts,
    keel::SourceLocation loc
) {

    const Id id = ctx.MakeChildId(loc);
    const UiSize activeSize = opts.size;
    const bool effectiveDisabled = opts.disabled || ctx.IsDisabled();
    const UiTheme theme = ctx.GetTheme();

    ColorPickerOptions optsCopy = opts;
    if (optsCopy.width.IsAuto()) {
        optsCopy.width = Control::ResolveWidthMode(ctx, opts.width, SizeMode::Stretch);
    }

    auto& state = ctx.GetOrCreatePersistentState<ColorPickerState>(id);
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
                isOpen = true;
                state.dragTarget = 0;
            }
        }
    }

    if (!optsCopy.inlinePanel && isOpen) {
        Vec2 popDims = DefaultPanelDimensions(optsCopy, theme);
        const Rect vp = ctx.GetPopupViewportRect();
        if (vp.w > 40.0f && popDims.x > vp.w - 8.0f) {
            popDims.x = keel::max(140.0f, vp.w - 8.0f);
        }
        if (vp.h > 80.0f && popDims.y > vp.h - 12.0f) {
            popDims.y = keel::max(120.0f, vp.h - 12.0f);
        }
        keel::f32 popupX = ctrlRect.x;
        keel::f32 popupY = ctrlRect.Bottom() + 4.0f;
        if (vp.w > 0.0f && vp.h > 0.0f) {
            const keel::f32 margin = 6.0f;
            const keel::f32 downY = ctrlRect.Bottom() + 4.0f;
            const keel::f32 upY = ctrlRect.y - 4.0f - popDims.y;
            const bool fitsDown = (downY + popDims.y <= vp.Bottom() - margin);
            const bool fitsUp = (upY >= vp.y + margin);

            if (fitsDown) {
                popupY = downY;
            } else if (fitsUp) {
                popupY = upY;
            } else {
                const keel::f32 rightX = ctrlRect.Right() + 6.0f;
                const keel::f32 leftX = ctrlRect.x - 6.0f - popDims.x;
                if (rightX + popDims.x <= vp.Right() - margin) {
                    popupX = rightX;
                    popupY = ctrlRect.y;
                } else if (leftX >= vp.x + margin) {
                    popupX = leftX;
                    popupY = ctrlRect.y;
                } else {
                    const keel::f32 availDown = vp.Bottom() - downY - margin;
                    const keel::f32 availUp = ctrlRect.y - 4.0f - (vp.y + margin);
                    popupY = (availUp > availDown) ? upY : downY;
                }
            }
            popupX = keel::clamp(popupX, vp.x + 4.0f, keel::max(vp.x + 4.0f, vp.Right() - popDims.x - 4.0f));
            popupY = keel::clamp(popupY, vp.y + 4.0f, keel::max(vp.y + 4.0f, vp.Bottom() - popDims.y - 4.0f));
        }
        const Rect popRect(popupX, popupY, popDims.x, popDims.y);
        ctx.OpenPopup(id, popRect);
        ctx.RegisterOverlayHitRegion(id, popRect);
    }

    // Interactive handling for either the open floating popup OR the inline panel
    const bool panelActive = !effectiveDisabled && (optsCopy.inlinePanel || (isOpen && wasOpen));
    state.hoveredSwatch = -1;

    if (panelActive) {
        auto hitScope = ctx.ScopedOverlayHitCheck(optsCopy.inlinePanel ? Id::Null() : id);
        const Rect activePanelRect = optsCopy.inlinePanel ? ctrlRect : ctx.GetOpenPopupRect();
        const PanelLayout L = ComputePanelLayout(activePanelRect, optsCopy);
        const Vec2 mouse = ctx.GetMousePos();
        const keel::ArrayRef<Color> customSwatches = !optsCopy.swatches.empty() ? optsCopy.swatches : optsCopy.presets;
        const keel::ArrayRef<Color> swatches = customSwatches.empty()
            ? kDefaultPresets.as_ref()
            : customSwatches;

        // Check swatch hover & click
        if (optsCopy.showSwatches && !swatches.empty() && state.dragTarget == 0 && L.swatchesRowRect.Contains(mouse)) {
            const keel::f32 gap = 5.0f;
            const keel::f32 countF = keel::as<keel::f32>(swatches.size());
            const keel::f32 swW = keel::max(10.0f, (L.swatchesRowRect.w - gap * (countF - 1.0f)) / countF);
            for (keel::usize i = 0; i < swatches.size(); ++i) {
                const Rect sr(L.swatchesRowRect.x + keel::as<keel::f32>(i) * (swW + gap), L.swatchesRowRect.y, swW, L.swatchesRowRect.h);
                if (sr.Contains(mouse)) {
                    state.hoveredSwatch = keel::as<keel::i32>(i);
                    if (ctx.IsMouseClicked()) {
                        const keel::u8 newAlpha = optsCopy.showAlpha ? swatches[i].a : value.a;
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
                    const keel::f32 newS = keel::clamp((mouse.x - L.svRect.x) / L.svRect.w, 0.0f, 1.0f);
                    const keel::f32 newV = keel::clamp(1.0f - (mouse.y - L.svRect.y) / L.svRect.h, 0.0f, 1.0f);
                    state.s = newS;
                    state.v = newV;
                    const Color nextCol = HsvToRgb(state.h, state.s, state.v, value.a);
                    if (nextCol != value) {
                        value = nextCol;
                        res.changed = true;
                    }
                    state.lastSyncedColor = value;
                } else if (state.dragTarget == 2 && L.hueRect.w > 1.0f) {
                    const keel::f32 newH = keel::clamp((mouse.x - L.hueRect.x) / L.hueRect.w, 0.0f, 1.0f) * 359.9f;
                    state.h = newH;
                    const Color nextCol = HsvToRgb(state.h, state.s, state.v, value.a);
                    if (nextCol != value) {
                        value = nextCol;
                        res.changed = true;
                    }
                    state.lastSyncedColor = value;
                } else if (state.dragTarget == 3 && L.alphaRect.w > 1.0f) {
                    const keel::f32 alphaNorm = keel::clamp((mouse.x - L.alphaRect.x) / L.alphaRect.w, 0.0f, 1.0f);
                    const keel::u8 newA = keel::as<keel::u8>(
                        keel::clamp(keel::round_i32(alphaNorm * 255.0f), 0, 255)
                    );
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
    const keel::StringRef hexView = ctx.AllocString(hexBuf);

    optsCopy.tooltip = ctx.AllocText(optsCopy.tooltip);
    if (!optsCopy.swatches.empty()) {
        Color* arenaSwatches = ctx.GetArena().create_array<Color>(optsCopy.swatches.size());
        for (keel::usize i = 0; i < optsCopy.swatches.size(); ++i) {
            arenaSwatches[i] = optsCopy.swatches[i];
        }
        optsCopy.swatches = keel::ArrayRef<Color>(arenaSwatches, optsCopy.swatches.size());
    }
    if (!optsCopy.presets.empty()) {
        Color* arenaPresets = ctx.GetArena().create_array<Color>(optsCopy.presets.size());
        for (keel::usize i = 0; i < optsCopy.presets.size(); ++i) {
            arenaPresets[i] = optsCopy.presets[i];
        }
        optsCopy.presets = keel::ArrayRef<Color>(arenaPresets, optsCopy.presets.size());
    }

    // Register floating popover overlay if open
    if (!optsCopy.inlinePanel && isOpen) {
        const Rect popRect = ctx.GetOpenPopupRect();
        const Color curVal = value;
        const keel::f32 curH = state.h;
        const keel::f32 curS = state.s;
        const keel::f32 curV = state.v;
        const keel::i32 hovSwatch = state.hoveredSwatch;
        ctx.SetOpenPopupVisualHash(detail::HashArgs(
            popRect.x,
            popRect.y,
            popRect.w,
            popRect.h,
            curVal.ToUint32(),
            curH,
            curS,
            curV,
            hovSwatch
        ));
        ctx.RegisterOverlay([popRect, curVal, curH, curS, curV, hexView, hovSwatch, optsCopy, activeSize, effectiveDisabled, theme](ICanvas& c) {
            detail::ColorPicker::RenderPanel(c, popRect, curVal, curH, curS, curV, hexView, hovSwatch, optsCopy, activeSize, true, effectiveDisabled, theme);
        });
    }

    if (res.hovered && !isOpen && !optsCopy.tooltip.empty()) {
        ctx.SetTooltip(optsCopy.tooltip, prevBounds);
    }

    Node* node = ctx.CreateNode(NodeType::ColorPicker, id);

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
        optsCopy.width,
        optsCopy.height,
        optsCopy.minWidth.value_or(0.0f),
        optsCopy.maxWidth.value_or(0.0f),
        optsCopy.minHeight.value_or(0.0f),
        optsCopy.maxHeight.value_or(0.0f),
        keel::as<keel::u8>(optsCopy.style),
        keel::as<keel::u8>(optsCopy.status),
        optsCopy.background,
        optsCopy.border,
        optsCopy.showHex,
        optsCopy.showAlpha,
        optsCopy.showSwatches,
        optsCopy.inlinePanel
    );

    node->measureFn = [](const Node* n, keel::f32 availableWidth, ICanvas* c, const UiTheme& t) noexcept -> Vec2 {
        const auto* d = n->As<ColorPickerData>();
        return detail::ColorPicker::Measure(d ? d->opts : ColorPickerOptions{}, n->uiSize, c, t, availableWidth);
    };
    node->renderFn = [](const Node* n, ICanvas& c, const UiTheme& t) noexcept {
        const auto* d = n->As<ColorPickerData>();
        if (!d) return;
        detail::ColorPicker::Render(
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
    node->focused = isOpen || ctx.IsFocusVisible(id);

    const ControlSpec ctrl = EffectiveColorPickerControl(optsCopy, activeSize, theme);
    Control::SetupNode(ctx, node, ctrl);

    return res;
}

void detail::ColorPicker::Render(
    ICanvas& canvas,
    Rect bounds,
    Color value,
    keel::f32 h,
    keel::f32 s,
    keel::f32 v,
    keel::StringRef hexText,
    keel::i32 hoveredSwatch,
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
    const keel::f32 radius = opts.border.radius.has_value() ? *opts.border.radius : m.cornerRadius;

    FieldFrame::Draw(canvas, theme, FieldFrameParams{
        .rect = ctrlRect,
        .style = opts.style,
        .status = opts.status,
        .background = opts.background,
        .border = opts.border,
        .defaultRadius = m.cornerRadius,
        .defaultStrokeWidth = m.strokeWidth,
        .hovered = hovered,
        .focused = focused,
        .active = open,
        .disabled = disabled
    });

    const keel::f32 pad = keel::max(4.0f, ctrlRect.h * 0.16f);
    const keel::f32 swatchH = keel::max(8.0f, ctrlRect.h - pad * 2.0f);
    const keel::f32 swatchRadius = keel::max(2.0f, radius - pad * 0.6f);

    if (!opts.showHex) {
        const Rect fullSwatch = ctrlRect.Inset(pad);
        DrawCheckerboardSwatch(canvas, fullSwatch, disabled ? value.WithAlphaF(theme.disabledOpacity) : value, swatchRadius, pal.borderHover.WithAlpha(150));
        return;
    }

    const keel::f32 swatchW = keel::min(ctrlRect.w - pad * 2.0f, keel::max(swatchH * 1.25f, 22.0f));
    const Rect swatchRect(ctrlRect.x + pad, ctrlRect.y + pad, swatchW, swatchH);
    DrawCheckerboardSwatch(canvas, swatchRect, disabled ? value.WithAlphaF(theme.disabledOpacity) : value, swatchRadius, pal.borderHover.WithAlpha(150));

    // Chevron trigger on right via FieldFrame::DrawTrigger
    const Icon chevron = open ? Icon::ChevronUp() : Icon::ChevronDown();
    const Color chevCol = disabled ? pal.textMuted : (open ? FieldFrame::ResolveAccent(theme, opts.status, false) : pal.textSecondary);
    const keel::f32 rightEdgeX = FieldFrame::DrawTrigger(
        canvas, theme, ctrlRect, chevron,
        m.paddingX * 0.75f, m.iconSize, m.cornerRadius, opts.border.radius.has_value(),
        hovered, false, open, disabled, false, chevCol
    );

    // Draw #HEX code and optional alpha percentage
    const keel::f32 fontSize = theme.GetFontSize(TextRole::LabelMedium, size);
    const Color textCol = disabled ? pal.textMuted : pal.textPrimary;
    const keel::f32 textStartX = swatchRect.Right() + 8.0f;
    const keel::f32 textW = keel::max(0.0f, rightEdgeX - textStartX);
    if (textW > 8.0f) {
        const keel::f32 hexW = canvas.MeasureText(hexText, fontSize).x;
        char alphaBuf[16]{};
        const keel::i32 alphaPct = keel::round_i32((keel::as<keel::f32>(value.a) / 255.0f) * 100.0f);
        std::snprintf(alphaBuf, sizeof(alphaBuf), "%d%%", alphaPct);
        const keel::f32 alphaW = opts.showAlpha ? canvas.MeasureText(alphaBuf, fontSize * 0.9f).x : 0.0f;
        const bool drawAlpha = opts.showAlpha && (textW >= hexW + alphaW + 14.0f);
        const keel::f32 hexAvailW = drawAlpha ? keel::max(0.0f, textW - alphaW - 10.0f) : textW;

        const Rect textRect(textStartX, ctrlRect.y, hexAvailW, ctrlRect.h);
        canvas.DrawTextBox(hexText, textRect, fontSize, textCol, 0.0f, 0.5f);

        if (drawAlpha) {
            const Rect alphaRect(textStartX, ctrlRect.y, textW, ctrlRect.h);
            canvas.DrawTextBox(alphaBuf, alphaRect.Inset(4.0f, 0.0f), fontSize * 0.9f, pal.textSecondary, 1.0f, 0.5f);
        }
    }
}

void detail::ColorPicker::RenderPanel(
    ICanvas& canvas,
    Rect panelRect,
    Color value,
    keel::f32 h,
    keel::f32 s,
    keel::f32 v,
    keel::StringRef hexText,
    keel::i32 hoveredSwatch,
    const ColorPickerOptions& opts,
    UiSize size,
    bool isFloatingPopup,
    bool disabled,
    const UiTheme& theme
) {
    if (panelRect.w <= 10.0f || panelRect.h <= 10.0f) return;

    const auto& pal = theme.palette;
    const keel::f32 radius = opts.border.radius.has_value() ? *opts.border.radius : 8.0f;
    if (isFloatingPopup) {
        theme.Painter().DrawSurface(canvas, theme, SurfacePaintParams{
            .rect = panelRect,
            .role = SurfaceRole::Popup,
            .radius = radius,
            .explicitRadius = opts.border.radius.has_value(),
            .bg = pal.surfaceElevated,
            .border = pal.borderHover,
            .borderWidth = 1.0f,
            .elevation = Elevation::Md
        });
    } else {
        (void)size;
        const bool hasBg = opts.background.IsBackgroundEnabled(true);
        Color panelBg = hasBg ? pal.surface : Color::Transparent();
        if (hasBg && opts.background.HasAny()) {
            if (!opts.background.IsSurface()) {
                panelBg = theme.ResolveColor(opts.background, false, false, disabled, panelBg);
            } else if (opts.background.HasAlphaScale()) {
                panelBg = panelBg.ScaleAlpha(opts.background.alphaScale);
            }
        }
        Color panelBorder = opts.border.color.HasAny()
            ? theme.ResolveColor(opts.border.color, false, false, disabled, pal.inputBorder)
            : pal.inputBorder;
        const keel::f32 strokeW = (opts.border.width > 0.0f) ? opts.border.width : 1.0f;
        theme.Painter().DrawSurface(canvas, theme, SurfacePaintParams{
            .rect = panelRect,
            .role = SurfaceRole::Flat,
            .radius = radius,
            .explicitRadius = opts.border.radius.has_value(),
            .bg = panelBg,
            .border = panelBorder,
            .borderWidth = strokeW
        });
    }

    const PanelLayout L = ComputePanelLayout(panelRect, opts);

    // 1. Render 2D Saturation-Value (SV) gradient square via DrawPixels (48x36)
    constexpr keel::u32 kSvW = 48;
    constexpr keel::u32 kSvH = 36;
    keel::StaticArray<keel::u32, kSvW * kSvH> svPixels{};
    for (keel::u32 py = 0; py < kSvH; ++py) {
        const keel::f32 rowV = 1.0f - keel::as<keel::f32>(py) / keel::as<keel::f32>(kSvH - 1);
        for (keel::u32 px = 0; px < kSvW; ++px) {
            const keel::f32 colS = keel::as<keel::f32>(px) / keel::as<keel::f32>(kSvW - 1);
            const Color c = HsvToRgb(h, colS, rowV, 255);
            svPixels[py * kSvW + px] = c.ToUint32();
        }
    }
    canvas.DrawPixels({svPixels, kSvW, kSvH}, L.svRect, 4.0f, disabled ? 130 : 255);
    canvas.DrawRoundRect(L.svRect, 4.0f, Color::Transparent(), pal.borderSubtle, 1.0f);

    // SV thumb ring
    const keel::f32 svThumbX = L.svRect.x + keel::clamp(s, 0.0f, 1.0f) * L.svRect.w;
    const keel::f32 svThumbY = L.svRect.y + (1.0f - keel::clamp(v, 0.0f, 1.0f)) * L.svRect.h;
    const keel::f32 svR = 5.5f;
    canvas.DrawRoundRect(Rect(svThumbX - svR - 1.0f, svThumbY - svR - 1.0f, (svR + 1.0f) * 2.0f, (svR + 1.0f) * 2.0f), svR + 1.0f, Color::Transparent(), Color(0, 0, 0, 190), 1.5f);
    canvas.DrawRoundRect(Rect(svThumbX - svR, svThumbY - svR, svR * 2.0f, svR * 2.0f), svR, HsvToRgb(h, s, v, 255), Color(255, 255, 255, 245), 2.0f);

    // 2. Render Hue rainbow slider (96x1)
    constexpr keel::u32 kHueW = 96;
    keel::StaticArray<keel::u32, kHueW> huePixels{};
    for (keel::u32 px = 0; px < kHueW; ++px) {
        const keel::f32 deg = (keel::as<keel::f32>(px) / keel::as<keel::f32>(kHueW - 1)) * 360.0f;
        huePixels[px] = HsvToRgb(deg, 1.0f, 1.0f, 255).ToUint32();
    }
    canvas.DrawPixels({huePixels, kHueW, 1}, L.hueRect, L.hueRect.h * 0.5f, disabled ? 130 : 255);
    canvas.DrawRoundRect(L.hueRect, L.hueRect.h * 0.5f, Color::Transparent(), pal.borderSubtle, 1.0f);

    const keel::f32 hueNorm = keel::clamp(h / 360.0f, 0.0f, 1.0f);
    const keel::f32 hueThumbX = L.hueRect.x + hueNorm * L.hueRect.w;
    const keel::f32 barThumbR = L.hueRect.h * 0.58f;
    canvas.DrawRoundRect(
        Rect(hueThumbX - barThumbR, L.hueRect.Center().y - barThumbR, barThumbR * 2.0f, barThumbR * 2.0f),
        barThumbR,
        HsvToRgb(h, 1.0f, 1.0f, 255),
        Color(255, 255, 255, 240),
        2.0f
    );

    // 3. Render Alpha slider if enabled
    constexpr keel::u32 kAlphaW = 64;
    constexpr keel::u32 kAlphaH = 8;
    keel::StaticArray<keel::u32, kAlphaW * kAlphaH> alphaPixels{};
    if (opts.showAlpha) {
        const Color solidRgb = HsvToRgb(h, s, v, 255);
        for (keel::u32 py = 0; py < kAlphaH; ++py) {
            for (keel::u32 px = 0; px < kAlphaW; ++px) {
                const bool darkCell = (((px / 4) + (py / 4)) & 1u) != 0u;
                const Color checker = darkCell ? Color(132, 138, 152, 255) : Color(210, 214, 224, 255);
                const keel::f32 aFactor = keel::as<keel::f32>(px) / keel::as<keel::f32>(kAlphaW - 1);
                const Color blended = Color::Lerp(checker, solidRgb, aFactor);
                alphaPixels[py * kAlphaW + px] = blended.ToUint32();
            }
        }
        canvas.DrawPixels({alphaPixels, kAlphaW, kAlphaH}, L.alphaRect, L.alphaRect.h * 0.5f, disabled ? 130 : 255);
        canvas.DrawRoundRect(L.alphaRect, L.alphaRect.h * 0.5f, Color::Transparent(), pal.borderSubtle, 1.0f);

        const keel::f32 alphaNorm = keel::clamp(keel::as<keel::f32>(value.a) / 255.0f, 0.0f, 1.0f);
        const keel::f32 alphaThumbX = L.alphaRect.x + alphaNorm * L.alphaRect.w;
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
    const keel::f32 fontSm = theme.GetFontSize(TextRole::LabelSmall, size);
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
        const keel::ArrayRef<Color> customSwatches = !opts.swatches.empty() ? opts.swatches : opts.presets;
        const keel::ArrayRef<Color> swatches = customSwatches.empty()
            ? kDefaultPresets.as_ref()
            : customSwatches;
        if (!swatches.empty()) {
            const keel::f32 gap = 5.0f;
            const keel::f32 countF = keel::as<keel::f32>(swatches.size());
            const keel::f32 swW = keel::max(10.0f, (L.swatchesRowRect.w - gap * (countF - 1.0f)) / countF);
            for (keel::usize i = 0; i < swatches.size(); ++i) {
                Rect sr(L.swatchesRowRect.x + keel::as<keel::f32>(i) * (swW + gap), L.swatchesRowRect.y, swW, L.swatchesRowRect.h);
                const bool isHovered = !disabled && (hoveredSwatch == keel::as<keel::i32>(i));
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
