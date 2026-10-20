#pragma once

#include <any>
#include <chrono>
#include <functional>
#include <initializer_list>
#include <optional>
#include <span>
#include <source_location>
#include <string_view>
#include <type_traits>
#include <typeindex>
#include <unordered_map>
#include <vector>
#include "guikit/backend/ICanvas.hpp"
#include "guikit/core/Allocator.hpp"
#include "guikit/core/Id.hpp"
#include "guikit/core/Response.hpp"
#include "guikit/core/Theme.hpp"
#include "guikit/core/Tokens.hpp"
#include "guikit/core/Types.hpp"
#include "guikit/core/UiString.hpp"
#include "guikit/graphics/Icon.hpp"
#include "guikit/core/Solver.hpp"
#include "guikit/core/Node.hpp"
#include "guikit/core/ContainerScope.hpp"
#include "guikit/layout/Row.hpp"
#include "guikit/layout/Column.hpp"
#include "guikit/layout/Wrap.hpp"
#include "guikit/layout/Grid.hpp"
#include "guikit/layout/Spacer.hpp"
#include "guikit/panels/Accordion.hpp"
#include "guikit/panels/ActivityBar.hpp"
#include "guikit/panels/BottomPanel.hpp"
#include "guikit/panels/Box.hpp"
#include "guikit/panels/Card.hpp"
#include "guikit/panels/Carousel.hpp"
#include "guikit/panels/ScrollArea.hpp"
#include "guikit/panels/Scene.hpp"
#include "guikit/panels/MapView.hpp"
#include "guikit/panels/ContextMenu.hpp"
#include "guikit/panels/Modal.hpp"
#include "guikit/panels/Sidebar.hpp"
#include "guikit/panels/StatusBar.hpp"
#include "guikit/panels/Table.hpp"
#include "guikit/panels/Toolbar.hpp"
#include "guikit/panels/TreeView.hpp"
#include "guikit/widgets/Button.hpp"
#include "guikit/widgets/Checkbox.hpp"
#include "guikit/widgets/ColorPicker.hpp"
#include "guikit/widgets/Heading.hpp"
#include "guikit/widgets/Image.hpp"
#include "guikit/widgets/InputField.hpp"
#include "guikit/widgets/Knob.hpp"
#include "guikit/widgets/Markdown.hpp"
#include "guikit/widgets/Radio.hpp"
#include "guikit/widgets/Segmented.hpp"
#include "guikit/widgets/Select.hpp"
#include "guikit/widgets/SidebarMenu.hpp"
#include "guikit/widgets/Slider.hpp"
#include "guikit/widgets/SpinBox.hpp"
#include "guikit/widgets/Switch.hpp"
#include "guikit/widgets/TabBar.hpp"
#include "guikit/widgets/Text.hpp"
#include "guikit/widgets/TextArea.hpp"
#include "guikit/widgets/Toast.hpp"
#include "guikit/widgets/SignalView.hpp"
#include "guikit/widgets/SonarView.hpp"
#include "guikit/widgets/WaterfallView.hpp"
#include "guikit/graphics/Colormap.hpp"
#include "guikit/widgets/Histogram.hpp"
#include "guikit/widgets/ImageView.hpp"
#include "guikit/widgets/AttitudeView.hpp"
#include "guikit/widgets/BpmnEditor.hpp"
#include "guikit/widgets/CarClusterView.hpp"

namespace guikit {

class Context;
class SizeScope;

/// C++20 concept for any autonomous widget providing a static Run method.
template <typename T, typename... Args>
concept WidgetRunnable = requires(Context& ctx, Args&&... args) {
    { T::Run(ctx, std::forward<Args>(args)...) };
};

/// C++20 concept for any autonomous layout or panel container providing a static Open method.
template <typename T, typename... Args>
concept ContainerOpenable = requires(Context& ctx, Args&&... args) {
    { T::Open(ctx, std::forward<Args>(args)...) };
};

template <typename T>
concept SpinBoxNumeric =
    std::is_same_v<std::decay_t<T>, float> ||
    std::is_same_v<std::decay_t<T>, double> ||
    std::is_same_v<std::decay_t<T>, int>;

template <typename T>
concept SelectTarget =
    std::is_same_v<std::decay_t<T>, size_t> ||
    std::is_same_v<std::decay_t<T>, int> ||
    std::is_same_v<std::decay_t<T>, std::string> ||
    std::is_same_v<std::decay_t<T>, std::string_view>;

template <typename T>
concept SliderNumeric = std::integral<std::decay_t<T>> || std::floating_point<std::decay_t<T>>;

namespace detail {

struct StateKey {
    Id id;
    std::type_index type;

    bool operator==(const StateKey& other) const noexcept {
        return id == other.id && type == other.type;
    }
};

struct StateKeyHash {
    size_t operator()(const StateKey& k) const noexcept {
        size_t h1 = std::hash<Id>{}(k.id);
        size_t h2 = k.type.hash_code();
        return h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
    }
};

struct StoredState {
    std::any value;
    uint64_t lastFrameActive{0};
};

} // namespace detail

/// Primary immediate mode context coordinating input, layout, and rendering.
class Context {
public:
    Context();
    ~Context() = default;

    Context(const Context&) = delete;
    Context& operator=(const Context&) = delete;

    // --- Frame Lifecycle ---
    void BeginFrame(Vec2 viewportSize, float dpiScale = 1.0f);
    void EndFrame(ICanvas& canvas);

    // --- Input Handling ---
    void OnMouseMove(Vec2 pos);
    void OnMouseDown(int button);
    void OnMouseUp(int button);
    void OnTextInput(std::string_view utf8Text);
    void OnKeyDown(Key key, KeyModifiers mods = {});
    void OnMouseWheel(float x, float y);

    // --- Reactive Repaint & Idle Management ---
    /// Request the event loop to repaint a given number of frames (e.g. 3 frames to settle hover/click).
    void RequestRepaint(int frames = 3) noexcept;

    /// Returns the number of pending repaint frames left in the counter.
    [[nodiscard]] int GetRepaintFramesLeft() const noexcept { return m_repaintFrames; }

    /// Returns true if the UI requires a repaint (pending frames, mouse held down, or active edit/popup).
    [[nodiscard]] bool NeedsRepaint() const noexcept;

    /// Sets the idle heartbeat interval in milliseconds (e.g. 500ms). Set to -1 or 0 to disable periodic heartbeat (deep sleep).
    void SetIdleHeartbeatMs(int timeoutMs) noexcept { m_idleHeartbeatMs = timeoutMs; }

    /// Returns the configured idle heartbeat interval in milliseconds.
    [[nodiscard]] int GetIdleHeartbeatMs() const noexcept { return m_idleHeartbeatMs; }

    /// Recommended timeout in milliseconds for SDL_WaitEventTimeout in reactive mode.
    [[nodiscard]] int GetRecommendedSleepTimeoutMs() const noexcept;

    // --- Dirty Rect & Damage Tracking ---
    /// Enables or disables dirty damage rect tracking (enabled by default).
    void SetDirtyRectEnabled(bool enabled) noexcept { m_dirtyRectEnabled = enabled; }

    /// Returns whether dirty damage rect tracking is currently enabled.
    [[nodiscard]] bool IsDirtyRectEnabled() const noexcept { return m_dirtyRectEnabled; }

    // --- Window Live Resize Policy ---
    /// Enables or disables live UI reflow and redraw during interactive OS window border dragging (enabled by default).
    /// When false (deferred resize), the UI updates after the OS resize gesture step completes.
    void SetLiveResize(bool enabled) noexcept { m_liveResizeEnabled = enabled; }

    /// Returns whether live window resize is currently enabled.
    [[nodiscard]] bool IsLiveResizeEnabled() const noexcept { return m_liveResizeEnabled; }

    /// Sets the active color theme mode (Dark or Light) and adapts clear color & palette.
    void SetThemeMode(ThemeMode mode) noexcept;

    /// Returns the current theme mode.
    [[nodiscard]] ThemeMode GetThemeMode() const noexcept { return m_theme.mode; }

    /// Returns true if dark mode is active.
    [[nodiscard]] bool IsDarkMode() const noexcept { return m_theme.mode == ThemeMode::Dark; }

    /// Returns the active theme palette tokens.
    [[nodiscard]] const ThemePalette& GetThemePalette() const noexcept { return m_theme.palette; }

    /// Sets the full UI theme (palette, variants, metrics, typography, etc.).
    void SetTheme(const UiTheme& theme) noexcept {
        m_theme = theme;
        m_clearColor = m_theme.palette.background;
        InvalidateAll();
    }

    /// Returns the active UI theme (or top of theme scope stack if scoped).
    [[nodiscard]] const UiTheme& GetTheme() const noexcept {
        return m_themeStack.empty() ? m_theme : *m_themeStack.back();
    }
    [[nodiscard]] UiTheme& GetTheme() noexcept {
        return m_themeStack.empty() ? m_theme : *m_themeStack.back();
    }

    /// Sets the clear color used by EndFrame when clearing dirty damage regions.
    void SetClearColor(Color color) noexcept { m_clearColor = color; }

    /// Returns the current clear color.
    [[nodiscard]] Color GetClearColor() const noexcept { return m_clearColor; }


    /// Sets the UI density preset (Standard, Compact, Touch).
    void SetDensity(Density density) noexcept { m_theme.SetDensity(density); InvalidateAll(); }

    /// Returns the current UI density preset.
    [[nodiscard]] Density GetDensity() const noexcept { return m_theme.density; }

    // --- Developer Debug & Inspector Tools ---
    /// Sets whether 1px color-coded debug bounding boxes and padding overlays are rendered.
    void SetDebugBounds(bool enabled) noexcept;
    [[nodiscard]] bool IsDebugBounds() const noexcept { return m_debugBoundsEnabled; }

    /// Sets whether the interactive element inspector (Chrome DevTools style) is active.
    void SetInspectMode(bool enabled) noexcept;
    [[nodiscard]] bool IsInspectMode() const noexcept { return m_inspectModeEnabled; }

    /// Returns true if this frame produced any visual damage requiring rasterization / blitting.
    [[nodiscard]] bool HasDirtyDamage() const noexcept { return m_hasDirtyDamage; }

    /// Returns true if the entire frame required a full repaint (e.g. window resize or tab switch).
    [[nodiscard]] bool IsFullRepaint() const noexcept { return m_isFullRepaint; }

    /// Returns the active bounding rectangle of the dirty damage region for this frame (in logical points).
    [[nodiscard]] Rect GetDirtyRect() const noexcept { return m_dirtyRect; }

    /// Returns the list of mutually disjoint dirty damage rectangles for this frame (in logical points).
    [[nodiscard]] const std::vector<Rect>& GetDirtyRects() const noexcept { return m_dirtyRects; }

    /// Returns the active bounding rectangle of the dirty damage region in physical pixels for GPU blitting.
    [[nodiscard]] Rect GetPhysicalDirtyRect() const noexcept {
        const float x1 = std::floor(m_dirtyRect.x * m_dpiScale);
        const float y1 = std::floor(m_dirtyRect.y * m_dpiScale);
        const float x2 = std::ceil(m_dirtyRect.Right() * m_dpiScale);
        const float y2 = std::ceil(m_dirtyRect.Bottom() * m_dpiScale);
        return Rect(x1, y1, std::max(0.0f, x2 - x1), std::max(0.0f, y2 - y1));
    }

    /// Returns the active DPI / UI scale factor.
    [[nodiscard]] float GetDpiScale() const noexcept { return m_dpiScale; }

    /// Returns the current logical viewport size used for layout.
    [[nodiscard]] Vec2 GetLogicalViewport() const noexcept {
        return m_viewportSize / (m_dpiScale > 0.0f ? m_dpiScale : 1.0f);
    }

