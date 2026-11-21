#include "guikit/widgets/pagination.hpp"
#include <cstdio>
#include "guikit/context.hpp"
#include "guikit/backend/i_canvas.hpp"
#include "guikit/core/node.hpp"
#include "guikit/core/text_util.hpp"
#include "guikit/graphics/icon.hpp"
#include "keel/math.h"
#include "keel/small_array.h"

namespace guikit {

namespace {

enum class PageTokenType : keel::u8 {
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
    keel::usize targetPage{0};
    bool disabled{false};
};

ControlSpec EffectivePaginationControl(const PaginationOptions& opts, UiSize size, const UiTheme& theme) noexcept {
    const SelectMetrics& sm = theme.GetSelect(size);
    return Control::FromOptions(
        opts,
        SizeMode::Fit,
        SizeMode::Fit,
        ElementLayout::Stacked,
        sm.labelGap,
        sm.descGap,
        sm.paddingX
    );
}

Color ResolveContrastingTextOnFill(Color fill, const ThemePalette& pal) noexcept {
    const keel::f32 lum = (0.299f * keel::as<keel::f32>(fill.r) +
                       0.587f * keel::as<keel::f32>(fill.g) +
                       0.114f * keel::as<keel::f32>(fill.b)) / 255.0f;
    return (lum > 0.62f) ? Color(15, 23, 42, 255) : pal.controlKnobActive;
}

/// Canonical fixed-slot pagination algorithm (2 * siblingCount + 5 page/ellipsis slots).
/// Guarantees a constant token count across all pages so the pagination bar never shifts horizontally.
void BuildPaginationTokens(
    keel::usize currentPage,
    keel::usize totalPages,
    const PaginationOptions& opts,
    keel::SmallArray<PageToken, 16>& outTokens
) {
    outTokens.clear();
    const keel::usize safeTotal = keel::max<keel::usize>(1, totalPages);
    const keel::usize curr = keel::min(currentPage, safeTotal - 1);
    const keel::usize sib = keel::as<keel::usize>(opts.siblingCount);
    const keel::usize jumpStep = keel::max<keel::usize>(5, sib * 2 + 3);
    const keel::usize totalPageSlots = sib * 2 + 5;

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

    auto pushPage = [&](keel::usize p) {
        outTokens.push_back(PageToken{
            .type = PageTokenType::PageNumber,
            .targetPage = p,
            .disabled = false
        });
    };

    auto pushEllipsis = [&](bool isLeft) {
        const keel::usize jumpTarget = isLeft
            ? ((curr > jumpStep) ? (curr - jumpStep) : 0)
            : keel::min(safeTotal - 1, curr + jumpStep);
        outTokens.push_back(PageToken{
            .type = isLeft ? PageTokenType::LeftEllipsis : PageTokenType::RightEllipsis,
            .targetPage = jumpTarget,
            .disabled = !opts.interactiveEllipsis
        });
    };

    if (safeTotal <= totalPageSlots) {
        for (keel::usize p = 0; p < safeTotal; ++p) {
            pushPage(p);
        }
    } else {
        const keel::usize leftSibling = (curr > sib) ? (curr - sib) : 0;
        const keel::usize rightSibling = keel::min(safeTotal - 1, curr + sib);

        const bool showLeftEllipsis = (leftSibling > 2);
        const bool showRightEllipsis = (rightSibling < safeTotal - 3);

        if (!showLeftEllipsis && showRightEllipsis) {
            // Left boundary window: show first (2 * sib + 3) pages + "..." + last page
            const keel::usize leftItemCount = sib * 2 + 3;
            for (keel::usize p = 0; p < leftItemCount; ++p) {
                pushPage(p);
            }
            pushEllipsis(false);
            pushPage(safeTotal - 1);
        } else if (showLeftEllipsis && !showRightEllipsis) {
            // Right boundary window: show first page + "..." + last (2 * sib + 3) pages
            const keel::usize rightItemCount = sib * 2 + 3;
            pushPage(0);
            pushEllipsis(true);
            for (keel::usize p = safeTotal - rightItemCount; p < safeTotal; ++p) {
                pushPage(p);
            }
        } else {
            // Middle window: show first page + "..." + [leftSibling .. rightSibling] + "..." + last page
            pushPage(0);
            pushEllipsis(true);
            for (keel::usize p = leftSibling; p <= rightSibling; ++p) {
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
keel::f32 MeasureUniformSlotWidth(
    keel::usize totalPages,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme
) noexcept {
    const ComponentMetrics& cm = theme.GetMetrics(size);
    const keel::f32 minBtnW = cm.defaultHeight;

    char buf[24];
    std::snprintf(buf, sizeof(buf), "%zu", keel::max<keel::usize>(1, totalPages));
    const keel::f32 fontSize = theme.GetFontSize(TextRole::LabelMedium, size);
    const keel::f32 textW = TextUtil::MeasureWidth(buf, fontSize, canvas);
    const keel::f32 padX = keel::max(8.0f, cm.paddingX * 0.65f);
    return keel::max(minBtnW, keel::ceil(textW + padX * 2.0f));
}

keel::StringRef FormatPageInfoText(keel::usize currentPage, keel::usize totalPages, char (&buf)[64]) noexcept {
    const keel::usize safeTotal = keel::max<keel::usize>(1, totalPages);
    const keel::usize curr = keel::min(currentPage, safeTotal - 1);
    const keel::i32 n = std::snprintf(buf, sizeof(buf), "Page %zu of %zu", curr + 1, safeTotal);
    return (n > 0) ? keel::StringRef(buf, keel::as<keel::usize>(keel::min<keel::i32>(n, keel::as<keel::i32>(sizeof(buf) - 1)))) : keel::StringRef{};
}

keel::f32 MeasureReservedPageInfoWidth(
    keel::usize totalPages,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme
) noexcept {
    const keel::usize safeTotal = keel::max<keel::usize>(1, totalPages);
    char buf[64];
    const keel::StringRef maxInfoStr = FormatPageInfoText(safeTotal - 1, safeTotal, buf);
    const keel::f32 fontSize = theme.GetFontSize(TextRole::LabelMedium, size);
    return keel::ceil(TextUtil::MeasureWidth(maxInfoStr, fontSize, canvas) + 4.0f);
}

Vec2 MeasurePaginationNaturalBox(
    keel::usize currentPage,
    keel::usize totalPages,
    const PaginationOptions& opts,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme
) {
    const ComponentMetrics& cm = theme.GetMetrics(size);
    keel::SmallArray<PageToken, 16> tokens;
    BuildPaginationTokens(currentPage, totalPages, opts, tokens);

    const keel::f32 gap = 4.0f;
    const keel::f32 slotW = MeasureUniformSlotWidth(totalPages, size, canvas, theme);
    keel::f32 totalW = 0.0f;
    if (!tokens.empty()) {
        totalW = keel::as<keel::f32>(tokens.size()) * slotW + keel::as<keel::f32>(tokens.size() - 1) * gap;
    }

    if (opts.showPageInfo) {
        totalW += 10.0f + MeasureReservedPageInfoWidth(totalPages, size, canvas, theme);
    }

    if (opts.width.IsFixed()) {
        totalW = opts.width.Dp();
    }

    const keel::f32 boxH = opts.height.IsFixed()
        ? opts.height.Dp()
        : cm.defaultHeight;

    return {keel::max(cm.defaultHeight, totalW), boxH};
}

void ComputePaginationLayout(
    Rect innerRect,
    keel::usize currentPage,
    keel::usize totalPages,
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
    const keel::f32 gap = 4.0f;
    const keel::f32 slotW = MeasureUniformSlotWidth(totalPages, size, canvas, theme);
    keel::SmallArray<keel::f32, 16> widths(outTokens.size(), slotW);
    keel::f32 totalW = keel::as<keel::f32>(outTokens.size()) * slotW + keel::as<keel::f32>(outTokens.size() - 1) * gap;

    keel::f32 infoW = 0.0f;
    const keel::f32 infoGap = 10.0f;
    if (opts.showPageInfo) {
        infoW = MeasureReservedPageInfoWidth(totalPages, size, canvas, theme);
        totalW += infoGap + infoW;
    }

    keel::f32 effGap = gap;
    bool showInfo = opts.showPageInfo && (infoW > 0.0f);
    if (innerRect.w > 1.0f && totalW > innerRect.w + 0.5f) {
        if (showInfo) {
            totalW -= (infoGap + infoW);
            showInfo = false;
            infoW = 0.0f;
        }
        if (totalW > innerRect.w + 0.5f && !outTokens.empty()) {
            const keel::f32 ratio = innerRect.w / keel::max(1.0f, totalW);
            effGap *= ratio;
            for (keel::f32& w : widths) {
                w *= ratio;
            }
            totalW = innerRect.w;
        }
    }

    keel::f32 curX = innerRect.x;
    for (keel::usize i = 0; i < outTokens.size(); ++i) {
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
    keel::f32 iconSize,
    Color tint
) {
    const Vec2 c = bounds.Center();
    const keel::f32 offset = iconSize * 0.18f;
    const Icon ic = left ? Icon::ChevronLeft() : Icon::ChevronRight();
    const Rect r1(c.x - iconSize * 0.5f - offset, c.y - iconSize * 0.5f, iconSize, iconSize);
    const Rect r2(c.x - iconSize * 0.5f + offset, c.y - iconSize * 0.5f, iconSize, iconSize);
    ic.Draw(canvas, r1, tint);
    ic.Draw(canvas, r2, tint);
}

} // namespace

Vec2 Pagination::Measure(
    keel::usize currentPage,
    keel::usize totalPages,
    const PaginationOptions& opts,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme
) {
    const Vec2 boxDims = MeasurePaginationNaturalBox(currentPage, totalPages, opts, size, canvas, theme);
    const ControlSpec ctrl = EffectivePaginationControl(opts, size, theme);
    return Control::Measure(boxDims, ctrl, theme);
}

Response Pagination::Run(
    Context& ctx,
    keel::i32& currentPage,
    keel::i32 totalPages,
    const PaginationOptions& opts,
    keel::SourceLocation loc
) {
    keel::usize curr = (currentPage >= 0) ? keel::as<keel::usize>(currentPage) : 0;
    const keel::usize total = (totalPages > 0) ? keel::as<keel::usize>(totalPages) : 1;
    Response res = Run(ctx, curr, total, opts, loc);
    if (res.changed) {
        currentPage = keel::as<keel::i32>(curr);
    }
    return res;
}

Response Pagination::Run(
    Context& ctx,
    keel::usize& currentPage,
    keel::usize totalPages,
    const PaginationOptions& opts,
    keel::SourceLocation loc
) {
    const Id id = ctx.MakeChildId(loc);
    const UiSize activeSize = opts.size;
    const bool effectiveDisabled = opts.disabled || ctx.IsDisabled();
    const UiTheme theme = ctx.GetTheme();
    ICanvas* canvas = ctx.GetCanvas();

    const keel::usize safeTotal = keel::max<keel::usize>(1, totalPages);
    keel::usize curr = keel::min(currentPage, safeTotal - 1);

    PaginationOptions optsCopy = opts;

    ctx.RegisterFocusable(id, effectiveDisabled);
    Response res = ctx.Interact(id);

    const Rect prevBounds = ctx.GetPrevFrameBounds(id);
    const Rect innerBounds = prevBounds;

    keel::SmallArray<PageToken, 16> tokens;
    keel::SmallArray<Rect, 16> tokenRects;
    Rect infoRect{};
    ComputePaginationLayout(innerBounds, curr, safeTotal, optsCopy, activeSize, canvas, theme, tokens, tokenRects, infoRect);

    keel::i32 hoveredToken = -1;
    keel::i32 pressedToken = -1;

    if (effectiveDisabled) {
        res.clicked = false;
        res.hovered = false;
        res.pressed = false;
        res.focused = false;
    } else {
        const Vec2 mouse = ctx.GetMousePos();
        if (res.hovered || res.pressed || res.clicked) {
            for (keel::usize i = 0; i < tokenRects.size(); ++i) {
                if (tokenRects[i].Contains(mouse)) {
                    if (!tokens[i].disabled) {
                        hoveredToken = keel::as<keel::i32>(i);
                        if (res.pressed) {
                            pressedToken = keel::as<keel::i32>(i);
                        }
                    }
                    break;
                }
            }
        }

        if (res.clicked) {
            ctx.RequestFocus(id);
            if (hoveredToken >= 0 && keel::as<keel::usize>(hoveredToken) < tokens.size()) {
                const PageToken& tok = tokens[keel::as<keel::usize>(hoveredToken)];
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
        ctx.SetTooltip(optsCopy.tooltip, prevBounds);
    }

    const ControlSpec ctrl = EffectivePaginationControl(optsCopy, activeSize, theme);

    Node* node = ctx.CreateNode(NodeType::Pagination, id);

    optsCopy.tooltip = ctx.AllocText(optsCopy.tooltip);

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
        optsCopy.width,
        optsCopy.height,
        optsCopy.minWidth.value_or(0.0f),
        optsCopy.maxWidth.value_or(0.0f),
        optsCopy.minHeight.value_or(0.0f),
        optsCopy.maxHeight.value_or(0.0f),
        optsCopy.color
    );

    node->measureFn = [](const Node* n, keel::f32 /*availableWidth*/, ICanvas* c, const UiTheme& t) noexcept -> Vec2 {
        const auto* d = n->As<PaginationData>();
        if (!d) return {0.0f, 0.0f};
        return Pagination::Measure(d->currentPage, d->totalPages, d->opts, n->uiSize, c, t);
    };
    node->renderFn = &Pagination::Render;
    node->uiSize = activeSize;
    node->disabled = effectiveDisabled;
    node->hovered = res.hovered;
    node->active = res.pressed;
    node->focused = ctx.IsFocusVisible(id);

    Control::SetupNode(ctx, node, ctrl);


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

    const Rect innerRect = bounds;

    keel::SmallArray<PageToken, 16> tokens;
    keel::SmallArray<Rect, 16> tokenRects;
    Rect infoRect{};
    ComputePaginationLayout(innerRect, d->currentPage, d->totalPages, d->opts, size, &canvas, theme, tokens, tokenRects, infoRect);

    const keel::f32 fontSize = theme.GetFontSize(TextRole::LabelMedium, size);
    const keel::f32 pillRadius = keel::max(4.0f, cm.cornerRadius);
    const keel::f32 iconSz = keel::round(cm.iconSize * 0.85f);

    canvas.PushClip(bounds);

    for (keel::usize i = 0; i < tokens.size(); ++i) {
        const PageToken& tok = tokens[i];
        const Rect tr = tokenRects[i];
        const bool isEllipsis = (tok.type == PageTokenType::LeftEllipsis || tok.type == PageTokenType::RightEllipsis);
        const bool tokDisabled = disabled || tok.disabled;
        const bool isHovered = !tokDisabled && (d->hoveredToken == keel::as<keel::i32>(i));
        const bool isPressed = !tokDisabled && (d->pressedToken == keel::as<keel::i32>(i));
        const bool isActivePage = (tok.type == PageTokenType::PageNumber) && (tok.targetPage == d->currentPage);

        const Color accentColor = d->opts.color.HasAny()
            ? d->opts.color.Resolve(isHovered, isPressed, tokDisabled, pal.accent)
            : pal.accent;

        Color pillFill = Color::Transparent();
        Color pillStroke = pal.borderSubtle;
        keel::f32 pillStrokeW = 1.0f;
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
        const keel::StringRef infoStr = FormatPageInfoText(d->currentPage, d->totalPages, infoBuf);
        const Color infoCol = disabled ? pal.textDisabled : pal.textSecondary;
        canvas.DrawTextBox(infoStr, infoRect, fontSize, infoCol, 0.0f, 0.5f);
    }

    canvas.PopClip();
}

} // namespace guikit
