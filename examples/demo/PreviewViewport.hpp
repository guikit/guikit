#pragma once

#include <SDL3/SDL.h>
#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#ifdef DrawText
#undef DrawText
#endif
#ifdef small
#undef small
#endif
#ifdef RGB
#undef RGB
#endif
#elif defined(__EMSCRIPTEN__)
#include <emscripten/html5.h>
#endif

#include <algorithm>
#include <cmath>
#include "DemoContext.hpp"
#include "guikit/backend/ICanvas.hpp"
#include "guikit/core/Node.hpp"

namespace guikit::demo {

/// Queries the physical pixel density of the active display in pixels per millimeter (px/mm).
/// - Windows (`_WIN32`): Queries hardware EDID physical dimensions via GDI `GetDeviceCaps(HORZSIZE / DESKTOPHORZRES)`.
/// - WebAssembly (`__EMSCRIPTEN__`): Uses `emscripten_get_device_pixel_ratio()` relative to standard 96 CSS DPI.
/// - Linux / macOS / Fallback: Uses `SDL_GetWindowDisplayScale(window)` relative to 96 DPI.
inline float QueryDisplayPixelsPerMm(SDL_Window* window) noexcept {
#if defined(_WIN32)
    HDC hdc = ::GetDC(nullptr);
    if (hdc != nullptr) {
        const int widthMm = ::GetDeviceCaps(hdc, HORZSIZE);
        const int heightMm = ::GetDeviceCaps(hdc, VERTSIZE);
        const int widthPx = ::GetDeviceCaps(hdc, DESKTOPHORZRES);
        const int heightPx = ::GetDeviceCaps(hdc, DESKTOPVERTRES);
        ::ReleaseDC(nullptr, hdc);
        if (widthMm > 50 && heightMm > 50 && widthPx > 320 && heightPx > 240) {
            const float pxPerMmX = static_cast<float>(widthPx) / static_cast<float>(widthMm);
            const float pxPerMmY = static_cast<float>(heightPx) / static_cast<float>(heightMm);
            const float avgPxPerMm = 0.5f * (pxPerMmX + pxPerMmY);
            if (avgPxPerMm >= 1.5f && avgPxPerMm <= 25.0f) {
                return avgPxPerMm;
            }
        }
    }
#elif defined(__EMSCRIPTEN__)
    const double dpr = emscripten_get_device_pixel_ratio();
    if (dpr > 0.1) {
        return static_cast<float>((96.0 * dpr) / 25.4);
    }
#endif
    const float displayScale = (window != nullptr) ? SDL_GetWindowDisplayScale(window) : 1.0f;
    const float safeScale = (displayScale > 0.1f) ? displayScale : 1.0f;
    return (96.0f * safeScale) / 25.4f;
}

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
    canvas.DrawText("9:41", {r.x + 18.0f * s, textY}, fontSize, fg);

    // 2. Center: Dynamic Island (Phone) or Front Camera Lens (Tablet)
    if constexpr (IsPhone) {
        const float pillW = std::min(r.w * 0.34f, 96.0f * s);
        const float pillH = std::min(r.h - 6.0f * s, 19.0f * s);
        const float pillX = r.Center().x - pillW * 0.5f;
        const float pillY = r.Center().y - pillH * 0.5f;
        canvas.DrawRoundRect(
            guikit::Rect(pillX, pillY, pillW, pillH),
            pillH * 0.5f,
            guikit::Color(6, 9, 15, 255),
            guikit::Color(51, 65, 85, 160),
            1.0f
        );
        // Subtle camera lens ring inside Dynamic Island
        const float lensD = 7.0f * s;
        canvas.DrawRoundRect(
            guikit::Rect(pillX + pillW - lensD - 6.0f * s, r.Center().y - lensD * 0.5f, lensD, lensD),
            lensD * 0.5f,
            guikit::Color(17, 24, 39, 255),
            guikit::Color(56, 189, 248, 120),
            1.0f
        );
    } else {
        const float camD = 7.0f * s;
        canvas.DrawRoundRect(
            guikit::Rect(r.Center().x - camD * 0.5f, r.Center().y - camD * 0.5f, camD, camD),
            camD * 0.5f,
            guikit::Color(15, 23, 42, 255),
            guikit::Color(100, 116, 139, 180),
            1.0f
        );
    }

    // 3. Right: Signal Bars + Battery Icon
    const float battW = 18.0f * s;
    const float battH = 9.0f * s;
    const float battX = r.Right() - 18.0f * s - battW;
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

    // Subtle 1px bottom safe-area separator
    canvas.DrawRect(guikit::Rect(r.x, r.Bottom() - 1.0f, r.w, 1.0f), theme.palette.borderSubtle.WithAlphaF(0.45f));
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
    const bool isFullStage = IsFullStageCategory(selectedCategory);
    const bool isShellScreen = (selectedCategory == NavCategory::PanelSidebar);
    const float clientPad = isShellScreen ? 0.0f : (isFullStage ? 8.0f : 24.0f);