    /// Explicitly forces the next frame to perform a full screen redraw.
    void InvalidateAll() noexcept;

    /// Explicitly invalidates a rectangular region to be repainted in the next frame.
    void InvalidateRect(Rect rect) noexcept;

    // --- Generic Autonomous Widget & Container Dispatch (Open-Closed Platform) ---
    /// Dispatches execution to any autonomous widget component satisfying WidgetRunnable.
    template <typename WidgetT, typename... Args>
        requires WidgetRunnable<WidgetT, Args...>
    decltype(auto) Run(Args&&... args) {
        struct ExecGuard {
            Context& c;
            ~ExecGuard() noexcept { c.EndWidgetExecution(); }
        } guard{*this};
        BeginWidgetExecution();
        return WidgetT::Run(*this, std::forward<Args>(args)...);
    }

    /// Dispatches opening to any autonomous layout or panel container satisfying ContainerOpenable.
    template <typename ContainerT, typename... Args>
        requires ContainerOpenable<ContainerT, Args...>
    [[nodiscard]] decltype(auto) Open(Args&&... args) {
        return ContainerT::Open(*this, std::forward<Args>(args)...);
    }

    // --- Layout Scopes ---
    [[nodiscard]] ContainerScope Row(
        RowOptions opts = {},
        std::source_location loc = std::source_location::current()
    );

    [[nodiscard]] ContainerScope Column(
        ColumnOptions opts = {},
        std::source_location loc = std::source_location::current()
    );

    [[nodiscard]] ContainerScope Wrap(
        WrapOptions opts = {},
        std::source_location loc = std::source_location::current()
    );

    [[nodiscard]] GridScope Grid(
        GridOptions opts = {},
        std::source_location loc = std::source_location::current()
    );

    void Spacer(
        float flex = 1.0f,
        std::source_location loc = std::source_location::current()
    );

    // --- Panel Scopes ---
    [[nodiscard]] AccordionScope Accordion(
        AccordionOptions opts = {},
        std::source_location loc = std::source_location::current()
    );

    [[nodiscard]] AccordionScope Accordion(
        std::string_view label,
        AccordionOptions opts = {},
        std::source_location loc = std::source_location::current()
    );

    [[nodiscard]] AccordionScope Accordion(
        bool& open,
        AccordionOptions opts = {},
        std::source_location loc = std::source_location::current()
    );

    template <typename T, typename U>
        requires (!std::is_same_v<std::decay_t<T>, bool> && requires(T& a, const U& b) {
            { a == b } -> std::convertible_to<bool>;
            a = static_cast<T>(b);
        })
    [[nodiscard]] AccordionScope Accordion(
        T& activeValue,
        const U& sectionValue,
        AccordionOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        bool isOpen = (activeValue == sectionValue);
        auto scope = guikit::Accordion::Open(*this, &isOpen, opts, loc);
        if (scope.response.changed && isOpen) {
            activeValue = static_cast<T>(sectionValue);
        }
        return scope;
    }

    [[nodiscard]] ContainerScope Box(
        BoxOptions opts = {},
        std::source_location loc = std::source_location::current()
    );

    [[nodiscard]] ContainerScope Card(
        CardOptions opts = {},
        std::source_location loc = std::source_location::current()
    );

    [[nodiscard]] ContainerScope ScrollArea(
        ScrollAreaOptions opts = {},
        std::source_location loc = std::source_location::current()
    );

    [[nodiscard]] ContainerScope Carousel(
        CarouselOptions opts = {},
        std::source_location loc = std::source_location::current()
    );

    [[nodiscard]] SidebarScope Sidebar(
        SidebarOptions opts = {},
        std::source_location loc = std::source_location::current()
    );

    [[nodiscard]] SidebarScope Sidebar(
        SidebarMode& mode,
        SidebarOptions opts = {},
        std::source_location loc = std::source_location::current()
    );

    [[nodiscard]] SidebarScope Sidebar(
        bool& open,
        SidebarOptions opts = {},
        std::source_location loc = std::source_location::current()
    );

    [[nodiscard]] ActivityBarScope ActivityBar(
        ActivityBarOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return guikit::ActivityBar::Open(*this, opts, loc);
    }

    [[nodiscard]] ActivityBarScope ActivityBar(
        SidebarMode& linkedSidebarMode,
        ActivityBarOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return guikit::ActivityBar::Open(*this, linkedSidebarMode, std::move(opts), loc);
    }

    Response ActivityBar(
        int& activeIndex,
        std::span<const ActivityBarItem> items,
        ActivityBarOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        struct ExecGuard {
            Context& c;
            ~ExecGuard() noexcept { c.EndWidgetExecution(); }
        } guard{*this};
        BeginWidgetExecution();
        return guikit::ActivityBar::Run(*this, activeIndex, items, opts, loc);
    }

    Response ActivityBar(
        int& activeIndex,
        std::initializer_list<ActivityBarItem> items,
        ActivityBarOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return ActivityBar(activeIndex, std::span<const ActivityBarItem>(items.begin(), items.size()), std::move(opts), loc);
    }

    [[nodiscard]] BottomPanelScope BottomPanel(
        BottomPanelOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return guikit::BottomPanel::Open(*this, opts, loc);
    }

    [[nodiscard]] BottomPanelScope BottomPanel(
        BottomPanelMode& mode,
        BottomPanelOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return guikit::BottomPanel::Open(*this, &mode, opts, loc);
    }

    [[nodiscard]] BottomPanelScope BottomPanel(
        BottomPanelMode* mode,
        BottomPanelOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return guikit::BottomPanel::Open(*this, mode, opts, loc);
    }

    [[nodiscard]] BottomPanelScope BottomPanel(
        bool& open,
        BottomPanelOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return guikit::BottomPanel::Open(*this, &open, opts, loc);
    }

    [[nodiscard]] BottomPanelScope BottomPanel(
        bool* open,
        BottomPanelOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return guikit::BottomPanel::Open(*this, open, opts, loc);
    }

    [[nodiscard]] StatusBarScope StatusBar(
        StatusBarOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return guikit::StatusBar::Open(*this, opts, loc);
    }

    [[nodiscard]] SceneScope Scene(
        SceneOptions opts = {},
        std::source_location loc = std::source_location::current()
    );

    [[nodiscard]] MapViewScope MapView(
        MapViewOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return Open<guikit::MapView>(opts, loc);
    }

    [[nodiscard]] TableScope Table(
        std::span<const TableColumn> columns,
        const TableOptions& opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return guikit::Table::Open(*this, columns, opts, loc);
    }

    [[nodiscard]] TableScope Table(
        std::initializer_list<TableColumn> columns,
        const TableOptions& opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return guikit::Table::Open(*this, std::span<const TableColumn>(columns.begin(), columns.size()), opts, loc);
    }

    [[nodiscard]] TreeViewScope TreeView(
        TreeViewOptions opts = {},
        std::source_location loc = std::source_location::current()
    );

    [[nodiscard]] TreeViewScope TreeView(
        std::span<const TableColumn> columns,
        TreeViewOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        opts.columns = columns;
        return TreeView(std::move(opts), loc);
    }

    [[nodiscard]] TreeViewScope TreeView(
        std::initializer_list<TableColumn> columns,
        TreeViewOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        opts.columns = std::span<const TableColumn>(columns.begin(), columns.size());
        return TreeView(std::move(opts), loc);
    }

    [[nodiscard]] TreeItemScope TreeItem(
        TreeItemOptions opts = {},
        std::source_location loc = std::source_location::current()
    );

    [[nodiscard]] TreeItemScope TreeItem(
        std::string_view label,
        TreeItemOptions opts = {},
        std::source_location loc = std::source_location::current()
    );

    [[nodiscard]] TreeItemScope TreeItem(
        bool& open,
        TreeItemOptions opts = {},
        std::source_location loc = std::source_location::current()
    );

    [[nodiscard]] TreeItemScope TreeItem(
        bool& open,
        std::string_view label,
        TreeItemOptions opts = {},
        std::source_location loc = std::source_location::current()
    );

    // --- Scale & State Scopes ---
    void PushSize(UiSize size);
    void PopSize();
    void ReplaceTopSize(UiSize size) noexcept {
        if (!m_sizeStack.empty()) {
            m_sizeStack.back() = size;
        }
    }
    [[nodiscard]] UiSize GetCurrentSize() const;

    [[nodiscard]] SizeScope Size(UiSize size) noexcept;

    template<typename Fn>
    void Size(UiSize size, Fn&& fn) {
        PushSize(size);
        fn();
        PopSize();
    }

    void PushDisabled(bool disabled = true);
    void PopDisabled();
    [[nodiscard]] bool IsDisabled() const;

    template<typename Fn>
    void Disabled(bool disabled, Fn&& fn) {
        PushDisabled(disabled);
        fn();
        PopDisabled();
    }

    // --- Theme & Scale Scopes ---
    UiTheme* PushTheme(const UiTheme& theme);
    UiTheme* PushScopedTheme(const UiTheme* explicitTheme, std::optional<float> explicitScale);
    void PopTheme();

    void PushScale(float scale);
    void PopScale();
    [[nodiscard]] float GetCurrentScale() const noexcept {
        return m_scaleStack.empty() ? 1.0f : m_scaleStack.back();
    }

    template<typename Fn>
    void Theme(const UiTheme& theme, Fn&& fn) {
        PushTheme(theme);
        fn();
        PopTheme();
    }

    template<typename Fn>
    void Scale(float scale, Fn&& fn) {
        PushScale(scale);
        fn();
        PopScale();
    }

    // --- Widgets ---
    Response Button(
        ButtonOptions opts = {},
        std::source_location loc = std::source_location::current()
    );

    Response Text(
        UiText label,
        TextOptions opts = {},
        std::source_location loc = std::source_location::current()
    );


    // --- Element / Heading Widgets ---
    ElementScope Element(
        ElementOptions opts = {},
        std::source_location loc = std::source_location::current()
    );

    Response Heading(
        HeadingOptions opts = {},
        std::source_location loc = std::source_location::current()
    );

    // --- Image Widget ---
    Response Image(
        std::string_view source,
        ImageOptions opts = {},
        std::source_location loc = std::source_location::current()
    );

    Response Image(
        ImageOptions opts,
        std::source_location loc = std::source_location::current()
    );

    // --- Markdown Document Viewer Widget ---
    MarkdownResponse Markdown(
        std::string_view markdownText,
        const MarkdownOptions& opts = {},
        std::source_location loc = std::source_location::current()
    );

    // --- SignalView (Waveform + Spectrogram DAW Analyzer) Widget ---
    SignalViewResponse SignalView(
        std::span<const float> samples,
        const SignalViewOptions& opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return Run<guikit::SignalView>(samples, opts, loc);
    }

    // --- SonarView (Polar Heatmap / Diver Detection Sonar / PPI Radar) Widget ---
    SonarViewResponse SonarView(
        std::span<const float> polarMatrix,
        const SonarViewOptions& opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return Run<guikit::SonarView>(polarMatrix, opts, loc);
    }

    // --- WaterfallView (RTSA Spectrum Analyzer & DOA / BTR Target Tracker) Widget ---
    WaterfallViewResponse WaterfallView(
        std::span<const float> latestSliceDb,
        const WaterfallViewOptions& opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return Run<guikit::WaterfallView>(latestSliceDb, opts, loc);
    }

    // --- Histogram (Single-Channel & Multi-Channel RGB+Lum Window/Level) Widget ---
    HistogramResponse Histogram(
        const HistogramData& data,
        ChannelTransferParams& transfer,
        const HistogramOptions& opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return Run<guikit::Histogram>(data, transfer, opts, loc);
    }

    // --- ImageView (Single-Channel 16-bit/Float & Multi-Channel RGB Workstation) Widget ---
    ImageViewResponse ImageView(
        const ImageViewData& data,
        ImageViewParams& params,
        const ImageViewOptions& opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return Run<guikit::ImageView>(data, params, opts, loc);
    }

