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
#include "guikit/panels/Box.hpp"
#include "guikit/panels/Card.hpp"
#include "guikit/panels/Carousel.hpp"
#include "guikit/panels/ScrollArea.hpp"
#include "guikit/panels/Toolbar.hpp"
#include "guikit/widgets/Button.hpp"
#include "guikit/widgets/Checkbox.hpp"
#include "guikit/widgets/Heading.hpp"
#include "guikit/widgets/InputField.hpp"
#include "guikit/widgets/Select.hpp"
#include "guikit/widgets/Switch.hpp"
#include "guikit/widgets/TabBar.hpp"
#include "guikit/widgets/Text.hpp"

namespace guikit {

class Context;

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
concept InputFieldEditable =
    std::is_same_v<std::decay_t<T>, std::string> ||
    std::is_same_v<std::decay_t<T>, float> ||
    std::is_same_v<std::decay_t<T>, double> ||
    std::is_same_v<std::decay_t<T>, int>;

template <typename T>
concept SelectTarget =
    std::is_same_v<std::decay_t<T>, size_t> ||
    std::is_same_v<std::decay_t<T>, int> ||
    std::is_same_v<std::decay_t<T>, std::string> ||
    std::is_same_v<std::decay_t<T>, std::string_view>;

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

    /// Returns the active UI theme.
    [[nodiscard]] const UiTheme& GetTheme() const noexcept { return m_theme; }
    [[nodiscard]] UiTheme& GetTheme() noexcept { return m_theme; }

    /// Sets the clear color used by EndFrame when clearing dirty damage regions.
    void SetClearColor(Color color) noexcept { m_clearColor = color; }

    /// Returns the current clear color.
    [[nodiscard]] Color GetClearColor() const noexcept { return m_clearColor; }

    // --- Style & Density (Backward compatibility alias) ---
    /// Sets the application UI style metrics and spacing scale.
    void SetStyle(const UiTheme& style) noexcept { SetTheme(style); }

    /// Returns the current application UI style.
    [[nodiscard]] const UiTheme& GetStyle() const noexcept { return m_theme; }
    [[nodiscard]] UiTheme& GetStyle() noexcept { return m_theme; }

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

