#include "guikit/core.hpp"
#include "keel/io.h"

namespace {

/// Minimal custom `IPlatformWindow` blueprint (zero SDL3, zero ThorVG, zero OS dependencies).
/// Replace `Create`, `PumpEvents`, `MakeContextCurrent`, `SwapBuffers`, and `GetGlProcAddressFn`
/// with X11/EGL, Wayland/EGL, or Win32/WGL calls to plug a native platform surface into `MainWindow`.
class HeadlessPlatformWindow final : public guikit::IPlatformWindow {
public:
    explicit HeadlessPlatformWindow(keel::i32 maxFrames) noexcept
        : m_maxFrames(maxFrames) {
    }

    bool Create(
        const guikit::PlatformWindowDesc& desc,
        guikit::Context& /*ui*/,
        guikit::WindowHostCallbacks callbacks
    ) override {
        m_width = desc.width;
        m_height = desc.height;
        m_vsync = desc.vsync;
        m_callbacks = keel::move(callbacks);
        return true;
    }

    void Destroy() noexcept override {
        m_callbacks = {};
    }

    bool PumpEvents(guikit::Context& /*ui*/, bool /*reactiveMode*/, keel::i32 /*sleepTimeoutMs*/) override {
        if (m_framesPumped >= m_maxFrames) {
            if (m_callbacks.onCloseRequested) {
                (void)m_callbacks.onCloseRequested();
            }
            return false;
        }
        ++m_framesPumped;
        return true;
    }

    void SwapBuffers() noexcept override {
        ++m_swapCount;
    }

    [[nodiscard]] keel::i32 GetFramebufferWidth() const noexcept override { return m_width; }
    [[nodiscard]] keel::i32 GetFramebufferHeight() const noexcept override { return m_height; }
    [[nodiscard]] bool IsVsyncEnabled() const noexcept override { return m_vsync; }
    void SetVsyncEnabled(bool enabled) noexcept override { m_vsync = enabled; }
    [[nodiscard]] keel::StringRef GetPlatformName() const noexcept override { return "Headless"; }

    [[nodiscard]] keel::i32 GetSwapCount() const noexcept { return m_swapCount; }

private:
    keel::i32 m_maxFrames{3};
    keel::i32 m_framesPumped{0};
    keel::i32 m_swapCount{0};
    keel::i32 m_width{800};
    keel::i32 m_height{600};
    bool m_vsync{true};
    guikit::WindowHostCallbacks m_callbacks{};
};

} // namespace

int main() {
    auto platformWindow = keel::make_unique<HeadlessPlatformWindow>(3);
    auto* rawWindow = platformWindow.get();

    auto backend = keel::make_unique<guikit::PlatformWindowBackend>(
        keel::move(platformWindow),
        [](guikit::IPlatformWindow& /*win*/) -> keel::UniquePtr<guikit::ICanvas> {
            return keel::make_unique<guikit::NullCanvas>();
        },
        guikit::GraphicsApi::None
    );

    guikit::MainWindowConfig cfg{};
    cfg.title = "GuiKit Headless Custom Backend";
    cfg.width = 800;
    cfg.height = 600;
    cfg.reactiveMode = false;

    keel::i32 renderedFrames = 0;
    guikit::MainWindow app(cfg, keel::move(backend));
    const keel::i32 exitCode = app.Run([&](guikit::Context& ui) {
        ++renderedFrames;
        if (auto col = ui.Column({.gap = 8.0f, .padding = guikit::Padding::All(16.0f)})) {
            ui.Heading({.label = "GuiKit Headless Platform Blueprint"});
            ui.Text(ui.Format("Frame #{} rendered via PlatformWindowBackend + NullCanvas", renderedFrames));
            (void)ui.Button({.text = "Action"});
        }
    });

    keel::println(
        "[guikit_headless] exitCode={}, renderedFrames={}, swaps={}",
        exitCode,
        renderedFrames,
        rawWindow->GetSwapCount()
    );
    return (exitCode == 0 && renderedFrames == 3 && rawWindow->GetSwapCount() >= 1) ? 0 : 1;
}
