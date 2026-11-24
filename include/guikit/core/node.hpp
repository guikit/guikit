#pragma once

#include <cassert>
#include "guikit/core/id.hpp"
#include "guikit/core/tokens.hpp"
#include "guikit/core/types.hpp"
#include "guikit/core/zone.hpp"
#include "keel/core.h"

namespace guikit {

class Context;
class ICanvas;
struct Node;
struct UiTheme;

/// Function signature for measuring intrinsic content dimensions of a leaf widget or container.
/// Takes available width (0.0f if unconstrained) for width-dependent height resolution (e.g. wrapped text).
using MeasureFunc = Vec2 (*)(const Node* node, keel::f32 availableWidth, ICanvas* canvas, const UiTheme& theme);

/// Function signature for custom arrangement of a container node and its children.
using ArrangeFunc = void (*)(Node* node, const Rect& availableRect, const UiTheme& theme);

/// Function signature for rendering an element's visual contents.
using RenderFunc = void (*)(const Node* node, ICanvas& canvas, const UiTheme& theme);

/// Optional open-closed panel lifecycle hooks attached via static `const PanelHooks*` on `Node`.
/// Keeps `Context` and `Solver` completely decoupled from concrete panel implementations.
struct PanelHooks {
    using PlaceOutOfFlowFn = Rect (*)(const Node* node, keel::f32 targetWidth, keel::f32 targetHeight, const Rect& availableRect);
    using ContentClipFn    = Rect (*)(const Node* node, const UiTheme& theme);
    using UpdateFn         = void (*)(Context& ctx, Node* node);
    using ScrollToChildFn  = void (*)(Context& ctx, Node* container, const Node* target);
    using RecordBoundsFn   = void (*)(void* user, Id id, Rect rect);
    using CollectBoundsFn  = void (*)(Node* node, RecordBoundsFn record, void* user, const Rect& parentClip, const Transform2D& space);