    [[nodiscard]] GridScope CellGrid(
        GridOptions opts = {},
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

    // --- Checkbox Widget ---
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

    /// Convenience aliases for developers familiar with Toggle terminology.
    Response Toggle(
        bool& value,
        SwitchOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return Switch(value, opts, loc);
    }

    Response Toggle(
        const bool& checked,
        SwitchOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return Switch(checked, opts, loc);
    }

    Response Toggle(const bool*, SwitchOptions = {}, std::source_location = std::source_location::current()) = delete;

    // --- InputField Widget ---
    template <InputFieldEditable T>
    Response InputField(
        T& value,
        InputFieldOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        if constexpr (std::is_same_v<std::decay_t<T>, std::string>) {
            return Run<guikit::InputField>(&value, nullptr, nullptr, nullptr, std::string_view{}, opts, loc);
        } else if constexpr (std::is_same_v<std::decay_t<T>, float>) {
            return Run<guikit::InputField>(nullptr, &value, nullptr, nullptr, std::string_view{}, opts, loc);
        } else if constexpr (std::is_same_v<std::decay_t<T>, double>) {
            return Run<guikit::InputField>(nullptr, nullptr, &value, nullptr, std::string_view{}, opts, loc);
        } else if constexpr (std::is_same_v<std::decay_t<T>, int>) {
            return Run<guikit::InputField>(nullptr, nullptr, nullptr, &value, std::string_view{}, opts, loc);
        }
    }

    Response InputField(
        std::string_view value,
        InputFieldOptions opts = {},
        std::source_location loc = std::source_location::current()
    );

    template <InputFieldEditable T>
    Response InputNumber(
        T& value,
        InputFieldOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return InputField(value, opts, loc);
    }

    // --- Select (Dropdown) Widget ---
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

    template <typename IndexT>
    Response TabBar(
        IndexT& activeIndex,
        std::span<const std::string_view> tabs,
        TabBarOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) requires (std::is_integral_v<IndexT>) {
        std::vector<TabItem> items;
        items.reserve(tabs.size());
        for (const auto& label : tabs) {
            items.push_back(TabItem{.label = label});
        }
        return TabBar(activeIndex, std::span<const TabItem>(items), opts, loc);
    }

    template <typename IndexT>
    Response TabBar(
        IndexT& activeIndex,
        std::initializer_list<std::string_view> tabs,
        TabBarOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) requires (std::is_integral_v<IndexT>) {
        return TabBar(activeIndex, std::span<const std::string_view>(tabs.begin(), tabs.size()), opts, loc);
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

    // --- String & Formatting Services ---
    [[nodiscard]] UiString String(std::string_view text);
    [[nodiscard]] std::string_view AllocString(std::string_view text);

    [[deprecated("Use AllocString instead")]]
    [[nodiscard]] std::string_view InternString(std::string_view text) {
        return AllocString(text);
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

    [[nodiscard]] Id GetActiveId() const noexcept { return m_activeId; }
    void SetActiveId(Id id) noexcept { m_activeId = id; }
    [[nodiscard]] uint64_t GetFrameIndex() const noexcept { return m_frameIndex; }
    [[nodiscard]] size_t GetCustomStateCount() const noexcept { return m_customStates.size(); }

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
    UiTheme m_theme{UiTheme::Dark()};
    Color m_clearColor{11, 15, 25, 255}; // Tailwind Slate-950 #0B0F19
    bool m_hasDirtyDamage{true};
    bool m_isFullRepaint{true};
    bool m_forceFullRepaint{true};
    bool m_dirtyRectEnabled{true};
    bool m_debugBoundsEnabled{false};
    bool m_inspectModeEnabled{false};

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

        void Insert(std::string_view utf8Text) {
            if (cursor <= text.size()) {
                text.insert(cursor, utf8Text);
                cursor += utf8Text.size();
                changed = true;
            }
        }

        bool HandleKey(Key key, bool& outLostFocus) {
            switch (key) {
                case Key::Backspace:
                    if (cursor > 0 && !text.empty()) {
                        text.erase(cursor - 1, 1);
                        cursor--;
                        changed = true;
                    }
                    return true;
                case Key::Delete:
                    if (cursor < text.size()) {
                        text.erase(cursor, 1);
                        changed = true;
                    }
                    return true;
                case Key::Left:
                    if (cursor > 0) cursor--;
                    return true;
                case Key::Right:
                    if (cursor < text.size()) cursor++;
                    return true;
                case Key::Home:
                    cursor = 0;
                    return true;
                case Key::End:
                    cursor = text.size();
                    return true;
                case Key::Enter:
                    submitted = true;
                    outLostFocus = true;
                    return true;
                case Key::Escape:
                    outLostFocus = true;
                    id = Id::Null();
                    return true;
                default:
                    return false;
            }
        }
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

    // Floating popup & overlay state
    Id m_openPopupId{Id::Null()};
    Rect m_openPopupRect{0.0f, 0.0f, 0.0f, 0.0f};
    std::vector<std::function<void(ICanvas&)>> m_overlayPasses;
    Vec2 m_mouseWheel{0.0f, 0.0f};
    std::unordered_map<detail::StateKey, detail::StoredState, detail::StateKeyHash> m_customStates;
    uint64_t m_frameIndex{0};

    void RenderDebugBoundsRecursive(Node* node, ICanvas& canvas);
    Node* FindDeepestNodeAt(Node* root, Vec2 pos, Rect clipRect);
    void RenderInspectOverlay(Node* root, ICanvas& canvas, Vec2 logicalSize);
};

} // namespace guikit