    // 1. Recessed Studio Artboard Backdrop (2D ScrollArea in Device mode, Stretch Box in Window/Full mode)
    if (auto backdropBox = ui.ScrollArea({
        .orientation = guikit::Orientation::Both,
        .scrollbar = isDeviceMode ? guikit::VisibilityPolicy::Auto : guikit::VisibilityPolicy::Never,
        .align = isDeviceMode ? guikit::Alignment::Center : guikit::Alignment::Stretch,
        .justify = isDeviceMode ? guikit::Justify::Center : guikit::Justify::Start,
        .padding = guikit::Padding::All(18.0f)
    })) {
        guikit::Node* backdropNode = backdropBox.GetNode();
        if (!isDeviceMode) {
            backdropNode->type = guikit::NodeType::Box;
        }
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
        const float basePortraitW = isPhone ? 375.0f : 540.0f;
        const float basePortraitH = isPhone ? 784.0f : 782.0f;
        const float baseMmW       = isPhone ? 71.5f  : 134.8f;
        const float baseMmH       = isPhone ? 149.6f : 195.4f;
        const float targetW       = demoCtx.previewLandscape ? basePortraitH : basePortraitW;
        const float targetH       = demoCtx.previewLandscape ? basePortraitW : basePortraitH;
        const float realMmW       = demoCtx.previewLandscape ? baseMmH       : baseMmW;
        const float realMmH       = demoCtx.previewLandscape ? baseMmW       : baseMmH;

        const PreviewZoomMode zoomMode = static_cast<PreviewZoomMode>(
            std::min<size_t>(demoCtx.previewZoomModeIdx, static_cast<size_t>(PreviewZoomMode::Custom))
        );
        const float pxPerMm = (demoCtx.displayPxPerMm > 0.5f) ? demoCtx.displayPxPerMm : (96.0f / 25.4f);

        float effectiveZoom = 1.0f;
        switch (zoomMode) {
            case PreviewZoomMode::Fit:
                if (isDeviceMode) {
                    effectiveZoom = std::clamp(std::min(availPxW / targetW, availPxH / targetH), 0.35f, 2.50f);
                } else {
                    effectiveZoom = 1.0f;
                }
                break;
            case PreviewZoomMode::Real1To1:
                if (isDeviceMode) {
                    effectiveZoom = std::clamp((realMmW * pxPerMm) / targetW, 0.35f, 2.50f);
                } else {
                    effectiveZoom = std::clamp(pxPerMm / (96.0f / 25.4f), 0.50f, 2.50f);
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
        if (isDeviceMode) {
            const std::string_view modeTag = (zoomMode == PreviewZoomMode::Real1To1)
                ? "1:1 Physical"
                : (zoomMode == PreviewZoomMode::Fit ? "Fit" : "Zoom");
            backdropNode->text = ui.AllocString(ui.Format(
                "{}x{} dp  |  {:.1f}x{:.1f} mm  |  {} ({}%)",
                static_cast<int>(targetW),
                static_cast<int>(targetH),
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
            zoomPct
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
            // Desktop OS Window Bezel (Traffic lights + Screen Title + Client WxH badge)
            const guikit::Color winBorderCol = (demoCtx.shellTheme.mode == guikit::ThemeMode::Light)
                ? guikit::Color(148, 163, 184, 255)
                : guikit::Color(71, 85, 105, 255);
            if (auto windowFrame = ui.Box({
                .gap = 0.0f,
                .widthMode = guikit::SizeMode::Stretch,
                .heightMode = guikit::SizeMode::Stretch,
                .padding = guikit::Padding::All(0.0f),
                .background = {demoCtx.previewTheme.palette.surface},
                .border = {.width = 1.5f, .radius = 10.0f, .color = {winBorderCol}},
                .clipChildren = true
            })) {
                const guikit::Color titlebarBg = (demoCtx.previewTheme.mode == guikit::ThemeMode::Light)
                    ? guikit::Color(241, 245, 249, 255)
                    : guikit::Color(15, 23, 42, 255);
                if (auto titlebar = ui.Box({
                    .widthMode = guikit::SizeMode::Stretch,
                    .heightMode = guikit::SizeMode::Fixed,
                    .fixedHeight = 28.0f,
                    .padding = guikit::Padding::Symmetric(12.0f, 0.0f),
                    .background = {titlebarBg}
                })) {
                    titlebar.GetNode()->postRenderFn = &detail::RenderWindowTitlebarTrafficLights;
                    if (auto titleRow = ui.Row({
                        .align = guikit::Alignment::Center,
                        .heightMode = guikit::SizeMode::Stretch,
                        .theme = &demoCtx.previewTheme
                    })) {
                        ui.Spacer();
                        ui.Text(CategoryDisplayTitle(selectedCategory), {
                            .role = guikit::TextRole::LabelSmall,
                            .variant = guikit::UiVariant::Secondary
                        });
                        ui.Spacer();
                        const int clientW = std::max(0, static_cast<int>(std::round(availPxW / effectiveZoom)));
                        const int clientH = std::max(0, static_cast<int>(std::round((availPxH - 28.0f * shellScale) / effectiveZoom)));
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
                    .background = {demoCtx.previewTheme.palette.surface},
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
                chassisBox.GetNode()->postRenderFn = &detail::RenderDeviceChassisButtons;

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
                    // 1. Top Safe Area (9:41 Clock + Dynamic Island / Camera + Signal/Battery)
                    if (auto safeTop = ui.Box({
                        .widthMode = guikit::SizeMode::Stretch,
                        .heightMode = guikit::SizeMode::Fixed,
                        .fixedHeight = isPhone ? 30.0f : 24.0f,
                        .background = {demoCtx.previewTheme.palette.surface},
                        .theme = &demoCtx.previewTheme
                    })) {
                        safeTop.GetNode()->postRenderFn = isPhone
                            ? &detail::RenderDeviceStatusSafeArea<true>
                            : &detail::RenderDeviceStatusSafeArea<false>;
                    }

                    // 2. Isolated Application Client Area
                    if (auto clientBox = ui.Box({
                        .widthMode = guikit::SizeMode::Stretch,
                        .heightMode = guikit::SizeMode::Stretch,
                        .padding = guikit::Padding::All(devicePad),
                        .background = {demoCtx.previewTheme.palette.surface},
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
                        .background = {demoCtx.previewTheme.palette.surface},
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
