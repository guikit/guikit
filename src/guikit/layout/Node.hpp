#pragma once

#include <cstdint>
#include <string_view>
#include "guikit/core/Id.hpp"
#include "guikit/core/Tokens.hpp"
#include "guikit/core/Types.hpp"
#include "guikit/layout/LayoutTypes.hpp"
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

/// Type of element represented by a frame graph node.
enum class NodeType : uint8_t {
    Container,  ///< General flex layout container (Row, Column, Box)
    ScrollArea, ///< Viewport container providing scrollable view into overflowed children
    Carousel,   ///< Horizontal ribbon container with automatic navigation arrows on overflow
    Text,       ///< Text label
    Button,     ///< Clickable button widget
    Switch,     ///< Sliding pill toggle switch
    Checkbox,   ///< Box toggle with checkmark
    InputField, ///< Text or numeric editable field
    Select,     ///< Dropdown combo box widget
    Heading,    ///< Structured title and subtitle/description header widget
    Spacer,     ///< Elastic spacer expanding to fill remaining room
    Custom,     ///< User or custom renderer node
    TabItem,    ///< Individual tab item in a TabBar
    Separator   ///< Divider line separator (Toolbar/menu divider)
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

    // Ambient state and widget configuration
    UiSize uiSize{UiSize::Md};
    ThemeMode themeMode{ThemeMode::Dark};
    bool disabled{false};
    bool hovered{false};
    bool pressed{false};
    ButtonOptions buttonOpts{};
    TextOptions textOpts{};
    SwitchOptions switchOpts{};
    CheckboxOptions checkboxOpts{};
    InputFieldOptions inputOpts{};
    SelectOptions selectOpts{};
    HeadingOptions headingOpts{};
    TabBarOptions tabBarOpts{};
    TabItem tabItemOpts{};
    ToolButtonOptions toolButtonOpts{};
    CarouselOptions carouselOpts{};
    Rect leftArrowRect{0.0f, 0.0f, 0.0f, 0.0f};
    Rect rightArrowRect{0.0f, 0.0f, 0.0f, 0.0f};
    Rect carouselViewportRect{0.0f, 0.0f, 0.0f, 0.0f};
    bool showLeftArrow{false};
    bool showRightArrow{false};
    bool leftArrowHovered{false};
    bool rightArrowHovered{false};
    bool leftArrowPressed{false};
    bool rightArrowPressed{false};
    std::string_view displayText{};
    bool checked{false};
    bool focused{false};
    bool tabActive{false};
    bool scrollToActiveTab{false};
    bool cursorVisible{true};
    float cursorWidth{1.5f};
    size_t cursor{0};

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

} // namespace guikit