    ImageViewResponse ImageView(
        const ImageViewData& data,
        const ImageViewOptions& opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return Run<guikit::ImageView>(data, opts, loc);
    }

    // --- AttitudeView (Avionics Artificial Horizon & Primary Flight Display) Widget ---
    AttitudeViewResponse AttitudeView(
        const AttitudeViewOptions& opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return Run<guikit::AttitudeView>(opts, loc);
    }

    // --- CarClusterView (Automotive Digital Instrument Cluster & ADAS Cockpit) Widget ---
    CarClusterViewResponse CarClusterView(
        const CarClusterViewOptions& opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return Run<guikit::CarClusterView>(opts, loc);
    }

    // --- BpmnEditor (BPMN 2.0 Diagram Modeler & Token Simulation) Widget ---
    BpmnEditorResponse BpmnEditor(
        BpmnDiagram& diagram,
        const BpmnEditorOptions& opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return Run<guikit::BpmnEditor>(diagram, opts, loc);
    }

    // --- Switch Widget ---
    Response Switch(
        bool& value,
        SwitchOptions opts = {},
        std::source_location loc = std::source_location::current()
    );

    Response Switch(
        const bool& checked,
        SwitchOptions opts = {},
        std::source_location loc = std::source_location::current()
    );

    Response Switch(const bool*, SwitchOptions = {}, std::source_location = std::source_location::current()) = delete;

    // --- Checkbox & Radio Widgets ---
    Response Checkbox(
        bool& value,
        CheckboxOptions opts = {},
        std::source_location loc = std::source_location::current()
    );

    Response Checkbox(
        const bool& checked,
        CheckboxOptions opts = {},
        std::source_location loc = std::source_location::current()
    );

    Response Checkbox(const bool*, CheckboxOptions = {}, std::source_location = std::source_location::current()) = delete;

    template <typename T, typename U = T>
        requires (std::equality_comparable_with<T, U>)
    Response Radio(
        T& currentValue,
        const U& optionValue,
        RadioOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        const bool isSelected = (currentValue == optionValue);
        bool state = isSelected;
        Response r = Run<guikit::Radio>(&state, isSelected, opts, loc);
        if (r.clicked && !isSelected) {
            currentValue = static_cast<T>(optionValue);
            r.changed = true;
        } else {
            r.changed = false;
        }
        return r;
    }

    Response Radio(
        bool& value,
        RadioOptions opts = {},
        std::source_location loc = std::source_location::current()
    );

    Response Radio(
        const bool& checked,
        RadioOptions opts = {},
        std::source_location loc = std::source_location::current()
    );

    Response Radio(const bool*, RadioOptions = {}, std::source_location = std::source_location::current()) = delete;

    template <typename T, typename U = T>
        requires (std::equality_comparable_with<T, U>)
    Response RadioButton(
        T& currentValue,
        const U& optionValue,
        RadioOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return Radio(currentValue, optionValue, std::move(opts), loc);
    }

    Response RadioButton(
        bool& checked,
        RadioOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return Radio(checked, std::move(opts), loc);
    }

    // --- InputField & SpinBox Widgets ---
    Response InputField(
        std::string& value,
        InputFieldOptions opts = {},
        std::source_location loc = std::source_location::current()
    );

    Response InputField(
        std::string_view value,
        InputFieldOptions opts = {},
        std::source_location loc = std::source_location::current()
    );

    template <SpinBoxNumeric T>
    Response SpinBox(
        T& value,
        SpinBoxOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        if constexpr (std::is_same_v<std::decay_t<T>, float>) {
            return Run<guikit::SpinBox>(&value, nullptr, nullptr, opts, loc);
        } else if constexpr (std::is_same_v<std::decay_t<T>, double>) {
            return Run<guikit::SpinBox>(nullptr, &value, nullptr, opts, loc);
        } else if constexpr (std::is_same_v<std::decay_t<T>, int>) {
            return Run<guikit::SpinBox>(nullptr, nullptr, &value, opts, loc);
        }
    }

    // --- TextArea Widget ---
    TextAreaResponse TextArea(
        std::string& value,
        const TextAreaOptions& opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return Run<guikit::TextArea>(&value, std::string_view{}, opts, loc);
    }

    TextAreaResponse TextArea(
        std::string_view value,
        const TextAreaOptions& opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return Run<guikit::TextArea>(nullptr, value, opts, loc);
    }


    // --- Select (Dropdown) Widget ---
    template <SelectTarget T>
    Response Select(
        T& selected,
        std::span<const SelectItem> items,
        SelectOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        if constexpr (std::is_same_v<std::decay_t<T>, size_t>) {
            return Run<guikit::Select>(&selected, nullptr, nullptr, nullptr, items, opts, loc);
        } else if constexpr (std::is_same_v<std::decay_t<T>, int>) {
            return Run<guikit::Select>(nullptr, &selected, nullptr, nullptr, items, opts, loc);
        } else if constexpr (std::is_same_v<std::decay_t<T>, std::string>) {
            return Run<guikit::Select>(nullptr, nullptr, &selected, nullptr, items, opts, loc);
        } else if constexpr (std::is_same_v<std::decay_t<T>, std::string_view>) {
            return Run<guikit::Select>(nullptr, nullptr, nullptr, &selected, items, opts, loc);
        }
    }

    template <SelectTarget T>
    Response Select(
        T& selected,
        std::initializer_list<SelectItem> items,
        SelectOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return Select(selected, std::span<const SelectItem>(items.begin(), items.size()), opts, loc);
    }

    template <SelectTarget T>
    Response Select(
        T& selected,
        std::span<const std::string_view> items,
        SelectOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        if constexpr (std::is_same_v<std::decay_t<T>, size_t>) {
            return Run<guikit::Select>(&selected, nullptr, nullptr, nullptr, items, opts, loc);
        } else if constexpr (std::is_same_v<std::decay_t<T>, int>) {
            return Run<guikit::Select>(nullptr, &selected, nullptr, nullptr, items, opts, loc);
        } else if constexpr (std::is_same_v<std::decay_t<T>, std::string>) {
            return Run<guikit::Select>(nullptr, nullptr, &selected, nullptr, items, opts, loc);
        } else if constexpr (std::is_same_v<std::decay_t<T>, std::string_view>) {
            return Run<guikit::Select>(nullptr, nullptr, nullptr, &selected, items, opts, loc);
        }
    }

    template <SelectTarget T>
    Response Select(
        T& selected,
        std::initializer_list<std::string_view> items,
        SelectOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return Select(selected, std::span<const std::string_view>(items.begin(), items.size()), opts, loc);
    }

    // --- TabBar Component ---
    template <typename IndexT>
    Response TabBar(
        IndexT& activeIndex,
        std::span<const TabItem> tabs,
        TabBarOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) requires (std::is_integral_v<IndexT>) {
        struct ExecGuard {
            Context& c;
            ~ExecGuard() noexcept { c.EndWidgetExecution(); }
        } guard{*this};
        BeginWidgetExecution();
        int idx = static_cast<int>(activeIndex);
        Response r = TabBar::Run(*this, idx, tabs, opts, loc);
        if (r.changed && idx >= 0) activeIndex = static_cast<IndexT>(idx);
        return r;
    }

    template <typename IndexT>
    Response TabBar(
        IndexT& activeIndex,
        std::initializer_list<TabItem> tabs,
        TabBarOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) requires (std::is_integral_v<IndexT>) {
        return TabBar(activeIndex, std::span<const TabItem>(tabs.begin(), tabs.size()), opts, loc);
    }

    // --- SidebarMenu Component ---
    [[nodiscard]] SidebarMenuScope SidebarMenu(
        SidebarMenuOptions opts = {},
        std::source_location loc = std::source_location::current()
    );

    template <typename IndexT>
    Response SidebarMenu(
        IndexT& activeIndex,
        std::span<const SidebarMenuItem> items,
        SidebarMenuOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) requires (std::is_integral_v<IndexT> || std::is_enum_v<IndexT>) {
        struct ExecGuard {
            Context& c;
            ~ExecGuard() noexcept { c.EndWidgetExecution(); }
        } guard{*this};
        BeginWidgetExecution();
        int idx = static_cast<int>(activeIndex);
        Response r = SidebarMenu::Run(*this, idx, items, opts, loc);
        if (r.changed && idx >= 0) activeIndex = static_cast<IndexT>(idx);
        return r;
    }

    template <typename IndexT>
    Response SidebarMenu(
        IndexT& activeIndex,
        std::initializer_list<SidebarMenuItem> items,
        SidebarMenuOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) requires (std::is_integral_v<IndexT> || std::is_enum_v<IndexT>) {
        return SidebarMenu(activeIndex, std::span<const SidebarMenuItem>(items.begin(), items.size()), opts, loc);
    }

    // --- Slider, StepSlider, RangeSlider & ProgressBar Widgets ---
    template <SliderNumeric T>
    Response Slider(
        T& value,
        T min,
        T max,
        SliderOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        if constexpr (std::is_same_v<std::decay_t<T>, double>) {
            return Run<guikit::Slider>(&value, nullptr, nullptr, min, max, opts, loc);
        } else if constexpr (std::is_same_v<std::decay_t<T>, float>) {
            return Run<guikit::Slider>(nullptr, &value, nullptr, static_cast<double>(min), static_cast<double>(max), opts, loc);
        } else {
            int64_t val64 = static_cast<int64_t>(value);
            Response res = Run<guikit::Slider>(nullptr, nullptr, &val64, static_cast<double>(min), static_cast<double>(max), opts, loc);
            if (res.changed) {
                value = static_cast<T>(val64);
            }
            return res;
        }
    }

    template <SliderNumeric T>
    Response RangeSlider(
        T& lowValue,
        T& highValue,
        T min,
        T max,
        RangeSliderOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        if constexpr (std::is_same_v<std::decay_t<T>, double>) {
            return Run<guikit::RangeSlider>(&lowValue, &highValue, nullptr, nullptr, nullptr, nullptr, min, max, opts, loc);
        } else if constexpr (std::is_same_v<std::decay_t<T>, float>) {
            return Run<guikit::RangeSlider>(nullptr, nullptr, &lowValue, &highValue, nullptr, nullptr, static_cast<double>(min), static_cast<double>(max), opts, loc);
        } else {
            int64_t low64 = static_cast<int64_t>(lowValue);
            int64_t high64 = static_cast<int64_t>(highValue);
            Response res = Run<guikit::RangeSlider>(nullptr, nullptr, nullptr, nullptr, &low64, &high64, static_cast<double>(min), static_cast<double>(max), opts, loc);
            if (res.changed) {
                lowValue = static_cast<T>(low64);
                highValue = static_cast<T>(high64);
            }
            return res;
        }
    }

    Response ProgressBar(
        double value,
        double min,
        double max,
        ProgressBarOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return Run<guikit::ProgressBar>(value, min, max, opts, loc);
    }

    Response ProgressBar(
        double percent0to100,
        ProgressBarOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return Run<guikit::ProgressBar>(percent0to100, 0.0, 100.0, opts, loc);
    }

    template <typename IndexT>
    Response StepSlider(
        IndexT& index,
        size_t totalSteps,
        StepSliderOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) requires (std::is_integral_v<IndexT>) {
        if constexpr (std::is_same_v<std::decay_t<IndexT>, size_t>) {
            return Run<guikit::StepSlider>(&index, nullptr, totalSteps, opts, loc);
        } else if constexpr (std::is_same_v<std::decay_t<IndexT>, int>) {
            return Run<guikit::StepSlider>(nullptr, &index, totalSteps, opts, loc);
        } else {
            int idxInt = static_cast<int>(index);
            Response res = Run<guikit::StepSlider>(nullptr, &idxInt, totalSteps, opts, loc);
            if (res.changed) {
                index = static_cast<IndexT>(idxInt);
            }
            return res;
        }
    }

