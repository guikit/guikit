#pragma once

#include <chrono>
#include <functional>
#include <initializer_list>
#include <optional>
#include <span>
#include <source_location>
#include <string_view>
#include <unordered_map>
#include <vector>
#include "guikit/backend/ICanvas.hpp"
#include "guikit/core/BumpAllocator.hpp"
#include "guikit/core/Id.hpp"
#include "guikit/core/Response.hpp"
#include "guikit/core/Tokens.hpp"
#include "guikit/core/Types.hpp"
#include "guikit/core/UiString.hpp"
#include "guikit/core/UiStyle.hpp"
#include "guikit/graphics/Icon.hpp"
#include "guikit/layout/LayoutSolver.hpp"
#include "guikit/layout/Node.hpp"
#include "guikit/layout/Scope.hpp"
#include "guikit/widgets/Button.hpp"
#include "guikit/widgets/Carousel.hpp"
#include "guikit/widgets/Checkbox.hpp"
#include "guikit/widgets/Heading.hpp"
#include "guikit/widgets/InputField.hpp"
#include "guikit/widgets/Select.hpp"
#include "guikit/widgets/Switch.hpp"
#include "guikit/widgets/TabBar.hpp"
#include "guikit/widgets/Text.hpp"
#include "guikit/widgets/Toolbar.hpp"

namespace guikit {

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

    /// Sets the active color theme mode (Dark or Light) and adapts clear color & palette.
    void SetThemeMode(ThemeMode mode) noexcept;

    /// Returns the current theme mode.
    [[nodiscard]] ThemeMode GetThemeMode() const noexcept { return m_themeMode; }

    /// Returns true if dark mode is active.
    [[nodiscard]] bool IsDarkMode() const noexcept { return m_themeMode == ThemeMode::Dark; }

    /// Returns the active theme palette tokens.
    [[nodiscard]] ThemePalette GetThemePalette() const noexcept { return guikit::GetThemePalette(m_themeMode); }

    /// Sets the clear color used by EndFrame when clearing dirty damage regions.
    void SetClearColor(Color color) noexcept { m_clearColor = color; }

    /// Returns the current clear color.
    [[nodiscard]] Color GetClearColor() const noexcept { return m_clearColor; }

    // --- Style & Density ---
    /// Sets the application UI style metrics and spacing scale.
    void SetStyle(const UiStyle& style) noexcept { m_style = style; InvalidateAll(); }

    /// Returns the current application UI style.
    [[nodiscard]] const UiStyle& GetStyle() const noexcept { return m_style; }
    [[nodiscard]] UiStyle& GetStyle() noexcept { return m_style; }

    /// Sets the UI density preset (Standard, Compact, Touch).
    void SetDensity(Density density) noexcept { m_style.SetDensity(density); InvalidateAll(); }

    /// Returns the current UI density preset.
    [[nodiscard]] Density GetDensity() const noexcept { return m_style.GetDensity(); }

    /// Returns true if this frame produced any visual damage requiring rasterization / blitting.
    [[nodiscard]] bool HasDirtyDamage() const noexcept { return m_hasDirtyDamage; }

    /// Returns true if the entire frame required a full repaint (e.g. window resize or tab switch).
    [[nodiscard]] bool IsFullRepaint() const noexcept { return m_isFullRepaint; }

    /// Returns the active bounding rectangle of the dirty damage region for this frame (in logical points).
    [[nodiscard]] Rect GetDirtyRect() const noexcept { return m_dirtyRect; }

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

    // --- Container Scopes ---
    RowScope Row(
        RowOptions opts = {},
        std::source_location loc = std::source_location::current()
    );

    ColumnScope Column(
        ColumnOptions opts = {},
        std::source_location loc = std::source_location::current()
    );

    BoxScope Box(
        BoxOptions opts = {},
        std::source_location loc = std::source_location::current()
    );

    WrapScope Wrap(
        WrapOptions opts = {},
        std::source_location loc = std::source_location::current()
    );

    CellGridScope CellGrid(
        CellGridOptions opts = {},
        std::source_location loc = std::source_location::current()
    );