    PlaceOutOfFlowFn placeOutOfFlow{nullptr};
    ContentClipFn    contentClip{nullptr};
    UpdateFn         update{nullptr};
    ScrollToChildFn  scrollToChild{nullptr};
    CollectBoundsFn  collectBounds{nullptr};
};

/// Arena-allocated scroll configuration and per-frame scrollbar state for scrollable containers.
struct ScrollData {
    Vec2 offset{0.0f, 0.0f};            ///< Active scroll offset in logical dp
    Vec2 contentSize{0.0f, 0.0f};       ///< Total extent of child content
    keel::f32 maxHeight{0.0f};          ///< Optional maximum viewport height (> 0 clamps Fit/Stretch viewport)
    Orientation orientation{Orientation::Vertical};
    VisibilityPolicy policy{VisibilityPolicy::Auto};
    bool solverManaged{true};           ///< True for solver-laid-out scroll containers (ScrollArea, Sidebar, BottomPanel)
    bool hoveredY{false};
    bool activeY{false};
    bool hoveredX{false};
    bool activeX{false};
    keel::u8 partHoveredY{0};
    keel::u8 partPressedY{0};
    keel::u8 partHoveredX{0};
    keel::u8 partPressedX{0};
};

/// Compact docking flag bits for shell panels (`Sidebar`, `ActivityBar`, `BottomPanel`).
namespace dock_flags {
inline constexpr keel::u8 kNone      = 0u;
inline constexpr keel::u8 kRight     = 1u << 0;
inline constexpr keel::u8 kCompact   = 1u << 1;
inline constexpr keel::u8 kCollapsed = 1u << 2;
inline constexpr keel::u8 kFlush     = 1u << 3;
} // namespace dock_flags

/// Arena-allocated configuration payload for `NodeType::Grid` containers.
struct GridData {
    keel::u8 columns{12};
    keel::f32 minColWidth{0.0f};
};

/// Fundamental semantic category of a scene graph element.
enum class NodeCategory : keel::u8 {
    Layout,     ///< Pure geometry and flow distribution: Row, Column, Wrap, Grid, Cell
    Container,  ///< Visual surface and viewport boundary: Card, Box, ScrollArea, Carousel
    Widget,     ///< Terminal interactive or content leaf element: Button, Input, Switch, Text...
    Spacer      ///< Elastic expanding spring
};

/// Type of element represented by a frame graph node.
/// Partitioned by numeric ranges for fast categorization and user extensibility.
enum class NodeType : keel::u16 {
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
    Sidebar     = 105,  ///< Dockable vertical sidebar container (Left or Right)
    Scene       = 106,  ///< Interactive 2D world camera & pan/zoom viewport
    Table       = 107,  ///< Virtualized data grid / table container with sticky header and columns
    Element     = 108,  ///< Semantic form-field wrapper (Label + [Control Slot] + Description)
    Accordion   = 109,  ///< Collapsible accordion section container
    TreeView    = 110,  ///< Hierarchical tree view container
    TreeItem    = 111,  ///< Individual interactive tree item row container (hosts optional trailing controls)
    TreeBranch  = 112,  ///< Vertical column wrapper grouping a TreeItem row and its expanded child TreeBranches
    Modal       = 113,  ///< Out-of-flow floating modal dialog container with backdrop overlay
    BottomPanel = 114,  ///< Dockable bottom panel container with top-edge resize sash and collapse
    StatusBar   = 115,  ///< Compact single-line status bar container docked at the bottom edge
    Toolbar     = 116,  ///< Horizontal or vertical command toolbar container
    SplitView   = 117,  ///< Resizable two-pane split container with draggable sash
    Popover     = 118,  ///< Out-of-flow non-modal floating popover anchored to a trigger rect
    MenuBar     = 119,  ///< Desktop top-level menu bar hosting cascading menus with hover-scrubbing

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
    RangeSlider   = 220,
    ProgressBar   = 221,
    Histogram      = 222,
    ImageView      = 223,
    AccordionHeader = 227,
    Radio           = 228,
    SpinBox         = 229,
    Knob            = 230,
    Segmented       = 231,
    ColorPicker     = 232,
    DatePicker      = 233,
    Breadcrumb      = 234,
    Pagination      = 235,
    CommandPalette  = 236,
    LevelMeter      = 237,
    TagInput        = 238,
    Tag             = 239,
    Readout         = 240,

    // --- User Extension Base (1000+) ---
    User          = 1000
};

/// Lightweight, intrusive node in the per-frame layout graph allocated from BumpAllocator.
struct Node {
    // Intrusive tree pointers (O(1) hierarchy without dynamic heap vectors)
    Node* parent{nullptr};
    Node* firstChild{nullptr};
    Node* lastChild{nullptr};
    Node* nextSibling{nullptr};
    /// Resolved zone of the subtree the node was built in (never null: defaults to `kDefaultZone` for standalone nodes).
    const Zone* zone{&kDefaultZone};

    keel::u32 childIndex{0};     ///< Index of this node within its parent container
    keel::u32 childCount{0};     ///< Total number of direct children

    Id id{Id::Null()};          ///< Unique identity
    keel::u64 visualHash{0};     ///< Fast visual fingerprint for frame-to-frame damage tracking

    // Layout configuration & ambient state
    NodeType type{NodeType::Box};
    keel::u16 colSpan{12};             ///< Grid cell column span or Table cell column index
    UiSize uiSize{UiSize::Md};         ///< Control size token or custom size (does not inherit across containers)
    FlexDirection direction{FlexDirection::Column};
    SizeMode widthMode{SizeMode::Stretch};
    SizeMode heightMode{SizeMode::Fit};
    Alignment align{Alignment::Stretch};
    Justify justify{Justify::Start};
    keel::u8 dockFlags{dock_flags::kNone};
    bool isSpacer : 1 {false};
    bool clipChildren : 1 {false};
    bool equalWidths : 1 {false};
    bool disabled : 1 {false};
    bool hovered : 1 {false};
    bool pressed : 1 {false};
    bool focused : 1 {false};
    bool checked : 1 {false};
    bool active : 1 {false};
    bool explicitGap : 1 {false};
    bool explicitPadding : 1 {false};
    bool explicitRadius : 1 {false};