    template <typename IndexT>
    Response StepSlider(
        IndexT& index,
        std::span<const std::string_view> labels,
        StepSliderOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) requires (std::is_integral_v<IndexT>) {
        opts.stepLabels = labels;
        return StepSlider(index, labels.size(), opts, loc);
    }

    template <typename IndexT>
    Response StepSlider(
        IndexT& index,
        std::initializer_list<std::string_view> labels,
        StepSliderOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) requires (std::is_integral_v<IndexT>) {
        return StepSlider(index, std::span<const std::string_view>(labels.begin(), labels.size()), opts, loc);
    }

    Response StepSlider(
        UiSize& size,
        StepSliderOptions opts = {},
        std::source_location loc = std::source_location::current()
    );

    Response StepSlider(
        UiVariant& variant,
        StepSliderOptions opts = {},
        std::source_location loc = std::source_location::current()
    );

    // --- Knob Widget ---
    template <SliderNumeric T>
    Response Knob(
        T& value,
        T min,
        T max,
        KnobOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        if constexpr (std::is_same_v<std::decay_t<T>, double>) {
            return Run<guikit::Knob>(&value, nullptr, nullptr, min, max, opts, loc);
        } else if constexpr (std::is_same_v<std::decay_t<T>, float>) {
            return Run<guikit::Knob>(nullptr, &value, nullptr, static_cast<double>(min), static_cast<double>(max), opts, loc);
        } else {
            int valInt = static_cast<int>(value);
            Response res = Run<guikit::Knob>(nullptr, nullptr, &valInt, static_cast<double>(min), static_cast<double>(max), opts, loc);
            if (res.changed) {
                value = static_cast<T>(valInt);
            }
            return res;
        }
    }

    // --- Segmented Control Widget ---
    template <typename IndexT>
    Response Segmented(
        IndexT& selectedIndex,
        std::span<const SegmentedItem> items,
        SegmentedOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) requires (std::is_integral_v<IndexT> || std::is_enum_v<IndexT>) {
        if constexpr (std::is_same_v<std::decay_t<IndexT>, size_t>) {
            return Run<guikit::Segmented>(&selectedIndex, nullptr, nullptr, items, opts, loc);
        } else {
            int idx = static_cast<int>(selectedIndex);
            Response r = Run<guikit::Segmented>(nullptr, &idx, nullptr, items, opts, loc);
            if (r.changed && idx >= 0) {
                selectedIndex = static_cast<IndexT>(idx);
            }
            return r;
        }
    }

    template <typename IndexT>
    Response Segmented(
        IndexT& selectedIndex,
        std::initializer_list<SegmentedItem> items,
        SegmentedOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) requires (std::is_integral_v<IndexT> || std::is_enum_v<IndexT>) {
        return Segmented(selectedIndex, std::span<const SegmentedItem>(items.begin(), items.size()), std::move(opts), loc);
    }

    template <typename IndexT>
    Response Segmented(
        IndexT& selectedIndex,
        std::span<const std::string_view> items,
        SegmentedOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) requires (std::is_integral_v<IndexT> || std::is_enum_v<IndexT>) {
        if constexpr (std::is_same_v<std::decay_t<IndexT>, size_t>) {
            return Run<guikit::Segmented>(&selectedIndex, nullptr, nullptr, items, opts, loc);
        } else {
            int idx = static_cast<int>(selectedIndex);
            Response r = Run<guikit::Segmented>(nullptr, &idx, nullptr, items, opts, loc);
            if (r.changed && idx >= 0) {
                selectedIndex = static_cast<IndexT>(idx);
            }
            return r;
        }
    }

    template <typename IndexT>
    Response Segmented(
        IndexT& selectedIndex,
        std::initializer_list<std::string_view> items,
        SegmentedOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) requires (std::is_integral_v<IndexT> || std::is_enum_v<IndexT>) {
        return Segmented(selectedIndex, std::span<const std::string_view>(items.begin(), items.size()), std::move(opts), loc);
    }

    Response SegmentedMask(
        uint32_t& selectedMask,
        std::span<const SegmentedItem> items,
        SegmentedOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return Run<guikit::Segmented>(nullptr, nullptr, &selectedMask, items, opts, loc);
    }

    Response SegmentedMask(
        uint32_t& selectedMask,
        std::initializer_list<SegmentedItem> items,
        SegmentedOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return SegmentedMask(selectedMask, std::span<const SegmentedItem>(items.begin(), items.size()), std::move(opts), loc);
    }

    Response SegmentedMask(
        uint32_t& selectedMask,
        std::span<const std::string_view> items,
        SegmentedOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return Run<guikit::Segmented>(nullptr, nullptr, &selectedMask, items, opts, loc);
    }

    Response SegmentedMask(
        uint32_t& selectedMask,
        std::initializer_list<std::string_view> items,
        SegmentedOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return SegmentedMask(selectedMask, std::span<const std::string_view>(items.begin(), items.size()), std::move(opts), loc);
    }

    // --- ColorPicker Widget ---
    Response ColorPicker(
        Color& value,
        ColorPickerOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return Run<guikit::ColorPicker>(value, opts, loc);
    }

    // --- Toolbar Component ---
    [[nodiscard]] ToolbarScope Toolbar(
        ToolbarOptions opts = {},
        std::source_location loc = std::source_location::current()
    );

    Response ToolButton(
        ToolButtonOptions opts = {},
        std::source_location loc = std::source_location::current()
    );

    // --- Modal Dialog Container ---
    [[nodiscard]] ModalScope Modal(
        bool& open,
        const ModalOptions& opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return guikit::Modal::Open(*this, open, opts, loc);
    }

    // --- ContextMenu Popup Component ---
    ContextMenuResponse ContextMenu(
        const Response& triggerTarget,
        std::span<const ContextMenuItem> items,
        const ContextMenuOptions& opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return guikit::ContextMenu::Run(*this, nullptr, std::nullopt, triggerTarget.rect, items, opts, loc);
    }

    ContextMenuResponse ContextMenu(
        const Response& triggerTarget,
        std::initializer_list<ContextMenuItem> items,
        const ContextMenuOptions& opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return ContextMenu(triggerTarget, std::span<const ContextMenuItem>(items.begin(), items.size()), opts, loc);
    }

    ContextMenuResponse ContextMenu(
        Rect triggerRect,
        std::span<const ContextMenuItem> items,
        const ContextMenuOptions& opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return guikit::ContextMenu::Run(*this, nullptr, std::nullopt, triggerRect, items, opts, loc);
    }

    ContextMenuResponse ContextMenu(
        Rect triggerRect,
        std::initializer_list<ContextMenuItem> items,
        const ContextMenuOptions& opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return ContextMenu(triggerRect, std::span<const ContextMenuItem>(items.begin(), items.size()), opts, loc);
    }

    ContextMenuResponse ContextMenu(
        std::span<const ContextMenuItem> items,
        const ContextMenuOptions& opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        const Id parentId = m_containerStack.empty() ? Id::Null() : m_containerStack.back()->id;
        const Rect parentRect = GetPrevFrameBounds(parentId);
        return guikit::ContextMenu::Run(*this, nullptr, std::nullopt, parentRect, items, opts, loc);
    }

    ContextMenuResponse ContextMenu(
        std::initializer_list<ContextMenuItem> items,
        const ContextMenuOptions& opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return ContextMenu(std::span<const ContextMenuItem>(items.begin(), items.size()), opts, loc);
    }

    ContextMenuResponse ContextMenu(
        bool& open,
        std::span<const ContextMenuItem> items,
        const ContextMenuOptions& opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        const Id parentId = m_containerStack.empty() ? Id::Null() : m_containerStack.back()->id;
        const Rect parentRect = GetPrevFrameBounds(parentId);
        return guikit::ContextMenu::Run(*this, &open, std::nullopt, parentRect, items, opts, loc);
    }

    ContextMenuResponse ContextMenu(
        bool& open,
        std::initializer_list<ContextMenuItem> items,
        const ContextMenuOptions& opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return ContextMenu(open, std::span<const ContextMenuItem>(items.begin(), items.size()), opts, loc);
    }

    ContextMenuResponse ContextMenu(
        bool& open,
        Vec2 anchorPos,
        std::span<const ContextMenuItem> items,
        const ContextMenuOptions& opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return guikit::ContextMenu::Run(*this, &open, anchorPos, Rect{}, items, opts, loc);
    }

    ContextMenuResponse ContextMenu(
        bool& open,
        Vec2 anchorPos,
        std::initializer_list<ContextMenuItem> items,
        const ContextMenuOptions& opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return ContextMenu(open, anchorPos, std::span<const ContextMenuItem>(items.begin(), items.size()), opts, loc);
    }

    // --- Toast Notification System ---
    uint64_t ShowToast(ToastOptions opts);
    void DismissToast(uint64_t id) noexcept;
    void ClearToasts() noexcept;
    [[nodiscard]] size_t GetActiveToastCount() const noexcept { return m_toasts.size(); }
    [[nodiscard]] std::vector<ToastEntry>& GetToastEntries() noexcept { return m_toasts; }
    [[nodiscard]] const std::vector<ToastEntry>& GetToastEntries() const noexcept { return m_toasts; }
    void MarkToastHostVisited() noexcept { m_toastHostVisitedThisFrame = true; }
    void SetToastHostOptions(const ToastHostOptions& opts) noexcept { m_defaultToastHostOpts = opts; }
    [[nodiscard]] const ToastHostOptions& GetToastHostOptions() const noexcept { return m_defaultToastHostOpts; }
    void ToastHost(const ToastHostOptions& opts = {}) {
        guikit::Toast::RunHost(*this, opts);
    }

    // --- String & Formatting Services ---
    [[nodiscard]] UiString String(std::string_view text);
    [[nodiscard]] std::string_view AllocString(std::string_view text);
    [[nodiscard]] UiText AllocText(UiText t) {
        if (!t.text.empty()) {
            t.text = AllocString(t.text);
        }
        if (!t.variants.empty()) {
            const size_t count = t.variants.size();
            auto* arr = static_cast<std::string_view*>(
                m_arena.AllocateRaw(sizeof(std::string_view) * count, alignof(std::string_view))
            );
            size_t i = 0;
            for (std::string_view v : t.variants) {
                arr[i++] = v.empty() ? std::string_view{} : AllocString(v);
            }
            t.variants = std::span<const std::string_view>(arr, count);
        }
        return t;
    }

    [[nodiscard]] UiAffix AllocAffix(UiAffix a) {
        a.text = AllocText(a.text);
        if (!a.icon.GetPath().empty()) {
            a.icon = Icon::File(AllocString(a.icon.GetPath()));
        }
        return a;
    }

    template <typename... Args>
    [[nodiscard]] UiString Format(std::format_string<Args...> fmt, Args&&... args) {
        BasicUiString<128> temp;
        temp.Format(fmt, std::forward<Args>(args)...);
        return UiString(AllocString(temp.View()));
    }

    // --- Core Engine Services for Widgets ---
    Id MakeChildId(std::source_location loc);
    Response Interact(Id id);
    Node* CreateNode(NodeType type, Id id);
    
    /// Allocates and constructs arbitrary widget payload data in the per-frame scratch arena.
    template <typename T, typename... Args>
    [[nodiscard]] T* AllocPayload(Args&&... args) {
        return m_arena.Allocate<T>(std::forward<Args>(args)...);
    }

    [[nodiscard]] BumpAllocator& GetArena() noexcept { return m_arena; }

    void AdaptChildToContainer(Node* node);
    [[nodiscard]] Rect GetPrevFrameBounds(Id id) const;
    [[nodiscard]] Rect GetPrevUnclippedBounds(Id id) const;
    [[nodiscard]] Rect GetParentPrevFrameBounds() const;

