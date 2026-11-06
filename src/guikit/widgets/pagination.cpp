#include "guikit/widgets/pagination.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include "guikit/context.hpp"
#include "guikit/backend/i_canvas.hpp"
#include "guikit/core/node.hpp"
#include "guikit/core/text_util.hpp"
#include "guikit/graphics/icon.hpp"
#include "keel/small_array.h"

namespace guikit {

namespace {

enum class PageTokenType : uint8_t {
    First,
    Prev,
    PageNumber,
    LeftEllipsis,
    RightEllipsis,
    Next,
    Last
};

struct PageToken {
    PageTokenType type{PageTokenType::PageNumber};
    size_t targetPage{0};
    bool disabled{false};
};

ControlSpec EffectivePaginationControl(const PaginationOptions& opts, UiSize size, const UiTheme& theme) noexcept {
    const SelectMetrics& sm = theme.GetSelect(size);
    ControlSpec c{};
    c.width = opts.width;
    c.height = opts.height;
    c.align = opts.align;
    c.background = opts.background;
    c.border = opts.border;
    c.defaultLayout = ElementLayout::Stacked;
    c.defaultInlineOrder = InlineOrder::LabelFirst;
    c.labelGap = sm.labelGap;
    c.descGap = sm.descGap;
    c.inlineGap = sm.paddingX;
    c.stretchable = true;
    c.stretchHeightWhenStandalone = false;
    c.hasIntrinsicBox = false;
    return c;
}

Color ResolveContrastingTextOnFill(Color fill, const ThemePalette& pal) noexcept {
    const float lum = (0.299f * static_cast<float>(fill.r) +
                       0.587f * static_cast<float>(fill.g) +
                       0.114f * static_cast<float>(fill.b)) / 255.0f;
    return (lum > 0.62f) ? Color(15, 23, 42, 255) : pal.controlKnobActive;
}

/// Canonical fixed-slot pagination algorithm (2 * siblingCount + 5 page/ellipsis slots).
/// Guarantees a constant token count across all pages so the pagination bar never shifts horizontally.
void BuildPaginationTokens(
    size_t currentPage,
    size_t totalPages,
    const PaginationOptions& opts,
    keel::SmallArray<PageToken, 16>& outTokens
) {
    outTokens.clear();
    const size_t safeTotal = std::max<size_t>(1, totalPages);
    const size_t curr = std::min(currentPage, safeTotal - 1);
    const size_t sib = static_cast<size_t>(opts.siblingCount);
    const size_t jumpStep = std::max<size_t>(5, sib * 2 + 3);
    const size_t totalPageSlots = sib * 2 + 5;

    if (opts.showFirstLast) {
        outTokens.push_back(PageToken{
            .type = PageTokenType::First,
            .targetPage = 0,
            .disabled = (curr == 0)
        });
    }
    if (opts.showPrevNext) {
        outTokens.push_back(PageToken{
            .type = PageTokenType::Prev,
            .targetPage = (curr > 0) ? (curr - 1) : 0,
            .disabled = (curr == 0)
        });
    }

    auto pushPage = [&](size_t p) {
        outTokens.push_back(PageToken{
            .type = PageTokenType::PageNumber,
            .targetPage = p,
            .disabled = false
        });
    };

    auto pushEllipsis = [&](bool isLeft) {
        const size_t jumpTarget = isLeft
            ? ((curr > jumpStep) ? (curr - jumpStep) : 0)
            : std::min(safeTotal - 1, curr + jumpStep);
        outTokens.push_back(PageToken{
            .type = isLeft ? PageTokenType::LeftEllipsis : PageTokenType::RightEllipsis,
            .targetPage = jumpTarget,
            .disabled = !opts.interactiveEllipsis
        });
    };

    if (safeTotal <= totalPageSlots) {
        for (size_t p = 0; p < safeTotal; ++p) {
            pushPage(p);
        }
    } else {
        const size_t leftSibling = (curr > sib) ? (curr - sib) : 0;
        const size_t rightSibling = std::min(safeTotal - 1, curr + sib);

        const bool showLeftEllipsis = (leftSibling > 2);
        const bool showRightEllipsis = (rightSibling < safeTotal - 3);

        if (!showLeftEllipsis && showRightEllipsis) {
            // Left boundary window: show first (2 * sib + 3) pages + "..." + last page
            const size_t leftItemCount = sib * 2 + 3;
            for (size_t p = 0; p < leftItemCount; ++p) {
                pushPage(p);
            }
            pushEllipsis(false);
            pushPage(safeTotal - 1);
        } else if (showLeftEllipsis && !showRightEllipsis) {
            // Right boundary window: show first page + "..." + last (2 * sib + 3) pages
            const size_t rightItemCount = sib * 2 + 3;
            pushPage(0);
            pushEllipsis(true);
            for (size_t p = safeTotal - rightItemCount; p < safeTotal; ++p) {
                pushPage(p);
            }
        } else {
            // Middle window: show first page + "..." + [leftSibling .. rightSibling] + "..." + last page
            pushPage(0);
            pushEllipsis(true);
            for (size_t p = leftSibling; p <= rightSibling; ++p) {
                pushPage(p);
            }
            pushEllipsis(false);
            pushPage(safeTotal - 1);
        }
    }

    if (opts.showPrevNext) {
        outTokens.push_back(PageToken{
            .type = PageTokenType::Next,
            .targetPage = (curr + 1 < safeTotal) ? (curr + 1) : (safeTotal - 1),
            .disabled = (curr + 1 >= safeTotal)
        });
    }
    if (opts.showFirstLast) {
        outTokens.push_back(PageToken{
            .type = PageTokenType::Last,
            .targetPage = safeTotal - 1,
            .disabled = (curr + 1 >= safeTotal)
        });
    }
}

/// Uniform slot width based on the widest page number (totalPages) so switching between 1-digit and 2/3-digit pages never shifts the bar.
float MeasureUniformSlotWidth(
    size_t totalPages,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme
) noexcept {
    const ComponentMetrics& cm = theme.GetMetrics(size);
    const float minBtnW = cm.defaultHeight;

    char buf[24];
    std::snprintf(buf, sizeof(buf), "%zu", std::max<size_t>(1, totalPages));
    const float fontSize = theme.GetFontSize(TextRole::LabelMedium, size);
    const float textW = TextUtil::MeasureWidth(buf, fontSize, canvas);
    const float padX = std::max(theme.Scale(8.0f), cm.paddingX * 0.65f);
    return std::max(minBtnW, std::ceil(textW + padX * 2.0f));
}

std::string_view FormatPageInfoText(size_t currentPage, size_t totalPages, char (&buf)[64]) noexcept {
    const size_t safeTotal = std::max<size_t>(1, totalPages);
    const size_t curr = std::min(currentPage, safeTotal - 1);
    const int n = std::snprintf(buf, sizeof(buf), "Page %zu of %zu", curr + 1, safeTotal);
    return (n > 0) ? std::string_view(buf, static_cast<size_t>(std::min<int>(n, static_cast<int>(sizeof(buf) - 1)))) : std::string_view{};
}

float MeasureReservedPageInfoWidth(
    size_t totalPages,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme
) noexcept {
    const size_t safeTotal = std::max<size_t>(1, totalPages);
    char buf[64];
    const std::string_view maxInfoStr = FormatPageInfoText(safeTotal - 1, safeTotal, buf);
    const float fontSize = theme.GetFontSize(TextRole::LabelMedium, size);
    return std::ceil(TextUtil::MeasureWidth(maxInfoStr, fontSize, canvas) + theme.Scale(4.0f));
}

Vec2 MeasurePaginationNaturalBox(
    size_t currentPage,
    size_t totalPages,
    const PaginationOptions& opts,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme
) {
    const ComponentMetrics& cm = theme.GetMetrics(size);
    keel::SmallArray<PageToken, 16> tokens;
    BuildPaginationTokens(currentPage, totalPages, opts, tokens);

    const float gap = theme.Scale(4.0f);
    const float slotW = MeasureUniformSlotWidth(totalPages, size, canvas, theme);
    float totalW = 0.0f;
    if (!tokens.empty()) {
        totalW = static_cast<float>(tokens.size()) * slotW + static_cast<float>(tokens.size() - 1) * gap;
    }

    if (opts.showPageInfo) {
        totalW += theme.Scale(10.0f) + MeasureReservedPageInfoWidth(totalPages, size, canvas, theme);
    }

    if (opts.stretch != StretchMode::Fill && opts.width.has_value() && *opts.width > 0.0f) {
        totalW = theme.Scale(*opts.width);
    }

    const float boxH = (opts.height.has_value() && *opts.height > 0.0f)
        ? theme.Scale(*opts.height)
        : cm.defaultHeight;

    return {std::max(cm.defaultHeight, totalW), boxH};
}

void ComputePaginationLayout(
    Rect innerRect,
    size_t currentPage,
    size_t totalPages,
    const PaginationOptions& opts,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme,
    keel::SmallArray<PageToken, 16>& outTokens,
    keel::SmallArray<Rect, 16>& outTokenRects,
    Rect& outInfoRect
) {
    BuildPaginationTokens(currentPage, totalPages, opts, outTokens);
    outTokenRects.clear();
    outInfoRect = Rect{};
    if (outTokens.empty()) return;

    outTokenRects.reserve(outTokens.size());
    const float gap = theme.Scale(4.0f);
    const float slotW = MeasureUniformSlotWidth(totalPages, size, canvas, theme);
    keel::SmallArray<float, 16> widths(outTokens.size(), slotW);
    float totalW = static_cast<float>(outTokens.size()) * slotW + static_cast<float>(outTokens.size() - 1) * gap;

    float infoW = 0.0f;
    const float infoGap = theme.Scale(10.0f);
    if (opts.showPageInfo) {
        infoW = MeasureReservedPageInfoWidth(totalPages, size, canvas, theme);
        totalW += infoGap + infoW;
    }

    float effGap = gap;
    bool showInfo = opts.showPageInfo && (infoW > 0.0f);
    if (innerRect.w > 1.0f && totalW > innerRect.w + 0.5f) {
        if (showInfo) {
            totalW -= (infoGap + infoW);
            showInfo = false;
            infoW = 0.0f;
        }
        if (totalW > innerRect.w + 0.5f && !outTokens.empty()) {
            const float ratio = innerRect.w / std::max(1.0f, totalW);
            effGap *= ratio;
            for (float& w : widths) {
                w *= ratio;
            }
            totalW = innerRect.w;
        }
    }

    float startX = innerRect.x;
    if (opts.stretch == StretchMode::Fill && innerRect.w > totalW) {
        if (opts.align == Align::Center) {
            startX += (innerRect.w - totalW) * 0.5f;
        } else if (opts.align == Align::End) {
            startX += (innerRect.w - totalW);
        }
    }

    float curX = startX;
    for (keel::isize i = 0; i < outTokens.size(); ++i) {
        if (i > 0) curX += effGap;
        outTokenRects.emplace_back(curX, innerRect.y, widths[i], innerRect.h);
        curX += widths[i];
    }

    if (showInfo) {
        curX += infoGap;
        outInfoRect = Rect(curX, innerRect.y, infoW, innerRect.h);
    }
}

void DrawDoubleChevron(
    ICanvas& canvas,
    Rect bounds,
    bool left,
    float iconSize,
    Color tint
) {
    const Vec2 c = bounds.Center();
    const float offset = iconSize * 0.18f;
    const Icon ic = left ? Icon::ChevronLeft() : Icon::ChevronRight();
    const Rect r1(c.x - iconSize * 0.5f - offset, c.y - iconSize * 0.5f, iconSize, iconSize);
    const Rect r2(c.x - iconSize * 0.5f + offset, c.y - iconSize * 0.5f, iconSize, iconSize);
    ic.Draw(canvas, r1, tint);
    ic.Draw(canvas, r2, tint);
}

} // namespace

Vec2 Pagination::Measure(
    size_t currentPage,
    size_t totalPages,
    const PaginationOptions& opts,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme
) {
    const Vec2 boxDims = MeasurePaginationNaturalBox(currentPage, totalPages, opts, size, canvas, theme);
    const ControlSpec ctrl = EffectivePaginationControl(opts, size, theme);
    return Control::Measure(boxDims, ctrl, opts.stretch, size, theme);
}

Response Pagination::Run(
    Context& ctx,
    int& currentPage,
    int totalPages,
    const PaginationOptions& opts,
    std::source_location loc
) {
    size_t curr = (currentPage >= 0) ? static_cast<size_t>(currentPage) : 0;
    const size_t total = (totalPages > 0) ? static_cast<size_t>(totalPages) : 1;
    Response res = Run(ctx, curr, total, opts, loc);
    if (res.changed) {
        currentPage = static_cast<int>(curr);
    }
    return res;
}

Response Pagination::Run(
    Context& ctx,
    size_t& currentPage,
    size_t totalPages,
    const PaginationOptions& opts,
    std::source_location loc
) {
    UiTheme* pushedTheme = ctx.PushScopedTheme(opts.theme, opts.scale);
    const Id id = ctx.MakeChildId(loc);
    const UiSize activeSize = ctx.GetCurrentSize();
    const bool effectiveDisabled = opts.disabled || ctx.IsDisabled();
    const UiTheme theme = ctx.GetTheme();
    ICanvas* canvas = ctx.GetCanvas();

    const size_t safeTotal = std::max<size_t>(1, totalPages);
    size_t curr = std::min(currentPage, safeTotal - 1);

    PaginationOptions optsCopy = opts;
    Control::InheritElementSizing(ctx, optsCopy.stretch, optsCopy.width, optsCopy.height);

    ctx.RegisterFocusable(id, effectiveDisabled);
    Response res = ctx.Interact(id);

    const ControlSpec prevCtrl = EffectivePaginationControl(optsCopy, activeSize, theme);
    const Rect prevBounds = ctx.GetPrevFrameBounds(id);
    const Rect innerBounds = Control::ResolveInnerRect(prevBounds, prevCtrl, activeSize, theme);

    keel::SmallArray<PageToken, 16> tokens;
    keel::SmallArray<Rect, 16> tokenRects;
    Rect infoRect{};
    ComputePaginationLayout(innerBounds, curr, safeTotal, optsCopy, activeSize, canvas, theme, tokens, tokenRects, infoRect);

    int hoveredToken = -1;
    int pressedToken = -1;

    if (effectiveDisabled) {
        res.clicked = false;
        res.hovered = false;
        res.pressed = false;
        res.focused = false;
    } else {
        const Vec2 mouse = ctx.GetMousePos();
        if (res.hovered || res.pressed || res.clicked) {
            for (keel::isize i = 0; i < tokenRects.size(); ++i) {
                if (tokenRects[i].Contains(mouse)) {
                    if (!tokens[i].disabled) {
                        hoveredToken = static_cast<int>(i);
                        if (res.pressed) {
                            pressedToken = static_cast<int>(i);
                        }
                    }
                    break;
                }
            }
        }

        if (res.clicked) {
            ctx.RequestFocus(id);
            if (hoveredToken >= 0 && static_cast<keel::isize>(hoveredToken) < tokens.size()) {
                const PageToken& tok = tokens[static_cast<keel::isize>(hoveredToken)];
                if (!tok.disabled && tok.targetPage < safeTotal && curr != tok.targetPage) {
                    curr = tok.targetPage;
                    currentPage = curr;
                    res.changed = true;
                    ctx.InvalidateAll();
                    ctx.RequestRepaint(3);
                }
            }
        }

        // Keyboard navigation when focused: Left, Right, Home, End
        if (ctx.IsFocused(id)) {
            if (ctx.ConsumeKey(Key::Left) || ctx.ConsumeKey(Key::Up)) {
                if (curr > 0) {
                    --curr;
                    currentPage = curr;
                    res.changed = true;
                }
            } else if (ctx.ConsumeKey(Key::Right) || ctx.ConsumeKey(Key::Down)) {
                if (curr + 1 < safeTotal) {
                    ++curr;
                    currentPage = curr;
                    res.changed = true;
                }
            } else if (ctx.ConsumeKey(Key::Home)) {
                if (curr != 0) {
                    curr = 0;
                    currentPage = 0;
                    res.changed = true;
                }
            } else if (ctx.ConsumeKey(Key::End)) {
                if (curr != safeTotal - 1) {
                    curr = safeTotal - 1;
                    currentPage = curr;
                    res.changed = true;
                }
            }
        }
    }

    if (res.hovered && !optsCopy.tooltip.empty()) {
        ctx.SetTooltip(std::string_view(optsCopy.tooltip), prevBounds);
    }

    ControlSpec ctrl = EffectivePaginationControl(optsCopy, activeSize, theme);
    if (optsCopy.stretch != StretchMode::Fill && !ctrl.width.has_value()) {
        const Vec2 naturalDims = MeasurePaginationNaturalBox(curr, safeTotal, optsCopy, activeSize, canvas, theme);
        ctrl.width = (theme.scale > 0.0f) ? (naturalDims.x / theme.scale) : naturalDims.x;
    }

    Node* node = ctx.CreateNode(NodeType::Pagination, id);
    node->theme = pushedTheme;
    node->debugBoundsOverride = opts.debugBounds;
    node->inspectModeOverride = opts.inspectMode;

    optsCopy.tooltip = ctx.String(optsCopy.tooltip);

    auto* data = ctx.AllocPayload<PaginationData>();
    data->opts = optsCopy;
    data->currentPage = curr;
    data->totalPages = safeTotal;
    data->hoveredToken = hoveredToken;
    data->pressedToken = pressedToken;

    node->SetPayload(data);
    node->visualHash = detail::HashArgs(
        curr,
        safeTotal,
        hoveredToken,
        pressedToken,
        optsCopy.siblingCount,
        optsCopy.showFirstLast,
        optsCopy.showPrevNext,
        optsCopy.showPageInfo,
        optsCopy.interactiveEllipsis,
        static_cast<uint8_t>(optsCopy.stretch),
        static_cast<uint8_t>(optsCopy.align),
        optsCopy.width.value_or(0.0f),
        optsCopy.height.value_or(0.0f),
        optsCopy.color,
        optsCopy.background,
        optsCopy.border
    );

    node->measureFn = [](const Node* n, float /*availableWidth*/, ICanvas* c, const UiTheme& t) noexcept -> Vec2 {
        const auto* d = n->As<PaginationData>();
        if (!d) return {0.0f, 0.0f};
        return Pagination::Measure(d->currentPage, d->totalPages, d->opts, n->uiSize, c, t);
    };
    node->renderFn = &Pagination::Render;
    node->uiSize = activeSize;
    node->disabled = effectiveDisabled;
    node->hovered = res.hovered;
    node->active = res.pressed;
    node->focused = ctx.IsFocused(id);

    Control::SetupNode(ctx, node, ctrl, optsCopy.stretch, optsCopy.align);

    if (pushedTheme) {
        ctx.PopTheme();
    }

    return res;
}

void Pagination::Render(
    const Node* node,
    ICanvas& canvas,
    const UiTheme& theme
) {
    if (!node) return;
    const auto* d = node->As<PaginationData>();
    if (!d) return;

    const Rect bounds = node->computedRect;
    if (bounds.w <= 0.0f || bounds.h <= 0.0f) return;

    const UiSize size = node->uiSize;
    const bool disabled = node->disabled;
    const bool focused = node->focused;
    const ComponentMetrics& cm = theme.GetMetrics(size);
    const auto& pal = theme.palette;

    const ControlSpec ctrl = EffectivePaginationControl(d->opts, size, theme);
    Control::RenderFrame(canvas, bounds, ctrl, size, disabled, theme, d->hoveredToken >= 0, d->pressedToken >= 0 || focused);
    const Rect innerRect = Control::ResolveInnerRect(bounds, ctrl, size, theme);

    keel::SmallArray<PageToken, 16> tokens;
    keel::SmallArray<Rect, 16> tokenRects;
    Rect infoRect{};
    ComputePaginationLayout(innerRect, d->currentPage, d->totalPages, d->opts, size, &canvas, theme, tokens, tokenRects, infoRect);

    const float fontSize = theme.GetFontSize(TextRole::LabelMedium, size);
    const float pillRadius = std::max(theme.Scale(4.0f), cm.cornerRadius);
    const float iconSz = std::round(cm.iconSize * 0.85f);

    canvas.PushClip(bounds);

    for (keel::isize i = 0; i < tokens.size(); ++i) {
        const PageToken& tok = tokens[i];
        const Rect tr = tokenRects[i];
        const bool isEllipsis = (tok.type == PageTokenType::LeftEllipsis || tok.type == PageTokenType::RightEllipsis);
        const bool tokDisabled = disabled || tok.disabled;
        const bool isHovered = !tokDisabled && (d->hoveredToken == static_cast<int>(i));
        const bool isPressed = !tokDisabled && (d->pressedToken == static_cast<int>(i));
        const bool isActivePage = (tok.type == PageTokenType::PageNumber) && (tok.targetPage == d->currentPage);

        const Color accentColor = d->opts.color.HasAny()
            ? d->opts.color.Resolve(isHovered, isPressed, tokDisabled, pal.accent)
            : pal.accent;

        Color pillFill = Color::Transparent();
        Color pillStroke = pal.borderSubtle;
        float pillStrokeW = 1.0f;
        Color fgColor = tokDisabled ? pal.textDisabled : (isHovered ? pal.textPrimary : pal.textSecondary);

        if (isActivePage) {
            pillFill = tokDisabled ? accentColor.WithAlphaF(0.40f) : accentColor;
            pillStroke = (focused && !tokDisabled) ? pal.focusRing : Color::Transparent();
            pillStrokeW = (focused && !tokDisabled) ? 1.5f : 0.0f;
            fgColor = tokDisabled ? pal.textDisabled : ResolveContrastingTextOnFill(pillFill, pal);
        } else if (isEllipsis) {
            fgColor = disabled ? pal.textDisabled : pal.textSecondary;
            if (d->opts.interactiveEllipsis && (isHovered || isPressed)) {
                pillFill = isPressed ? pal.surfaceActive : pal.surfaceHover;
                pillStroke = accentColor.WithAlphaF(0.65f);
            } else {
                pillStrokeW = 0.0f;
            }
        } else {
            pillFill = tokDisabled
                ? pal.surface.WithAlphaF(0.35f)
                : (isPressed ? pal.surfaceActive : (isHovered ? pal.surfaceHover : pal.surface));
            pillStroke = tokDisabled ? pal.borderSubtle.WithAlphaF(0.45f) : (isHovered ? pal.borderHover : pal.borderSubtle);
        }

        if (pillStrokeW > 0.0f || pillFill.a > 0) {
            canvas.DrawRoundRect(tr, pillRadius, pillFill, pillStroke, pillStrokeW);
        }

        const Vec2 center = tr.Center();
        switch (tok.type) {
            case PageTokenType::First: {
                DrawDoubleChevron(canvas, tr, true, iconSz, fgColor);
                break;
            }
            case PageTokenType::Prev: {
                const Rect ir(center.x - iconSz * 0.5f, center.y - iconSz * 0.5f, iconSz, iconSz);
                Icon::ChevronLeft().Draw(canvas, ir, fgColor);
                break;
            }
            case PageTokenType::Next: {
                const Rect ir(center.x - iconSz * 0.5f, center.y - iconSz * 0.5f, iconSz, iconSz);
                Icon::ChevronRight().Draw(canvas, ir, fgColor);
                break;
            }
            case PageTokenType::Last: {
                DrawDoubleChevron(canvas, tr, false, iconSz, fgColor);
                break;
            }
            case PageTokenType::LeftEllipsis:
            case PageTokenType::RightEllipsis: {
                if (d->opts.interactiveEllipsis && isHovered) {
                    DrawDoubleChevron(canvas, tr, tok.type == PageTokenType::LeftEllipsis, iconSz * 0.90f, accentColor);
                } else {
                    canvas.DrawTextBox("...", tr, fontSize, fgColor, 0.5f, 0.5f);
                }
                break;
            }
            case PageTokenType::PageNumber: {
                char buf[24];
                std::snprintf(buf, sizeof(buf), "%zu", tok.targetPage + 1);
                canvas.DrawTextBox(buf, tr, fontSize, fgColor, 0.5f, 0.5f);
                break;
            }
        }
    }

    if (d->opts.showPageInfo && infoRect.w > 0.0f) {
        char infoBuf[64];
        const std::string_view infoStr = FormatPageInfoText(d->currentPage, d->totalPages, infoBuf);
        const Color infoCol = disabled ? pal.textDisabled : pal.textSecondary;
        canvas.DrawTextBox(infoStr, infoRect, fontSize, infoCol, 0.0f, 0.5f);
    }

    canvas.PopClip();
}

} // namespace guikit
