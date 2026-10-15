#pragma once

#include <cassert>
#include <cstdint>
#include <optional>
#include <string_view>
#include <type_traits>
#include "guikit/core/Id.hpp"
#include "guikit/core/Tokens.hpp"
#include "guikit/core/Types.hpp"

namespace guikit {

namespace detail {
inline uint16_t NextPayloadTypeId() noexcept {
    static uint16_t counter = 0;
    return ++counter;
}

template <typename T>
[[nodiscard]] inline uint16_t GetPayloadTypeId() noexcept {
    static const uint16_t id = NextPayloadTypeId();
    return id;
}
} // namespace detail

class ICanvas;
struct Node;
struct UiTheme;

/// Function signature for measuring intrinsic content dimensions of a leaf widget or container.
/// Takes available width (0.0f if unconstrained) for width-dependent height resolution (e.g. wrapped text).
using MeasureFunc = Vec2 (*)(const Node* node, float availableWidth, ICanvas* canvas, const UiTheme& theme);

/// Function signature for custom arrangement of a container node and its children.
using ArrangeFunc = void (*)(Node* node, const Rect& availableRect, const UiTheme& theme);

/// Function signature for rendering an element's visual contents.
using RenderFunc = void (*)(const Node* node, ICanvas& canvas, const UiTheme& theme);

/// Fundamental semantic category of a scene graph element.
enum class NodeCategory : uint8_t {
    Layout,     ///< Pure geometry and flow distribution: Row, Column, Wrap, Grid, Cell
    Container,  ///< Visual surface and viewport boundary: Card, Box, ScrollArea, Carousel
    Widget,     ///< Terminal interactive or content leaf element: Button, Input, Switch, Text...
    Spacer      ///< Elastic expanding spring
};

/// Type of element represented by a frame graph node.
/// Partitioned by numeric ranges for fast categorization and user extensibility.
enum class NodeType : uint16_t {
    // --- Layouts (1..99): Pure flow distribution ---
    Row         = 1,
    Column      = 2,
    Wrap        = 3,
    Grid        = 4,
    Cell        = 5,

    // --- Containers (100..199): Surfaces, boundaries, viewports ---
    Card        = 100,
    Box         = 101,
    ScrollArea  = 102,
    Carousel    = 103,
    Container   = 104,  ///< General container alias for backward compatibility
    Sidebar     = 105,  ///< Dockable vertical sidebar container (Left or Right)
    Scene       = 106,  ///< Interactive 2D world camera & pan/zoom viewport

    // --- Leaf Widgets (200..999): Self-rendering elements ---
    Button      = 200,
    InputField  = 201,
    Select      = 202,
    Switch      = 203,
    Checkbox    = 204,
    Text        = 205,
    Heading     = 206,
    TabItem     = 207,
    Separator   = 208,
    Spacer      = 209,
    Slider      = 210,
    StepSlider  = 211,
    Custom      = 212,
    TextArea    = 213,
    SidebarItem = 214,
    Image       = 215,
    Markdown    = 216,

    // --- User Extension Base (1000+) ---
    User        = 1000
};

/// Lightweight, intrusive node in the per-frame layout graph allocated from BumpAllocator.
struct Node {
    // Intrusive tree pointers (O(1) hierarchy without dynamic heap vectors)
    Node* parent{nullptr};
    Node* firstChild{nullptr};
    Node* lastChild{nullptr};
    Node* nextSibling{nullptr};
    const UiTheme* theme{nullptr};

    uint32_t childIndex{0};     ///< Index of this node within its parent container
    uint32_t childCount{0};     ///< Total number of direct children

    Id id{Id::Null()};          ///< Unique identity
    uint64_t visualHash{0};     ///< Fast visual fingerprint for frame-to-frame damage tracking

    // Layout configuration & ambient state
    NodeType type{NodeType::Container};
    FlexDirection direction{FlexDirection::Column};
    SizeMode widthMode{SizeMode::Stretch};
    SizeMode heightMode{SizeMode::Fit};
    uint8_t gridColumns{12};    ///< Number of columns in Grid layout container
    uint8_t colSpan{12};        ///< Number of grid columns occupied by this item (1..12)
    Orientation scrollOrientation{Orientation::Vertical};
    VisibilityPolicy scrollbarPolicy{VisibilityPolicy::Auto};
    Alignment align{Alignment::Stretch};
    Justify justify{Justify::Start};
    UiSize uiSize{UiSize::Md};
    std::optional<bool> debugBoundsOverride{std::nullopt};
    std::optional<bool> inspectModeOverride{std::nullopt};

    // Interaction & container flags (packed 1-bit flags to maintain tight cache footprint)
    bool isSpacer : 1 {false};
    bool clipChildren : 1 {false};
    bool scrollbarHovered : 1 {false};
    bool scrollbarActive : 1 {false};
    bool scrollbarHoveredX : 1 {false};
    bool scrollbarActiveX : 1 {false};
    bool disabled : 1 {false};
    bool hovered : 1 {false};
    bool pressed : 1 {false};
    bool focused : 1 {false};
    bool checked : 1 {false};
    bool active : 1 {false};
    bool sidebarRight : 1 {false};
    bool sidebarScrollable : 1 {true};
    uint16_t payloadTypeId{0};  ///< Runtime type tag for safe Node::As<T>() casting

    float fixedWidth{0.0f};
    float fixedHeight{0.0f};
    float flexGrow{1.0f};       ///< Distribution factor for Stretch/Spacer elements
    float gap{0.0f};            ///< Gap between consecutive children on same line
    float runGap{0.0f};         ///< Vertical gap between consecutive wrapped lines
    float minWidth{0.0f};       ///< Minimum width constraint for responsive wrapping
    float minHeight{0.0f};      ///< Minimum height constraint for 2D elements and Stretch fallback
    float aspectRatio{0.0f};    ///< Optional width/height ratio (> 0 enables automatic height from width)
    float minColWidth{0.0f};    ///< Minimum column width constraint for Grid container
    Padding padding{};          ///< Internal padding inside container