    // --- Internal Hierarchy & Scope Management ---
    void PushContainer(Node* node);
    void PopContainer(Node* node);
    [[nodiscard]] Node* GetCurrentContainer() const noexcept {
        return m_containerStack.empty() ? nullptr : m_containerStack.back();
    }

    // Imperative actions requested from Response
    void RequestFocus(Id id);
    [[nodiscard]] Id GetFocusedId() const noexcept { return m_focusedId; }
    [[nodiscard]] bool IsFocused(Id id) const noexcept {
        if (!id.IsValid() || m_focusedId != id) return false;
        if (m_openPopupId.IsValid() && m_openPopupId != id) return false;
        if (m_modalScopeDepth == 0 && (m_currFrameModalId.IsValid() || m_prevFrameModalId.IsValid())) return false;
        return true;
    }
    [[nodiscard]] bool HasActiveTextEdit(Id id) const noexcept { return m_editState.id == id && id.IsValid(); }
    void ClearFocus() noexcept {
        if (m_editState.id == m_focusedId) {
            m_editState.id = Id::Null();
        }
        m_focusedId = Id::Null();
    }

    [[nodiscard]] Id GetActiveId() const noexcept { return m_activeId; }
    void SetActiveId(Id id) noexcept { m_activeId = id; }
    [[nodiscard]] uint64_t GetFrameIndex() const noexcept { return m_frameIndex; }
    [[nodiscard]] size_t GetCustomStateCount() const noexcept { return m_customStates.size(); }
    [[nodiscard]] ICanvas* GetCanvas() const noexcept { return m_lastCanvas; }

    // --- Clipboard Services ---
    using ClipboardGetFn = std::function<std::string()>;
    using ClipboardSetFn = std::function<void(std::string_view)>;

    void SetClipboardCallbacks(ClipboardGetFn getFn, ClipboardSetFn setFn);
    void SetClipboardText(std::string_view text);
    [[nodiscard]] std::string GetClipboardText() const;

    std::string_view EnsureTextEdit(
        Id id,
        std::string_view initialText,
        bool multiline = false,
        bool readOnly = false
    );

    std::string_view GetOrStartTextEdit(
        Id id,
        std::string_view initialText,
        size_t& outCursor,
        bool& outChanged,
        bool& outSubmitted,
        bool multiline = false
    );

    std::string_view GetOrStartTextEdit(
        Id id,
        std::string_view initialText,
        size_t& outCursor,
        size_t& outSelectionAnchor,
        bool& outChanged,
        bool& outSubmitted,
        bool multiline = false,
        bool readOnly = false
    );

    void SetTextEditBuffer(Id id, std::string_view text);
    void SetTextEditCursor(Id id, size_t cursor, size_t selectionAnchor) noexcept;
    void SetTextEditAutoClose(Id id, bool enabled) noexcept;
    void ApplyTextEditChange(Id id, std::string_view newText, size_t newCursor, size_t newSelectionAnchor);
    bool UndoTextEdit(Id id);
    bool RedoTextEdit(Id id);
    void SetSuspendDirectEditInput(bool suspend) noexcept { m_suspendDirectEditInput = suspend; }
    [[nodiscard]] bool GetTextEditSelection(Id id, size_t& outStart, size_t& outEnd) const noexcept;
    [[nodiscard]] size_t GetTextEditAnchor(Id id) const noexcept {
        return (m_editState.id == id && id.IsValid()) ? m_editState.selectionAnchor : 0;
    }

    // --- Cursor Configuration & State ---
    void SetCursorConfig(const CursorConfig& config) noexcept { m_cursorConfig = config; }
    [[nodiscard]] const CursorConfig& GetCursorConfig() const noexcept { return m_cursorConfig; }
    [[nodiscard]] bool IsCursorVisible(std::optional<CursorBlinkMode> modeOverride = std::nullopt) const noexcept;
    void ResetCursorBlink() noexcept;

    // --- Deterministic Time Control (for unit tests / animations) ---
    void SetCurrentTime(double timeSeconds) noexcept;
    void AdvanceTime(double deltaSeconds) noexcept;
    [[nodiscard]] double GetCurrentTime() const noexcept { return m_currentTimeSec; }

    // --- Popup, Tooltip, Modal & Overlay Management ---
    struct OverlayHitRegion {
        Id id{Id::Null()};
        Rect rect{};
    };

    void OpenPopup(Id id, Rect bounds);
    void ClosePopup();
    [[nodiscard]] bool IsPopupOpen(Id id) const noexcept { return m_openPopupId == id && id.IsValid(); }
    [[nodiscard]] Id GetOpenPopupId() const noexcept { return m_openPopupId; }
    [[nodiscard]] Rect GetOpenPopupRect() const noexcept { return m_openPopupRect; }
    void SetOpenPopupVisualHash(uint64_t hash) noexcept { m_openPopupVisualHash = hash; }
    void RegisterOverlay(std::function<void(ICanvas&)> drawFn);
    void RegisterOverlayHitRegion(Id id, Rect rect);
    void RegisterOverlayHitBuilder(std::function<void()> builderFn);
    void BeginOverlayHitCheck(Id id) noexcept { m_activeOverlayHitCheckId = id; }
    void EndOverlayHitCheck() noexcept { m_activeOverlayHitCheckId = Id::Null(); }
    void BeginWidgetExecution() noexcept { ++m_widgetExecDepth; }
    void EndWidgetExecution() noexcept { if (m_widgetExecDepth > 0) --m_widgetExecDepth; }
    [[nodiscard]] bool IsMouseOccluded(Id id, Vec2 pos) const noexcept;
    [[nodiscard]] bool IsRectHovered(Id id, const Rect& rect) const noexcept;
    [[nodiscard]] bool IsMouseBlockedForCurrentScope() const noexcept;
    void SetTooltip(UiText tooltip, Rect anchorRect);
    void RegisterFocusable(Id id, bool disabled = false);

    void BeginModalScope(Id id, Rect backdropRect) noexcept {
        ++m_modalScopeDepth;
        m_currFrameModalId = id;
        m_currFrameModalBackdropRect = backdropRect;
    }

    void EndModalScope(bool stillOpen = true) noexcept {
        if (m_modalScopeDepth > 0) {
            --m_modalScopeDepth;
        }
        if (!stillOpen && m_modalScopeDepth == 0) {
            ClearModalBlock();
        }
    }

    void ClearModalBlock() noexcept {
        m_currFrameModalId = Id::Null();
        m_currFrameModalBackdropRect = Rect{};
        m_prevFrameModalId = Id::Null();
        m_prevFrameModalBackdropRect = Rect{};
    }

    [[nodiscard]] bool IsInModalScope() const noexcept { return m_modalScopeDepth > 0; }
    [[nodiscard]] Id GetPrevFrameModalId() const noexcept { return m_prevFrameModalId; }

    [[nodiscard]] bool IsModalBlocking(Vec2 pos) const noexcept {
        if (m_modalScopeDepth > 0) return false;
        if (m_currFrameModalId.IsValid() && m_currFrameModalBackdropRect.Contains(pos)) return true;
        if (m_prevFrameModalId.IsValid() && m_prevFrameModalBackdropRect.Contains(pos)) return true;
        return false;
    }

    // --- Input Queries & Consumption ---
    [[nodiscard]] Vec2 GetMousePos() const noexcept { return m_mousePos; }
    [[nodiscard]] bool IsMouseDown() const noexcept { return m_mouseDown && !IsMouseBlockedForCurrentScope(); }
    [[nodiscard]] bool IsMouseClicked() const noexcept { return m_mouseClicked && !IsMouseBlockedForCurrentScope(); }
    [[nodiscard]] bool IsMouseReleased() const noexcept { return m_mouseReleased; }
    [[nodiscard]] bool IsMouseDoubleClicked() const noexcept { return m_mouseDoubleClicked && !IsMouseBlockedForCurrentScope(); }
    [[nodiscard]] bool IsRightMouseDown() const noexcept { return m_rightMouseDown && !IsMouseBlockedForCurrentScope(); }
    [[nodiscard]] bool IsRightMouseClicked() const noexcept { return m_rightMouseClicked && !IsMouseBlockedForCurrentScope(); }
    [[nodiscard]] bool IsRightMouseReleased() const noexcept { return m_rightMouseReleased; }
    [[nodiscard]] bool IsMiddleMouseDown() const noexcept { return m_middleMouseDown && !IsMouseBlockedForCurrentScope(); }
    [[nodiscard]] bool IsMiddleMouseClicked() const noexcept { return m_middleMouseClicked && !IsMouseBlockedForCurrentScope(); }
    [[nodiscard]] bool IsMiddleMouseReleased() const noexcept { return m_middleMouseReleased; }
    void ConsumeMouseClick() noexcept { m_mouseClicked = false; m_mouseReleased = false; m_mouseDoubleClicked = false; m_rightMouseClicked = false; m_rightMouseReleased = false; }
    void SetKeyModifiers(KeyModifiers mods) noexcept { m_keyMods = mods; }
    [[nodiscard]] KeyModifiers GetKeyModifiers() const noexcept { return m_keyMods; }
    [[nodiscard]] Vec2 GetViewportSize() const noexcept { return m_viewportSize; }
    [[nodiscard]] Vec2 GetMouseWheel() const noexcept {
        if (m_openPopupId.IsValid() && m_openPopupRect.Contains(m_mousePos)) {
            return {0.0f, 0.0f};
        }
        if (IsModalBlocking(m_mousePos) || IsMouseBlockedForCurrentScope()) {
            return {0.0f, 0.0f};
        }
        return m_mouseWheel;
    }
    [[nodiscard]] Vec2 GetPopupMouseWheel(Id popupId) const noexcept {
        if (m_openPopupId == popupId && popupId.IsValid()) {
            return m_mouseWheel;
        }
        return {0.0f, 0.0f};
    }
    void ConsumeMouseWheel() noexcept { m_mouseWheel = {0.0f, 0.0f}; }
    bool ConsumeKey(Key key);
    bool ConsumeKey(Key key, KeyModifiers& outMods);
    bool ConsumeShortcut(Key key, bool ctrl, bool shift = false, bool alt = false);

    // --- Semantic Keymap & Action Routing ---
    [[nodiscard]] Keymap& GetKeymap() noexcept { return m_keymap; }
    [[nodiscard]] const Keymap& GetKeymap() const noexcept { return m_keymap; }
    void SetKeymap(const Keymap& keymap) noexcept { m_keymap = keymap; }
    void BindAction(KeyAction action, KeyChord primary, KeyChord secondary = {}) noexcept {
        m_keymap.Bind(action, primary, secondary);
    }
    [[nodiscard]] bool MatchesAction(KeyAction action, Key key, KeyModifiers mods) const noexcept {
        return m_keymap.Matches(action, key, mods);
    }
    [[nodiscard]] KeyAction ResolveAction(Key key, KeyModifiers mods) const noexcept {
        return m_keymap.Resolve(key, mods);
    }
    bool ConsumeAction(KeyAction action);

    [[nodiscard]] std::vector<std::string> ConsumePendingTextInput();
    void ScrollToId(Id id);

    // --- Generic Persistent State Storage ---
    /// Persistent typed state storage across frames for widgets by deterministic Id and Type.
    template <typename T>
    [[nodiscard]] T& GetOrCreateState(Id id, T defaultVal = T{}) {
        detail::StateKey key{id, std::type_index(typeid(T))};
        auto it = m_customStates.find(key);
        if (it == m_customStates.end()) {
            it = m_customStates.emplace(key, detail::StoredState{std::make_any<T>(std::move(defaultVal)), m_frameIndex}).first;
        } else {
            it->second.lastFrameActive = m_frameIndex;
        }
        return std::any_cast<T&>(it->second.value);
    }