    keel::f32 fixedWidth{0.0f};
    keel::f32 fixedHeight{0.0f};
    keel::f32 flexGrow{1.0f};       ///< Distribution factor for Stretch/Spacer elements
    keel::f32 gap{0.0f};            ///< Gap between consecutive children on same line
    keel::f32 runGap{0.0f};         ///< Vertical gap between consecutive wrapped lines
    keel::f32 minWidth{0.0f};       ///< Minimum width constraint for responsive wrapping
    keel::f32 minHeight{0.0f};      ///< Minimum height constraint for 2D elements and Stretch fallback
    keel::f32 maxWidth{0.0f};       ///< Optional maximum width constraint (> 0 clamps width)
    keel::f32 maxHeight{0.0f};      ///< Optional maximum height constraint (> 0 clamps height)
    keel::f32 aspectRatio{0.0f};    ///< Optional width/height ratio (> 0 enables automatic height from width)
    Padding padding{};              ///< Internal padding inside container

    // Measured and computed bounds
    Vec2 intrinsicSize{0.0f, 0.0f};            ///< Natural content size (bottom-up measure pass)
    Rect computedRect{0.0f, 0.0f, 0.0f, 0.0f}; ///< Final placed coordinates (top-down arrange pass)

    // Visual & text attributes (rendered by backend)
    Color backgroundColor{0, 0, 0, 0};
    Color borderColor{0, 0, 0, 0};
    keel::f32 borderWidth{0.0f};
    keel::f32 cornerRadius{0.0f};
    keel::StringRef text{};

    // Optional arena-allocated scroll extension (null for non-scrollable nodes)
    ScrollData* scroll{nullptr};

    // Generic arena-allocated widget payload (ButtonPayload, InputFieldData, GridData, user data, etc.)
    void* payload{nullptr};
    keel::TypeId payloadType{};

    // Autonomous measure, arrange, panel lifecycle, and render delegates (polymorphism with zero heap/vtable overhead)
    MeasureFunc measureFn{nullptr};
    ArrangeFunc arrangeFn{nullptr};
    const PanelHooks* panelHooks{nullptr};
    RenderFunc renderFn{nullptr};
    RenderFunc postRenderFn{nullptr};

    [[nodiscard]] constexpr bool IsScrollable() const noexcept { return scroll != nullptr; }
    [[nodiscard]] constexpr bool IsSolverScrollable() const noexcept { return scroll != nullptr && scroll->solverManaged; }
    [[nodiscard]] constexpr Vec2 GetScrollOffset() const noexcept { return scroll ? scroll->offset : Vec2{0.0f, 0.0f}; }
    [[nodiscard]] constexpr Vec2 GetScrollContentSize() const noexcept { return scroll ? scroll->contentSize : Vec2{0.0f, 0.0f}; }

    [[nodiscard]] constexpr bool IsCollapsed() const noexcept { return (dockFlags & dock_flags::kCollapsed) != 0u; }
    [[nodiscard]] constexpr bool IsDockedRight() const noexcept { return (dockFlags & dock_flags::kRight) != 0u; }
    [[nodiscard]] constexpr bool IsCompactDock() const noexcept { return (dockFlags & dock_flags::kCompact) != 0u; }
    [[nodiscard]] constexpr bool IsFlushDock() const noexcept { return (dockFlags & dock_flags::kFlush) != 0u; }

    [[nodiscard]] keel::u8 GetGridColumns() const noexcept {
        if (const auto* g = As<GridData>()) {
            return (g->columns > 0) ? g->columns : 12;
        }
        return 12;
    }

    [[nodiscard]] keel::f32 GetMinColWidth() const noexcept {
        if (const auto* g = As<GridData>()) {
            return g->minColWidth;
        }
        return 0.0f;
    }

