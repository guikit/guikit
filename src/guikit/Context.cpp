#include "guikit/Context.hpp"
#include <algorithm>
#include <format>
#include <string>

namespace guikit {

Context::Context() {
    m_containerStack.reserve(16);
    m_sizeStack.reserve(16);
    m_disabledStack.reserve(16);
    m_sizeStack.push_back(UiSize::Md);
    m_disabledStack.push_back(false);
}

void Context::BeginFrame(Vec2 viewportSize, float dpiScale) {
    const float effScale = (dpiScale > 0.0f) ? dpiScale : 1.0f;
    if (m_viewportSize != viewportSize || m_dpiScale != effScale) {
        InvalidateAll();
    }
    m_viewportSize = viewportSize;
    m_dpiScale = effScale;

    if (!m_manualTime) {
        const auto now = std::chrono::steady_clock::now();
        m_currentTimeSec = std::chrono::duration<double>(now - m_startTime).count();
    }

    m_overlayPasses.clear();

    if (m_mouseClicked && m_openPopupId.IsValid()) {
        auto it = m_prevFrameBounds.find(m_openPopupId);
        const bool overSelect = (it != m_prevFrameBounds.end()) && it->second.Contains(m_mousePos);
        const bool overPopup = m_openPopupRect.Contains(m_mousePos);
        if (!overSelect && !overPopup) {
            ClosePopup();
            ConsumeMouseClick();
        }
    }

    if (m_mouseClicked && m_focusedId.IsValid()) {
        auto it = m_prevFrameBounds.find(m_focusedId);
        if (it != m_prevFrameBounds.end()) {
            if (!it->second.Contains(m_mousePos)) {
                m_focusedId = Id::Null();
            }
        } else {
            m_focusedId = Id::Null();
        }
    }

    m_arena.Reset();
    m_containerStack.clear();
    m_sizeStack.clear();
    m_disabledStack.clear();
    m_currFrameBounds.clear();

    m_sizeStack.push_back(UiSize::Md);
    m_disabledStack.push_back(false);

    // Create root container spanning the entire logical viewport
    const Vec2 logicalSize = m_viewportSize / m_dpiScale;
    m_root = m_arena.Allocate<Node>();
    m_root->type = NodeType::Column;
    m_root->direction = FlexDirection::Column;
    m_root->widthMode = SizeMode::Stretch;
    m_root->heightMode = SizeMode::Stretch;
    m_root->computedRect = Rect(0.0f, 0.0f, logicalSize.x, logicalSize.y);
    m_root->id = Id{0xFFFF'FFFF'FFFF'FFFF};
    m_root->uiSize = UiSize::Md;
    m_root->disabled = false;

    m_containerStack.push_back(m_root);
}

Node* Context::CreateNode(NodeType type, Id id) {
    Node* node = m_arena.Allocate<Node>();
    node->type = type;
    node->id = id;

    if (!m_containerStack.empty()) {
        m_containerStack.back()->AddChild(node);
    }
    return node;
}

Id Context::MakeChildId(std::source_location loc) {
    const Id parentId = m_containerStack.empty() ? Id::Null() : m_containerStack.back()->id;
    const uint32_t childIndex = m_containerStack.empty() ? 0 : m_containerStack.back()->childCount;
    return MakeId(childIndex, loc, parentId);
}

Response Context::Interact(Id id) {
    Response res;
    res.id = id;
    res.ctx = this;

    // Suppress interaction on background widgets occluded by a floating overlay popup
    if (m_openPopupId.IsValid() && id != m_openPopupId && m_openPopupRect.Contains(m_mousePos)) {
        return res;
    }

    auto it = m_prevFrameBounds.find(id);
    if (it != m_prevFrameBounds.end()) {
        res.rect = it->second;
        res.hovered = res.rect.Contains(m_mousePos);
    }

    if (res.hovered) {
        m_hoveredId = id;
        if (m_mouseClicked) {
            m_activeId = id;
        }
    }

    res.pressed = (m_activeId == id && m_mouseDown);
    res.clicked = (m_activeId == id && m_mouseReleased && res.hovered);

    if (m_mouseReleased && m_activeId == id) {
        m_activeId = Id::Null();
    }

    return res;
}

void Context::AdaptChildToContainer(Node* node) {
    if (!node || m_containerStack.empty()) return;
    Node* parent = m_containerStack.back();
    if (parent->direction == FlexDirection::Column || parent->direction == FlexDirection::Grid) {
        node->widthMode = SizeMode::Stretch;
        if (parent->heightMode == SizeMode::Fixed) {
            node->heightMode = SizeMode::Stretch;
        }
    }
}

Rect Context::GetPrevFrameBounds(Id id) const {
    auto it = m_prevFrameBounds.find(id);
    if (it != m_prevFrameBounds.end()) {
        return it->second;
    }
    return Rect{};
}

void Context::PushContainer(Node* node) {
    m_containerStack.push_back(node);
}

void Context::PopContainer(Node* node) {
    if (!m_containerStack.empty() && m_containerStack.back() == node) {
        m_containerStack.pop_back();
        PopSize();
        PopDisabled();
    }
}

void Context::PushSize(UiSize size) {
    m_sizeStack.push_back(size);
}

void Context::PopSize() {
    if (m_sizeStack.size() > 1) {
        m_sizeStack.pop_back();
    }
}

UiSize Context::GetCurrentSize() const {
    return m_sizeStack.empty() ? UiSize::Md : m_sizeStack.back();
}

void Context::PushDisabled(bool disabled) {
    const bool current = IsDisabled();
    m_disabledStack.push_back(current || disabled);
}

void Context::PopDisabled() {
    if (m_disabledStack.size() > 1) {
        m_disabledStack.pop_back();
    }
}

bool Context::IsDisabled() const {
    return !m_disabledStack.empty() && m_disabledStack.back();
}

RowScope Context::Row(RowOptions opts, std::source_location loc) {
    const Id id = MakeChildId(loc);
    const UiSize activeSize = opts.size.value_or(GetCurrentSize());
    const bool activeDisabled = opts.disabled || IsDisabled();

    Node* node = CreateNode(NodeType::Row, id);
    node->direction = FlexDirection::Row;
    node->gap = opts.gap.has_value() ? m_theme.GetGap(*opts.gap) : m_theme.GetGap(activeSize);
    node->padding = opts.padding;
    node->align = opts.align;
    node->justify = opts.justify;
    node->heightMode = opts.heightMode;
    node->widthMode = SizeMode::Stretch;
    node->uiSize = activeSize;
    node->disabled = activeDisabled;

    PushSize(activeSize);
    PushDisabled(activeDisabled);
    PushContainer(node);
    return RowScope(this, node);
}

ColumnScope Context::Column(ColumnOptions opts, std::source_location loc) {
    const Id id = MakeChildId(loc);
    const UiSize activeSize = opts.size.value_or(GetCurrentSize());
    const bool activeDisabled = opts.disabled || IsDisabled();

    Node* node = CreateNode(NodeType::Column, id);
    node->direction = FlexDirection::Column;
    node->gap = opts.gap.has_value() ? m_theme.GetGap(*opts.gap) : m_theme.GetGap(activeSize);
    node->padding = opts.padding;
    node->align = opts.align;
    node->justify = opts.justify;
    node->widthMode = opts.widthMode;
    node->heightMode = opts.heightMode;
    node->uiSize = activeSize;
    node->disabled = activeDisabled;

    PushSize(activeSize);
    PushDisabled(activeDisabled);
    PushContainer(node);
    return ColumnScope(this, node);
}

BoxScope Context::Box(BoxOptions opts, std::source_location loc) {
    const Id id = MakeChildId(loc);
    const UiSize activeSize = opts.size.value_or(GetCurrentSize());
    const bool activeDisabled = opts.disabled || IsDisabled();

    Node* node = CreateNode(NodeType::Box, id);
    node->direction = FlexDirection::Column;
    node->gap = opts.gap.has_value() ? m_theme.GetGap(*opts.gap) : m_theme.GetGap(activeSize);
    node->widthMode = opts.widthMode;
    node->heightMode = opts.heightMode;

    if (opts.fixedWidth > 0.0f) {
        node->widthMode = SizeMode::Fixed;
        node->fixedWidth = opts.fixedWidth;
    }
    if (opts.fixedHeight > 0.0f) {
        node->heightMode = SizeMode::Fixed;
        node->fixedHeight = opts.fixedHeight;
    }

    // If explicit size token is set and no fixedHeight was provided, use token height
    if (opts.size.has_value() && opts.fixedHeight <= 0.0f) {
        node->fixedHeight = m_theme.GetMetrics(*opts.size).defaultHeight;
        node->heightMode = SizeMode::Fixed;
    }

    node->padding = opts.padding;
    node->backgroundColor = opts.backgroundColor;
    node->borderColor = opts.borderColor;
    node->borderWidth = opts.borderWidth;
    node->cornerRadius = opts.radius.has_value() ? m_theme.GetRadius(*opts.radius) : opts.cornerRadius;
    node->clipChildren = opts.clipChildren;
    node->uiSize = activeSize;
    node->disabled = activeDisabled;

    PushSize(activeSize);
    PushDisabled(activeDisabled);
    PushContainer(node);
    return BoxScope(this, node);
}

WrapScope Context::Wrap(WrapOptions opts, std::source_location loc) {
    const Id id = MakeChildId(loc);
    const UiSize activeSize = opts.size.value_or(GetCurrentSize());
    const bool activeDisabled = opts.disabled || IsDisabled();

    Node* node = CreateNode(NodeType::Wrap, id);
    node->direction = FlexDirection::Wrap;
    node->gap = opts.gap.has_value() ? m_theme.GetGap(*opts.gap) : m_theme.GetGap(activeSize);
    node->runGap = opts.runGap.has_value() ? m_theme.GetGap(*opts.runGap) : node->gap;
    node->padding = opts.padding;
    node->align = opts.align;
    node->justify = opts.justify;
    node->widthMode = opts.widthMode;
    node->heightMode = opts.heightMode;
    node->uiSize = activeSize;
    node->disabled = activeDisabled;

    PushSize(activeSize);
    PushDisabled(activeDisabled);
    PushContainer(node);
    return WrapScope(this, node);
}

CellGridScope Context::CellGrid(CellGridOptions opts, std::source_location loc) {
    const Id id = MakeChildId(loc);
    const UiSize activeSize = opts.size.value_or(GetCurrentSize());
    const bool activeDisabled = opts.disabled || IsDisabled();

    Node* node = CreateNode(NodeType::Grid, id);
    node->direction = FlexDirection::Grid;
    node->gridColumns = (opts.columns > 0) ? opts.columns : 12;
    node->minColWidth = opts.minColWidth;
    node->gap = opts.gap.has_value() ? m_theme.GetGap(*opts.gap) : m_theme.GetGap(activeSize);
    node->runGap = opts.runGap.has_value() ? m_theme.GetGap(*opts.runGap) : node->gap;
    node->padding = opts.padding;
    node->align = opts.align;
    node->justify = opts.justify;
    node->widthMode = opts.widthMode;
    node->heightMode = opts.heightMode;
    node->uiSize = activeSize;
    node->disabled = activeDisabled;

    PushSize(activeSize);
    PushDisabled(activeDisabled);
    PushContainer(node);
    return CellGridScope(this, node);
}

CellGridScope Context::Grid(CellGridOptions opts, std::source_location loc) {
    return CellGrid(opts, loc);
}

namespace {

void RenderScrollAreaScrollbar(const Node* node, ICanvas& canvas, const UiTheme& theme) {
    if (!node) return;
    bool showScrollbar = false;
    if (node->scrollbarPolicy == VisibilityPolicy::Always) {
        showScrollbar = true;
    } else if (node->scrollbarPolicy == VisibilityPolicy::Auto) {
        showScrollbar = (node->scrollContentSize.y > node->computedRect.h);
    }

    if (showScrollbar && node->computedRect.h > 10.0f) {
        const ScrollbarMetrics& m = theme.GetScrollbar(node->uiSize);
        const float trackX = node->computedRect.Right() - m.trackWidth - m.margin;
        const float trackY = node->computedRect.y + m.margin;
        const float trackH = std::max(0.0f, node->computedRect.h - 2.0f * m.margin);

        const float contentH = std::max(node->computedRect.h, node->scrollContentSize.y);
        const float maxScrollY = contentH - node->computedRect.h;
        const float thumbH = std::clamp((node->computedRect.h / contentH) * trackH, m.minThumbLength, trackH);
        const float travelH = trackH - thumbH;
        const float scrollRatio = (maxScrollY > 0.0f) ? (node->scrollOffset.y / maxScrollY) : 0.0f;
        const float thumbY = trackY + scrollRatio * travelH;

        const Rect trackRect(trackX, trackY, m.trackWidth, trackH);
        const Rect thumbRect(trackX, thumbY, m.trackWidth, thumbH);

        const auto& pal = theme.palette;

        // Subtle track background
        const Color trackCol = pal.borderSubtle;
        canvas.DrawRoundRect(trackRect, m.cornerRadius, trackCol);

        // Thumb pill
        Color thumbCol = pal.textMuted;
        if (node->scrollbarActive) {
            thumbCol = pal.accent;
        } else if (node->scrollbarHovered) {
            thumbCol = pal.textSecondary;
        }

        canvas.DrawRoundRect(thumbRect, m.cornerRadius, thumbCol);
    }
}

} // namespace

ScrollAreaScope Context::ScrollArea(ScrollAreaOptions opts, std::source_location loc) {
    const Id id = MakeChildId(loc);
    const UiSize activeSize = opts.size.value_or(GetCurrentSize());
    const bool activeDisabled = opts.disabled || IsDisabled();

    Node* node = CreateNode(NodeType::ScrollArea, id);
    node->direction = FlexDirection::Column;
    node->clipChildren = true;
    node->widthMode = SizeMode::Stretch;
    node->heightMode = SizeMode::Stretch;
    node->flexGrow = 1.0f;
    node->padding = opts.padding;
    node->scrollOrientation = opts.orientation;
    node->scrollbarPolicy = opts.scrollbar;
    node->uiSize = activeSize;
    node->disabled = activeDisabled;
    node->postRenderFn = &RenderScrollAreaScrollbar;

    // Retrieve persistent scroll state
    auto& state = m_scrollStates[id];

    // Handle immediate mouse wheel scroll if hovered on previous frame bounds
    auto itPrev = m_prevFrameBounds.find(id);
    if (itPrev != m_prevFrameBounds.end() && !node->disabled) {
        if (itPrev->second.Contains(m_mousePos) && (m_mouseWheel.x != 0.0f || m_mouseWheel.y != 0.0f)) {
            const ScrollbarMetrics& m = m_theme.GetScrollbar(activeSize);
            state.offset.y -= m_mouseWheel.y * m.scrollStep;
            state.offset.x -= m_mouseWheel.x * m.scrollStep;
            if (state.offset.y < 0.0f) state.offset.y = 0.0f;
            if (state.offset.x < 0.0f) state.offset.x = 0.0f;
        }
    }

    node->scrollOffset = state.offset;

    PushSize(activeSize);
    PushDisabled(activeDisabled);
    PushContainer(node);
    return ScrollAreaScope(this, node);
}

CarouselScope Context::Carousel(CarouselOptions opts, std::source_location loc) {
    const Id id = MakeChildId(loc);
    const UiSize activeSize = opts.size.value_or(GetCurrentSize());
    const bool activeDisabled = opts.disabled || IsDisabled();
    const ComponentMetrics& m = m_theme.GetMetrics(activeSize);

    Node* node = CreateNode(NodeType::Carousel, id);
    node->direction = FlexDirection::Row;
    node->align = Alignment::Center;
    node->clipChildren = true;
    node->gap = opts.gap.has_value() ? m_theme.GetGap(*opts.gap) : m_theme.GetGap(activeSize);
    node->padding = opts.padding.has_value() ? m_theme.GetPadding(*opts.padding) : Padding{};
    node->widthMode = SizeMode::Stretch;
    node->heightMode = SizeMode::Fit;
    node->uiSize = activeSize;
    node->disabled = activeDisabled;
    auto* cData = AllocPayload<CarouselData>();
    cData->opts = opts;
    node->payload = cData;
    node->measureFn = [](const Node* n, ICanvas* canvas, const UiTheme& theme) noexcept -> Vec2 {
        return Carousel::Measure(n, canvas, theme);
    };
    node->arrangeFn = &Carousel::Arrange;
    node->postRenderFn = &Carousel::RenderArrows;

    // Retrieve persistent scroll state
    auto& state = m_scrollStates[id];

    // Navigation arrow IDs
    const Id leftArrowId = GetCarouselLeftArrowId(id);
    const Id rightArrowId = GetCarouselRightArrowId(id);

    Response leftRes = Interact(leftArrowId);
    Response rightRes = Interact(rightArrowId);

    cData->leftArrowHovered = leftRes.hovered;
    cData->leftArrowPressed = leftRes.pressed;
    cData->rightArrowHovered = rightRes.hovered;
    cData->rightArrowPressed = rightRes.pressed;

    const float step = m.defaultHeight * 3.0f;

    if (leftRes.clicked && !activeDisabled) {
        state.offset.x = std::max(0.0f, state.offset.x - step);
        RequestRepaint(3);
    }
    if (rightRes.clicked && !activeDisabled) {
        state.offset.x += step;
        RequestRepaint(3);
    }

    // Handle horizontal mouse wheel scroll
    if (opts.scrollWheel && !activeDisabled) {
        auto itPrev = m_prevFrameBounds.find(id);
        if (itPrev != m_prevFrameBounds.end() && itPrev->second.Contains(m_mousePos)) {
            const float wheelDelta = (m_mouseWheel.x != 0.0f) ? m_mouseWheel.x : m_mouseWheel.y;
            if (wheelDelta != 0.0f) {
                state.offset.x -= wheelDelta * (m.defaultHeight * 0.8f);
                if (state.offset.x < 0.0f) state.offset.x = 0.0f;
                RequestRepaint(3);
            }
        }
    }

    node->scrollOffset = state.offset;

    PushSize(activeSize);
    PushDisabled(activeDisabled);
    PushContainer(node);
    return CarouselScope(this, node);
}

CardScope Context::Card(CardOptions opts, std::source_location loc) {
    const Id idOuter = MakeChildId(loc);
    const UiSize activeSize = opts.size.value_or(GetCurrentSize());
    const bool activeDisabled = opts.disabled || IsDisabled();

    // Outer styled surface frame
    Node* outerNode = CreateNode(NodeType::Card, idOuter);
    outerNode->direction = FlexDirection::Column;
    outerNode->gap = opts.gap.has_value() ? m_theme.GetGap(*opts.gap) : m_theme.GetGap(activeSize);
    outerNode->minWidth = opts.minWidth;
    outerNode->widthMode = opts.widthMode;
    outerNode->heightMode = opts.heightMode;
    outerNode->padding = opts.padding.has_value() ? *opts.padding : m_theme.GetPadding(activeSize);
    const auto& pal = m_theme.palette;
    outerNode->backgroundColor = (opts.backgroundColor.a > 0) ? opts.backgroundColor : pal.surface;
    outerNode->borderColor = (opts.borderColor.a > 0) ? opts.borderColor : pal.border;
    outerNode->borderWidth = opts.borderWidth;
    outerNode->cornerRadius = opts.radius.has_value() ? m_theme.GetRadius(*opts.radius) : (opts.cornerRadius > 0.0f ? opts.cornerRadius : m_theme.GetRadius(activeSize));
    outerNode->clipChildren = opts.clipChildren;
    outerNode->uiSize = activeSize;
    outerNode->disabled = activeDisabled;

    PushSize(activeSize);
    PushDisabled(activeDisabled);
    PushContainer(outerNode);

    return CardScope(this, outerNode);
}

CellScope Context::CreateCell(int span, CellOptions opts, std::source_location loc) {
    const Id id = MakeChildId(loc);
    const UiSize activeSize = opts.size.value_or(GetCurrentSize());
    const bool activeDisabled = opts.disabled || IsDisabled();

    Node* node = CreateNode(NodeType::Cell, id);
    node->direction = FlexDirection::Column;
    node->colSpan = std::clamp(span, 1, 12);
    node->minWidth = opts.minWidth;
    node->clipChildren = true;
    node->gap = opts.gap.has_value() ? m_theme.GetGap(*opts.gap) : m_theme.GetGap(activeSize);
    node->padding = opts.padding;
    node->align = opts.align;
    node->justify = opts.justify;
    node->widthMode = SizeMode::Stretch;
    node->heightMode = opts.heightMode;
    node->uiSize = activeSize;
    node->disabled = activeDisabled;

    PushSize(activeSize);
    PushDisabled(activeDisabled);
    PushContainer(node);
    return CellScope(this, node);
}

void Context::Spacer(float flex, std::source_location loc) {
    const Id id = MakeChildId(loc);
    Node* node = CreateNode(NodeType::Spacer, id);
    node->isSpacer = true;
    node->widthMode = SizeMode::Stretch;
    node->heightMode = SizeMode::Stretch;
    node->flexGrow = std::max(0.01f, flex);
    node->measureFn = [](const Node*, ICanvas*, const UiTheme&) noexcept -> Vec2 {
        return {0.0f, 0.0f};
    };
}

Response Context::Button(ButtonOptions opts, std::source_location loc) {
    return Button::Run(*this, opts, loc);
}

Response Context::Button(std::string_view label, ButtonOptions opts, std::source_location loc) {
    opts.label = label;
    return Button::Run(*this, opts, loc);
}

UiString Context::String(std::string_view text) {
    return UiString(InternString(text));
}

std::string_view Context::InternString(std::string_view text) {
    if (text.empty()) return {};
    char* buf = static_cast<char*>(m_arena.AllocateRaw(text.size() + 1, alignof(char)));
    std::memcpy(buf, text.data(), text.size());
    buf[text.size()] = '\0';
    return std::string_view(buf, text.size());
}

Response Context::Text(std::string_view label, TextOptions opts, std::source_location loc) {
    return Text::Run(*this, label, opts, loc);
}

Response Context::Display(std::string_view label, UiVariant variant, std::source_location loc) {
    return Text(label, {.role = TextRole::DisplayLarge, .variant = variant}, loc);
}

Response Context::Headline(std::string_view label, UiVariant variant, std::source_location loc) {
    return Text(label, {.role = TextRole::HeadlineMedium, .variant = variant}, loc);
}

Response Context::Title(std::string_view label, UiVariant variant, std::source_location loc) {
    return Text(label, {.role = TextRole::TitleLarge, .variant = variant}, loc);
}

Response Context::Subtitle(std::string_view label, UiVariant variant, std::source_location loc) {
    return Text(label, {.role = TextRole::TitleMedium, .variant = variant}, loc);
}

Response Context::Body(std::string_view label, UiVariant variant, std::source_location loc) {
    return Text(label, {.role = TextRole::BodyMedium, .variant = variant}, loc);
}

Response Context::Caption(std::string_view label, UiVariant variant, std::source_location loc) {
    return Text(label, {.role = TextRole::LabelMedium, .variant = variant}, loc);
}

Response Context::Code(std::string_view label, UiVariant variant, std::source_location loc) {
    return Text(label, {.role = TextRole::CodeMedium, .variant = variant}, loc);
}

Response Context::Heading(HeadingOptions opts, std::source_location loc) {
    return Heading::Run(*this, opts, loc);
}

Response Context::Switch(std::string_view label, bool* value, SwitchOptions opts, std::source_location loc) {
    return Switch::Run(*this, label, value, value ? *value : false, opts, loc);
}

Response Context::Switch(bool* value, SwitchOptions opts, std::source_location loc) {
    return Switch::Run(*this, {}, value, value ? *value : false, opts, loc);
}

Response Context::Switch(std::string_view label, bool checked, SwitchOptions opts, std::source_location loc) {
    return Switch::Run(*this, label, nullptr, checked, opts, loc);
}

Response Context::Switch(bool checked, SwitchOptions opts, std::source_location loc) {
    return Switch::Run(*this, {}, nullptr, checked, opts, loc);
}

Response Context::Checkbox(std::string_view label, bool* value, CheckboxOptions opts, std::source_location loc) {
    return Checkbox::Run(*this, label, value, value ? *value : false, opts, loc);
}

Response Context::Checkbox(bool* value, CheckboxOptions opts, std::source_location loc) {
    return Checkbox::Run(*this, {}, value, value ? *value : false, opts, loc);
}

Response Context::Checkbox(std::string_view label, bool checked, CheckboxOptions opts, std::source_location loc) {
    return Checkbox::Run(*this, label, nullptr, checked, opts, loc);
}

Response Context::Checkbox(bool checked, CheckboxOptions opts, std::source_location loc) {
    return Checkbox::Run(*this, {}, nullptr, checked, opts, loc);
}

Response Context::InputField(std::string_view label, std::string& value, InputFieldOptions opts, std::source_location loc) {
    return InputField::Run(*this, label, &value, nullptr, nullptr, nullptr, {}, opts, loc);
}

Response Context::InputField(std::string& value, InputFieldOptions opts, std::source_location loc) {
    return InputField::Run(*this, {}, &value, nullptr, nullptr, nullptr, {}, opts, loc);
}

Response Context::InputField(std::string_view label, float& value, InputFieldOptions opts, std::source_location loc) {
    return InputField::Run(*this, label, nullptr, &value, nullptr, nullptr, {}, opts, loc);
}

Response Context::InputField(float& value, InputFieldOptions opts, std::source_location loc) {
    return InputField::Run(*this, {}, nullptr, &value, nullptr, nullptr, {}, opts, loc);
}

Response Context::InputField(std::string_view label, double& value, InputFieldOptions opts, std::source_location loc) {
    return InputField::Run(*this, label, nullptr, nullptr, &value, nullptr, {}, opts, loc);
}

Response Context::InputField(double& value, InputFieldOptions opts, std::source_location loc) {
    return InputField::Run(*this, {}, nullptr, nullptr, &value, nullptr, {}, opts, loc);
}

Response Context::InputField(std::string_view label, int& value, InputFieldOptions opts, std::source_location loc) {
    return InputField::Run(*this, label, nullptr, nullptr, nullptr, &value, {}, opts, loc);
}

Response Context::InputField(int& value, InputFieldOptions opts, std::source_location loc) {
    return InputField::Run(*this, {}, nullptr, nullptr, nullptr, &value, {}, opts, loc);
}

Response Context::InputField(std::string_view label, std::string_view value, InputFieldOptions opts, std::source_location loc) {
    return InputField::Run(*this, label, nullptr, nullptr, nullptr, nullptr, value, opts, loc);
}

Response Context::Select(std::string_view label, size_t& selectedIndex, std::span<const std::string_view> items, SelectOptions opts, std::source_location loc) {
    return Select::Run(*this, label, &selectedIndex, nullptr, nullptr, nullptr, items, opts, loc);
}

Response Context::Select(size_t& selectedIndex, std::span<const std::string_view> items, SelectOptions opts, std::source_location loc) {
    return Select::Run(*this, {}, &selectedIndex, nullptr, nullptr, nullptr, items, opts, loc);
}

Response Context::Select(std::string_view label, int& selectedIndex, std::span<const std::string_view> items, SelectOptions opts, std::source_location loc) {
    return Select::Run(*this, label, nullptr, &selectedIndex, nullptr, nullptr, items, opts, loc);
}

Response Context::Select(int& selectedIndex, std::span<const std::string_view> items, SelectOptions opts, std::source_location loc) {
    return Select::Run(*this, {}, nullptr, &selectedIndex, nullptr, nullptr, items, opts, loc);
}

Response Context::Select(std::string_view label, std::string& selectedValue, std::span<const std::string_view> items, SelectOptions opts, std::source_location loc) {
    return Select::Run(*this, label, nullptr, nullptr, &selectedValue, nullptr, items, opts, loc);
}

Response Context::Select(std::string& selectedValue, std::span<const std::string_view> items, SelectOptions opts, std::source_location loc) {
    return Select::Run(*this, {}, nullptr, nullptr, &selectedValue, nullptr, items, opts, loc);
}

Response Context::Select(std::string_view label, std::string_view& selectedValue, std::span<const std::string_view> items, SelectOptions opts, std::source_location loc) {
    return Select::Run(*this, label, nullptr, nullptr, nullptr, &selectedValue, items, opts, loc);
}

Response Context::Select(std::string_view& selectedValue, std::span<const std::string_view> items, SelectOptions opts, std::source_location loc) {
    return Select::Run(*this, {}, nullptr, nullptr, nullptr, &selectedValue, items, opts, loc);
}

Response Context::TabBar(
    int& activeIndex,
    std::span<const TabItem> tabs,
    TabBarOptions opts,
    std::source_location loc
) {
    return TabBar::Run(*this, activeIndex, tabs, opts, loc);
}

Response Context::TabBar(
    int& activeIndex,
    std::span<const std::string_view> tabs,
    TabBarOptions opts,
    std::source_location loc
) {
    std::vector<TabItem> items;
    items.reserve(tabs.size());
    for (const auto& label : tabs) {
        items.push_back(TabItem{.label = label});
    }
    return TabBar::Run(*this, activeIndex, items, opts, loc);
}

ToolbarScope Context::Toolbar(
    ToolbarOptions opts,
    std::source_location loc
) {
    return Toolbar::Open(*this, opts, loc);
}

Response Context::ToolButton(
    std::string_view label,
    ToolButtonOptions opts,
    std::source_location loc
) {
    return Toolbar::ToolButton(*this, label, opts, loc);
}

void Context::RequestFocus(Id id) {
    m_focusedId = id;
    ResetCursorBlink();
}

void Context::RequestRepaint(int frames) noexcept {
    m_repaintFrames = std::max(m_repaintFrames, frames);
}

bool Context::NeedsRepaint() const noexcept {
    if (m_repaintFrames > 0) return true;
    if (m_mouseDown) return true;
    if (m_activeId.IsValid()) return true;
    if (m_openPopupId.IsValid()) return true;
    return false;
}

int Context::GetRecommendedSleepTimeoutMs() const noexcept {
    if (NeedsRepaint()) {
        return 0;
    }
    if (m_editState.id.IsValid() && m_cursorConfig.mode == CursorBlinkMode::Blinking) {
        return 120; // Periodically wake up for smooth cursor blinking
    }
    return m_idleHeartbeatMs; // User-configurable heartbeat interval (or <= 0 for deep sleep)
}

void Context::OnMouseMove(Vec2 pos) {
    const float scale = (m_dpiScale > 0.0f) ? m_dpiScale : 1.0f;
    m_mousePos = pos / scale;
    RequestRepaint(3);
    if (m_inspectModeEnabled) {
        InvalidateAll();
    }
}

void Context::OnMouseDown(int button) {
    if (button == 1) { // Left mouse button
        m_mouseDown = true;
        m_mouseClicked = true;
        ResetCursorBlink();
        RequestRepaint(3);
    }
}

void Context::OnMouseUp(int button) {
    if (button == 1) { // Left mouse button
        m_mouseDown = false;
        m_mouseReleased = true;
        RequestRepaint(3);
    }
}

void Context::OnMouseWheel(float x, float y) {
    m_mouseWheel = {x, y};
    RequestRepaint(3);
}

void Context::OnTextInput(std::string_view utf8Text) {
    ResetCursorBlink();
    RequestRepaint(3);
    if (m_focusedId.IsValid() && m_editState.id == m_focusedId) {
        if (m_editState.cursor <= m_editState.text.size()) {
            m_editState.text.insert(m_editState.cursor, utf8Text);
            m_editState.cursor += utf8Text.size();
            m_editState.changed = true;
        }
    } else {
        m_pendingTextInput.emplace_back(utf8Text);
    }
}

void Context::OnKeyDown(Key key, KeyModifiers mods) {
    ResetCursorBlink();
    RequestRepaint(3);

    if (key == Key::Escape && m_openPopupId.IsValid()) {
        ClosePopup();
    }

    if (m_focusedId.IsValid() && m_editState.id == m_focusedId) {
        switch (key) {
            case Key::Backspace:
                if (m_editState.cursor > 0 && !m_editState.text.empty()) {
                    m_editState.text.erase(m_editState.cursor - 1, 1);
                    m_editState.cursor--;
                    m_editState.changed = true;
                }
                break;
            case Key::Delete:
                if (m_editState.cursor < m_editState.text.size()) {
                    m_editState.text.erase(m_editState.cursor, 1);
                    m_editState.changed = true;
                }
                break;
            case Key::Left:
                if (m_editState.cursor > 0) {
                    m_editState.cursor--;
                }
                break;
            case Key::Right:
                if (m_editState.cursor < m_editState.text.size()) {
                    m_editState.cursor++;
                }
                break;
            case Key::Home:
                m_editState.cursor = 0;
                break;
            case Key::End:
                m_editState.cursor = m_editState.text.size();
                break;
            case Key::Enter:
                m_editState.submitted = true;
                m_focusedId = Id::Null();
                break;
            case Key::Escape:
                m_focusedId = Id::Null();
                m_editState.id = Id::Null();
                break;
            default:
                break;
        }
    } else {
        m_pendingKeyEvents.push_back({key, mods});
    }
}

void Context::OpenPopup(Id id, Rect bounds) {
    m_openPopupId = id;
    m_openPopupRect = bounds;
    InvalidateAll();
}

void Context::ClosePopup() {
    m_openPopupId = Id::Null();
    m_openPopupRect = Rect{};
    InvalidateAll();
}

void Context::InvalidateAll() noexcept {
    m_forceFullRepaint = true;
    RequestRepaint(2);
}

void Context::InvalidateRect(Rect rect) noexcept {
    if (rect.w > 0.0f && rect.h > 0.0f) {
        m_explicitDamageRects.push_back(rect);
        RequestRepaint(2);
    }
}

void Context::RegisterOverlay(std::function<void(ICanvas&)> drawFn) {
    m_overlayPasses.push_back(std::move(drawFn));
}

bool Context::ConsumeKey(Key key) {
    for (auto it = m_pendingKeyEvents.begin(); it != m_pendingKeyEvents.end(); ++it) {
        if (it->key == key) {
            m_pendingKeyEvents.erase(it);
            return true;
        }
    }
    return false;
}

std::string_view Context::GetOrStartTextEdit(
    Id id,
    std::string_view initialText,
    size_t& outCursor,
    bool& outChanged,
    bool& outSubmitted
) {
    if (m_editState.id != id) {
        m_editState.id = id;
        m_editState.text = initialText;
        m_editState.cursor = m_editState.text.size();
        m_editState.changed = false;
        m_editState.submitted = false;
    }

    for (const auto& txt : m_pendingTextInput) {
        if (m_editState.cursor <= m_editState.text.size()) {
            m_editState.text.insert(m_editState.cursor, txt);
            m_editState.cursor += txt.size();
            m_editState.changed = true;
        }
    }
    m_pendingTextInput.clear();

    for (const auto& ev : m_pendingKeyEvents) {
        switch (ev.key) {
            case Key::Backspace:
                if (m_editState.cursor > 0 && !m_editState.text.empty()) {
                    m_editState.text.erase(m_editState.cursor - 1, 1);
                    m_editState.cursor--;
                    m_editState.changed = true;
                }
                break;
            case Key::Delete:
                if (m_editState.cursor < m_editState.text.size()) {
                    m_editState.text.erase(m_editState.cursor, 1);
                    m_editState.changed = true;
                }
                break;
            case Key::Left:
                if (m_editState.cursor > 0) m_editState.cursor--;
                break;
            case Key::Right:
                if (m_editState.cursor < m_editState.text.size()) m_editState.cursor++;
                break;
            case Key::Home:
                m_editState.cursor = 0;
                break;
            case Key::End:
                m_editState.cursor = m_editState.text.size();
                break;
            case Key::Enter:
                m_editState.submitted = true;
                m_focusedId = Id::Null();
                break;
            case Key::Escape:
                m_focusedId = Id::Null();
                m_editState.id = Id::Null();
                break;
            default:
                break;
        }
    }
    m_pendingKeyEvents.clear();

    outCursor = m_editState.cursor;
    outChanged = m_editState.changed;
    outSubmitted = m_editState.submitted;

    m_editState.changed = false;
    m_editState.submitted = false;
    if (outSubmitted) {
        m_editState.id = Id::Null();
    }

    return m_editState.text;
}

void Context::ResetCursorBlink() noexcept {
    m_lastInputTimeSec = m_currentTimeSec;
}

void Context::SetCurrentTime(double timeSeconds) noexcept {
    m_manualTime = true;
    m_currentTimeSec = timeSeconds;
}

void Context::AdvanceTime(double deltaSeconds) noexcept {
    m_manualTime = true;
    m_currentTimeSec += deltaSeconds;
}

bool Context::IsCursorVisible(std::optional<CursorBlinkMode> modeOverride) const noexcept {
    const CursorBlinkMode mode = modeOverride.value_or(m_cursorConfig.mode);
    switch (mode) {
        case CursorBlinkMode::Hidden:
            return false;
        case CursorBlinkMode::Solid:
            return true;
        case CursorBlinkMode::Blinking: {
            const double elapsedSinceInput = m_currentTimeSec - m_lastInputTimeSec;
            // Typing reset phase: cursor remains solid/visible for 0.5s after user interaction
            if (elapsedSinceInput >= 0.0 && elapsedSinceInput < 0.5) {
                return true;
            }
            if (m_cursorConfig.blinkPeriod <= 0.0f) {
                return true;
            }
            const double cycleTime = elapsedSinceInput - 0.5;
            const double phase = std::fmod(cycleTime, static_cast<double>(m_cursorConfig.blinkPeriod));
            return phase < (static_cast<double>(m_cursorConfig.blinkPeriod) * 0.5);
        }
    }
    return true;
}

static void CollectBoundsRecursive(Node* node, std::unordered_map<Id, Rect>& outBounds, Rect parentClip, const UiTheme& theme) {
    if (!node) return;

    Rect effectiveClip = parentClip;
    if (node->clipChildren) {
        if (node->type == NodeType::ScrollArea) {
            const ScrollbarMetrics& sm = theme.GetScrollbar(node->uiSize);
            const bool showScrollbar = (node->scrollbarPolicy == VisibilityPolicy::Always) ||
                (node->scrollbarPolicy == VisibilityPolicy::Auto && node->scrollContentSize.y > node->computedRect.h);
            const float gutter = (showScrollbar && node->scrollOrientation != Orientation::Horizontal) ? sm.gutter : 0.0f;
            const Rect viewRect(
                node->computedRect.x + node->padding.left,
                node->computedRect.y + node->padding.top,
                std::max(0.0f, node->computedRect.w - node->padding.Horizontal() - gutter),
                std::max(0.0f, node->computedRect.h - node->padding.Vertical())
            );
            effectiveClip = parentClip.Intersect(viewRect);
        } else if (node->type == NodeType::Carousel) {
            auto* cData = node->As<CarouselData>();
            effectiveClip = parentClip.Intersect(cData ? cData->viewportRect : node->computedRect);
        } else {
            effectiveClip = parentClip.Intersect(node->computedRect);
        }
    }

    if (node->id.IsValid()) {
        const Rect clipped = node->computedRect.Intersect(effectiveClip);
        if (clipped.w > 0.0f && clipped.h > 0.0f) {
            outBounds[node->id] = clipped;
        }

        // Register interactive arrow button bounds for Carousel
        if (node->type == NodeType::Carousel) {
            if (auto* cData = node->As<CarouselData>()) {
                if (cData->showLeftArrow && cData->leftArrowRect.w > 0.0f) {
                    const Rect clippedLeft = cData->leftArrowRect.Intersect(parentClip);
                    if (clippedLeft.w > 0.0f && clippedLeft.h > 0.0f) {
                        outBounds[GetCarouselLeftArrowId(node->id)] = clippedLeft;
                    }
                }
                if (cData->showRightArrow && cData->rightArrowRect.w > 0.0f) {
                    const Rect clippedRight = cData->rightArrowRect.Intersect(parentClip);
                    if (clippedRight.w > 0.0f && clippedRight.h > 0.0f) {
                        outBounds[GetCarouselRightArrowId(node->id)] = clippedRight;
                    }
                }
            }
        }
    }

    for (Node* child = node->firstChild; child != nullptr; child = child->nextSibling) {
        CollectBoundsRecursive(child, outBounds, effectiveClip, theme);
    }
}

void Context::UpdateScrollAreasRecursive(Node* node) {
    if (!node) return;

    if (node->type == NodeType::Carousel && node->id.IsValid()) {
        auto& state = m_scrollStates[node->id];
        state.offset.x = node->scrollOffset.x;
    }

    if (node->type == NodeType::ScrollArea && node->id.IsValid()) {
        auto& state = m_scrollStates[node->id];
        const ScrollbarMetrics& m = m_theme.GetScrollbar(node->uiSize);

        const float viewportW = node->computedRect.w;
        const float viewportH = node->computedRect.h;
        const float contentW = node->scrollContentSize.x;
        const float contentH = node->scrollContentSize.y;

        const float maxScrollY = std::max(0.0f, contentH - viewportH);
        const float maxScrollX = std::max(0.0f, contentW - viewportW);

        state.offset.y = std::clamp(state.offset.y, 0.0f, maxScrollY);
        state.offset.x = std::clamp(state.offset.x, 0.0f, maxScrollX);
        node->scrollOffset = state.offset;

        // Thumb geometry for vertical scroll
        if (viewportH > 10.0f && maxScrollY > 0.0f) {
            const float trackX = node->computedRect.Right() - m.trackWidth - m.margin;
            const float trackY = node->computedRect.y + m.margin;
            const float trackH = std::max(0.0f, viewportH - 2.0f * m.margin);

            const float thumbH = std::clamp((viewportH / contentH) * trackH, m.minThumbLength, trackH);
            const float travelH = trackH - thumbH;
            const float scrollRatio = (maxScrollY > 0.0f) ? (state.offset.y / maxScrollY) : 0.0f;
            const float thumbY = trackY + scrollRatio * travelH;
            const Rect trackRect(trackX, trackY, m.trackWidth, trackH);
            const Rect thumbRect(trackX, thumbY, m.trackWidth, thumbH);

            const bool hoverThumb = thumbRect.Contains(m_mousePos);
            const bool hoverTrack = trackRect.Contains(m_mousePos);
            node->scrollbarHovered = hoverThumb || hoverTrack;

            if (hoverThumb && m_mouseClicked) {
                state.draggingY = true;
                state.dragStartMouse = m_mousePos;
                state.dragStartOffset = state.offset.y;
                m_activeId = node->id;
            } else if (hoverTrack && m_mouseClicked && !hoverThumb) {
                // Page jump towards click position on track
                if (m_mousePos.y < thumbY) {
                    state.offset.y = std::max(0.0f, state.offset.y - viewportH * 0.8f);
                } else {
                    state.offset.y = std::min(maxScrollY, state.offset.y + viewportH * 0.8f);
                }
                node->scrollOffset = state.offset;
            }

            if (state.draggingY) {
                node->scrollbarActive = true;
                if (m_mouseDown) {
                    const float deltaY = m_mousePos.y - state.dragStartMouse.y;
                    if (travelH > 0.0f) {
                        state.offset.y = std::clamp(state.dragStartOffset + (deltaY / travelH) * maxScrollY, 0.0f, maxScrollY);
                        node->scrollOffset = state.offset;
                    }
                } else {
                    state.draggingY = false;
                    if (m_activeId == node->id) {
                        m_activeId = Id::Null();
                    }
                }
            }
        } else {
            state.draggingY = false;
        }
    }

    for (Node* child = node->firstChild; child != nullptr; child = child->nextSibling) {
        UpdateScrollAreasRecursive(child);
    }
}

void Context::ScrollToId(Id id) {
    if (!id.IsValid() || !m_root) return;

    Node* target = nullptr;
    std::function<void(Node*)> findNode = [&](Node* n) {
        if (!n || target) return;
        if (n->id == id) {
            target = n;
            return;
        }
        for (Node* c = n->firstChild; c != nullptr; c = c->nextSibling) {
            findNode(c);
        }
    };
    findNode(m_root);

    if (!target) return;

    for (Node* p = target->parent; p != nullptr; p = p->parent) {
        if (p->type == NodeType::ScrollArea && p->id.IsValid()) {
            auto& state = m_scrollStates[p->id];
            const float targetTop = target->computedRect.y;
            const float targetBottom = target->computedRect.Bottom();
            const float viewTop = p->computedRect.y;
            const float viewBottom = p->computedRect.Bottom();

            if (targetTop < viewTop) {
                state.offset.y -= (viewTop - targetTop);
            } else if (targetBottom > viewBottom) {
                state.offset.y += (targetBottom - viewBottom);
            }
            if (state.offset.y < 0.0f) state.offset.y = 0.0f;
            p->scrollOffset = state.offset;
            break;
        } else if (p->type == NodeType::Carousel && p->id.IsValid()) {
            auto& state = m_scrollStates[p->id];
            const float targetLeft = target->computedRect.x;
            const float targetRight = target->computedRect.Right();
            auto* cData = p->As<CarouselData>();
            const float viewLeft = cData ? cData->viewportRect.x : p->computedRect.x;
            const float viewRight = cData ? cData->viewportRect.Right() : p->computedRect.Right();

            if (targetLeft < viewLeft) {
                state.offset.x -= (viewLeft - targetLeft);
            } else if (targetRight > viewRight) {
                state.offset.x += (targetRight - viewRight);
            }
            if (state.offset.x < 0.0f) state.offset.x = 0.0f;
            p->scrollOffset = state.offset;
            break;
        }
    }
}

void Context::EndFrame(ICanvas& canvas) {
    // Balance any unclosed container scopes
    while (m_containerStack.size() > 1) {
        m_containerStack.pop_back();
    }

    const Vec2 logicalSize = m_viewportSize / ((m_dpiScale > 0.0f) ? m_dpiScale : 1.0f);

    // Resolve geometry using canvas font metrics
    LayoutSolver::Measure(m_root, &canvas, m_theme);
    LayoutSolver::Arrange(m_root, Rect(0.0f, 0.0f, logicalSize.x, logicalSize.y), m_theme);

    // Update interactive scroll areas
    UpdateScrollAreasRecursive(m_root);

    // Cache computed bounds for instantaneous interaction feedback next frame
    CollectBoundsRecursive(m_root, m_currFrameBounds, Rect(0.0f, 0.0f, logicalSize.x, logicalSize.y), m_theme);
    m_prevFrameBounds = std::move(m_currFrameBounds);

    // Reset single-frame mouse impulse states
    m_mouseClicked = false;
    m_mouseReleased = false;
    m_mouseWheel = {0.0f, 0.0f};

    // Calculate dirty damage regions by snapshot diffing
    std::unordered_map<Id, WidgetSnapshot> currSnapshots;
    currSnapshots.reserve(m_prevSnapshots.size() + 16);
    std::vector<Rect> damageRects = std::move(m_explicitDamageRects);
    m_explicitDamageRects.clear();

    auto addDamage = [&](Rect r) {
        if (r.w > 0.0f && r.h > 0.0f) {
            damageRects.push_back(r);
        }
    };

    const bool cursorVis = IsCursorVisible();

    std::function<void(Node*)> collectSnapshots = [&](Node* n) {
        if (!n) return;
        if (n->id.IsValid()) {
            WidgetSnapshot snap;
            snap.rect = n->computedRect;
            snap.flags = (n->hovered ? 1u : 0u) |
                         (n->pressed ? 2u : 0u) |
                         (n->disabled ? 4u : 0u) |
                         (n->checked ? 8u : 0u) |
                         (n->active ? 64u : 0u);
            snap.bgColor = n->backgroundColor.ToUint32();
            snap.borderColor = n->borderColor.ToUint32();

            uint64_t h = 14695981039346656037ULL;
            auto hashStr = [&](std::string_view sv) {
                for (char c : sv) {
                    h = (h ^ static_cast<uint8_t>(c)) * 1099511628211ULL;
                }
            };
            auto hashBytes = [&](const void* data, size_t len) {
                const uint8_t* p = static_cast<const uint8_t*>(data);
                for (size_t i = 0; i < len; ++i) {
                    h = (h ^ p[i]) * 1099511628211ULL;
                }
            };
            auto hashVal = [&](auto val) {
                hashBytes(&val, sizeof(val));
            };
            auto hashIcon = [&](const Icon& ic) {
                hashVal(static_cast<uint8_t>(ic.GetType()));
                hashVal(static_cast<uint8_t>(ic.GetId()));
                if (!ic.GetPath().empty()) hashStr(ic.GetPath());
            };

            if (m_editState.id.IsValid() && n->id == m_editState.id) {
                snap.flags |= (cursorVis ? 16u : 0u) | 32u;
                snap.cursor = m_editState.cursor;
                hashStr(m_editState.text);
            } else {
                hashStr(n->text);
            }

            hashVal(static_cast<uint8_t>(n->type));
            hashVal(static_cast<uint8_t>(n->uiSize));
            hashVal(n->borderWidth);
            hashVal(n->cornerRadius);

            switch (n->type) {
                case NodeType::TabItem: {
                    if (const auto* t = n->As<TabItemData>()) {
                        hashVal(static_cast<uint8_t>(t->barOpts.style));
                        hashVal(static_cast<uint8_t>(t->barOpts.showSeparator));
                        hashVal(t->barOpts.activeColor.ToUint32());
                        hashVal(static_cast<uint8_t>(t->item.variant));
                        hashStr(t->item.badge);
                        hashIcon(t->item.icon);
                    }
                    break;
                }
                case NodeType::Button: {
                    if (const auto* b = n->As<ButtonOptions>()) {
                        hashVal(static_cast<uint8_t>(b->variant));
                        hashVal(static_cast<uint8_t>(b->iconPlacement));
                        hashVal(static_cast<uint8_t>(b->active));
                        hashIcon(b->icon);
                    }
                    break;
                }
                case NodeType::Switch: {
                    if (const auto* s = n->As<SwitchOptions>()) {
                        hashVal(static_cast<uint8_t>(s->variant));
                        hashVal(static_cast<uint8_t>(s->labelPlacement));
                    }
                    break;
                }
                case NodeType::Checkbox: {
                    if (const auto* c = n->As<CheckboxOptions>()) {
                        hashVal(static_cast<uint8_t>(c->variant));
                        hashVal(static_cast<uint8_t>(c->labelPlacement));
                        hashVal(static_cast<uint8_t>(c->indeterminate));
                    }
                    break;
                }
                case NodeType::InputField: {
                    if (const auto* in = n->As<InputFieldData>()) {
                        hashVal(static_cast<uint8_t>(in->opts.variant));
                        hashStr(in->opts.prefix);
                        hashStr(in->opts.suffix);
                        hashIcon(in->opts.prefixIcon);
                        hashIcon(in->opts.suffixIcon);
                        hashVal(static_cast<uint8_t>(in->opts.readOnly));
                        if (!in->displayText.empty()) {
                            hashStr(in->displayText);
                        }
                    }
                    break;
                }
                case NodeType::Select: {
                    if (const auto* sel = n->As<SelectData>()) {
                        hashVal(static_cast<uint8_t>(sel->opts.variant));
                        hashIcon(sel->opts.prefixIcon);
                        if (!sel->displayText.empty()) {
                            hashStr(sel->displayText);
                        }
                    }
                    break;
                }
                case NodeType::Text: {
                    if (const auto* t = n->As<TextOptions>()) {
                        hashVal(static_cast<uint8_t>(t->role));
                        hashVal(static_cast<uint8_t>(t->variant));
                        hashVal(static_cast<uint8_t>(t->align));
                    }
                    break;
                }
                case NodeType::Heading: {
                    if (const auto* h = n->As<HeadingOptions>()) {
                        hashStr(h->title);
                        hashStr(h->subtitle);
                        hashStr(h->badge);
                        hashIcon(h->icon);
                        hashVal(static_cast<uint8_t>(h->variant));
                    }
                    break;
                }
                case NodeType::Carousel: {
                    if (const auto* c = n->As<CarouselData>()) {
                        hashVal(static_cast<uint8_t>(c->showLeftArrow));
                        hashVal(static_cast<uint8_t>(c->showRightArrow));
                        hashVal(static_cast<uint8_t>(c->leftArrowHovered));
                        hashVal(static_cast<uint8_t>(c->rightArrowHovered));
                        hashVal(static_cast<uint8_t>(c->leftArrowPressed));
                        hashVal(static_cast<uint8_t>(c->rightArrowPressed));
                    }
                    hashVal(n->scrollOffset.x);
                    break;
                }
                default: break;
            }

            snap.visualHash = h;

            auto it = m_prevSnapshots.find(n->id);
            if (it != m_prevSnapshots.end()) {
                if (it->second != snap) {
                    addDamage(snap.rect.Union(it->second.rect));
                }
            } else {
                addDamage(snap.rect);
            }

            currSnapshots.emplace(n->id, snap);
        }

        for (Node* c = n->firstChild; c != nullptr; c = c->nextSibling) {
            collectSnapshots(c);
        }
    };

    collectSnapshots(m_root);

    // Detect removed widgets
    for (const auto& [id, prevSnap] : m_prevSnapshots) {
        if (!currSnapshots.contains(id)) {
            addDamage(prevSnap.rect);
        }
    }

    // Determine whether full repaint or partial dirty rect is required
    const float totalViewportArea = logicalSize.x * logicalSize.y;
    if (!m_dirtyRectEnabled || m_forceFullRepaint || m_prevSnapshots.empty() || m_openPopupId.IsValid() ||
        m_debugBoundsEnabled || m_inspectModeEnabled) {
        m_isFullRepaint = true;
        m_hasDirtyDamage = true;
        m_dirtyRect = Rect(0.0f, 0.0f, logicalSize.x, logicalSize.y);
    } else if (damageRects.empty()) {
        m_isFullRepaint = false;
        m_hasDirtyDamage = false;
        m_dirtyRect = Rect{0.0f, 0.0f, 0.0f, 0.0f};
    } else {
        Rect unionRect = damageRects[0];
        for (size_t i = 1; i < damageRects.size(); ++i) {
            unionRect = unionRect.Union(damageRects[i]);
        }
        // Expand by 2.0px margin to cover anti-aliasing edges and clamp to logical viewport
        const float pad = 2.0f;
        const float x1 = std::max(0.0f, std::floor(unionRect.x - pad));
        const float y1 = std::max(0.0f, std::floor(unionRect.y - pad));
        const float x2 = std::min(logicalSize.x, std::ceil(unionRect.Right() + pad));
        const float y2 = std::min(logicalSize.y, std::ceil(unionRect.Bottom() + pad));
        unionRect = Rect(x1, y1, std::max(0.0f, x2 - x1), std::max(0.0f, y2 - y1));

        if (unionRect.Area() >= 0.5f * totalViewportArea) {
            m_isFullRepaint = true;
            m_dirtyRect = Rect(0.0f, 0.0f, logicalSize.x, logicalSize.y);
        } else {
            m_isFullRepaint = false;
            m_dirtyRect = unionRect;
        }
        m_hasDirtyDamage = true;
    }

    m_forceFullRepaint = false;
    m_prevSnapshots = std::move(currSnapshots);

    // If damage exists, clear target buffer and rasterize scene graph
    if (m_hasDirtyDamage) {
        canvas.Begin(static_cast<uint32_t>(m_viewportSize.x), static_cast<uint32_t>(m_viewportSize.y), m_dpiScale);
        canvas.PushClip(m_dirtyRect);

        if (m_isFullRepaint) {
            canvas.Clear(m_clearColor);
        } else {
            canvas.Clear(m_clearColor, m_dirtyRect);
        }

        RenderNodeRecursive(m_root, canvas, m_dirtyRect);

        // Render floating overlay passes (e.g. dropdown popups, tooltips)
        for (const auto& overlayFn : m_overlayPasses) {
            overlayFn(canvas);
        }

        // Developer Debug Pass: Element bounds and padding
        if (m_debugBoundsEnabled) {
            RenderDebugBoundsRecursive(m_root, canvas);
        }

        // Developer Debug Pass: Chrome DevTools-style hover inspector & floating badge
        if (m_inspectModeEnabled) {
            RenderInspectOverlay(m_root, canvas, logicalSize);
        }

        canvas.PopClip();
        canvas.End();
    }

    m_pendingKeyEvents.clear();
    m_pendingTextInput.clear();

    if (m_repaintFrames > 0) {
        --m_repaintFrames;
    }
}

void Context::RenderNodeRecursive(Node* node, ICanvas& canvas, const Rect& dirtyRect) {
    if (!node) return;
    if (node->computedRect.w <= 0.0f || node->computedRect.h <= 0.0f) return;
    if (!node->computedRect.Intersects(dirtyRect)) return;

    if (node->clipChildren) {
        if (node->type == NodeType::ScrollArea) {
            const ScrollbarMetrics& sm = m_theme.GetScrollbar(node->uiSize);
            const bool showScrollbar = (node->scrollbarPolicy == VisibilityPolicy::Always) ||
                (node->scrollbarPolicy == VisibilityPolicy::Auto && node->scrollContentSize.y > node->computedRect.h);
            const float gutter = (showScrollbar && node->scrollOrientation != Orientation::Horizontal) ? sm.gutter : 0.0f;
            const Rect clipRect(
                node->computedRect.x + node->padding.left,
                node->computedRect.y + node->padding.top,
                std::max(0.0f, node->computedRect.w - node->padding.Horizontal() - gutter),
                std::max(0.0f, node->computedRect.h - node->padding.Vertical())
            );
            canvas.PushClip(clipRect);
        } else if (node->type == NodeType::Carousel) {
            auto* cData = node->As<CarouselData>();
            canvas.PushClip(cData ? cData->viewportRect : node->computedRect);
        } else {
            canvas.PushClip(node->computedRect);
        }
    }

    // Render background / border if present (skip for self-rendering leaf widgets)
    if (!node->IsLeaf()) {
        if (node->backgroundColor.a > 0 || (node->borderColor.a > 0 && node->borderWidth > 0.0f)) {
            if (node->cornerRadius > 0.0f) {
                canvas.DrawRoundRect(
                    node->computedRect,
                    node->cornerRadius,
                    node->backgroundColor,
                    node->borderColor,
                    node->borderWidth
                );
            } else {
                canvas.DrawRect(node->computedRect, node->backgroundColor);
            }
        }
    }

    // Render specific widget contents
    if (node->renderFn) {
        node->renderFn(node, canvas, m_theme);
    }

    // Recurse on children
    for (Node* child = node->firstChild; child != nullptr; child = child->nextSibling) {
        RenderNodeRecursive(child, canvas, dirtyRect);
    }

    if (node->clipChildren) {
        canvas.PopClip();
    }

    // Render post-pass overlay contents (after children and outside clip)
    if (node->postRenderFn) {
        node->postRenderFn(node, canvas, m_theme);
    }
}

void Context::SetThemeMode(ThemeMode mode) noexcept {
    if (m_theme.mode != mode) {
        m_theme = (mode == ThemeMode::Dark) ? UiTheme::Dark(m_theme.density) : UiTheme::Light(m_theme.density);
        m_clearColor = m_theme.palette.background;
        InvalidateAll();
    }
}

void Context::SetDebugBounds(bool enabled) noexcept {
    if (m_debugBoundsEnabled != enabled) {
        m_debugBoundsEnabled = enabled;
        InvalidateAll();
    }
}

void Context::SetInspectMode(bool enabled) noexcept {
    if (m_inspectModeEnabled != enabled) {
        m_inspectModeEnabled = enabled;
        InvalidateAll();
    }
}

void Context::RenderDebugBoundsRecursive(Node* node, ICanvas& canvas) {
    if (!node) return;
    if (node->computedRect.w <= 0.0f || node->computedRect.h <= 0.0f) return;

    if (node != m_root) {
        const NodeCategory cat = node->GetCategory();

        Color stroke;
        Color padColor;
        switch (cat) {
            case NodeCategory::Container:
                stroke = Color(192, 132, 252, 220); // Purple-400 (#C084FC)
                padColor = Color(192, 132, 252, 35);
                break;
            case NodeCategory::Layout:
                stroke = Color(56, 189, 248, 220);  // Sky-400 (#38BDF8)
                padColor = Color(56, 189, 248, 30);
                break;
            case NodeCategory::Widget:
                stroke = Color(74, 222, 128, 220);  // Green-400 (#4ADE80)
                padColor = Color(74, 222, 128, 35);
                break;
            case NodeCategory::Spacer:
                stroke = Color(245, 158, 11, 220);  // Amber-500 (#F59E0B)
                padColor = Color(245, 158, 11, 35);
                break;
        }

        // Draw 1px crisp bounding outline inset by 0.5px
        const Rect strokeRect(
            node->computedRect.x + 0.5f,
            node->computedRect.y + 0.5f,
            std::max(0.0f, node->computedRect.w - 1.0f),
            std::max(0.0f, node->computedRect.h - 1.0f)
        );
        canvas.DrawRoundRect(strokeRect, 0.0f, Color(0, 0, 0, 0), stroke, 1.0f);

        // Draw spacer fill
        if (cat == NodeCategory::Spacer) {
            canvas.DrawRect(node->computedRect, padColor);
        }

        // Draw padding strips if node defines internal padding
        const auto& p = node->padding;
        if (p.top > 0.0f || p.bottom > 0.0f || p.left > 0.0f || p.right > 0.0f) {
            const float x = node->computedRect.x;
            const float y = node->computedRect.y;
            const float w = node->computedRect.w;
            const float h = node->computedRect.h;

            if (p.top > 0.0f) {
                canvas.DrawRect(Rect(x, y, w, p.top), padColor);
            }
            if (p.bottom > 0.0f) {
                canvas.DrawRect(Rect(x, y + h - p.bottom, w, p.bottom), padColor);
            }
            const float midH = std::max(0.0f, h - p.top - p.bottom);
            if (midH > 0.0f) {
                if (p.left > 0.0f) {
                    canvas.DrawRect(Rect(x, y + p.top, p.left, midH), padColor);
                }
                if (p.right > 0.0f) {
                    canvas.DrawRect(Rect(x + w - p.right, y + p.top, p.right, midH), padColor);
                }
            }
        }
    }

    for (Node* child = node->firstChild; child != nullptr; child = child->nextSibling) {
        RenderDebugBoundsRecursive(child, canvas);
    }
}

Node* Context::FindDeepestNodeAt(Node* root, Vec2 pos, Rect clipRect) {
    if (!root) return nullptr;
    if (root->computedRect.w <= 0.0f || root->computedRect.h <= 0.0f) return nullptr;

    Rect effectiveClip = clipRect;
    if (root->clipChildren) {
        if (root->type == NodeType::ScrollArea) {
            effectiveClip = clipRect.Intersect(root->computedRect);
        } else if (root->type == NodeType::Carousel) {
            auto* cData = root->As<CarouselData>();
            effectiveClip = clipRect.Intersect(cData ? cData->viewportRect : root->computedRect);
        } else {
            effectiveClip = clipRect.Intersect(root->computedRect);
        }
    }

    const Rect visibleRect = root->computedRect.Intersect(effectiveClip);
    if (!visibleRect.Contains(pos)) {
        return nullptr;
    }

    Node* bestHit = nullptr;
    for (Node* c = root->firstChild; c != nullptr; c = c->nextSibling) {
        Node* hit = FindDeepestNodeAt(c, pos, effectiveClip);
        if (hit) {
            bestHit = hit;
        }
    }

    return bestHit ? bestHit : root;
}

void Context::RenderInspectOverlay(Node* root, ICanvas& canvas, Vec2 logicalSize) {
    if (!root) return;

    Node* target = FindDeepestNodeAt(root, m_mousePos, Rect(0.0f, 0.0f, logicalSize.x, logicalSize.y));
    if (!target || target == m_root) return;

    // 1. Highlight inspected target with Chrome DevTools Box Model styling
    // Content box: translucent blue (#6FA8DC, ~40% opacity)
    const Rect contentRect(
        target->computedRect.x + target->padding.left,
        target->computedRect.y + target->padding.top,
        std::max(0.0f, target->computedRect.w - target->padding.Horizontal()),
        std::max(0.0f, target->computedRect.h - target->padding.Vertical())
    );
    if (contentRect.w > 0.0f && contentRect.h > 0.0f) {
        canvas.DrawRect(contentRect, Color(111, 168, 220, 100));
    }

    // Padding strips: translucent green (#93C47D, ~55% opacity)
    const auto& p = target->padding;
    if (p.top > 0.0f || p.bottom > 0.0f || p.left > 0.0f || p.right > 0.0f) {
        const Color devToolsPadColor(147, 196, 125, 140);
        const float x = target->computedRect.x;
        const float y = target->computedRect.y;
        const float w = target->computedRect.w;
        const float h = target->computedRect.h;

        if (p.top > 0.0f) {
            canvas.DrawRect(Rect(x, y, w, p.top), devToolsPadColor);
        }
        if (p.bottom > 0.0f) {
            canvas.DrawRect(Rect(x, y + h - p.bottom, w, p.bottom), devToolsPadColor);
        }
        const float midH = std::max(0.0f, h - p.top - p.bottom);
        if (midH > 0.0f) {
            if (p.left > 0.0f) {
                canvas.DrawRect(Rect(x, y + p.top, p.left, midH), devToolsPadColor);
            }
            if (p.right > 0.0f) {
                canvas.DrawRect(Rect(x + w - p.right, y + p.top, p.right, midH), devToolsPadColor);
            }
        }
    }

    // Outer 2px solid cyan boundary
    canvas.DrawRoundRect(target->computedRect, 0.0f, Color(0, 0, 0, 0), Color(56, 189, 248, 255), 2.0f);

    // 2. Build DevTools floating info badge
    std::string typeStr;
    if (target->type == NodeType::Cell) {
        typeStr = std::format("Cell ({})", target->colSpan);
    } else {
        typeStr = std::string(target->GetTypeName());
    }

    std::string labelStr;
    if (!target->text.empty()) {
        labelStr = std::string(target->text.substr(0, 24));
    } else if (target->type == NodeType::Heading) {
        if (const auto* h = target->As<HeadingOptions>()) {
            if (!h->title.empty()) {
                labelStr = std::string(h->title.substr(0, 24));
            }
        }
    }

    std::string line1 = labelStr.empty() ? typeStr : std::format("{} \"{}\"", typeStr, labelStr);
    std::string line2 = std::format("{:.1f} × {:.1f} px", target->computedRect.w, target->computedRect.h);
    std::string line3;
    if (p.top > 0.0f || p.bottom > 0.0f || p.left > 0.0f || p.right > 0.0f) {
        line3 = std::format("pad: {:.0f}, {:.0f}, {:.0f}, {:.0f}", p.top, p.right, p.bottom, p.left);
    } else if (target->gap > 0.0f) {
        line3 = std::format("gap: {:.0f} px", target->gap);
    } else if (target->isSpacer) {
        line3 = std::format("flex: {:.1f}", target->flexGrow);
    } else {
        const char* alignStr = "Start";
        if (target->align == Alignment::Center) alignStr = "Center";
        else if (target->align == Alignment::End) alignStr = "End";
        else if (target->align == Alignment::Stretch) alignStr = "Stretch";
        line3 = std::format("align: {}", alignStr);
    }

    const float fontSize = 11.0f;
    const float w1 = canvas.MeasureText(line1, fontSize).x;
    const float w2 = canvas.MeasureText(line2, fontSize).x;
    const float w3 = canvas.MeasureText(line3, fontSize).x;
    const float maxTextW = std::max({w1, w2, w3});

    const float badgeW = std::max(140.0f, maxTextW + 24.0f);
    const float badgeH = 58.0f;

    // Positioning with smart flip if close to viewport boundaries
    float bx = m_mousePos.x + 16.0f;
    float by = m_mousePos.y + 16.0f;
    if (bx + badgeW > logicalSize.x - 8.0f) {
        bx = m_mousePos.x - badgeW - 12.0f;
    }
    if (by + badgeH > logicalSize.y - 8.0f) {
        by = m_mousePos.y - badgeH - 12.0f;
    }
    bx = std::clamp(bx, 8.0f, std::max(8.0f, logicalSize.x - badgeW - 8.0f));
    by = std::clamp(by, 8.0f, std::max(8.0f, logicalSize.y - badgeH - 8.0f));

    const Rect badgeRect(bx, by, badgeW, badgeH);

    // Draw dark tooltip backdrop
    canvas.DrawRoundRect(badgeRect, 6.0f, Color(15, 23, 42, 245), Color(71, 85, 105, 220), 1.0f);

    // Render 3 lines of text inside badge
    canvas.DrawTextBox(line1, Rect(bx + 10.0f, by + 5.0f, badgeW - 20.0f, 16.0f), fontSize, Color(56, 189, 248, 255), 0.0f, 0.5f);
    canvas.DrawTextBox(line2, Rect(bx + 10.0f, by + 21.0f, badgeW - 20.0f, 16.0f), fontSize, Color(241, 245, 249, 255), 0.0f, 0.5f);
    canvas.DrawTextBox(line3, Rect(bx + 10.0f, by + 37.0f, badgeW - 20.0f, 16.0f), fontSize, Color(148, 163, 184, 255), 0.0f, 0.5f);
}

} // namespace guikit