    /// Finds persistent typed state associated with a unique widget Id, or nullptr if not found.
    template <typename T>
    [[nodiscard]] const T* FindState(Id id) const {
        detail::StateKey key{id, std::type_index(typeid(T))};
        auto it = m_customStates.find(key);
        if (it == m_customStates.end()) {
            return nullptr;
        }
        return std::any_cast<T>(&it->second.value);
    }

private:
    void RenderNodeRecursive(Node* node, ICanvas& canvas, const Rect& dirtyRect, const UiTheme& parentTheme);
    void RenderModalsRecursive(Node* node, ICanvas& canvas, const Rect& dirtyRect, const UiTheme& parentTheme);
    bool UpdateScrollAreasRecursive(Node* node);

    // Widget snapshot for frame-to-frame change detection
    struct WidgetSnapshot {
        Rect rect{};
        uint64_t visualHash{0};
        uint32_t flags{0}; // bits: 0=hovered, 1=pressed, 2=disabled, 3=checked, 4=cursorVisible, 5=activeEdit, 6=tabActive, 7=focused, 8..11=scrollbar
        size_t cursor{0};
        size_t selectionAnchor{0};
        uint32_t bgColor{0};
        uint32_t borderColor{0};

        bool operator==(const WidgetSnapshot& o) const noexcept {
            return rect == o.rect &&
                   visualHash == o.visualHash &&
                   flags == o.flags &&
                   cursor == o.cursor &&
                   selectionAnchor == o.selectionAnchor &&
                   bgColor == o.bgColor &&
                   borderColor == o.borderColor;
        }

        bool operator!=(const WidgetSnapshot& o) const noexcept {
            return !(*this == o);
        }
    };

    std::unordered_map<Id, WidgetSnapshot> m_prevSnapshots;
    std::vector<Rect> m_explicitDamageRects;
    std::vector<Rect> m_dirtyRects;
    Rect m_dirtyRect{0.0f, 0.0f, 0.0f, 0.0f};
    UiTheme m_theme{UiTheme::Dark()};
    Color m_clearColor{11, 15, 25, 255}; // Tailwind Slate-950 #0B0F19
    bool m_hasDirtyDamage{true};
    bool m_isFullRepaint{true};
    bool m_forceFullRepaint{true};
    bool m_dirtyRectEnabled{true};
    bool m_liveResizeEnabled{true};
    bool m_debugBoundsEnabled{false};
    bool m_inspectModeEnabled{false};
    bool m_anyScopedInspectActive{false};

    BumpAllocator m_arena;
    std::vector<Node*> m_containerStack;
    std::vector<UiSize> m_sizeStack;
    std::vector<bool> m_disabledStack;
    std::vector<UiTheme*> m_themeStack;
    std::vector<float> m_scaleStack;
    Node* m_root{nullptr};
    ICanvas* m_lastCanvas{nullptr};

    Vec2 m_viewportSize{800.0f, 600.0f};
    float m_dpiScale{1.0f};

    // Time & Cursor animation state
    CursorConfig m_cursorConfig{};
    std::chrono::steady_clock::time_point m_startTime{std::chrono::steady_clock::now()};
    double m_currentTimeSec{0.0};
    double m_lastInputTimeSec{0.0};
    bool m_manualTime{false};

    // Input state
    Vec2 m_mousePos{-10000.0f, -10000.0f};
    bool m_mouseDown{false};
    bool m_mouseClicked{false};
    bool m_mouseReleased{false};
    bool m_mouseDoubleClicked{false};
    double m_lastClickTimeSec{-1.0};
    Vec2 m_lastClickPos{-10000.0f, -10000.0f};
    bool m_rightMouseDown{false};
    bool m_rightMouseClicked{false};
    bool m_rightMouseReleased{false};
    bool m_middleMouseDown{false};
    bool m_middleMouseClicked{false};
    bool m_middleMouseReleased{false};
    KeyModifiers m_keyMods{};
    Keymap m_keymap{Keymap::Default()};
    int m_repaintFrames{3};
    int m_idleHeartbeatMs{500};

    // Clipboard state
    ClipboardGetFn m_clipboardGetFn{};
    ClipboardSetFn m_clipboardSetFn{};
    std::string m_clipboardText{};

    Id m_hoveredId{Id::Null()};
    Id m_activeId{Id::Null()};
    Id m_focusedId{Id::Null()};

    // Active text edit state
    struct ActiveEditState {
        struct EditSnapshot {
            std::string text;
            size_t cursor{0};
            size_t selectionAnchor{0};
        };

        Id id{Id::Null()};
        std::string text;
        size_t cursor{0};
        size_t selectionAnchor{0};
        std::vector<EditSnapshot> undoStack;
        std::vector<EditSnapshot> redoStack;
        std::optional<size_t> preferredColumn;
        bool changed{false};
        bool submitted{false};
        bool multiline{false};
        bool readOnly{false};
        bool autoCloseBrackets{false};

        [[nodiscard]] bool HasSelection() const noexcept { return cursor != selectionAnchor; }
        [[nodiscard]] size_t SelectionStart() const noexcept { return std::min(cursor, selectionAnchor); }
        [[nodiscard]] size_t SelectionEnd() const noexcept { return std::max(cursor, selectionAnchor); }

        [[nodiscard]] std::string_view SelectedText() const noexcept {
            if (!HasSelection()) return {};
            const size_t s = std::min(SelectionStart(), text.size());
            const size_t e = std::min(SelectionEnd(), text.size());
            return std::string_view(text).substr(s, e - s);
        }

        void SaveUndo() {
            if (undoStack.empty() || undoStack.back().text != text) {
                if (undoStack.size() >= 128) {
                    undoStack.erase(undoStack.begin());
                }
                undoStack.push_back(EditSnapshot{text, cursor, selectionAnchor});
                redoStack.clear();
            }
        }

        bool Undo() {
            while (!undoStack.empty()) {
                EditSnapshot snap = std::move(undoStack.back());
                undoStack.pop_back();
                if (snap.text != text) {
                    redoStack.push_back(EditSnapshot{text, cursor, selectionAnchor});
                    text = std::move(snap.text);
                    cursor = std::min(snap.cursor, text.size());
                    selectionAnchor = std::min(snap.selectionAnchor, text.size());
                    preferredColumn.reset();
                    changed = true;
                    return true;
                }
            }
            return false;
        }

        bool Redo() {
            while (!redoStack.empty()) {
                EditSnapshot snap = std::move(redoStack.back());
                redoStack.pop_back();
                if (snap.text != text) {
                    undoStack.push_back(EditSnapshot{text, cursor, selectionAnchor});
                    text = std::move(snap.text);
                    cursor = std::min(snap.cursor, text.size());
                    selectionAnchor = std::min(snap.selectionAnchor, text.size());
                    preferredColumn.reset();
                    changed = true;
                    return true;
                }
            }
            return false;
        }

        void SelectAll() noexcept {
            selectionAnchor = 0;
            cursor = text.size();
            preferredColumn.reset();
        }

        bool DeleteSelection();
        void Insert(std::string_view utf8Text);
        bool HandleKey(Context& ctx, Key key, KeyModifiers mods, bool& outLostFocus);
    };
    ActiveEditState m_editState;
    bool m_suspendDirectEditInput{false};

    struct QueuedKeyEvent {
        Key key;
        KeyModifiers mods;
    };
    std::vector<std::string> m_pendingTextInput;
    std::vector<QueuedKeyEvent> m_pendingKeyEvents;

    // Previous frame widget bounds for instantaneous hit testing
    struct HitNodeRecord {
        Id id{Id::Null()};
        Rect rect{};
        uint32_t preIndex{0};
        uint32_t postIndex{0};
        NodeType type{NodeType::Container};
        uint8_t zLayer{0};
        bool blocksHit{false};
    };

    std::unordered_map<Id, Rect> m_prevFrameBounds;
    std::unordered_map<Id, Rect> m_currFrameBounds;
    std::unordered_map<Id, Rect> m_prevUnclippedBounds;
    std::unordered_map<Id, Rect> m_currUnclippedBounds;
    std::vector<HitNodeRecord> m_prevHitNodes;
    std::vector<HitNodeRecord> m_currHitNodes;
    std::unordered_map<Id, size_t> m_prevHitNodeMap;
    std::vector<OverlayHitRegion> m_prevOverlayHitRegions;
    std::vector<OverlayHitRegion> m_currOverlayHitRegions;
    std::vector<std::function<void()>> m_overlayHitBuilders;
    Id m_activeOverlayHitCheckId{Id::Null()};
    int m_widgetExecDepth{0};
    std::vector<Id> m_prevFocusableIds;
    std::vector<Id> m_currFocusableIds;
    std::vector<Id> m_prevModalFocusableIds;
    std::vector<Id> m_currModalFocusableIds;

    // Floating popup, tooltip, modal & toast overlay state
    Id m_openPopupId{Id::Null()};
    Rect m_openPopupRect{0.0f, 0.0f, 0.0f, 0.0f};
    uint64_t m_openPopupVisualHash{0};
    Id m_prevOpenPopupId{Id::Null()};
    Rect m_prevOpenPopupRect{0.0f, 0.0f, 0.0f, 0.0f};
    uint64_t m_prevOpenPopupVisualHash{0};
    Id m_prevFrameModalId{Id::Null()};
    Rect m_prevFrameModalBackdropRect{0.0f, 0.0f, 0.0f, 0.0f};
    Id m_currFrameModalId{Id::Null()};
    Rect m_currFrameModalBackdropRect{0.0f, 0.0f, 0.0f, 0.0f};
    int m_modalScopeDepth{0};
    UiText m_activeTooltip{};
    Rect m_activeTooltipAnchor{0.0f, 0.0f, 0.0f, 0.0f};
    bool m_hadTooltipLastFrame{false};
    std::vector<ToastEntry> m_toasts;
    ToastHostOptions m_defaultToastHostOpts{};
    uint64_t m_nextToastId{0};
    bool m_toastHostVisitedThisFrame{false};
    bool m_hadToastsLastFrame{false};
    std::vector<std::function<void(ICanvas&)>> m_overlayPasses;
    Vec2 m_mouseWheel{0.0f, 0.0f};
    std::unordered_map<detail::StateKey, detail::StoredState, detail::StateKeyHash> m_customStates;
    uint64_t m_frameIndex{0};

    void RenderDebugBoundsRecursive(Node* node, ICanvas& canvas, bool inheritedDebugBounds);
    void RenderTooltipOverlay(ICanvas& canvas, Vec2 logicalSize);
    Node* FindDeepestNodeAt(Node* root, Vec2 pos, Rect clipRect);
    void RenderInspectOverlay(Node* root, ICanvas& canvas, Vec2 logicalSize);
};

/// Scoped UI size modifier returned by `ui.Size(...)`.
/// Supports both single-call chaining (`ui.Size(UiSize::Lg).Element({...})`, `ui.Size(UiSize::Sm).Button({...})`)
/// and RAII block scoping (`if (auto s = ui.Size(UiSize::Sm)) { ... }`).
class SizeScope {
public:
    SizeScope() noexcept = default;

    SizeScope(Context* ctx, UiSize size) noexcept
        : m_ctx(ctx)
        , m_prevSize(ctx ? ctx->GetCurrentSize() : UiSize::Md)
        , m_size(size) {
        if (m_ctx) {
            m_ctx->PushSize(m_size);
        }
    }

    ~SizeScope() noexcept {
        if (m_ctx) {
            m_ctx->PopSize();
        }
    }

    SizeScope(const SizeScope&) = delete;
    SizeScope& operator=(const SizeScope&) = delete;