    CellGridScope Grid(
        CellGridOptions opts = {},
        std::source_location loc = std::source_location::current()
    );

    ScrollAreaScope ScrollArea(
        ScrollAreaOptions opts = {},
        std::source_location loc = std::source_location::current()
    );

    CarouselScope Carousel(
        CarouselOptions opts = {},
        std::source_location loc = std::source_location::current()
    );

    CardScope Card(
        CardOptions opts = {},
        std::source_location loc = std::source_location::current()
    );

    void Spacer(
        float flex = 1.0f,
        std::source_location loc = std::source_location::current()
    );

    // --- Scale & State Scopes ---
    void PushSize(UiSize size);
    void PopSize();
    [[nodiscard]] UiSize GetCurrentSize() const;

    void PushDisabled(bool disabled = true);
    void PopDisabled();
    [[nodiscard]] bool IsDisabled() const;

    template<typename Fn>
    void Disabled(bool disabled, Fn&& fn) {
        PushDisabled(disabled);
        fn();
        PopDisabled();
    }

    // --- Widgets ---
    Response Button(
        ButtonOptions opts,
        std::source_location loc = std::source_location::current()
    );

    Response Button(
        std::string_view label,
        ButtonOptions opts = {},
        std::source_location loc = std::source_location::current()
    );

    Response Text(
        std::string_view label,
        TextOptions opts = {},
        std::source_location loc = std::source_location::current()
    );

    template <typename... Args>
    Response TextFormat(
        std::format_string<Args...> fmt,
        Args&&... args
    ) {
        return Text(Format(fmt, std::forward<Args>(args)...));
    }

    // Semantic typography shorthands
    Response Display(std::string_view label, UiVariant variant = UiVariant::Default, std::source_location loc = std::source_location::current());
    Response Headline(std::string_view label, UiVariant variant = UiVariant::Default, std::source_location loc = std::source_location::current());
    Response Title(std::string_view label, UiVariant variant = UiVariant::Default, std::source_location loc = std::source_location::current());
    Response Subtitle(std::string_view label, UiVariant variant = UiVariant::Default, std::source_location loc = std::source_location::current());
    Response Body(std::string_view label, UiVariant variant = UiVariant::Default, std::source_location loc = std::source_location::current());
    Response Caption(std::string_view label, UiVariant variant = UiVariant::Muted, std::source_location loc = std::source_location::current());
    Response Code(std::string_view label, UiVariant variant = UiVariant::Default, std::source_location loc = std::source_location::current());

    // --- Heading Widget ---
    Response Heading(
        HeadingOptions opts,
        std::source_location loc = std::source_location::current()
    );

    // --- Switch Widget ---
    Response Switch(
        std::string_view label,
        bool* value,
        SwitchOptions opts = {},
        std::source_location loc = std::source_location::current()
    );

    Response Switch(
        bool* value,
        SwitchOptions opts = {},
        std::source_location loc = std::source_location::current()
    );

    Response Switch(
        std::string_view label,
        bool checked,
        SwitchOptions opts = {},
        std::source_location loc = std::source_location::current()
    );

    Response Switch(
        bool checked,
        SwitchOptions opts = {},
        std::source_location loc = std::source_location::current()
    );

    // --- Checkbox Widget ---
    Response Checkbox(
        std::string_view label,
        bool* value,
        CheckboxOptions opts = {},
        std::source_location loc = std::source_location::current()
    );

    Response Checkbox(
        bool* value,
        CheckboxOptions opts = {},
        std::source_location loc = std::source_location::current()
    );

    Response Checkbox(
        std::string_view label,
        bool checked,
        CheckboxOptions opts = {},
        std::source_location loc = std::source_location::current()
    );

    Response Checkbox(
        bool checked,
        CheckboxOptions opts = {},
        std::source_location loc = std::source_location::current()
    );

    /// Convenience aliases for developers familiar with Toggle terminology.
    Response Toggle(
        std::string_view label,
        bool* value,
        SwitchOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return Switch(label, value, opts, loc);
    }

    Response Toggle(
        bool* value,
        SwitchOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return Switch(value, opts, loc);
    }

