#include "guikit/widgets/Pagination.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>
#include "guikit/Context.hpp"
#include "guikit/backend/ICanvas.hpp"
#include "guikit/core/Node.hpp"
#include "guikit/core/TextUtil.hpp"
#include "guikit/graphics/Icon.hpp"

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

void BuildPaginationTokens(
    size_t currentPage,
    size_t totalPages,
    const PaginationOptions& opts,
    std::vector<PageToken>& outTokens
) {
    outTokens.clear();
    const size_t safeTotal = std::max<size_t>(1, totalPages);
    const size_t curr = std::min(currentPage, safeTotal - 1);
    const size_t sib = static_cast<size_t>(opts.siblingCount);
    const size_t jumpStep = sib * 2 + 1;

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

    // Collect sorted unique candidate pages: first (0), [curr - sib .. curr + sib], and last (safeTotal - 1)
    std::vector<size_t> pages;
    pages.reserve(sib * 2 + 5);
    pages.push_back(0);

    const size_t rangeStart = (curr > sib) ? (curr - sib) : 0;
    const size_t rangeEnd = std::min(safeTotal - 1, curr + sib);
    for (size_t p = rangeStart; p <= rangeEnd; ++p) {
        if (pages.empty() || pages.back() != p) {
            pages.push_back(p);
        }
    }
    if (pages.back() != safeTotal - 1) {
        pages.push_back(safeTotal - 1);
    }

    for (size_t i = 0; i < pages.size(); ++i) {
        if (i > 0) {
            const size_t prevP = pages[i - 1];
            const size_t nextP = pages[i];
            if (nextP == prevP + 2) {
                // Exactly 1 page gap: show the single intermediate page directly instead of "..."
                outTokens.push_back(PageToken{
                    .type = PageTokenType::PageNumber,
                    .targetPage = prevP + 1,
                    .disabled = false
                });
            } else if (nextP > prevP + 2) {
                // Gap of >= 2 pages: insert interactive "..." ellipsis token
                const bool isLeft = (prevP < curr);
                const size_t jumpTarget = isLeft
                    ? ((curr > jumpStep) ? (curr - jumpStep) : 0)
                    : std::min(safeTotal - 1, curr + jumpStep);
                outTokens.push_back(PageToken{
                    .type = isLeft ? PageTokenType::LeftEllipsis : PageTokenType::RightEllipsis,
                    .targetPage = jumpTarget,
                    .disabled = false
                });
            }
        }
        outTokens.push_back(PageToken{
            .type = PageTokenType::PageNumber,
            .targetPage = pages[i],
            .disabled = false
        });
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

float MeasurePageTokenWidth(
    const PageToken& tok,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme
) noexcept {
    const ComponentMetrics& cm = theme.GetMetrics(size);
    const float minBtnW = cm.defaultHeight;
    if (tok.type != PageTokenType::PageNumber) {
        return minBtnW;
    }

    char buf[24];
    std::snprintf(buf, sizeof(buf), "%zu", tok.targetPage + 1);
    const float fontSize = theme.GetFontSize(TextRole::LabelMedium, size);
    const float textW = TextUtil::MeasureWidth(buf, fontSize, canvas);
    const float padX = std::max(theme.Scale(8.0f), cm.paddingX * 0.65f);
    return std::max(minBtnW, textW + padX * 2.0f);
}

std::string FormatPageInfoText(size_t currentPage, size_t totalPages) {
    const size_t safeTotal = std::max<size_t>(1, totalPages);
    const size_t curr = std::min(currentPage, safeTotal - 1);
    char buf[64];
    std::snprintf(buf, sizeof(buf), "Page %zu of %zu", curr + 1, safeTotal);
    return std::string(buf);
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
    std::vector<PageToken> tokens;
    BuildPaginationTokens(currentPage, totalPages, opts, tokens);

    const float gap = theme.Scale(4.0f);
    float totalW = 0.0f;
    for (size_t i = 0; i < tokens.size(); ++i) {
        if (i > 0) totalW += gap;
        totalW += MeasurePageTokenWidth(tokens[i], size, canvas, theme);
    }

    if (opts.showPageInfo) {
        const std::string infoStr = FormatPageInfoText(currentPage, totalPages);
        const float fontSize = theme.GetFontSize(TextRole::LabelMedium, size);
        totalW += theme.Scale(10.0f) + TextUtil::MeasureWidth(infoStr, fontSize, canvas);
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
    std::vector<PageToken>& outTokens,
    std::vector<Rect>& outTokenRects,
    Rect& outInfoRect
) {
    BuildPaginationTokens(currentPage, totalPages, opts, outTokens);
    outTokenRects.clear();
    outInfoRect = Rect{};
    if (outTokens.empty()) return;

    outTokenRects.reserve(outTokens.size());
    const float gap = theme.Scale(4.0f);
    std::vector<float> widths(outTokens.size(), 0.0f);
    float totalW = 0.0f;
    for (size_t i = 0; i < outTokens.size(); ++i) {
        if (i > 0) totalW += gap;
        widths[i] = MeasurePageTokenWidth(outTokens[i], size, canvas, theme);
        totalW += widths[i];
    }

    float infoW = 0.0f;
    const float infoGap = theme.Scale(10.0f);
    if (opts.showPageInfo) {
        const std::string infoStr = FormatPageInfoText(currentPage, totalPages);
        const float fontSize = theme.GetFontSize(TextRole::LabelMedium, size);
        infoW = TextUtil::MeasureWidth(infoStr, fontSize, canvas) + theme.Scale(4.0f);
        totalW += infoGap + infoW;
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
    for (size_t i = 0; i < outTokens.size(); ++i) {
        if (i > 0) curX += gap;
        outTokenRects.emplace_back(curX, innerRect.y, widths[i], innerRect.h);
        curX += widths[i];
    }

    if (opts.showPageInfo) {
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

    const Vec2 naturalDims = MeasurePaginationNaturalBox(curr, safeTotal, optsCopy, activeSize, canvas, theme);
    const float unscaledW = (theme.scale > 0.0f) ? (naturalDims.x / theme.scale) : naturalDims.x;
    if (optsCopy.stretch != StretchMode::Fill && !optsCopy.width.has_value()) {
        optsCopy.width = unscaledW;
    }

    ctx.RegisterFocusable(id, effectiveDisabled);
    Response res = ctx.Interact(id);

    const ControlSpec ctrl = EffectivePaginationControl(optsCopy, activeSize, theme);
    const Rect prevBounds = ctx.GetPrevFrameBounds(id);
    const Rect innerBounds = Control::ResolveInnerRect(prevBounds, ctrl, activeSize, theme);

    std::vector<PageToken> tokens;
    std::vector<Rect> tokenRects;
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
            for (size_t i = 0; i < tokenRects.size(); ++i) {
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
            if (hoveredToken >= 0 && static_cast<size_t>(hoveredToken) < tokens.size()) {
                const PageToken& tok = tokens[static_cast<size_t>(hoveredToken)];
                if (!tok.disabled && tok.targetPage < safeTotal && curr != tok.targetPage) {
                    curr = tok.targetPage;
                    currentPage = curr;
                    res.changed = true;
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
        ctx.SetTooltip(optsCopy.tooltip.View(), prevBounds);
    }

    Node* node = ctx.CreateNode(NodeType::Pagination, id);
    node->theme = pushedTheme;
    node->debugBoundsOverride = opts.debugBounds;
    node->inspectModeOverride = opts.inspectMode;

    optsCopy.tooltip = ctx.String(optsCopy.tooltip.View());

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

    std::vector<PageToken> tokens;
    std::vector<Rect> tokenRects;
    Rect infoRect{};
    ComputePaginationLayout(innerRect, d->currentPage, d->totalPages, d->opts, size, &canvas, theme, tokens, tokenRects, infoRect);

    const float fontSize = theme.GetFontSize(TextRole::LabelMedium, size);
    const float pillRadius = std::max(theme.Scale(4.0f), cm.cornerRadius);
    const float iconSz = std::round(cm.iconSize * 0.85f);

    canvas.PushClip(bounds);

    for (size_t i = 0; i < tokens.size(); ++i) {
        const PageToken& tok = tokens[i];
        const Rect tr = tokenRects[i];
        const bool tokDisabled = disabled || tok.disabled;
        const bool isHovered = !tokDisabled && (d->hoveredToken == static_cast<int>(i));
        const bool isPressed = !tokDisabled && (d->pressedToken == static_cast<int>(i));
        const bool isActivePage = (tok.type == PageTokenType::PageNumber) && (tok.targetPage == d->currentPage);

        Color pillFill = Color::Transparent();
        Color pillStroke = pal.borderSubtle;
        float pillStrokeW = 1.0f;
        Color fgColor = tokDisabled ? pal.textDisabled : (isHovered ? pal.textPrimary : pal.textSecondary);

        if (isActivePage) {
            const Color baseAccent = d->opts.color.HasAny()
                ? d->opts.color.Resolve(isHovered, isPressed, tokDisabled, pal.accent)
                : pal.accent;
            pillFill = tokDisabled ? baseAccent.WithAlphaF(0.40f) : baseAccent;
            pillStroke = (focused && !tokDisabled) ? pal.focusRing : Color::Transparent();
            pillStrokeW = (focused && !tokDisabled) ? 1.5f : 0.0f;
            fgColor = tokDisabled ? pal.textDisabled : ResolveContrastingTextOnFill(pillFill, pal);
        } else if (tok.type == PageTokenType::LeftEllipsis || tok.type == PageTokenType::RightEllipsis) {
            if (isHovered || isPressed) {
                pillFill = isPressed ? pal.surfaceActive : pal.surfaceHover;
                pillStroke = pal.borderHover;
            } else {
                pillStrokeW = 0.0f;
            }
        } else {
            pillFill = tokDisabled
                ? pal.surface.WithAlphaF(0.35f)
                : (isPressed ? pal.surfaceActive : (isHovered ? pal.surfaceHover : pal.surface));
            pillStroke = tokDisabled ? pal.borderSubtle.WithAlphaF(0.45f) : (isHovered ? pal.borderHover : pal.borderSubtle);
        }

        canvas.DrawRoundRect(tr, pillRadius, pillFill, pillStroke, pillStrokeW);

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
                if (isHovered) {
                    DrawDoubleChevron(canvas, tr, tok.type == PageTokenType::LeftEllipsis, iconSz * 0.90f, pal.accent);
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
        const std::string infoStr = FormatPageInfoText(d->currentPage, d->totalPages);
        const Color infoCol = disabled ? pal.textDisabled : pal.textSecondary;
        canvas.DrawTextBox(infoStr, infoRect, fontSize, infoCol, 0.0f, 0.5f);
    }

    canvas.PopClip();
}

} // namespace guikit