    // Scroll area configuration and state
    Vec2 scrollOffset{0.0f, 0.0f};      ///< Active scroll offset in points
    Vec2 scrollContentSize{0.0f, 0.0f}; ///< Total extent of child content

    // Measured and computed bounds
    Vec2 intrinsicSize{0.0f, 0.0f};  ///< Natural content size (bottom-up measure pass)
    Rect computedRect{0.0f, 0.0f, 0.0f, 0.0f}; ///< Final placed coordinates (top-down arrange pass)

    // Visual attributes (rendered by backend)
    Color backgroundColor{0, 0, 0, 0};
    Color borderColor{0, 0, 0, 0};
    float borderWidth{0.0f};
    float cornerRadius{0.0f};

    // Text / Content attributes
    std::string_view text{};
    Color textColor{255, 255, 255, 255};
    float fontSize{14.0f};

    // Generic arena-allocated widget payload (ButtonOptions, InputFieldData, CarouselData, user data, etc.)
    void* payload{nullptr};

    // Autonomous measure, arrange, and render delegates (polymorphism with zero heap/vtable overhead)
    MeasureFunc measureFn{nullptr};
    ArrangeFunc arrangeFn{nullptr};
    RenderFunc renderFn{nullptr};
    RenderFunc postRenderFn{nullptr};

    /// Attaches a typed arena-allocated payload to this node with runtime type verification.
    template <typename T>
    void SetPayload(T* ptr) noexcept {
        payload = ptr;
        payloadTypeId = ptr ? detail::GetPayloadTypeId<std::remove_cv_t<T>>() : 0;
    }

    /// Casts payload to the specified widget data type, returning nullptr on type mismatch.
    template <typename T>
    [[nodiscard]] T* As() noexcept {
        if (!payload) return nullptr;
        if (payloadTypeId != 0 && payloadTypeId != detail::GetPayloadTypeId<std::remove_cv_t<T>>()) {
            return nullptr;
        }
        return static_cast<T*>(payload);
    }

    /// Casts payload to the specified const widget data type, returning nullptr on type mismatch.
    template <typename T>
    [[nodiscard]] const T* As() const noexcept {
        if (!payload) return nullptr;
        if (payloadTypeId != 0 && payloadTypeId != detail::GetPayloadTypeId<std::remove_cv_t<T>>()) {
            return nullptr;
        }
        return static_cast<const T*>(payload);
    }

    /// Returns the fundamental semantic category of this element.
    [[nodiscard]] constexpr NodeCategory GetCategory() const noexcept {
        const auto val = static_cast<uint16_t>(type);
        if (type == NodeType::Spacer || isSpacer) {
            return NodeCategory::Spacer;
        }
        if (val < 100) {
            return NodeCategory::Layout;
        }
        if (val < 200) {
            return NodeCategory::Container;
        }
        return NodeCategory::Widget;
    }

    /// Returns true if this is a leaf widget that cannot host child elements.
    [[nodiscard]] constexpr bool IsLeaf() const noexcept {
        const NodeCategory cat = GetCategory();
        return cat == NodeCategory::Widget || cat == NodeCategory::Spacer;
    }

    /// Returns the canonical human-readable element type name for inspector and debug tooling.
    [[nodiscard]] constexpr std::string_view GetTypeName() const noexcept {
        switch (type) {
            case NodeType::Row:         return "Row";
            case NodeType::Column:      return "Column";
            case NodeType::Wrap:        return "Wrap";
            case NodeType::Grid:        return "Grid";
            case NodeType::Cell:        return "Cell";
            case NodeType::Card:        return "Card";
            case NodeType::Box:         return "Box";
            case NodeType::ScrollArea:  return "ScrollArea";
            case NodeType::Carousel:    return "Carousel";
            case NodeType::Container:   return "Container";
            case NodeType::Sidebar:     return "Sidebar";
            case NodeType::Scene:       return "Scene";
            case NodeType::Button:      return "Button";
            case NodeType::InputField:  return "InputField";
            case NodeType::Select:      return "Select";
            case NodeType::Switch:      return "Switch";
            case NodeType::Checkbox:    return "Checkbox";
            case NodeType::Text:        return "Text";
            case NodeType::Heading:     return "Heading";
            case NodeType::TabItem:     return "TabItem";
            case NodeType::Separator:   return "Separator";
            case NodeType::Spacer:      return "Spacer";
            case NodeType::Slider:      return "Slider";
            case NodeType::StepSlider:  return "StepSlider";
            case NodeType::Custom:      return "Custom";
            case NodeType::TextArea:    return "TextArea";
            case NodeType::SidebarItem: return "SidebarItem";
            case NodeType::Image:       return "Image";
            case NodeType::Markdown:    return "Markdown";
            default:                    return (static_cast<uint16_t>(type) >= static_cast<uint16_t>(NodeType::User)) ? "UserWidget" : "Unknown";
        }
    }

    /// Appends a child node to this container in O(1).
    void AddChild(Node* child) noexcept {
        if (!child) return;
        child->parent = this;
        child->childIndex = childCount++;
        child->nextSibling = nullptr;

        if (!firstChild) {
            firstChild = child;
            lastChild = child;
        } else {
            lastChild->nextSibling = child;
            lastChild = child;
        }
    }
};

static_assert(sizeof(Node) <= 256, "Node must remain compact (<= 256 bytes / 4 cache lines)");

} // namespace guikit