    Response Toggle(
        std::string_view label,
        bool checked,
        SwitchOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return Switch(label, checked, opts, loc);
    }

    Response Toggle(
        bool checked,
        SwitchOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return Switch(checked, opts, loc);
    }

    // --- InputField Widget ---
    Response InputField(
        std::string_view label,
        std::string& value,
        InputFieldOptions opts = {},
        std::source_location loc = std::source_location::current()
    );

    Response InputField(
        std::string& value,
        InputFieldOptions opts = {},
        std::source_location loc = std::source_location::current()
    );

    Response InputField(
        std::string_view label,
        float& value,
        InputFieldOptions opts = {},
        std::source_location loc = std::source_location::current()
    );

    Response InputField(
        float& value,
        InputFieldOptions opts = {},
        std::source_location loc = std::source_location::current()
    );

    Response InputField(
        std::string_view label,
        double& value,
        InputFieldOptions opts = {},
        std::source_location loc = std::source_location::current()
    );

    Response InputField(
        double& value,
        InputFieldOptions opts = {},
        std::source_location loc = std::source_location::current()
    );

    Response InputField(
        std::string_view label,
        int& value,
        InputFieldOptions opts = {},
        std::source_location loc = std::source_location::current()
    );

    Response InputField(
        int& value,
        InputFieldOptions opts = {},
        std::source_location loc = std::source_location::current()
    );

    Response InputField(
        std::string_view label,
        std::string_view value,
        InputFieldOptions opts = {},
        std::source_location loc = std::source_location::current()
    );

    template <typename T>
    Response InputNumber(
        std::string_view label,
        T& value,
        std::string_view unit = {},
        InputFieldOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        if (!unit.empty()) {
            opts.suffix = unit;
        }
        return InputField(label, value, opts, loc);
    }

    // --- Select (Dropdown) Widget ---
    Response Select(
        std::string_view label,
        size_t& selectedIndex,
        std::span<const std::string_view> items,
        SelectOptions opts = {},
        std::source_location loc = std::source_location::current()
    );

    Response Select(
        size_t& selectedIndex,
        std::span<const std::string_view> items,
        SelectOptions opts = {},
        std::source_location loc = std::source_location::current()
    );

    Response Select(
        std::string_view label,
        int& selectedIndex,
        std::span<const std::string_view> items,
        SelectOptions opts = {},
        std::source_location loc = std::source_location::current()
    );

    Response Select(
        int& selectedIndex,
        std::span<const std::string_view> items,
        SelectOptions opts = {},
        std::source_location loc = std::source_location::current()
    );

    Response Select(
        std::string_view label,
        std::string& selectedValue,
        std::span<const std::string_view> items,
        SelectOptions opts = {},
        std::source_location loc = std::source_location::current()
    );

    Response Select(
        std::string& selectedValue,
        std::span<const std::string_view> items,
        SelectOptions opts = {},
        std::source_location loc = std::source_location::current()
    );

    Response Select(
        std::string_view label,
        std::string_view& selectedValue,
        std::span<const std::string_view> items,
        SelectOptions opts = {},
        std::source_location loc = std::source_location::current()
    );

    Response Select(
        std::string_view& selectedValue,
        std::span<const std::string_view> items,
        SelectOptions opts = {},
        std::source_location loc = std::source_location::current()
    );

    Response Select(
        std::string_view label,
        size_t& selectedIndex,
        std::initializer_list<std::string_view> items,
        SelectOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return Select(label, selectedIndex, std::span<const std::string_view>(items.begin(), items.size()), opts, loc);
    }

    Response Select(
        size_t& selectedIndex,
        std::initializer_list<std::string_view> items,
        SelectOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return Select(selectedIndex, std::span<const std::string_view>(items.begin(), items.size()), opts, loc);
    }

    Response Select(
        std::string_view label,
        int& selectedIndex,
        std::initializer_list<std::string_view> items,
        SelectOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return Select(label, selectedIndex, std::span<const std::string_view>(items.begin(), items.size()), opts, loc);
    }

