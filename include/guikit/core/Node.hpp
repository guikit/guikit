#pragma once

#include <cstdint>
#include <string_view>
#include "guikit/core/Id.hpp"
#include "guikit/core/Tokens.hpp"
#include "guikit/core/Types.hpp"

namespace guikit {

class ICanvas;
struct Node;
struct UiTheme;

/// Function signature for measuring intrinsic content dimensions of a leaf widget or container.
using MeasureFunc = Vec2 (*)(const Node* node, ICanvas* canvas, const UiTheme& theme);

/// Function signature for custom arrangement of a container node and its children.
using ArrangeFunc = void (*)(Node* node, const Rect& availableRect);

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
    Custom      = 210,

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

    uint32_t childIndex{0};     ///< Index of this node within its parent container
    uint32_t childCount{0};     ///< Total number of direct children

    Id id{Id::Null()};          ///< Unique identity
    NodeType type{NodeType::Container};

    // Layout configuration
    FlexDirection direction{FlexDirection::Column};
    SizeMode widthMode{SizeMode::Stretch};
    SizeMode heightMode{SizeMode::Fit};
    float fixedWidth{0.0f};
    float fixedHeight{0.0f};
    float flexGrow{1.0f};       ///< Distribution factor for Stretch/Spacer elements
    float gap{0.0f};            ///< Gap between consecutive children on same line
    float runGap{0.0f};         ///< Vertical gap between consecutive wrapped lines
    int gridColumns{12};        ///< Number of columns in Grid layout container
    int colSpan{12};            ///< Number of grid columns occupied by this item (1..12)
    float minWidth{0.0f};       ///< Minimum width constraint for responsive wrapping
    float minColWidth{0.0f};    ///< Minimum column width constraint for Grid container
    Padding padding{};          ///< Internal padding inside container
    Alignment align{Alignment::Stretch};
    Justify justify{Justify::Start};
    bool isSpacer{false};
    bool clipChildren{false};

    // Scroll area configuration and state
    Orientation scrollOrientation{Orientation::Vertical};
    VisibilityPolicy scrollbarPolicy{VisibilityPolicy::Auto};
    Vec2 scrollOffset{0.0f, 0.0f};      ///< Active scroll offset in points
    Vec2 scrollContentSize{0.0f, 0.0f}; ///< Total extent of child content
    bool scrollbarHovered{false};
    bool scrollbarActive{false};

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

    // Ambient state and interaction flags
    UiSize uiSize{UiSize::Md};
    bool disabled{false};
    bool hovered{false};
    bool pressed{false};
    bool focused{false};
    bool checked{false};
    bool active{false};

    // Generic arena-allocated widget payload (ButtonOptions, InputFieldData, CarouselData, user data, etc.)
    void* payload{nullptr};

    // Autonomous measure, arrange, and render delegates (polymorphism with zero heap/vtable overhead)
    MeasureFunc measureFn{nullptr};
    ArrangeFunc arrangeFn{nullptr};
    RenderFunc renderFn{nullptr};
    RenderFunc postRenderFn{nullptr};

    /// Casts payload to the specified widget data type.
    template <typename T>
    [[nodiscard]] T* As() noexcept {
        return static_cast<T*>(payload);
    }

    /// Casts payload to the specified const widget data type.
    template <typename T>
    [[nodiscard]] const T* As() const noexcept {
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
            case NodeType::Row:        return "Row";
            case NodeType::Column:     return "Column";
            case NodeType::Wrap:       return "Wrap";
            case NodeType::Grid:       return "Grid";
            case NodeType::Cell:       return "Cell";
            case NodeType::Card:       return "Card";
            case NodeType::Box:        return "Box";
            case NodeType::ScrollArea: return "ScrollArea";
            case NodeType::Carousel:   return "Carousel";
            case NodeType::Container:  return "Container";
            case NodeType::Button:     return "Button";
            case NodeType::InputField: return "InputField";
            case NodeType::Select:     return "Select";
            case NodeType::Switch:     return "Switch";
            case NodeType::Checkbox:   return "Checkbox";
            case NodeType::Text:       return "Text";
            case NodeType::Heading:    return "Heading";
            case NodeType::TabItem:    return "TabItem";
            case NodeType::Separator:  return "Separator";
            case NodeType::Spacer:     return "Spacer";
            case NodeType::Custom:     return "Custom";
            default:                   return (static_cast<uint16_t>(type) >= static_cast<uint16_t>(NodeType::User)) ? "UserWidget" : "Unknown";
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

static_assert(sizeof(Node) <= 248, "Node must remain compact (<= 248 bytes)");

} // namespace guikit
