#pragma once

#include <algorithm>
#include <cmath>
#include "demo_context.hpp"
#include "guikit/backend/i_canvas.hpp"
#include "guikit/core/node.hpp"

namespace guikit::demo {

namespace detail {

inline void RenderDotGridBackdrop(const guikit::Node* n, guikit::ICanvas& canvas, const guikit::UiTheme& theme) noexcept {
    const guikit::Rect& r = n->computedRect;
    if (r.w <= 1.0f || r.h <= 1.0f) return;

    const bool isLight = (theme.mode == guikit::ThemeMode::Light);
    const guikit::Color bg = isLight
        ? guikit::Color(222, 229, 238, 255) // Cool recessed studio slate in Light mode
        : guikit::Color(8, 12, 20, 255);    // Deep recessed obsidian canvas in Dark mode
    canvas.DrawRect(r, bg);

    // Subtle studio dot-grid pattern
    const float scale = std::max(1.0f, theme.scale);
    const float step = std::round(18.0f * scale);
    const float dotSize = std::max(1.5f, std::round(1.5f * scale));
    const guikit::Color dotColor = isLight
        ? guikit::Color(100, 116, 139, 46)
        : guikit::Color(148, 163, 184, 30);

    const float startX = std::floor(r.x + step * 0.5f);
    const float startY = std::floor(r.y + step * 0.5f);
    const float endX = r.Right() - 2.0f;
    const float endY = r.Bottom() - 2.0f;
    for (float y = startY; y < endY; y += step) {
        for (float x = startX; x < endX; x += step) {
            canvas.DrawRect(guikit::Rect(x, y, dotSize, dotSize), dotColor);
        }
    }

    // Top & left recessed inner shadow
    const guikit::Color shadowCol = isLight
        ? guikit::Color(15, 23, 42, 20)
        : guikit::Color(0, 0, 0, 95);
    canvas.DrawRect(guikit::Rect(r.x, r.y, r.w, 2.0f * scale), shadowCol);
    canvas.DrawRect(guikit::Rect(r.x, r.y, 2.0f * scale, r.h), shadowCol);
}

inline void RenderBackdropHudBadge(const guikit::Node* n, guikit::ICanvas& canvas, const guikit::UiTheme& theme) noexcept {
    if (n->type == guikit::NodeType::ScrollArea) {
        guikit::ScrollArea::RenderScrollbar(n, canvas, theme);
    }
    if (n->text.empty()) return;
    const guikit::Rect& r = n->computedRect;
    if (r.w <= 160.0f || r.h <= 60.0f) return;

    const float s = std::max(0.75f, theme.scale);
    const float fontSize = std::round(11.0f * s);
    const guikit::Vec2 textSz = canvas.MeasureText(n->text, fontSize);
    const float padX = 9.0f * s;
    const float padY = 4.0f * s;
    const float pillW = textSz.x + padX * 2.0f;
    const float pillH = std::max(fontSize + padY * 2.0f, 20.0f * s);
    const float extraRight = (n->type == guikit::NodeType::ScrollArea && n->scrollContentSize.y > r.h + 0.5f) ? (10.0f * s) : 0.0f;
    const float extraBottom = (n->type == guikit::NodeType::ScrollArea && n->scrollContentSize.x > r.w + 0.5f) ? (10.0f * s) : 0.0f;
    const float pillX = r.Right() - pillW - 10.0f * s - extraRight;
    const float pillY = r.Bottom() - pillH - 8.0f * s - extraBottom;

    const bool isLight = (theme.mode == guikit::ThemeMode::Light);
    const guikit::Color pillBg = isLight
        ? guikit::Color(248, 250, 252, 220)
        : guikit::Color(15, 23, 42, 215);
    const guikit::Color pillBorder = isLight
        ? guikit::Color(148, 163, 184, 150)
        : guikit::Color(51, 65, 85, 180);
    const guikit::Color pillFg = isLight
        ? guikit::Color(71, 85, 105, 240)
        : guikit::Color(148, 163, 184, 235);

    canvas.DrawRoundRect(guikit::Rect(pillX, pillY, pillW, pillH), pillH * 0.5f, pillBg, pillBorder, 1.0f);
    canvas.DrawText(n->text, {pillX + padX, pillY + (pillH - textSz.y) * 0.5f}, fontSize, pillFg);
}

inline void RenderWindowResizeGrip(const guikit::Node* n, guikit::ICanvas& canvas, const guikit::UiTheme& theme) noexcept {
    const guikit::Rect& r = n->computedRect;
    if (r.w <= 20.0f || r.h <= 20.0f) return;

    // Subtle diagonal resize grip lines in the bottom-right corner of the window frame
    const float s = std::max(0.5f, theme.scale);
    const guikit::Color gripCol = (n->scrollbarActive || n->scrollbarHovered)
        ? theme.GetVariant(guikit::UiVariant::Primary).bg
        : theme.palette.textSecondary.WithAlphaF(0.55f);
    const float bx = r.Right() - 4.0f * s;
    const float by = r.Bottom() - 4.0f * s;
    for (int i = 0; i < 3; ++i) {
        const float span = (3.5f + static_cast<float>(i) * 3.5f) * s;
        canvas.DrawLine({bx - span, by}, {bx, by - span}, gripCol, std::max(1.0f, 1.2f * s));
    }
}

inline void RenderWindowTitlebarTrafficLights(const guikit::Node* n, guikit::ICanvas& canvas, const guikit::UiTheme& theme) noexcept {
    const guikit::Rect& r = n->computedRect;
    if (r.w <= 20.0f || r.h <= 4.0f) return;

    const float s = std::max(0.5f, theme.scale);
    const float dotD = 10.0f * s;
    const float dotR = dotD * 0.5f;
    const float gap = 7.0f * s;
    const float cy = r.Center().y - dotR;
    const float x0 = r.x + 12.0f * s;

    // Traffic lights: Close (Red), Minimize (Amber), Zoom (Green)
    canvas.DrawRoundRect(guikit::Rect(x0, cy, dotD, dotD), dotR, guikit::Color(255, 95, 86, 255), guikit::Color(224, 68, 62, 180), 0.75f);
    canvas.DrawRoundRect(guikit::Rect(x0 + dotD + gap, cy, dotD, dotD), dotR, guikit::Color(255, 189, 46, 255), guikit::Color(222, 159, 34, 180), 0.75f);
    canvas.DrawRoundRect(guikit::Rect(x0 + (dotD + gap) * 2.0f, cy, dotD, dotD), dotR, guikit::Color(39, 201, 63, 255), guikit::Color(29, 171, 47, 180), 0.75f);

    // 1px bottom divider of the window titlebar
    canvas.DrawRect(guikit::Rect(r.x, r.Bottom() - 1.0f, r.w, 1.0f), theme.palette.border);
}

template <bool IsPhone>
inline void RenderDeviceChassisButtons(const guikit::Node* n, guikit::ICanvas& canvas, const guikit::UiTheme& theme) noexcept {
    const guikit::Rect& r = n->computedRect;
    if (r.w <= 40.0f || r.h <= 40.0f) return;

    const float s = std::max(0.35f, theme.scale);
    const guikit::Color btnCol(71, 85, 105, 255);
    const float btnThickness = std::max(1.5f, std::round(2.5f * s));

    if (r.h >= r.w) {
        // Portrait orientation: volume rockers on left edge, power button on right edge
        const float yBase = r.y + std::min(r.h * 0.20f, 84.0f * s);
        canvas.DrawRoundRect(guikit::Rect(r.x - btnThickness, yBase, btnThickness, 22.0f * s), 1.5f * s, btnCol);
        canvas.DrawRoundRect(guikit::Rect(r.x - btnThickness, yBase + 32.0f * s, btnThickness, 36.0f * s), 1.5f * s, btnCol);
        canvas.DrawRoundRect(guikit::Rect(r.x - btnThickness, yBase + 74.0f * s, btnThickness, 36.0f * s), 1.5f * s, btnCol);
        canvas.DrawRoundRect(guikit::Rect(r.Right(), yBase + 42.0f * s, btnThickness, 52.0f * s), 1.5f * s, btnCol);
    } else {
        // Landscape orientation: buttons on top edge
        const float xBase = r.x + std::min(r.w * 0.18f, 90.0f * s);
        canvas.DrawRoundRect(guikit::Rect(xBase, r.y - btnThickness, 34.0f * s, btnThickness), 1.5f * s, btnCol);
        canvas.DrawRoundRect(guikit::Rect(xBase + 42.0f * s, r.y - btnThickness, 34.0f * s, btnThickness), 1.5f * s, btnCol);
    }

    // Tablet front-facing camera is embedded inside the outer hardware bezel (not inside the LCD screen)
    if constexpr (!IsPhone) {
        const float bezelT = 10.0f * s;
        const float camD = std::max(3.5f, 5.2f * s);
        const float coreD = std::max(1.8f, 2.4f * s);
        const float cx = r.Center().x;
        const float cy = r.y + bezelT * 0.5f;
        canvas.DrawRoundRect(
            guikit::Rect(cx - camD * 0.5f, cy - camD * 0.5f, camD, camD),
            camD * 0.5f,
            guikit::Color(6, 10, 18, 255),
            guikit::Color(51, 65, 85, 180),
            0.75f
        );
        canvas.DrawRoundRect(
            guikit::Rect(cx - coreD * 0.5f, cy - coreD * 0.5f, coreD, coreD),
            coreD * 0.5f,
            guikit::Color(30, 58, 138, 170)
        );
    }
}

inline void RenderPhoneDynamicIsland(const guikit::Node* n, guikit::ICanvas& canvas, const guikit::UiTheme& theme) noexcept {
    const guikit::Rect& r = n->computedRect;
    if (r.w <= 40.0f || r.h <= 40.0f) return;

    const float s = std::max(0.35f, theme.scale);
    const guikit::Color pillFill(4, 6, 11, 255);
    const guikit::Color pillStroke(30, 41, 59, 135);
    const guikit::Color lensHousing(13, 18, 28, 255);
    const guikit::Color lensGlass(23, 37, 84, 195);
    const guikit::Color lensGlint(147, 197, 253, 135);
    const guikit::Color sensorDot(12, 16, 24, 255);

    if (r.h >= r.w) {
        // Portrait: horizontal Dynamic Island at top-center of the screen
        const float pillW = std::min(r.w * 0.31f, 92.0f * s);
        const float pillH = 22.0f * s;
        const float pillX = r.Center().x - pillW * 0.5f;
        const float pillY = r.y + 7.0f * s;
        const float cy = pillY + pillH * 0.5f;

        canvas.DrawRoundRect(guikit::Rect(pillX, pillY, pillW, pillH), pillH * 0.5f, pillFill, pillStroke, 0.75f);

        // Subtle left FaceID infrared sensor housing
        const float senD = 6.5f * s;
        canvas.DrawRoundRect(guikit::Rect(pillX + 7.5f * s, cy - senD * 0.5f, senD, senD), senD * 0.5f, sensorDot);

        // Right sapphire camera lens + inner glass + tiny specular reflection
        const float lensD = 8.5f * s;
        const float coreD = 4.2f * s;
        const float glintD = std::max(1.2f, 1.6f * s);
        const float lx = pillX + pillW - 6.5f * s - lensD * 0.5f;
        canvas.DrawRoundRect(guikit::Rect(lx - lensD * 0.5f, cy - lensD * 0.5f, lensD, lensD), lensD * 0.5f, lensHousing);
        canvas.DrawRoundRect(guikit::Rect(lx - coreD * 0.5f, cy - coreD * 0.5f, coreD, coreD), coreD * 0.5f, lensGlass);
        canvas.DrawRoundRect(guikit::Rect(lx - coreD * 0.30f, cy - coreD * 0.30f, glintD, glintD), glintD * 0.5f, lensGlint);
    } else {
        // Landscape (90 deg CCW): vertical Dynamic Island on the physical left edge of the screen
        const float pillW = 22.0f * s;
        const float pillH = std::min(r.h * 0.31f, 92.0f * s);
        const float pillX = r.x + 7.0f * s;
        const float pillY = r.Center().y - pillH * 0.5f;
        const float cx = pillX + pillW * 0.5f;

        canvas.DrawRoundRect(guikit::Rect(pillX, pillY, pillW, pillH), pillW * 0.5f, pillFill, pillStroke, 0.75f);

        // Top sapphire camera lens (rotated 90 deg CCW from right side of portrait island)
        const float lensD = 8.5f * s;
        const float coreD = 4.2f * s;
        const float glintD = std::max(1.2f, 1.6f * s);
        const float ly = pillY + 6.5f * s + lensD * 0.5f;
        canvas.DrawRoundRect(guikit::Rect(cx - lensD * 0.5f, ly - lensD * 0.5f, lensD, lensD), lensD * 0.5f, lensHousing);
        canvas.DrawRoundRect(guikit::Rect(cx - coreD * 0.5f, ly - coreD * 0.5f, coreD, coreD), coreD * 0.5f, lensGlass);
        canvas.DrawRoundRect(guikit::Rect(cx - coreD * 0.30f, ly - coreD * 0.30f, glintD, glintD), glintD * 0.5f, lensGlint);

        // Subtle bottom FaceID sensor housing
        const float senD = 6.5f * s;
        canvas.DrawRoundRect(guikit::Rect(cx - senD * 0.5f, pillY + pillH - 7.5f * s - senD, senD, senD), senD * 0.5f, sensorDot);
    }
}

template <bool IsPhone>
inline void RenderDeviceStatusSafeArea(const guikit::Node* n, guikit::ICanvas& canvas, const guikit::UiTheme& theme) noexcept {
    const guikit::Rect& r = n->computedRect;
    if (r.w <= 40.0f || r.h <= 6.0f) return;

    const float s = std::max(0.35f, theme.scale);
    const guikit::Color fg = theme.palette.textPrimary;
    const guikit::Color fgMuted = theme.palette.textSecondary;

    // 1. Left: System Clock ("9:41")
    const float fontSize = std::max(7.0f, std::round(11.0f * s));
    const float textY = r.y + (r.h - fontSize) * 0.5f;
    const float edgeInset = IsPhone ? (22.0f * s) : (18.0f * s);
    canvas.DrawText("9:41", {r.x + edgeInset, textY}, fontSize, fg);

    // 2. Right: Cellular Signal Bars + Battery Icon
    const float battW = 18.0f * s;
    const float battH = 9.0f * s;
    const float battX = r.Right() - edgeInset - battW;
    const float battY = r.Center().y - battH * 0.5f;
    canvas.DrawRoundRect(guikit::Rect(battX, battY, battW, battH), 2.5f * s, guikit::Color::Transparent(), fgMuted, 1.0f);
    canvas.DrawRect(guikit::Rect(battX + battW + 1.0f * s, r.Center().y - 2.0f * s, 1.5f * s, 4.0f * s), fgMuted);
    canvas.DrawRoundRect(guikit::Rect(battX + 2.0f * s, battY + 2.0f * s, battW - 5.5f * s, battH - 4.0f * s), 1.2f * s, fg);

    // 4-bar cellular signal indicator
    const float sigBaseX = battX - 18.0f * s;
    for (int i = 0; i < 4; ++i) {
        const float bh = (3.0f + static_cast<float>(i) * 1.8f) * s;
        const float bx = sigBaseX + static_cast<float>(i) * 3.5f * s;
        const float by = battY + battH - bh;
        canvas.DrawRect(guikit::Rect(bx, by, 2.2f * s, bh), fg);
    }
}

inline void RenderDeviceHomeIndicatorSafeArea(const guikit::Node* n, guikit::ICanvas& canvas, const guikit::UiTheme& theme) noexcept {
    const guikit::Rect& r = n->computedRect;
    if (r.w <= 20.0f || r.h <= 4.0f) return;

    const float s = std::max(0.35f, theme.scale);
    const float pillW = std::min(r.w * 0.36f, 114.0f * s);
    const float pillH = std::max(2.0f, std::round(4.0f * s));
    const float pillX = r.Center().x - pillW * 0.5f;
    const float pillY = r.Center().y - pillH * 0.5f;
    canvas.DrawRoundRect(
        guikit::Rect(pillX, pillY, pillW, pillH),
        pillH * 0.5f,
        theme.palette.textSecondary.WithAlphaF(0.45f)
    );
}

} // namespace detail

/// Renders the recessed studio dot-grid backdrop and the selected device/window bezel
/// (`Window`, `Tablet`, `Phone`, or `Full`), with support for `Fit`, `Real 1:1 (mm)`, preset `%`,
/// and `Ctrl + MouseWheel` zoom, invoking `renderClientContent()` inside the isolated client viewport.
template <typename ContentFn>
inline void RenderPreviewViewport(
    guikit::Context& ui,
    DemoContext& demoCtx,
    NavCategory selectedCategory,
    ContentFn&& renderClientContent
) {
    const PreviewFormFactor formFactor = static_cast<PreviewFormFactor>(
        std::min<size_t>(demoCtx.previewFormFactorIdx, 3)
    );
    const bool isDeviceMode = (formFactor == PreviewFormFactor::Tablet || formFactor == PreviewFormFactor::Phone);
    const bool isPhone = (formFactor == PreviewFormFactor::Phone);
    const bool isResizableWindow = (formFactor == PreviewFormFactor::Window);
    const bool useCenteredScrollCanvas = isDeviceMode || (isResizableWindow && demoCtx.previewWindowCustomSize);
    const bool isFullStage = IsFullStageCategory(selectedCategory);
    const bool isShellScreen = (selectedCategory == NavCategory::PanelSidebar);
    const float clientPad = isShellScreen ? 0.0f : (isFullStage ? 8.0f : 24.0f);

    // 1. Recessed Studio Artboard Backdrop (2D ScrollArea in Device / Custom-Sized Window mode, Stretch Box otherwise)
    if (auto backdropBox = ui.ScrollArea({
        .orientation = guikit::Orientation::Both,
        .scrollbar = useCenteredScrollCanvas ? guikit::VisibilityPolicy::Auto : guikit::VisibilityPolicy::Never,
        .align = useCenteredScrollCanvas ? guikit::Alignment::Center : guikit::Alignment::Stretch,
        .justify = useCenteredScrollCanvas ? guikit::Justify::Center : guikit::Justify::Start,
        .padding = guikit::Padding::All(18.0f)
    })) {
        guikit::Node* backdropNode = backdropBox.GetNode();
        backdropNode->backgroundColor = (demoCtx.shellTheme.mode == guikit::ThemeMode::Light)
            ? guikit::Color(222, 229, 238, 255)
            : guikit::Color(8, 12, 20, 255);
        backdropNode->renderFn = &detail::RenderDotGridBackdrop;
        backdropNode->postRenderFn = &detail::RenderBackdropHudBadge;

        const guikit::Rect backdropBounds = ui.GetPrevFrameBounds(backdropNode->id);
        const float shellScale = std::max(0.5f, ui.GetCurrentScale());
        const float padPx = 36.0f * shellScale;
        const float availPxW = (backdropBounds.w > 80.0f) ? std::max(120.0f, backdropBounds.w - padPx) : (860.0f * shellScale);
        const float availPxH = (backdropBounds.h > 80.0f) ? std::max(120.0f, backdropBounds.h - padPx) : (560.0f * shellScale);

        // Hardware device specifications:
        // - Phone (6.3" iPhone 16 Pro class): 375 x 784 dp  |  71.5 x 149.6 mm
        // - Tablet (8.3" iPad mini / Touch Panel class): 540 x 782 dp  |  134.8 x 195.4 mm
        const float rawPxPerMm    = (demoCtx.displayPxPerMm > 0.5f) ? demoCtx.displayPxPerMm : (96.0f / 25.4f);
        const float dpiScale      = std::max(0.1f, ui.GetDpiScale());
        const float pxPerMm       = rawPxPerMm / dpiScale;
        const float basePortraitW = isPhone ? 375.0f : 540.0f;
        const float basePortraitH = isPhone ? 784.0f : 782.0f;
        const float baseMmW       = isPhone ? 71.5f  : 134.8f;
        const float baseMmH       = isPhone ? 149.6f : 195.4f;
        const float targetW       = isDeviceMode ? (demoCtx.previewLandscape ? basePortraitH : basePortraitW) : demoCtx.previewWindowW;
        const float targetH       = isDeviceMode ? (demoCtx.previewLandscape ? basePortraitW : basePortraitH) : demoCtx.previewWindowH;

        const PreviewZoomMode zoomMode = static_cast<PreviewZoomMode>(
            std::min<size_t>(demoCtx.previewZoomModeIdx, static_cast<size_t>(PreviewZoomMode::Custom))
        );

        float effectiveZoom = 1.0f;
        switch (zoomMode) {
            case PreviewZoomMode::Fit:
                if (isDeviceMode) {
                    effectiveZoom = std::clamp(std::min(availPxW / targetW, availPxH / targetH), 0.35f, 2.50f);
                } else if (isResizableWindow && demoCtx.previewWindowCustomSize) {
                    effectiveZoom = std::clamp(std::min(1.0f, std::min(availPxW / targetW, availPxH / targetH)), 0.35f, 1.0f);
                } else {
                    effectiveZoom = 1.0f;
                }
                break;
            case PreviewZoomMode::Real1To1:
                if (isDeviceMode) {
                    const float devMmW = demoCtx.previewLandscape ? baseMmH : baseMmW;
                    effectiveZoom = std::clamp((devMmW * pxPerMm) / targetW, 0.35f, 2.50f);
                } else {
                    effectiveZoom = 1.0f;
                }
                break;
            case PreviewZoomMode::Zoom75:  effectiveZoom = 0.75f; break;
            case PreviewZoomMode::Zoom100: effectiveZoom = 1.00f; break;
            case PreviewZoomMode::Zoom125: effectiveZoom = 1.25f; break;
            case PreviewZoomMode::Zoom150: effectiveZoom = 1.50f; break;
            case PreviewZoomMode::Zoom175: effectiveZoom = 1.75f; break;
            case PreviewZoomMode::Zoom200: effectiveZoom = 2.00f; break;
            case PreviewZoomMode::Custom:
                effectiveZoom = std::clamp(demoCtx.previewCustomZoom, 0.35f, 2.50f);
                break;
        }

        const float realMmW = isDeviceMode
            ? (demoCtx.previewLandscape ? baseMmH : baseMmW)
            : ((targetW * effectiveZoom) / pxPerMm);
        const float realMmH = isDeviceMode
            ? (demoCtx.previewLandscape ? baseMmW : baseMmH)
            : ((targetH * effectiveZoom) / pxPerMm);

        // Interactive Ctrl + MouseWheel zoom over the preview viewport
        if (backdropBounds.w > 0.0f && backdropBounds.Contains(ui.GetMousePos()) && ui.GetKeyModifiers().ctrl) {
            const float wheelY = ui.GetMouseWheel().y;
            if (std::abs(wheelY) > 0.001f) {
                const float snappedBase = std::round(effectiveZoom * 20.0f) / 20.0f;
                const float step = (wheelY > 0.0f ? 0.05f : -0.05f) * std::max(1.0f, std::round(std::abs(wheelY)));
                effectiveZoom = std::clamp(snappedBase + step, 0.35f, 2.50f);
                demoCtx.previewCustomZoom = effectiveZoom;
                demoCtx.previewZoomModeIdx = static_cast<size_t>(PreviewZoomMode::Custom);
                ui.ConsumeMouseWheel();
                ui.InvalidateAll();
            }
        }
        demoCtx.previewEffectiveZoom = effectiveZoom;

        const int zoomPct = static_cast<int>(std::lround(effectiveZoom * 100.0f));
        if (useCenteredScrollCanvas) {
            const std::string_view modeTag = (zoomMode == PreviewZoomMode::Real1To1)
                ? "1:1 Physical"
                : (zoomMode == PreviewZoomMode::Fit ? "Fit" : "Zoom");
            backdropNode->text = ui.AllocString(ui.Format(
                "{}x{} dp  |  {:.1f}x{:.1f} mm  |  {} ({}%)",
                static_cast<int>(std::lround(targetW)),
                static_cast<int>(std::lround(targetH)),
                realMmW,
                realMmH,
                modeTag,
                zoomPct
            ));
        } else {
            backdropNode->text = {};
        }

        backdropNode->visualHash = guikit::detail::HashArgs(
            static_cast<uint8_t>(demoCtx.shellTheme.mode),
            static_cast<uint8_t>(formFactor),
            demoCtx.previewLandscape,
            demoCtx.previewZoomModeIdx,
            zoomPct,
            demoCtx.previewWindowCustomSize,
            static_cast<int>(demoCtx.previewWindowW),
            static_cast<int>(demoCtx.previewWindowH)
        );

        auto invokeStageInsideClient = [&]() {
            if (isFullStage) {
                renderClientContent();
            } else if (auto centerRow = ui.Row({
                .align = guikit::Alignment::Center,
                .justify = guikit::Justify::Center,
                .heightMode = guikit::SizeMode::Stretch
            })) {
                renderClientContent();
            }
        };

        if (formFactor == PreviewFormFactor::Window) {
            // Desktop OS Window Bezel (Traffic lights + Screen Title + Client WxH badge + Interactive Edge/Corner Resize)
            const bool resizeActiveOrHovered = (demoCtx.previewWindowResizeEdge != 0 || demoCtx.previewWindowHoverEdge != 0);
            const guikit::Color winBorderCol = resizeActiveOrHovered
                ? demoCtx.shellTheme.GetVariant(guikit::UiVariant::Primary).bg
                : ((demoCtx.shellTheme.mode == guikit::ThemeMode::Light)
                    ? guikit::Color(148, 163, 184, 255)
                    : guikit::Color(71, 85, 105, 255));

            if (auto windowFrame = ui.Box({
                .gap = 0.0f,
                .widthMode = demoCtx.previewWindowCustomSize ? guikit::SizeMode::Fixed : guikit::SizeMode::Stretch,
                .heightMode = demoCtx.previewWindowCustomSize ? guikit::SizeMode::Fixed : guikit::SizeMode::Stretch,
                .fixedWidth = demoCtx.previewWindowCustomSize ? (demoCtx.previewWindowW * effectiveZoom / shellScale) : 0.0f,
                .fixedHeight = demoCtx.previewWindowCustomSize ? (demoCtx.previewWindowH * effectiveZoom / shellScale) : 0.0f,
                .padding = guikit::Padding::All(0.0f),
                .background = {demoCtx.previewTheme.palette.surface},
                .border = {.width = resizeActiveOrHovered ? 2.0f : 1.5f, .radius = 10.0f, .color = {winBorderCol}},
                .clipChildren = true
            })) {
                guikit::Node* winNode = windowFrame.GetNode();
                winNode->postRenderFn = &detail::RenderWindowResizeGrip;

                const guikit::Id winId = winNode->id;
                const guikit::Id winResizeId(guikit::detail::HashArgs(winId.value, uint64_t(0x57494E5F52535Aull)));
                const guikit::Rect winBounds = ui.GetPrevFrameBounds(winId);

                if (winBounds.w > 40.0f && winBounds.h > 40.0f) {
                    const guikit::Vec2 mouse = ui.GetMousePos();
                    const float winScale = shellScale;
                    const float titleH = 28.0f * winScale;
                    const float gripThick = 7.0f * shellScale;
                    const float cornerSpan = 16.0f * shellScale;

                    const guikit::Rect brCorner(winBounds.Right() - cornerSpan, winBounds.Bottom() - cornerSpan, cornerSpan + 5.0f, cornerSpan + 5.0f);
                    const guikit::Rect blCorner(winBounds.x - 5.0f, winBounds.Bottom() - cornerSpan, cornerSpan + 5.0f, cornerSpan + 5.0f);
                    const guikit::Rect rEdge(winBounds.Right() - gripThick, winBounds.y + titleH, gripThick * 2.0f, std::max(0.0f, winBounds.h - titleH - cornerSpan));
                    const guikit::Rect lEdge(winBounds.x - gripThick, winBounds.y + titleH, gripThick * 2.0f, std::max(0.0f, winBounds.h - titleH - cornerSpan));
                    const guikit::Rect bEdge(winBounds.x + cornerSpan, winBounds.Bottom() - gripThick, std::max(0.0f, winBounds.w - cornerSpan * 2.0f), gripThick * 2.0f);

                    // Green traffic-light button & titlebar double-click (Maximize / Restore toggle)
                    const float dotD = 10.0f * winScale;
                    const float dotGap = 7.0f * winScale;
                    const guikit::Rect greenBtnRect(
                        winBounds.x + 12.0f * winScale + (dotD + dotGap) * 2.0f - 2.0f,
                        winBounds.y,
                        dotD + 4.0f,
                        titleH
                    );
                    const guikit::Rect titlebarRect(winBounds.x, winBounds.y, winBounds.w, titleH);

                    const bool occluded = ui.IsMouseOccluded(winId, mouse);
                    const bool otherActive = ui.GetActiveId().IsValid() && ui.GetActiveId() != winResizeId;

                    uint8_t hovEdge = 0;
                    if (!occluded && !otherActive) {
                        if (brCorner.Contains(mouse))      hovEdge = 2 | 4;
                        else if (blCorner.Contains(mouse)) hovEdge = 1 | 4;
                        else if (rEdge.Contains(mouse))    hovEdge = 2;
                        else if (lEdge.Contains(mouse))    hovEdge = 1;
                        else if (bEdge.Contains(mouse))    hovEdge = 4;
                    }
                    if (hovEdge != demoCtx.previewWindowHoverEdge) {
                        demoCtx.previewWindowHoverEdge = hovEdge;
                        ui.InvalidateAll();
                    }

                    auto applyEdgeCursor = [&](uint8_t edge) {
                        if (edge == (2 | 4))      ui.SetCursor(guikit::CursorType::ResizeNWSE);
                        else if (edge == (1 | 4)) ui.SetCursor(guikit::CursorType::ResizeNESW);
                        else if (edge & (1 | 2))  ui.SetCursor(guikit::CursorType::ResizeEW);
                        else if (edge & 4)        ui.SetCursor(guikit::CursorType::ResizeNS);
                    };

                    const float maxFitW = std::max(320.0f, std::floor(availPxW / effectiveZoom));
                    const float maxFitH = std::max(220.0f, std::floor(availPxH / effectiveZoom));

                    auto toggleMaximizeRestore = [&]() {
                        if (demoCtx.previewWindowCustomSize) {
                            demoCtx.previewWindowCustomSize = false;
                        } else {
                            demoCtx.previewWindowCustomSize = true;
                            demoCtx.previewWindowW = std::clamp(std::round(maxFitW * 0.78f), 320.0f, maxFitW);
                            demoCtx.previewWindowH = std::clamp(std::round(maxFitH * 0.78f), 220.0f, maxFitH);
                        }
                        ui.InvalidateAll();
                    };

                    if (demoCtx.previewWindowResizeEdge == 0 && !occluded && !otherActive) {
                        if (hovEdge != 0) {
                            applyEdgeCursor(hovEdge);
                            if (ui.IsMouseDoubleClicked()) {
                                demoCtx.previewWindowCustomSize = false;
                                ui.ConsumeMouseClick();
                                ui.InvalidateAll();
                            } else if (ui.IsMouseClicked()) {
                                demoCtx.previewWindowResizeEdge = hovEdge;
                                demoCtx.previewWindowDragStartMouse = mouse;
                                demoCtx.previewWindowDragStartW = demoCtx.previewWindowCustomSize
                                    ? demoCtx.previewWindowW
                                    : std::clamp(std::round(winBounds.w / effectiveZoom), 320.0f, maxFitW);
                                demoCtx.previewWindowDragStartH = demoCtx.previewWindowCustomSize
                                    ? demoCtx.previewWindowH
                                    : std::clamp(std::round(winBounds.h / effectiveZoom), 220.0f, maxFitH);
                                ui.SetActiveId(winResizeId);
                                ui.ConsumeMouseClick();
                                ui.InvalidateAll();
                            }
                        } else if (greenBtnRect.Contains(mouse)) {
                            ui.SetCursor(guikit::CursorType::Pointer);
                            if (ui.IsMouseClicked()) {
                                toggleMaximizeRestore();
                                ui.ConsumeMouseClick();
                            }
                        } else if (titlebarRect.Contains(mouse) && ui.IsMouseDoubleClicked()) {
                            toggleMaximizeRestore();
                            ui.ConsumeMouseClick();
                        }
                    }

                    if (demoCtx.previewWindowResizeEdge != 0) {
                        if (ui.IsMouseDown()) {
                            ui.SetActiveId(winResizeId);
                            applyEdgeCursor(demoCtx.previewWindowResizeEdge);
                            const uint8_t edge = demoCtx.previewWindowResizeEdge;
                            const float rawDx = mouse.x - demoCtx.previewWindowDragStartMouse.x;
                            const float rawDy = mouse.y - demoCtx.previewWindowDragStartMouse.y;
                            if (demoCtx.previewWindowCustomSize || std::abs(rawDx) > 1.0f || std::abs(rawDy) > 1.0f) {
                                demoCtx.previewWindowCustomSize = true;
                                // Symmetric 2x delta keeps the dragged edge locked 1:1 under the cursor while centered
                                if (edge & 2) {
                                    const float dx = rawDx * 2.0f / effectiveZoom;
                                    demoCtx.previewWindowW = std::clamp(std::round(demoCtx.previewWindowDragStartW + dx), 320.0f, maxFitW);
                                } else if (edge & 1) {
                                    const float dx = -rawDx * 2.0f / effectiveZoom;
                                    demoCtx.previewWindowW = std::clamp(std::round(demoCtx.previewWindowDragStartW + dx), 320.0f, maxFitW);
                                } else {
                                    demoCtx.previewWindowW = std::clamp(demoCtx.previewWindowDragStartW, 320.0f, maxFitW);
                                }
                                if (edge & 4) {
                                    const float dy = rawDy * 2.0f / effectiveZoom;
                                    demoCtx.previewWindowH = std::clamp(std::round(demoCtx.previewWindowDragStartH + dy), 220.0f, maxFitH);
                                } else {
                                    demoCtx.previewWindowH = std::clamp(demoCtx.previewWindowDragStartH, 220.0f, maxFitH);
                                }
                                ui.InvalidateAll();
                                ui.RequestRepaint(2);
                            }
                        } else {
                            demoCtx.previewWindowResizeEdge = 0;
                            if (demoCtx.previewWindowW >= maxFitW - 1.5f && demoCtx.previewWindowH >= maxFitH - 1.5f) {
                                demoCtx.previewWindowCustomSize = false;
                            }
                            if (ui.GetActiveId() == winResizeId) {
                                ui.SetActiveId(guikit::Id::Null());
                            }
                            ui.InvalidateAll();
                        }
                    }
                }

                winNode->scrollbarHovered = (demoCtx.previewWindowHoverEdge != 0);
                winNode->scrollbarActive = (demoCtx.previewWindowResizeEdge != 0);

                const guikit::Color titlebarBg = (demoCtx.previewTheme.mode == guikit::ThemeMode::Light)
                    ? guikit::Color(241, 245, 249, 255)
                    : guikit::Color(15, 23, 42, 255);
                if (auto titlebar = ui.Box({
                    .widthMode = guikit::SizeMode::Stretch,
                    .heightMode = guikit::SizeMode::Fixed,
                    .fixedHeight = 28.0f,
                    .padding = guikit::Padding::Symmetric(12.0f, 0.0f),
                    .background = {titlebarBg},
                    .border = {.width = 0.0f, .radius = 0.0f}
                })) {
                    titlebar.GetNode()->postRenderFn = &detail::RenderWindowTitlebarTrafficLights;
                    if (auto titleRow = ui.Row({
                        .gap = 8.0f,
                        .align = guikit::Alignment::Center,
                        .heightMode = guikit::SizeMode::Stretch,
                        .theme = &demoCtx.previewTheme,
                        .scale = shellScale
                    })) {
                        // Left slot reserved for Traffic Lights (3 dots * 10px + 2 * 7px gaps = 44px + 8px breathing room)
                        if (auto tlSlot = ui.Box({
                            .widthMode = guikit::SizeMode::Fixed,
                            .heightMode = guikit::SizeMode::Stretch,
                            .fixedWidth = 52.0f
                        })) {}
                        ui.Spacer();
                        ui.Text(CategoryDisplayTitle(selectedCategory), {
                            .role = guikit::TextRole::LabelSmall,
                            .variant = guikit::UiVariant::Secondary
                        });
                        ui.Spacer();
                        const int clientW = demoCtx.previewWindowCustomSize
                            ? std::max(0, static_cast<int>(std::lround(demoCtx.previewWindowW)))
                            : std::max(0, static_cast<int>(std::round(availPxW / effectiveZoom)));
                        const int clientH = demoCtx.previewWindowCustomSize
                            ? std::max(0, static_cast<int>(std::lround(demoCtx.previewWindowH - (28.0f * shellScale) / effectiveZoom)))
                            : std::max(0, static_cast<int>(std::round((availPxH - 28.0f * shellScale) / effectiveZoom)));
                        ui.Text(ui.Format("{} x {}", clientW, clientH), {
                            .role = guikit::TextRole::CodeSmall,
                            .variant = guikit::UiVariant::Muted
                        });
                    }
                }

                if (auto clientBox = ui.Box({
                    .widthMode = guikit::SizeMode::Stretch,
                    .heightMode = guikit::SizeMode::Stretch,
                    .padding = guikit::Padding::All(clientPad),
                    .clipChildren = true,
                    .theme = &demoCtx.previewTheme,
                    .scale = effectiveZoom,
                    .debugBounds = demoCtx.previewBounds,
                    .inspectMode = demoCtx.previewInspect
                })) {
                    invokeStageInsideClient();
                }
            }
        } else if (isDeviceMode) {
            // Hardware Device Chassis (Tablet or Smartphone with Safe-Area Top & Bottom Bars)
            const float devicePad = isShellScreen ? 0.0f : (isFullStage ? 6.0f : 16.0f);

            if (auto chassisBox = ui.Box({
                .gap = 0.0f,
                .widthMode = guikit::SizeMode::Fixed,
                .heightMode = guikit::SizeMode::Fixed,
                .fixedWidth = targetW,
                .fixedHeight = targetH,
                .padding = guikit::Padding::All(10.0f),
                .background = {guikit::Color(15, 20, 28, 255)},
                .border = {
                    .width = 2.0f,
                    .radius = isPhone ? 38.0f : 24.0f,
                    .color = {guikit::Color(100, 116, 139, 255)}
                },
                .scale = effectiveZoom
            })) {
                chassisBox.GetNode()->postRenderFn = isPhone
                    ? &detail::RenderDeviceChassisButtons<true>
                    : &detail::RenderDeviceChassisButtons<false>;

                if (auto screenBox = ui.Box({
                    .gap = 0.0f,
                    .widthMode = guikit::SizeMode::Stretch,
                    .heightMode = guikit::SizeMode::Stretch,
                    .padding = guikit::Padding::All(0.0f),
                    .background = {demoCtx.previewTheme.palette.surface},
                    .border = {
                        .width = 1.0f,
                        .radius = isPhone ? 28.0f : 14.0f,
                        .color = {guikit::Color(51, 65, 85, 190)}
                    },
                    .clipChildren = true
                })) {
                    if (isPhone) {
                        screenBox.GetNode()->postRenderFn = &detail::RenderPhoneDynamicIsland;
                    }

                    // 1. Top Safe Area (9:41 Clock + Signal/Battery)
                    if (auto safeTop = ui.Box({
                        .widthMode = guikit::SizeMode::Stretch,
                        .heightMode = guikit::SizeMode::Fixed,
                        .fixedHeight = (isPhone && !demoCtx.previewLandscape) ? 36.0f : 24.0f,
                        .theme = &demoCtx.previewTheme
                    })) {
                        safeTop.GetNode()->postRenderFn = isPhone
                            ? &detail::RenderDeviceStatusSafeArea<true>
                            : &detail::RenderDeviceStatusSafeArea<false>;
                    }

                    // 2. Isolated Application Client Area (includes left safe-area clearance in Phone Landscape)
                    const guikit::Padding clientSafePad = (isPhone && demoCtx.previewLandscape)
                        ? guikit::Padding{devicePad + 26.0f, devicePad, devicePad, devicePad}
                        : guikit::Padding::All(devicePad);
                    if (auto clientBox = ui.Box({
                        .widthMode = guikit::SizeMode::Stretch,
                        .heightMode = guikit::SizeMode::Stretch,
                        .padding = clientSafePad,
                        .clipChildren = true,
                        .theme = &demoCtx.previewTheme,
                        .scale = effectiveZoom,
                        .debugBounds = demoCtx.previewBounds,
                        .inspectMode = demoCtx.previewInspect
                    })) {
                        invokeStageInsideClient();
                    }

                    // 3. Bottom Safe Area (Home Indicator Bar)
                    if (auto safeBottom = ui.Box({
                        .widthMode = guikit::SizeMode::Stretch,
                        .heightMode = guikit::SizeMode::Fixed,
                        .fixedHeight = 16.0f,
                        .theme = &demoCtx.previewTheme
                    })) {
                        safeBottom.GetNode()->postRenderFn = &detail::RenderDeviceHomeIndicatorSafeArea;
                    }
                }
            }
        } else {
            // Full Artboard Mode (clean bordered surface on the recessed backdrop)
            if (auto fullBox = ui.Box({
                .widthMode = guikit::SizeMode::Stretch,
                .heightMode = guikit::SizeMode::Stretch,
                .padding = guikit::Padding::All(clientPad),
                .background = {demoCtx.previewTheme.palette.surface},
                .border = {
                    .width = 1.0f,
                    .radius = 8.0f,
                    .color = {demoCtx.previewTheme.palette.border}
                },
                .clipChildren = true,
                .theme = &demoCtx.previewTheme,
                .scale = effectiveZoom,
                .debugBounds = demoCtx.previewBounds,
                .inspectMode = demoCtx.previewInspect
            })) {
                invokeStageInsideClient();
            }
        }
    }
}

} // namespace guikit::demo