    Response Select(
        int& selectedIndex,
        std::initializer_list<std::string_view> items,
        SelectOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return Select(selectedIndex, std::span<const std::string_view>(items.begin(), items.size()), opts, loc);
    }

    Response Select(
        std::string_view label,
        std::string& selectedValue,
        std::initializer_list<std::string_view> items,
        SelectOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return Select(label, selectedValue, std::span<const std::string_view>(items.begin(), items.size()), opts, loc);
    }

    Response Select(
        std::string& selectedValue,
        std::initializer_list<std::string_view> items,
        SelectOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return Select(selectedValue, std::span<const std::string_view>(items.begin(), items.size()), opts, loc);
    }

    // --- TabBar Component ---
    Response TabBar(
        int& activeIndex,
        std::span<const TabItem> tabs,
        TabBarOptions opts = {},
        std::source_location loc = std::source_location::current()
    );

    Response TabBar(
        int& activeIndex,
        std::initializer_list<TabItem> tabs,
        TabBarOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return TabBar(activeIndex, std::span<const TabItem>(tabs.begin(), tabs.size()), opts, loc);
    }

    Response TabBar(
        int& activeIndex,
        std::span<const std::string_view> tabs,
        TabBarOptions opts = {},
        std::source_location loc = std::source_location::current()
    );

    Response TabBar(
        int& activeIndex,
        std::initializer_list<std::string_view> tabs,
        TabBarOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return TabBar(activeIndex, std::span<const std::string_view>(tabs.begin(), tabs.size()), opts, loc);
    }

    Response TabBar(
        size_t& activeIndex,
        std::span<const TabItem> tabs,
        TabBarOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        int idx = static_cast<int>(activeIndex);
        Response r = TabBar(idx, tabs, opts, loc);
        if (r.changed && idx >= 0) activeIndex = static_cast<size_t>(idx);
        return r;
    }

    Response TabBar(
        size_t& activeIndex,
        std::initializer_list<TabItem> tabs,
        TabBarOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return TabBar(activeIndex, std::span<const TabItem>(tabs.begin(), tabs.size()), opts, loc);
    }

    Response TabBar(
        size_t& activeIndex,
        std::span<const std::string_view> tabs,
        TabBarOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        int idx = static_cast<int>(activeIndex);
        Response r = TabBar(idx, tabs, opts, loc);
        if (r.changed && idx >= 0) activeIndex = static_cast<size_t>(idx);
        return r;
    }

    Response TabBar(
        size_t& activeIndex,
        std::initializer_list<std::string_view> tabs,
        TabBarOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return TabBar(activeIndex, std::span<const std::string_view>(tabs.begin(), tabs.size()), opts, loc);
    }

    // --- Toolbar Component ---
    ToolbarScope Toolbar(
        ToolbarOptions opts = {},
        std::source_location loc = std::source_location::current()
    );

    Response ToolButton(
        std::string_view label = {},
        ToolButtonOptions opts = {},
        std::source_location loc = std::source_location::current()
    );

    // --- String & Formatting Services ---
    [[nodiscard]] UiString String(std::string_view text);
    [[nodiscard]] std::string_view InternString(std::string_view text);

    template <typename... Args>
    [[nodiscard]] UiString Format(std::format_string<Args...> fmt, Args&&... args) {
        BasicUiString<128> temp;
        temp.Format(fmt, std::forward<Args>(args)...);
        return UiString(InternString(temp.View()));
    }

    // --- Core Engine Services for Widgets ---
    Id MakeChildId(std::source_location loc);
    Response Interact(Id id);
    Node* CreateNode(NodeType type, Id id);
    void AdaptChildToContainer(Node* node);
    [[nodiscard]] Rect GetPrevFrameBounds(Id id) const;

    // --- Internal Hierarchy & Scope Management ---
    void PushContainer(Node* node);
    void PopContainer(Node* node);

    // Imperative actions requested from Response
    void RequestFocus(Id id);
    [[nodiscard]] Id GetFocusedId() const noexcept { return m_focusedId; }
    [[nodiscard]] bool IsFocused(Id id) const noexcept { return m_focusedId == id && id.IsValid(); }
    [[nodiscard]] bool HasActiveTextEdit(Id id) const noexcept { return m_editState.id == id && id.IsValid(); }
    void ClearFocus() noexcept { m_focusedId = Id::Null(); }