    SizeScope(SizeScope&& other) noexcept
        : m_ctx(other.m_ctx)
        , m_prevSize(other.m_prevSize)
        , m_size(other.m_size) {
        other.m_ctx = nullptr;
    }

    SizeScope& operator=(SizeScope&& other) noexcept {
        if (this != &other) {
            if (m_ctx) {
                m_ctx->PopSize();
            }
            m_ctx = other.m_ctx;
            m_prevSize = other.m_prevSize;
            m_size = other.m_size;
            other.m_ctx = nullptr;
        }
        return *this;
    }

    [[nodiscard]] explicit constexpr operator bool() const noexcept {
        return m_ctx != nullptr;
    }

    // --- Generic Autonomous Widget & Container Dispatch ---
    template <typename WidgetT, typename... Args>
        requires WidgetRunnable<WidgetT, Args...>
    decltype(auto) Run(Args&&... args) {
        return m_ctx->Run<WidgetT>(std::forward<Args>(args)...);
    }

    template <typename ContainerT, typename... Args>
        requires ContainerOpenable<ContainerT, Args...>
    [[nodiscard]] decltype(auto) Open(Args&&... args) {
        return TransferToContainer(m_ctx->Open<ContainerT>(std::forward<Args>(args)...));
    }

    // --- Element (propagates its size to its child Control unless overridden) ---
    ElementScope Element(
        ElementOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return TransferToContainer(m_ctx->Element(std::move(opts), loc));
    }

    // --- Accordion (applies size to Accordion header; restores previous size for body children) ---
    [[nodiscard]] AccordionScope Accordion(
        AccordionOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        Context* ctx = m_ctx;
        AccordionScope scope = ctx->Accordion(opts, loc);
        RestoreAccordionBodySize(ctx, scope, opts);
        return scope;
    }

    [[nodiscard]] AccordionScope Accordion(
        std::string_view label,
        AccordionOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        Context* ctx = m_ctx;
        AccordionScope scope = ctx->Accordion(label, opts, loc);
        RestoreAccordionBodySize(ctx, scope, opts);
        return scope;
    }

    [[nodiscard]] AccordionScope Accordion(
        bool& open,
        AccordionOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        Context* ctx = m_ctx;
        AccordionScope scope = ctx->Accordion(open, opts, loc);
        RestoreAccordionBodySize(ctx, scope, opts);
        return scope;
    }

    template <typename T, typename U>
        requires (!std::is_same_v<std::decay_t<T>, bool> && requires(T& a, const U& b) {
            { a == b } -> std::convertible_to<bool>;
            a = static_cast<T>(b);
        })
    [[nodiscard]] AccordionScope Accordion(
        T& activeValue,
        const U& sectionValue,
        AccordionOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        Context* ctx = m_ctx;
        AccordionScope scope = ctx->Accordion(activeValue, sectionValue, opts, loc);
        RestoreAccordionBodySize(ctx, scope, opts);
        return scope;
    }

    // --- Layout & Panel Containers (children inherit m_size) ---
    [[nodiscard]] ContainerScope Row(
        RowOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return TransferToContainer(m_ctx->Row(std::move(opts), loc));
    }

    [[nodiscard]] ContainerScope Column(
        ColumnOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return TransferToContainer(m_ctx->Column(std::move(opts), loc));
    }

    [[nodiscard]] ContainerScope Wrap(
        WrapOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return TransferToContainer(m_ctx->Wrap(std::move(opts), loc));
    }

    [[nodiscard]] GridScope Grid(
        GridOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return TransferToContainer(m_ctx->Grid(std::move(opts), loc));
    }

    [[nodiscard]] ContainerScope Box(
        BoxOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return TransferToContainer(m_ctx->Box(std::move(opts), loc));
    }

    [[nodiscard]] ContainerScope Card(
        CardOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return TransferToContainer(m_ctx->Card(std::move(opts), loc));
    }

    [[nodiscard]] ContainerScope ScrollArea(
        ScrollAreaOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return TransferToContainer(m_ctx->ScrollArea(std::move(opts), loc));
    }

    [[nodiscard]] ContainerScope Carousel(
        CarouselOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return TransferToContainer(m_ctx->Carousel(std::move(opts), loc));
    }

    [[nodiscard]] SidebarScope Sidebar(
        SidebarOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return TransferToContainer(m_ctx->Sidebar(std::move(opts), loc));
    }

    [[nodiscard]] SidebarScope Sidebar(
        SidebarMode& mode,
        SidebarOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return TransferToContainer(m_ctx->Sidebar(mode, std::move(opts), loc));
    }

    [[nodiscard]] SidebarScope Sidebar(
        bool& open,
        SidebarOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return TransferToContainer(m_ctx->Sidebar(open, std::move(opts), loc));
    }

    [[nodiscard]] ActivityBarScope ActivityBar(
        ActivityBarOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return TransferToContainer(m_ctx->ActivityBar(std::move(opts), loc));
    }

    [[nodiscard]] ActivityBarScope ActivityBar(
        SidebarMode& linkedSidebarMode,
        ActivityBarOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return TransferToContainer(m_ctx->ActivityBar(linkedSidebarMode, std::move(opts), loc));
    }

    Response ActivityBar(
        int& activeIndex,
        std::span<const ActivityBarItem> items,
        ActivityBarOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return m_ctx->ActivityBar(activeIndex, items, std::move(opts), loc);
    }

    Response ActivityBar(
        int& activeIndex,
        std::initializer_list<ActivityBarItem> items,
        ActivityBarOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return m_ctx->ActivityBar(activeIndex, items, std::move(opts), loc);
    }

    [[nodiscard]] BottomPanelScope BottomPanel(
        BottomPanelOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return TransferToContainer(m_ctx->BottomPanel(std::move(opts), loc));
    }

    [[nodiscard]] BottomPanelScope BottomPanel(
        BottomPanelMode& mode,
        BottomPanelOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return TransferToContainer(m_ctx->BottomPanel(mode, std::move(opts), loc));
    }

    [[nodiscard]] BottomPanelScope BottomPanel(
        BottomPanelMode* mode,
        BottomPanelOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return TransferToContainer(m_ctx->BottomPanel(mode, std::move(opts), loc));
    }

    [[nodiscard]] BottomPanelScope BottomPanel(
        bool& open,
        BottomPanelOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return TransferToContainer(m_ctx->BottomPanel(open, std::move(opts), loc));
    }

    [[nodiscard]] BottomPanelScope BottomPanel(
        bool* open,
        BottomPanelOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return TransferToContainer(m_ctx->BottomPanel(open, std::move(opts), loc));
    }

    [[nodiscard]] StatusBarScope StatusBar(
        StatusBarOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return TransferToContainer(m_ctx->StatusBar(std::move(opts), loc));
    }

    [[nodiscard]] SceneScope Scene(
        SceneOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return TransferToContainer(m_ctx->Scene(std::move(opts), loc));
    }

    [[nodiscard]] MapViewScope MapView(
        MapViewOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return TransferToContainer(m_ctx->MapView(std::move(opts), loc));
    }

    [[nodiscard]] TableScope Table(
        std::span<const TableColumn> columns,
        const TableOptions& opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return TransferToContainer(m_ctx->Table(columns, opts, loc));
    }

    [[nodiscard]] TableScope Table(
        std::initializer_list<TableColumn> columns,
        const TableOptions& opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return TransferToContainer(m_ctx->Table(columns, opts, loc));
    }

    [[nodiscard]] ToolbarScope Toolbar(
        ToolbarOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return TransferToContainer(m_ctx->Toolbar(std::move(opts), loc));
    }

    [[nodiscard]] SidebarMenuScope SidebarMenu(
        SidebarMenuOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return TransferToContainer(m_ctx->SidebarMenu(std::move(opts), loc));
    }

    [[nodiscard]] TreeViewScope TreeView(
        TreeViewOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return TransferToContainer(m_ctx->TreeView(std::move(opts), loc));
    }

    [[nodiscard]] TreeViewScope TreeView(
        std::span<const TableColumn> columns,
        TreeViewOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return TransferToContainer(m_ctx->TreeView(columns, std::move(opts), loc));
    }

    [[nodiscard]] TreeViewScope TreeView(
        std::initializer_list<TableColumn> columns,
        TreeViewOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return TransferToContainer(m_ctx->TreeView(columns, std::move(opts), loc));
    }

    [[nodiscard]] TreeItemScope TreeItem(
        TreeItemOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return TransferToContainer(m_ctx->TreeItem(std::move(opts), loc));
    }

    [[nodiscard]] TreeItemScope TreeItem(
        std::string_view label,
        TreeItemOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return TransferToContainer(m_ctx->TreeItem(label, std::move(opts), loc));
    }

    [[nodiscard]] TreeItemScope TreeItem(
        bool& open,
        TreeItemOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return TransferToContainer(m_ctx->TreeItem(open, std::move(opts), loc));
    }

    [[nodiscard]] TreeItemScope TreeItem(
        bool& open,
        std::string_view label,
        TreeItemOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return TransferToContainer(m_ctx->TreeItem(open, label, std::move(opts), loc));
    }

    // --- Leaf Widgets & Controls ---
    Response Heading(
        HeadingOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return m_ctx->Heading(std::move(opts), loc);
    }

    Response Button(
        ButtonOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return m_ctx->Button(std::move(opts), loc);
    }

    Response ToolButton(
        ToolButtonOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return m_ctx->ToolButton(std::move(opts), loc);
    }

    Response Text(
        UiText label,
        TextOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return m_ctx->Text(std::move(label), std::move(opts), loc);
    }

    Response Image(
        std::string_view source,
        ImageOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return m_ctx->Image(source, std::move(opts), loc);
    }

    Response Image(
        ImageOptions opts,
        std::source_location loc = std::source_location::current()
    ) {
        return m_ctx->Image(std::move(opts), loc);
    }

    MarkdownResponse Markdown(
        std::string_view markdownText,
        const MarkdownOptions& opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return m_ctx->Markdown(markdownText, opts, loc);
    }

    SignalViewResponse SignalView(
        std::span<const float> samples,
        const SignalViewOptions& opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return m_ctx->SignalView(samples, opts, loc);
    }

    SonarViewResponse SonarView(
        std::span<const float> polarMatrix,
        const SonarViewOptions& opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return m_ctx->SonarView(polarMatrix, opts, loc);
    }

    WaterfallViewResponse WaterfallView(
        std::span<const float> latestSliceDb,
        const WaterfallViewOptions& opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return m_ctx->WaterfallView(latestSliceDb, opts, loc);
    }

    HistogramResponse Histogram(
        const HistogramData& data,
        ChannelTransferParams& transfer,
        const HistogramOptions& opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return m_ctx->Histogram(data, transfer, opts, loc);
    }

    ImageViewResponse ImageView(
        const ImageViewData& data,
        ImageViewParams& params,
        const ImageViewOptions& opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return m_ctx->ImageView(data, params, opts, loc);
    }

    ImageViewResponse ImageView(
        const ImageViewData& data,
        const ImageViewOptions& opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return m_ctx->ImageView(data, opts, loc);
    }

    AttitudeViewResponse AttitudeView(
        const AttitudeViewOptions& opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return m_ctx->AttitudeView(opts, loc);
    }

    CarClusterViewResponse CarClusterView(
        const CarClusterViewOptions& opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return m_ctx->CarClusterView(opts, loc);
    }

    BpmnEditorResponse BpmnEditor(
        BpmnDiagram& diagram,
        const BpmnEditorOptions& opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return m_ctx->BpmnEditor(diagram, opts, loc);
    }

    Response Switch(
        bool& value,
        SwitchOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return m_ctx->Switch(value, std::move(opts), loc);
    }

    Response Switch(
        const bool& checked,
        SwitchOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return m_ctx->Switch(checked, std::move(opts), loc);
    }

    Response Checkbox(
        bool& value,
        CheckboxOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return m_ctx->Checkbox(value, std::move(opts), loc);
    }