    [[nodiscard]] constexpr const UiTheme& GetTheme(const UiTheme& fallback) const noexcept {
        return (zone->theme != nullptr) ? *zone->theme : fallback;
    }

    [[nodiscard]] constexpr const UiTheme* GetThemePtr() const noexcept {
        return zone->theme;
    }

    [[nodiscard]] constexpr keel::f32 GetScale() const noexcept {
        return (zone->scale > 0.0f) ? zone->scale : 1.0f;
    }

    /// Relative scale jump from `parent` to this node (1.0f when both share the same scale).
    [[nodiscard]] constexpr keel::f32 GetRelativeScale() const noexcept {
        const Zone* parentZone = (parent != nullptr) ? parent->zone : &kDefaultZone;
        if (zone == parentZone || zone->scale == parentZone->scale) {
            return 1.0f;
        }
        const keel::f32 self = GetScale();
        const keel::f32 parentScale = parentZone->scale;
        return (parentScale > 0.0f) ? (self / parentScale) : 1.0f;
    }

    [[nodiscard]] constexpr keel::f32 GetDensity() const noexcept {
        return (zone->density > 0.0f) ? zone->density : 1.0f;
    }

    [[nodiscard]] constexpr keel::Optional<bool> GetDebugBoundsOverride() const noexcept {
        return zone->debugBounds;
    }

    [[nodiscard]] constexpr keel::Optional<bool> GetInspectModeOverride() const noexcept {
        return zone->inspectMode;
    }

    /// Converts outer sizing fields (fixed / min / max) from this node's local units into its parent's units.
    /// Call once after setting outer sizing fields on a scaled container node.
    constexpr void SetOuterScale(keel::f32 s) noexcept {
        if (!(s > 0.0f) || s == 1.0f) {
            return;
        }
        fixedWidth *= s;
        fixedHeight *= s;
        minWidth *= s;
        maxWidth *= s;
        minHeight *= s;
        maxHeight *= s;
    }

    /// Attaches a typed arena-allocated payload to this node with zero-RTTI runtime type verification.
    template <typename T>
    void SetPayload(T* ptr) noexcept {
        payload = ptr;
        payloadType = ptr ? keel::type_id<keel::RemoveCv<T>>() : keel::TypeId{};
    }

    /// Casts payload to the specified widget data type, returning nullptr on type mismatch.
    template <typename T>
    [[nodiscard]] T* As() noexcept {
        if (!payload) return nullptr;
        if (payloadType != keel::TypeId{} && payloadType != keel::type_id<keel::RemoveCv<T>>()) {
            return nullptr;
        }
        return static_cast<T*>(payload);
    }

    /// Casts payload to the specified const widget data type, returning nullptr on type mismatch.
    template <typename T>
    [[nodiscard]] const T* As() const noexcept {
        if (!payload) return nullptr;
        if (payloadType != keel::TypeId{} && payloadType != keel::type_id<keel::RemoveCv<T>>()) {
            return nullptr;
        }
        return static_cast<const T*>(payload);
    }