    std::string_view GetOrStartTextEdit(
        Id id,
        std::string_view initialText,
        size_t& outCursor,
        bool& outChanged,
        bool& outSubmitted
    );

    // --- Cursor Configuration & State ---
    void SetCursorConfig(const CursorConfig& config) noexcept { m_cursorConfig = config; }
    [[nodiscard]] const CursorConfig& GetCursorConfig() const noexcept { return m_cursorConfig; }
    [[nodiscard]] bool IsCursorVisible(std::optional<CursorBlinkMode> modeOverride = std::nullopt) const noexcept;
    void ResetCursorBlink() noexcept;

    // --- Deterministic Time Control (for unit tests / animations) ---
    void SetCurrentTime(double timeSeconds) noexcept;
    void AdvanceTime(double deltaSeconds) noexcept;
    [[nodiscard]] double GetCurrentTime() const noexcept { return m_currentTimeSec; }

    // --- Popup & Overlay Management ---
    void OpenPopup(Id id, Rect bounds);
    void ClosePopup();
    [[nodiscard]] bool IsPopupOpen(Id id) const noexcept { return m_openPopupId == id && id.IsValid(); }
    [[nodiscard]] Id GetOpenPopupId() const noexcept { return m_openPopupId; }
    [[nodiscard]] Rect GetOpenPopupRect() const noexcept { return m_openPopupRect; }
    void RegisterOverlay(std::function<void(ICanvas&)> drawFn);

    // --- Input Queries & Consumption ---
    [[nodiscard]] Vec2 GetMousePos() const noexcept { return m_mousePos; }
    [[nodiscard]] bool IsMouseDown() const noexcept { return m_mouseDown; }
    [[nodiscard]] bool IsMouseClicked() const noexcept { return m_mouseClicked; }
    [[nodiscard]] bool IsMouseReleased() const noexcept { return m_mouseReleased; }
    void ConsumeMouseClick() noexcept { m_mouseClicked = false; m_mouseReleased = false; }
    [[nodiscard]] Vec2 GetViewportSize() const noexcept { return m_viewportSize; }
    [[nodiscard]] Vec2 GetMouseWheel() const noexcept { return m_mouseWheel; }
    bool ConsumeKey(Key key);
    void ScrollToId(Id id);

private:
    void RenderNodeRecursive(Node* node, ICanvas& canvas, const Rect& dirtyRect);
    void UpdateScrollAreasRecursive(Node* node);

    // Widget snapshot for frame-to-frame change detection
    struct WidgetSnapshot {
        Rect rect{};
        uint64_t visualHash{0};
        uint32_t flags{0}; // bits: 0=hovered, 1=pressed, 2=disabled, 3=checked, 4=cursorVisible, 5=activeEdit, 6=tabActive
        size_t cursor{0};
        uint32_t bgColor{0};
        uint32_t borderColor{0};

        bool operator==(const WidgetSnapshot& o) const noexcept {
            return rect == o.rect &&
                   visualHash == o.visualHash &&
                   flags == o.flags &&
                   cursor == o.cursor &&
                   bgColor == o.bgColor &&
                   borderColor == o.borderColor;
        }

        bool operator!=(const WidgetSnapshot& o) const noexcept {
            return !(*this == o);
        }
    };

    std::unordered_map<Id, WidgetSnapshot> m_prevSnapshots;
    std::vector<Rect> m_explicitDamageRects;
    Rect m_dirtyRect{0.0f, 0.0f, 0.0f, 0.0f};
    ThemeMode m_themeMode{ThemeMode::Dark};
    UiStyle m_style{UiStyle::Standard()};
    Color m_clearColor{11, 15, 25, 255}; // Tailwind Slate-950 #0B0F19
    bool m_hasDirtyDamage{true};
    bool m_isFullRepaint{true};
    bool m_forceFullRepaint{true};
    bool m_dirtyRectEnabled{true};