    Response Checkbox(
        const bool& checked,
        CheckboxOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return m_ctx->Checkbox(checked, std::move(opts), loc);
    }

    template <typename T, typename U = T>
        requires (std::equality_comparable_with<T, U>)
    Response Radio(
        T& currentValue,
        const U& optionValue,
        RadioOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return m_ctx->Radio(currentValue, optionValue, std::move(opts), loc);
    }

    Response Radio(
        bool& value,
        RadioOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return m_ctx->Radio(value, std::move(opts), loc);
    }

    Response Radio(
        const bool& checked,
        RadioOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return m_ctx->Radio(checked, std::move(opts), loc);
    }

    template <typename T, typename U = T>
        requires (std::equality_comparable_with<T, U>)
    Response RadioButton(
        T& currentValue,
        const U& optionValue,
        RadioOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return m_ctx->RadioButton(currentValue, optionValue, std::move(opts), loc);
    }

    Response RadioButton(
        bool& checked,
        RadioOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return m_ctx->RadioButton(checked, std::move(opts), loc);
    }

    Response InputField(
        std::string& value,
        InputFieldOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return m_ctx->InputField(value, std::move(opts), loc);
    }

    Response InputField(
        std::string_view value,
        InputFieldOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return m_ctx->InputField(value, std::move(opts), loc);
    }

    template <SpinBoxNumeric T>
    Response SpinBox(
        T& value,
        SpinBoxOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return m_ctx->SpinBox(value, std::move(opts), loc);
    }

    TextAreaResponse TextArea(
        std::string& value,
        const TextAreaOptions& opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return m_ctx->TextArea(value, opts, loc);
    }

    TextAreaResponse TextArea(
        std::string_view value,
        const TextAreaOptions& opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return m_ctx->TextArea(value, opts, loc);
    }

    template <SelectTarget T>
    Response Select(
        T& selected,
        std::span<const SelectItem> items,
        SelectOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return m_ctx->Select(selected, items, std::move(opts), loc);
    }

    template <SelectTarget T>
    Response Select(
        T& selected,
        std::initializer_list<SelectItem> items,
        SelectOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return m_ctx->Select(selected, items, std::move(opts), loc);
    }

    template <SelectTarget T>
    Response Select(
        T& selected,
        std::span<const std::string_view> items,
        SelectOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return m_ctx->Select(selected, items, std::move(opts), loc);
    }

    template <SelectTarget T>
    Response Select(
        T& selected,
        std::initializer_list<std::string_view> items,
        SelectOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return m_ctx->Select(selected, items, std::move(opts), loc);
    }

    template <typename IndexT>
    Response TabBar(
        IndexT& activeIndex,
        std::span<const TabItem> tabs,
        TabBarOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) requires (std::is_integral_v<IndexT>) {
        return m_ctx->TabBar(activeIndex, tabs, std::move(opts), loc);
    }

    template <typename IndexT>
    Response TabBar(
        IndexT& activeIndex,
        std::initializer_list<TabItem> tabs,
        TabBarOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) requires (std::is_integral_v<IndexT>) {
        return m_ctx->TabBar(activeIndex, tabs, std::move(opts), loc);
    }

    template <typename IndexT>
    Response SidebarMenu(
        IndexT& activeIndex,
        std::span<const SidebarMenuItem> items,
        SidebarMenuOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) requires (std::is_integral_v<IndexT> || std::is_enum_v<IndexT>) {
        return m_ctx->SidebarMenu(activeIndex, items, std::move(opts), loc);
    }

    template <typename IndexT>
    Response SidebarMenu(
        IndexT& activeIndex,
        std::initializer_list<SidebarMenuItem> items,
        SidebarMenuOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) requires (std::is_integral_v<IndexT> || std::is_enum_v<IndexT>) {
        return m_ctx->SidebarMenu(activeIndex, items, std::move(opts), loc);
    }

    template <SliderNumeric T>
    Response Slider(
        T& value,
        T min,
        T max,
        SliderOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return m_ctx->Slider(value, min, max, std::move(opts), loc);
    }

    template <SliderNumeric T>
    Response RangeSlider(
        T& lowValue,
        T& highValue,
        T min,
        T max,
        RangeSliderOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return m_ctx->RangeSlider(lowValue, highValue, min, max, std::move(opts), loc);
    }

    Response ProgressBar(
        double value,
        double min,
        double max,
        ProgressBarOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return m_ctx->ProgressBar(value, min, max, std::move(opts), loc);
    }

    Response ProgressBar(
        double percent0to100,
        ProgressBarOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return m_ctx->ProgressBar(percent0to100, std::move(opts), loc);
    }

    template <typename IndexT>
    Response StepSlider(
        IndexT& index,
        size_t totalSteps,
        StepSliderOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) requires (std::is_integral_v<IndexT>) {
        return m_ctx->StepSlider(index, totalSteps, std::move(opts), loc);
    }

    template <typename IndexT>
    Response StepSlider(
        IndexT& index,
        std::span<const std::string_view> labels,
        StepSliderOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) requires (std::is_integral_v<IndexT>) {
        return m_ctx->StepSlider(index, labels, std::move(opts), loc);
    }

    template <typename IndexT>
    Response StepSlider(
        IndexT& index,
        std::initializer_list<std::string_view> labels,
        StepSliderOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) requires (std::is_integral_v<IndexT>) {
        return m_ctx->StepSlider(index, labels, std::move(opts), loc);
    }

    Response StepSlider(
        UiSize& size,
        StepSliderOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return m_ctx->StepSlider(size, std::move(opts), loc);
    }

    Response StepSlider(
        UiVariant& variant,
        StepSliderOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return m_ctx->StepSlider(variant, std::move(opts), loc);
    }

    template <SliderNumeric T>
    Response Knob(
        T& value,
        T min,
        T max,
        KnobOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return m_ctx->Knob(value, min, max, std::move(opts), loc);
    }

    template <typename IndexT>
    Response Segmented(
        IndexT& selectedIndex,
        std::span<const SegmentedItem> items,
        SegmentedOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) requires (std::is_integral_v<IndexT> || std::is_enum_v<IndexT>) {
        return m_ctx->Segmented(selectedIndex, items, std::move(opts), loc);
    }

    template <typename IndexT>
    Response Segmented(
        IndexT& selectedIndex,
        std::initializer_list<SegmentedItem> items,
        SegmentedOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) requires (std::is_integral_v<IndexT> || std::is_enum_v<IndexT>) {
        return m_ctx->Segmented(selectedIndex, items, std::move(opts), loc);
    }

    template <typename IndexT>
    Response Segmented(
        IndexT& selectedIndex,
        std::span<const std::string_view> items,
        SegmentedOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) requires (std::is_integral_v<IndexT> || std::is_enum_v<IndexT>) {
        return m_ctx->Segmented(selectedIndex, items, std::move(opts), loc);
    }

    template <typename IndexT>
    Response Segmented(
        IndexT& selectedIndex,
        std::initializer_list<std::string_view> items,
        SegmentedOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) requires (std::is_integral_v<IndexT> || std::is_enum_v<IndexT>) {
        return m_ctx->Segmented(selectedIndex, items, std::move(opts), loc);
    }

    Response SegmentedMask(
        uint32_t& selectedMask,
        std::span<const SegmentedItem> items,
        SegmentedOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return m_ctx->SegmentedMask(selectedMask, items, std::move(opts), loc);
    }

    Response SegmentedMask(
        uint32_t& selectedMask,
        std::initializer_list<SegmentedItem> items,
        SegmentedOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return m_ctx->SegmentedMask(selectedMask, items, std::move(opts), loc);
    }

    Response SegmentedMask(
        uint32_t& selectedMask,
        std::span<const std::string_view> items,
        SegmentedOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return m_ctx->SegmentedMask(selectedMask, items, std::move(opts), loc);
    }

    Response SegmentedMask(
        uint32_t& selectedMask,
        std::initializer_list<std::string_view> items,
        SegmentedOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return m_ctx->SegmentedMask(selectedMask, items, std::move(opts), loc);
    }

    Response ColorPicker(
        Color& value,
        ColorPickerOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return m_ctx->ColorPicker(value, std::move(opts), loc);
    }

    [[nodiscard]] ModalScope Modal(
        bool& open,
        const ModalOptions& opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return TransferToContainer(m_ctx->Modal(open, opts, loc));
    }

    ContextMenuResponse ContextMenu(
        const Response& triggerTarget,
        std::span<const ContextMenuItem> items,
        const ContextMenuOptions& opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return m_ctx->ContextMenu(triggerTarget, items, opts, loc);
    }

    ContextMenuResponse ContextMenu(
        const Response& triggerTarget,
        std::initializer_list<ContextMenuItem> items,
        const ContextMenuOptions& opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return m_ctx->ContextMenu(triggerTarget, items, opts, loc);
    }

    ContextMenuResponse ContextMenu(
        Rect triggerRect,
        std::span<const ContextMenuItem> items,
        const ContextMenuOptions& opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return m_ctx->ContextMenu(triggerRect, items, opts, loc);
    }

    ContextMenuResponse ContextMenu(
        Rect triggerRect,
        std::initializer_list<ContextMenuItem> items,
        const ContextMenuOptions& opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return m_ctx->ContextMenu(triggerRect, items, opts, loc);
    }

    ContextMenuResponse ContextMenu(
        std::span<const ContextMenuItem> items,
        const ContextMenuOptions& opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return m_ctx->ContextMenu(items, opts, loc);
    }

    ContextMenuResponse ContextMenu(
        std::initializer_list<ContextMenuItem> items,
        const ContextMenuOptions& opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return m_ctx->ContextMenu(items, opts, loc);
    }

    ContextMenuResponse ContextMenu(
        bool& open,
        std::span<const ContextMenuItem> items,
        const ContextMenuOptions& opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return m_ctx->ContextMenu(open, items, opts, loc);
    }

    ContextMenuResponse ContextMenu(
        bool& open,
        std::initializer_list<ContextMenuItem> items,
        const ContextMenuOptions& opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return m_ctx->ContextMenu(open, items, opts, loc);
    }

    ContextMenuResponse ContextMenu(
        bool& open,
        Vec2 anchorPos,
        std::span<const ContextMenuItem> items,
        const ContextMenuOptions& opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return m_ctx->ContextMenu(open, anchorPos, items, opts, loc);
    }

    ContextMenuResponse ContextMenu(
        bool& open,
        Vec2 anchorPos,
        std::initializer_list<ContextMenuItem> items,
        const ContextMenuOptions& opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return m_ctx->ContextMenu(open, anchorPos, items, opts, loc);
    }

private:
    template <typename ScopeT>
    ScopeT TransferToContainer(ScopeT&& scope) noexcept {
        if (m_ctx) {
            m_ctx->PopSize();
            m_ctx = nullptr;
        }
        return std::move(scope);
    }

    void RestoreAccordionBodySize(Context* ctx, AccordionScope& scope, const AccordionOptions& opts) noexcept {
        if (ctx) {
            ctx->PopSize();
            m_ctx = nullptr;
            if (Node* bodyNode = scope.GetNode()) {
                ctx->ReplaceTopSize(m_prevSize);
                bodyNode->uiSize = m_prevSize;
                bodyNode->gap = ctx->GetTheme().ResolveGap(opts.gap, m_prevSize);
                if (opts.padding.has_value()) {
                    bodyNode->padding = ctx->GetTheme().ResolvePadding(*opts.padding, m_prevSize);
                }
            }
        }
    }

    Context* m_ctx{nullptr};
    UiSize m_prevSize{UiSize::Md};
    UiSize m_size{UiSize::Md};
};

inline SizeScope Context::Size(UiSize size) noexcept {
    return SizeScope(this, size);
}

} // namespace guikit
