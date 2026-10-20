#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <vector>
#include "guikit/GuiKit.hpp"
#include "guikit/backend/ThorCanvas.hpp"

int main(int /*argc*/, char* /*argv*/[]) {
    if (!SDL_Init(SDL_INIT_VIDEO)) return 1;

    int winW = 720, winH = 480;
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    if (!SDL_CreateWindowAndRenderer("GuiKit Minimal Quickstart", winW, winH,
                                     SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY,
                                     &window, &renderer)) {
        SDL_Quit();
        return 1;
    }
    SDL_SetRenderVSync(renderer, 1);

    guikit::Context ui;
    guikit::ThorCanvas canvas;
    std::vector<uint32_t> pixels(static_cast<size_t>(winW * winH), 0);
    canvas.SetTarget(pixels.data(), winW, winH, winW);

    SDL_Texture* tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ABGR8888,
                                         SDL_TEXTUREACCESS_STREAMING, winW, winH);
    SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_NEAREST);

    bool running = true;
    bool autopilot = true;
    float thrustPct = 72.5f;
    int pulseCount = 0;

    while (running) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_EVENT_QUIT) running = false;
            else if (ev.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
                winW = std::max(1, ev.window.data1);
                winH = std::max(1, ev.window.data2);
                pixels.assign(static_cast<size_t>(winW * winH), 0);
                canvas.SetTarget(pixels.data(), winW, winH, winW);
                SDL_DestroyTexture(tex);
                tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ABGR8888,
                                        SDL_TEXTUREACCESS_STREAMING, winW, winH);
                SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_NEAREST);
                ui.InvalidateAll();
            } else if (ev.type == SDL_EVENT_MOUSE_MOTION) {
                ui.OnMouseMove({ev.motion.x, ev.motion.y});
            } else if (ev.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
                ui.OnMouseDown(ev.button.button);
            } else if (ev.type == SDL_EVENT_MOUSE_BUTTON_UP) {
                ui.OnMouseUp(ev.button.button);
            } else if (ev.type == SDL_EVENT_MOUSE_WHEEL) {
                ui.OnMouseWheel(ev.wheel.x, ev.wheel.y);
            }
        }

        const float dpi = SDL_GetWindowDisplayScale(window);
        ui.BeginFrame({static_cast<float>(winW), static_cast<float>(winH)}, dpi > 0.0f ? dpi : 1.0f);

        if (auto root = ui.Column({
            .padding = guikit::Padding::All(24.0f),
            .align = guikit::Alignment::Center,
            .justify = guikit::Justify::Center,
            .widthMode = guikit::SizeMode::Stretch,
            .heightMode = guikit::SizeMode::Stretch
        })) {
            if (auto card = ui.Card({
                .minWidth = 420.0f,
                .gap = 12.0f,
                .padding = guikit::Padding::All(20.0f),
                .widthMode = guikit::SizeMode::Fit
            })) {
                ui.Heading({
                    .label = "Mission Control",
                    .description = "GuiKit C++20 Immediate-Mode Vector UI",
                    .icon = guikit::Icon::Play(),
                    .divider = true
                });

                if (auto el = ui.Element({
                    .label = "Autopilot Loop",
                    .description = "Closed-loop attitude stabilization"
                })) {
                    ui.Switch(autopilot);
                }

                if (auto el = ui.Element({.label = "Main Thruster Output"})) {
                    ui.Slider(thrustPct, 0.0f, 100.0f, {.valueFormat = "%.1f %%"});
                }

                if (auto row = ui.Row({.gap = 8.0f, .align = guikit::Alignment::Center})) {
                    if (ui.Button({
                        .text = "Trigger Telemetry Pulse",
                        .icon = guikit::Icon::Flash(),
                        .color = guikit::StateColor::Primary()
                    })) {
                        ++pulseCount;
                    }
                    ui.Spacer();
                    ui.Text(ui.Format("Pulses: {}", pulseCount), {
                        .role = guikit::TextRole::CodeMedium,
                        .variant = guikit::UiVariant::Info
                    });
                }
            }
        }

        ui.EndFrame(canvas);

        if (ui.HasDirtyDamage()) {
            SDL_UpdateTexture(tex, nullptr, pixels.data(), winW * static_cast<int>(sizeof(uint32_t)));
            SDL_RenderClear(renderer);
            SDL_RenderTexture(renderer, tex, nullptr, nullptr);
            SDL_RenderPresent(renderer);
        } else {
            SDL_Delay(4);
        }
    }

    SDL_DestroyTexture(tex);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