    /// Returns the fundamental semantic category of this element.
    [[nodiscard]] constexpr NodeCategory GetCategory() const noexcept {
        const auto val = keel::as<keel::u16>(type);
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

    /// Returns true if this container floats out of normal flow at the overlay z-layer (Modal or Popover).
    [[nodiscard]] constexpr bool IsOutOfFlow() const noexcept {
        return type == NodeType::Modal || type == NodeType::Popover;
    }

    /// Returns the canonical human-readable element type name for inspector and debug tooling.
    [[nodiscard]] constexpr keel::StringRef GetTypeName() const noexcept {
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
            case NodeType::Sidebar:     return "Sidebar";
            case NodeType::Scene:       return "Scene";
            case NodeType::Table:       return "Table";
            case NodeType::Element:     return "Element";
            case NodeType::Accordion:   return "Accordion";
            case NodeType::TreeView:    return "TreeView";
            case NodeType::TreeItem:    return "TreeItem";
            case NodeType::TreeBranch:  return "TreeBranch";
            case NodeType::Modal:       return "Modal";
            case NodeType::BottomPanel: return "BottomPanel";
            case NodeType::StatusBar:   return "StatusBar";
            case NodeType::Toolbar:     return "Toolbar";
            case NodeType::SplitView:   return "SplitView";
            case NodeType::Popover:     return "Popover";
            case NodeType::MenuBar:     return "MenuBar";
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
            case NodeType::RangeSlider:   return "RangeSlider";
            case NodeType::ProgressBar:   return "ProgressBar";
            case NodeType::Histogram:     return "Histogram";
            case NodeType::ImageView:      return "ImageView";
            case NodeType::AccordionHeader: return "AccordionHeader";
            case NodeType::Radio:           return "Radio";
            case NodeType::SpinBox:         return "SpinBox";
            case NodeType::Knob:            return "Knob";
            case NodeType::Segmented:       return "Segmented";
            case NodeType::ColorPicker:     return "ColorPicker";
            case NodeType::DatePicker:      return "DatePicker";
            case NodeType::Breadcrumb:      return "Breadcrumb";
            case NodeType::Pagination:      return "Pagination";
            case NodeType::CommandPalette:  return "CommandPalette";
            case NodeType::LevelMeter:      return "LevelMeter";
            case NodeType::TagInput:        return "TagInput";
            case NodeType::Tag:             return "Tag";
            case NodeType::Readout:         return "Readout";
            default:
                if (keel::as<keel::u16>(type) >= keel::as<keel::u16>(NodeType::User)) {
                    return !text.empty() ? text : keel::StringRef("UserWidget");
                }
                return "Unknown";
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

    /// Removes a direct child node from this container.
    void RemoveChild(Node* child) noexcept {
        if (!child || !firstChild) return;
        if (firstChild == child) {
            firstChild = child->nextSibling;
            if (lastChild == child) {
                lastChild = nullptr;
            }
            if (childCount > 0) --childCount;
            child->parent = nullptr;
            child->nextSibling = nullptr;
            return;
        }
        for (Node* prev = firstChild; prev != nullptr; prev = prev->nextSibling) {
            if (prev->nextSibling == child) {
                prev->nextSibling = child->nextSibling;
                if (lastChild == child) {
                    lastChild = prev;
                }
                if (childCount > 0) --childCount;
                child->parent = nullptr;
                child->nextSibling = nullptr;
                return;
            }
        }
    }
};

static_assert(sizeof(Node) <= 256, "Node must remain compact (<= 256 bytes / 4 cache lines)");

/// Returns the local scale transform a node adds on top of its parent's space (scaling about its top-left corner).
[[nodiscard]] inline Transform2D NodeLocalTransform(const Node* node) noexcept {
    if (node == nullptr) {
        return Transform2D{};
    }
    const keel::f32 relScale = node->GetRelativeScale();
    if (relScale == 1.0f || !(relScale > 0.0f)) {
        return Transform2D{};
    }
    return Transform2D::ScaleAbout(Vec2{node->computedRect.x, node->computedRect.y}, relScale);
}

/// Returns the transform from `node`'s local coordinate space to frame space, given `parentSpace`.
[[nodiscard]] inline Transform2D NodeSpace(const Node* node, const Transform2D& parentSpace) noexcept {
    const Transform2D local = NodeLocalTransform(node);
    return local.IsIdentity() ? parentSpace : parentSpace.Then(local);
}

/// Resolves the content clip rect of `node` via its `PanelHooks::contentClip` delegate (or `node->computedRect`).
[[nodiscard]] inline Rect GetNodeContentClipRect(const Node* node, const UiTheme& theme) {
    if (!node) return Rect{};
    if (node->panelHooks && node->panelHooks->contentClip) {
        return node->panelHooks->contentClip(node, theme);
    }
    return node->computedRect;
}

} // namespace guikit