    BumpAllocator m_arena;
    std::vector<Node*> m_containerStack;
    std::vector<UiSize> m_sizeStack;
    std::vector<bool> m_disabledStack;
    Node* m_root{nullptr};

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
    int m_repaintFrames{3};
    int m_idleHeartbeatMs{500};

    Id m_hoveredId{Id::Null()};
    Id m_activeId{Id::Null()};
    Id m_focusedId{Id::Null()};

    // Active text edit state
    struct ActiveEditState {
        Id id{Id::Null()};
        std::string text;
        size_t cursor{0};
        bool changed{false};
        bool submitted{false};
    };
    ActiveEditState m_editState;

    struct QueuedKeyEvent {
        Key key;
        KeyModifiers mods;
    };
    std::vector<std::string> m_pendingTextInput;
    std::vector<QueuedKeyEvent> m_pendingKeyEvents;

    // Previous frame widget bounds for instantaneous hit testing
    std::unordered_map<Id, Rect> m_prevFrameBounds;
    std::unordered_map<Id, Rect> m_currFrameBounds;

    // Persistent scroll state for ScrollArea viewports
    struct ScrollState {
        Vec2 offset{0.0f, 0.0f};
        bool draggingY{false};
        bool draggingX{false};
        float dragStartOffset{0.0f};
        Vec2 dragStartMouse{0.0f, 0.0f};
    };
    std::unordered_map<Id, ScrollState> m_scrollStates;

    // Floating popup & overlay state
    Id m_openPopupId{Id::Null()};
    Rect m_openPopupRect{0.0f, 0.0f, 0.0f, 0.0f};
    std::vector<std::function<void(ICanvas&)>> m_overlayPasses;
    Vec2 m_mouseWheel{0.0f, 0.0f};
    std::unordered_map<Id, int> m_lastActiveTabs;

    friend class CellGridScope;
    friend class TabBar;
    friend class Carousel;
    CellScope CreateCell(int span, CellOptions opts, std::source_location loc = std::source_location::current());
};

// Inline implementations of Response imperative actions
inline void Response::Focus() {
    if (ctx && id.IsValid()) {
        ctx->RequestFocus(id);
    }
}

inline void Response::ScrollTo() {
    if (ctx && id.IsValid()) {
        ctx->ScrollToId(id);
    }
}

// Inline implementation of ContainerScope destructor
inline ContainerScope::~ContainerScope() {
    if (m_ctx && m_node) {
        m_ctx->PopContainer(m_node);
        m_ctx = nullptr;
        m_node = nullptr;
    }
}

// Inline implementations of CellGridScope slot helpers
inline CellScope CellGridScope::Cell12(CellOptions opts, std::source_location loc) {
    return m_ctx ? m_ctx->CreateCell(12, opts, loc) : CellScope(nullptr, nullptr);
}
inline CellScope CellGridScope::Cell8(CellOptions opts, std::source_location loc) {
    return m_ctx ? m_ctx->CreateCell(8, opts, loc) : CellScope(nullptr, nullptr);
}
inline CellScope CellGridScope::Cell6(CellOptions opts, std::source_location loc) {
    return m_ctx ? m_ctx->CreateCell(6, opts, loc) : CellScope(nullptr, nullptr);
}
inline CellScope CellGridScope::Cell4(CellOptions opts, std::source_location loc) {
    return m_ctx ? m_ctx->CreateCell(4, opts, loc) : CellScope(nullptr, nullptr);
}
inline CellScope CellGridScope::Cell3(CellOptions opts, std::source_location loc) {
    return m_ctx ? m_ctx->CreateCell(3, opts, loc) : CellScope(nullptr, nullptr);
}
inline CellScope CellGridScope::Cell2(CellOptions opts, std::source_location loc) {
    return m_ctx ? m_ctx->CreateCell(2, opts, loc) : CellScope(nullptr, nullptr);
}
inline CellScope CellGridScope::Cell1(CellOptions opts, std::source_location loc) {
    return m_ctx ? m_ctx->CreateCell(1, opts, loc) : CellScope(nullptr, nullptr);
}

} // namespace guikit
