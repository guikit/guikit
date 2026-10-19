#include "guikit/Context.hpp"
#include <algorithm>
#include <format>
#include <string>
#include "guikit/core/TextUtil.hpp"

namespace guikit {

Context::Context() {
    m_containerStack.reserve(16);
    m_sizeStack.reserve(16);
    m_disabledStack.reserve(16);
    m_sizeStack.push_back(UiSize::Md);
    m_disabledStack.push_back(false);
}

void Context::BeginFrame(Vec2 viewportSize, float dpiScale) {
    ++m_frameIndex;
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
    m_currFocusableIds.clear();
    m_activeTooltip = {};
    m_activeTooltipAnchor = {};

    if (!m_openPopupId.IsValid() && !m_prevFocusableIds.empty()) {
        const bool multilineEditorFocused =
            m_focusedId.IsValid() &&
            m_editState.id == m_focusedId &&
            (m_editState.multiline || m_suspendDirectEditInput) &&
            !m_editState.readOnly;
        for (auto it = m_pendingKeyEvents.begin(); it != m_pendingKeyEvents.end(); ) {
            const bool isIndent = m_keymap.Matches(KeyAction::Indent, it->key, it->mods);
            const bool isUnindent = m_keymap.Matches(KeyAction::Unindent, it->key, it->mods);
            if ((isIndent || isUnindent) && !multilineEditorFocused) {
                const bool reverse = isUnindent;
                it = m_pendingKeyEvents.erase(it);
                auto curIt = std::find(m_prevFocusableIds.begin(), m_prevFocusableIds.end(), m_focusedId);
                Id nextId = Id::Null();
                if (curIt == m_prevFocusableIds.end()) {
                    nextId = reverse ? m_prevFocusableIds.back() : m_prevFocusableIds.front();
                } else {
                    const size_t idx = static_cast<size_t>(std::distance(m_prevFocusableIds.begin(), curIt));
                    const size_t n = m_prevFocusableIds.size();
                    const size_t nextIdx = reverse ? ((idx + n - 1) % n) : ((idx + 1) % n);
                    nextId = m_prevFocusableIds[nextIdx];
                }
                if (m_editState.id.IsValid() && m_editState.id != nextId) {
                    m_editState.id = Id::Null();
                }
                m_focusedId = nextId;
                m_suspendDirectEditInput = false;
                InvalidateAll();
            } else {
                ++it;
            }
        }
    }

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
                if (m_editState.id == m_focusedId) {
                    m_editState.id = Id::Null();
                }
                m_focusedId = Id::Null();
                m_suspendDirectEditInput = false;
            }
        } else {
            if (m_editState.id == m_focusedId) {
                m_editState.id = Id::Null();
            }
            m_focusedId = Id::Null();
            m_suspendDirectEditInput = false;
        }
    }

    m_arena.Reset();
    m_containerStack.clear();
    m_sizeStack.clear();
    m_disabledStack.clear();
    m_themeStack.clear();
    m_scaleStack.clear();
    m_currFrameBounds.clear();
    m_currUnclippedBounds.clear();

    m_sizeStack.push_back(UiSize::Md);
    m_disabledStack.push_back(false);
    m_scaleStack.push_back(1.0f);

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
        if (m_mouseDoubleClicked) {
            res.doubleClicked = true;
        }
        if (m_rightMouseClicked) {
            res.rightClicked = true;
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
    if (!node) return;
    Node* parent = node->parent ? node->parent : (m_containerStack.empty() ? nullptr : m_containerStack.back());
    if (!parent) return;
    if (parent->direction == FlexDirection::Column || parent->direction == FlexDirection::Grid) {
        if (node->widthMode != SizeMode::Fit && node->widthMode != SizeMode::Fixed) {
            node->widthMode = SizeMode::Stretch;
        }
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

Rect Context::GetPrevUnclippedBounds(Id id) const {
    auto it = m_prevUnclippedBounds.find(id);
    if (it != m_prevUnclippedBounds.end()) {
        return it->second;
    }
    return GetPrevFrameBounds(id);
}

Rect Context::GetParentPrevFrameBounds() const {
    for (auto it = m_containerStack.rbegin(); it != m_containerStack.rend(); ++it) {
        const Node* parent = *it;
        if (!parent || !parent->id.IsValid()) continue;
        auto found = m_prevFrameBounds.find(parent->id);
        if (found != m_prevFrameBounds.end() && found->second.w > 1.0f && found->second.h > 1.0f) {
            Rect r = found->second;
            r.x += parent->padding.left;
            r.y += parent->padding.top;
            r.w = std::max(0.0f, r.w - (parent->padding.left + parent->padding.right));
            r.h = std::max(0.0f, r.h - (parent->padding.top + parent->padding.bottom));
            if (r.w > 1.0f && r.h > 1.0f) {
                return r;
            }
        }
    }
    return Rect{};
}

void Context::PushContainer(Node* node) {
    m_containerStack.push_back(node);
}

void Context::PopContainer(Node* node) {
    if (!m_containerStack.empty() && m_containerStack.back() == node) {
        m_containerStack.pop_back();
        if (node->theme != nullptr) {
            PopTheme();
        }
        PopScale();
        PopSize();
        PopDisabled();
    }
}

UiTheme* Context::PushTheme(const UiTheme& theme) {
    UiTheme* arenaTheme = m_arena.Allocate<UiTheme>(theme);
    m_themeStack.push_back(arenaTheme);
    return arenaTheme;
}

UiTheme* Context::PushScopedTheme(const UiTheme* explicitTheme, std::optional<float> explicitScale) {
    if (explicitTheme == nullptr && !explicitScale.has_value()) {
        return nullptr;
    }
    const float prevScale = GetCurrentScale();
    const float activeScale = explicitScale.value_or(prevScale);
    UiTheme scopedTheme = (explicitTheme != nullptr) ? *explicitTheme : GetTheme();
    if (explicitTheme != nullptr) {
        if (activeScale > 0.0f && activeScale != 1.0f) {
            scopedTheme = scopedTheme.Scaled(activeScale);
        }
    } else if (explicitScale.has_value() && *explicitScale > 0.0f) {
        const float relFactor = (prevScale > 0.0f) ? (*explicitScale / prevScale) : *explicitScale;
        if (relFactor != 1.0f) {
            scopedTheme = scopedTheme.Scaled(relFactor);
        }
    }
    return PushTheme(scopedTheme);
}

void Context::PopTheme() {
    if (!m_themeStack.empty()) {
        m_themeStack.pop_back();
    }
}

void Context::PushScale(float scale) {
    m_scaleStack.push_back((scale > 0.0f) ? scale : 1.0f);
}

void Context::PopScale() {
    if (m_scaleStack.size() > 1) {
        m_scaleStack.pop_back();
    }
}

void Context::PushSize(UiSize size) {
    m_sizeStack.push_back(size == UiSize::None ? GetCurrentSize() : size);
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

ContainerScope Context::Row(RowOptions opts, std::source_location loc) {
    return Open<guikit::Row>(opts, loc);
}

ContainerScope Context::Column(ColumnOptions opts, std::source_location loc) {
    return Open<guikit::Column>(opts, loc);
}

ContainerScope Context::Wrap(WrapOptions opts, std::source_location loc) {
    return Open<guikit::Wrap>(opts, loc);
}


GridScope Context::Grid(GridOptions opts, std::source_location loc) {
    return Open<guikit::Grid>(opts, loc);
}

void Context::Spacer(float flex, std::source_location loc) {
    Run<guikit::Spacer>(flex, loc);
}

AccordionScope Context::Accordion(AccordionOptions opts, std::source_location loc) {
    return guikit::Accordion::Open(*this, nullptr, opts, loc);
}

AccordionScope Context::Accordion(std::string_view label, AccordionOptions opts, std::source_location loc) {
    opts.label.text = label;
    return guikit::Accordion::Open(*this, nullptr, opts, loc);
}

AccordionScope Context::Accordion(bool& open, AccordionOptions opts, std::source_location loc) {
    return guikit::Accordion::Open(*this, &open, opts, loc);
}

ContainerScope Context::Box(BoxOptions opts, std::source_location loc) {
    return Open<guikit::Box>(opts, loc);
}

ContainerScope Context::Card(CardOptions opts, std::source_location loc) {
    return Open<guikit::Card>(opts, loc);
}

ContainerScope Context::ScrollArea(ScrollAreaOptions opts, std::source_location loc) {
    return Open<guikit::ScrollArea>(opts, loc);
}

ContainerScope Context::Carousel(CarouselOptions opts, std::source_location loc) {
    return Open<guikit::Carousel>(opts, loc);
}

SidebarScope Context::Sidebar(SidebarOptions opts, std::source_location loc) {
    return guikit::Sidebar::Open(*this, nullptr, opts, loc);
}

SidebarScope Context::Sidebar(SidebarMode& mode, SidebarOptions opts, std::source_location loc) {
    return guikit::Sidebar::Open(*this, &mode, opts, loc);
}

SidebarScope Context::Sidebar(bool& open, SidebarOptions opts, std::source_location loc) {
    SidebarMode mode = open ? SidebarMode::Expanded : SidebarMode::Collapsed;
    SidebarScope scope = guikit::Sidebar::Open(*this, &mode, opts, loc);
    open = (mode != SidebarMode::Collapsed);
    return scope;
}

SceneScope Context::Scene(SceneOptions opts, std::source_location loc) {
    return Open<guikit::Scene>(opts, loc);
}

TreeViewScope Context::TreeView(TreeViewOptions opts, std::source_location loc) {
    return Open<guikit::TreeView>(opts, loc);
}

TreeItemScope Context::TreeItem(TreeItemOptions opts, std::source_location loc) {
    return guikit::TreeView::OpenItem(*this, nullptr, opts, loc);
}

TreeItemScope Context::TreeItem(std::string_view label, TreeItemOptions opts, std::source_location loc) {
    opts.label.text = label;
    return guikit::TreeView::OpenItem(*this, nullptr, opts, loc);
}

TreeItemScope Context::TreeItem(bool& open, TreeItemOptions opts, std::source_location loc) {
    return guikit::TreeView::OpenItem(*this, &open, opts, loc);
}

TreeItemScope Context::TreeItem(bool& open, std::string_view label, TreeItemOptions opts, std::source_location loc) {
    opts.label.text = label;
    return guikit::TreeView::OpenItem(*this, &open, opts, loc);
}

Response Context::Button(ButtonOptions opts, std::source_location loc) {
    return Run<guikit::Button>(opts, loc);
}

UiString Context::String(std::string_view text) {
    return UiString(AllocString(text));
}

std::string_view Context::AllocString(std::string_view text) {
    if (text.empty()) return {};
    char* buf = static_cast<char*>(m_arena.AllocateRaw(text.size() + 1, alignof(char)));
    std::memcpy(buf, text.data(), text.size());
    buf[text.size()] = '\0';
    return std::string_view(buf, text.size());
}

Response Context::Text(UiText label, TextOptions opts, std::source_location loc) {
    return Run<guikit::Text>(label, opts, loc);
}

ElementScope Context::Element(ElementOptions opts, std::source_location loc) {
    return Open<guikit::Element>(opts, loc);
}

Response Context::Heading(HeadingOptions opts, std::source_location loc) {
    return Run<guikit::Heading>(opts, loc);
}

Response Context::Image(std::string_view source, ImageOptions opts, std::source_location loc) {
    return Run<guikit::Image>(source, opts, loc);
}

Response Context::Image(ImageOptions opts, std::source_location loc) {
    return Run<guikit::Image>(opts.source, opts, loc);
}

MarkdownResponse Context::Markdown(std::string_view markdownText, const MarkdownOptions& opts, std::source_location loc) {
    return Run<guikit::Markdown>(markdownText, opts, loc);
}

Response Context::Switch(bool& value, SwitchOptions opts, std::source_location loc) {
    return Run<guikit::Switch>(&value, value, opts, loc);
}

Response Context::Switch(const bool& checked, SwitchOptions opts, std::source_location loc) {
    return Run<guikit::Switch>(nullptr, checked, opts, loc);
}

Response Context::Checkbox(bool& value, CheckboxOptions opts, std::source_location loc) {
    return Run<guikit::Checkbox>(&value, value, opts, loc);
}

Response Context::Checkbox(const bool& checked, CheckboxOptions opts, std::source_location loc) {
    return Run<guikit::Checkbox>(nullptr, checked, opts, loc);
}

Response Context::Radio(bool& value, RadioOptions opts, std::source_location loc) {
    return Run<guikit::Radio>(&value, value, opts, loc);
}

Response Context::Radio(const bool& checked, RadioOptions opts, std::source_location loc) {
    return Run<guikit::Radio>(nullptr, checked, opts, loc);
}

Response Context::InputField(std::string& value, InputFieldOptions opts, std::source_location loc) {
    return Run<guikit::InputField>(&value, std::string_view{}, opts, loc);
}

Response Context::InputField(std::string_view value, InputFieldOptions opts, std::source_location loc) {
    return Run<guikit::InputField>(nullptr, value, opts, loc);
}

Response Context::StepSlider(
    UiSize& size,
    StepSliderOptions opts,
    std::source_location loc
) {
    static constexpr std::array<std::string_view, 6> kSizeLabelsWithNone = {"0", "XS", "SM", "MD", "LG", "XL"};
    static constexpr std::array<UiSize, 6> kSizesWithNone = {UiSize::None, UiSize::Xs, UiSize::Sm, UiSize::Md, UiSize::Lg, UiSize::Xl};

    static constexpr std::array<std::string_view, 5> kSizeLabels = {"XS", "SM", "MD", "LG", "XL"};
    static constexpr std::array<UiSize, 5> kSizes = {UiSize::Xs, UiSize::Sm, UiSize::Md, UiSize::Lg, UiSize::Xl};

    const bool hasNone = opts.includeNone || size == UiSize::None;
    const auto sizesSpan = hasNone ? std::span<const UiSize>(kSizesWithNone) : std::span<const UiSize>(kSizes);
    const auto labelsSpan = hasNone ? std::span<const std::string_view>(kSizeLabelsWithNone) : std::span<const std::string_view>(kSizeLabels);

    size_t idx = 0;
    for (size_t i = 0; i < sizesSpan.size(); ++i) {
        if (sizesSpan[i] == size) {
            idx = i;
            break;
        }
    }

    if (opts.stepLabels.empty()) {
        opts.stepLabels = labelsSpan;
    }

    Response res = StepSlider(idx, opts.stepLabels.size(), opts, loc);
    if (res.changed && idx < sizesSpan.size()) {
        size = sizesSpan[idx];
    }
    return res;
}

Response Context::StepSlider(
    UiVariant& variant,
    StepSliderOptions opts,
    std::source_location loc
) {
    static constexpr std::array<std::string_view, 8> kVariantLabels = {
        "Def", "Pri", "Sec", "Suc", "Wrn", "Dgr", "Inf", "Gho"
    };
    static constexpr std::array<std::string_view, 8> kVariantFullNames = {
        "Default", "Primary", "Secondary", "Success", "Warning", "Danger", "Info", "Ghost"
    };
    static constexpr std::array<UiVariant, 8> kVariants = {
        UiVariant::Default, UiVariant::Primary, UiVariant::Secondary, UiVariant::Success,
        UiVariant::Warning, UiVariant::Danger, UiVariant::Info, UiVariant::Ghost
    };

    size_t idx = 0;
    for (size_t i = 0; i < kVariants.size(); ++i) {
        if (kVariants[i] == variant) {
            idx = i;
            break;
        }
    }

    if (opts.stepLabels.empty()) {
        opts.stepLabels = kVariantLabels;
    }
    if (opts.valueText.empty() && idx < kVariantFullNames.size()) {
        opts.valueText.text = kVariantFullNames[idx];
    }
    if (opts.valueText.variants.empty()) {
        opts.valueText.variants = kVariantFullNames;
    }
    if (!opts.valueText.color.HasAny() && variant != UiVariant::Default) {
        opts.valueText.color = StateColor::FromTextVariant(variant);
    }
    if (!opts.color.HasAny() && variant != UiVariant::Default) {
        opts.color = StateColor::FromTextVariant(variant);
    }

    Response res = StepSlider(idx, opts.stepLabels.size(), opts, loc);
    if (res.changed && idx < kVariants.size()) {
        variant = kVariants[idx];
    }
    return res;
}

ToolbarScope Context::Toolbar(
    ToolbarOptions opts,
    std::source_location loc
) {
    return Open<guikit::Toolbar>(opts, loc);
}

Response Context::ToolButton(
    ToolButtonOptions opts,
    std::source_location loc
) {
    return Toolbar::ToolButton(*this, opts, loc);
}

SidebarMenuScope Context::SidebarMenu(
    SidebarMenuOptions opts,
    std::source_location loc
) {
    return Open<guikit::SidebarMenu>(opts, loc);
}

void Context::RequestFocus(Id id) {
    if (m_focusedId != id && m_editState.id.IsValid() && m_editState.id != id) {
        m_editState.id = Id::Null();
    }
    m_focusedId = id;
    ResetCursorBlink();
}

void Context::SetClipboardCallbacks(ClipboardGetFn getFn, ClipboardSetFn setFn) {
    m_clipboardGetFn = std::move(getFn);
    m_clipboardSetFn = std::move(setFn);
}

void Context::SetClipboardText(std::string_view text) {
    m_clipboardText = std::string(text);
    if (m_clipboardSetFn) {
        m_clipboardSetFn(text);
    }
}

std::string Context::GetClipboardText() const {
    if (m_clipboardGetFn) {
        std::string ext = m_clipboardGetFn();
        if (!ext.empty() || m_clipboardText.empty()) {
            return ext;
        }
    }
    return m_clipboardText;
}

bool Context::ActiveEditState::DeleteSelection() {
    if (!HasSelection() || readOnly) {
        return false;
    }
    SaveUndo();
    preferredColumn.reset();
    const size_t s = std::min(SelectionStart(), text.size());
    const size_t e = std::min(SelectionEnd(), text.size());
    if (e > s) {
        text.erase(s, e - s);
        cursor = s;
        selectionAnchor = s;
        changed = true;
        return true;
    }
    selectionAnchor = cursor;
    return false;
}

void Context::ActiveEditState::Insert(std::string_view utf8Text) {
    if (readOnly || utf8Text.empty()) {
        return;
    }

    std::string sanitized;
    sanitized.reserve(utf8Text.size());
    for (char c : utf8Text) {
        const uint8_t u = static_cast<uint8_t>(c);
        if (c == '\r') {
            continue;
        }
        if (c == '\n') {
            if (multiline) {
                sanitized.push_back('\n');
            }
            continue;
        }
        if (u < 32 && c != '\t') {
            continue;
        }
        sanitized.push_back(c);
    }

    if (sanitized.empty() && !HasSelection()) {
        return;
    }

    preferredColumn.reset();

    if (autoCloseBrackets && multiline && !readOnly && sanitized.size() == 1) {
        const char c = sanitized[0];
        const size_t safeCur = std::min(cursor, text.size());

        // Overtype closing bracket/quote if cursor is right before it
        if (!HasSelection() && safeCur < text.size() &&
            (c == ')' || c == ']' || c == '}' || c == '"' || c == '\'') &&
            text[safeCur] == c) {
            cursor = safeCur + 1;
            selectionAnchor = cursor;
            return;
        }

        char closeChar = 0;
        if (c == '(') closeChar = ')';
        else if (c == '[') closeChar = ']';
        else if (c == '{') closeChar = '}';
        else if (c == '"') closeChar = '"';
        else if (c == '\'') closeChar = '\'';

        if (closeChar != 0) {
            if (HasSelection()) {
                SaveUndo();
                const size_t s = std::min(SelectionStart(), text.size());
                const size_t e = std::min(SelectionEnd(), text.size());
                text.insert(e, 1, closeChar);
                text.insert(s, 1, c);
                selectionAnchor = s + 1;
                cursor = e + 1;
                changed = true;
                return;
            } else {
                bool allowAutoClose = true;
                if (c == '\'' && safeCur > 0) {
                    const char prev = text[safeCur - 1];
                    const bool prevIdentOrClose =
                        (prev >= 'a' && prev <= 'z') ||
                        (prev >= 'A' && prev <= 'Z') ||
                        (prev >= '0' && prev <= '9') ||
                        prev == '_' || prev == ')' || prev == ']' || prev == '}' || prev == '.';
                    if (prevIdentOrClose) {
                        allowAutoClose = false; // e.g. MATLAB matrix transpose A' or A.'
                    }
                }
                if ((c == '"' || c == '\'') && safeCur > 0 && (text[safeCur - 1] == '\\' || text[safeCur - 1] == c)) {
                    allowAutoClose = false;
                }
                const bool nextOk = (safeCur == text.size()) ||
                                    text[safeCur] == '\n' || text[safeCur] == ' ' || text[safeCur] == '\t' ||
                                    text[safeCur] == ')' || text[safeCur] == ']' || text[safeCur] == '}' ||
                                    text[safeCur] == ',' || text[safeCur] == ';';
                if (allowAutoClose && nextOk) {
                    SaveUndo();
                    const char pairStr[3] = {c, closeChar, '\0'};
                    text.insert(safeCur, pairStr, 2);
                    cursor = safeCur + 1;
                    selectionAnchor = cursor;
                    changed = true;
                    return;
                }
            }
        }
    }

    SaveUndo();
    if (HasSelection()) {
        const size_t s = std::min(SelectionStart(), text.size());
        const size_t e = std::min(SelectionEnd(), text.size());
        if (e > s) {
            text.erase(s, e - s);
        }
        cursor = s;
        selectionAnchor = s;
    }

    if (cursor > text.size()) {
        cursor = text.size();
    }
    if (!sanitized.empty()) {
        text.insert(cursor, sanitized);
        cursor += sanitized.size();
        selectionAnchor = cursor;
    }
    changed = true;
}

bool Context::ActiveEditState::HandleKey(Context& ctx, Key key, KeyModifiers mods, bool& outLostFocus) {
    const KeyAction action = ctx.ResolveAction(key, mods);
    const bool isVerticalMove =
        action == KeyAction::MoveUp || action == KeyAction::SelectUp ||
        action == KeyAction::MoveDown || action == KeyAction::SelectDown ||
        action == KeyAction::MovePageUp || action == KeyAction::SelectPageUp ||
        action == KeyAction::MovePageDown || action == KeyAction::SelectPageDown;
    if (!isVerticalMove) {
        preferredColumn.reset();
    }

    auto moveLinesVertically = [&](int lineDelta, bool extendSel) -> bool {
        if (!multiline) return false;
        const size_t safeCur = std::min(cursor, text.size());
        const size_t prevNl = (safeCur > 0) ? text.rfind('\n', safeCur - 1) : std::string::npos;
        size_t curLineStart = (prevNl == std::string::npos) ? 0 : (prevNl + 1);
        const size_t col = safeCur - curLineStart;
        if (!preferredColumn.has_value()) {
            preferredColumn = col;
        }
        const size_t targetCol = *preferredColumn;

        if (lineDelta < 0) {
            const int steps = -lineDelta;
            for (int s = 0; s < steps; ++s) {
                if (curLineStart == 0) {
                    if (s == 0) {
                        cursor = 0;
                        if (!extendSel) selectionAnchor = cursor;
                        return true;
                    }
                    break;
                }
                const size_t prevLineEnd = curLineStart - 1;
                const size_t prevPrevNl = (prevLineEnd > 0) ? text.rfind('\n', prevLineEnd - 1) : std::string::npos;
                curLineStart = (prevPrevNl == std::string::npos) ? 0 : (prevPrevNl + 1);
            }
            size_t lineEnd = text.find('\n', curLineStart);
            if (lineEnd == std::string::npos) lineEnd = text.size();
            cursor = curLineStart + std::min(targetCol, lineEnd - curLineStart);
        } else if (lineDelta > 0) {
            const int steps = lineDelta;
            size_t curLineEnd = text.find('\n', curLineStart);
            for (int s = 0; s < steps; ++s) {
                if (curLineEnd == std::string::npos) {
                    if (s == 0) {
                        cursor = text.size();
                        if (!extendSel) selectionAnchor = cursor;
                        return true;
                    }
                    break;
                }
                curLineStart = curLineEnd + 1;
                curLineEnd = text.find('\n', curLineStart);
            }
            const size_t lineEnd = (curLineEnd == std::string::npos) ? text.size() : curLineEnd;
            cursor = curLineStart + std::min(targetCol, lineEnd - curLineStart);
        }

        if (!extendSel) {
            selectionAnchor = cursor;
        }
        return true;
    };

    switch (action) {
        case KeyAction::MoveLineUp:
        case KeyAction::MoveLineDown: {
            if (!multiline || readOnly) return false;
            const size_t safeCur = std::min(cursor, text.size());
            const size_t prevNl = (safeCur > 0) ? text.rfind('\n', safeCur - 1) : std::string::npos;
            const size_t curLineStart = (prevNl == std::string::npos) ? 0 : (prevNl + 1);
            size_t curLineEnd = text.find('\n', safeCur);
            if (curLineEnd == std::string::npos) curLineEnd = text.size();
            const size_t col = safeCur - curLineStart;

            if (action == KeyAction::MoveLineUp) {
                if (curLineStart > 0) {
                    SaveUndo();
                    const size_t prevLineEnd = curLineStart - 1;
                    const size_t prevPrevNl = (prevLineEnd > 0) ? text.rfind('\n', prevLineEnd - 1) : std::string::npos;
                    const size_t prevLineStart = (prevPrevNl == std::string::npos) ? 0 : (prevPrevNl + 1);
                    std::string curLine = text.substr(curLineStart, curLineEnd - curLineStart);
                    std::string prevLine = text.substr(prevLineStart, prevLineEnd - prevLineStart);
                    text.replace(prevLineStart, curLineEnd - prevLineStart, curLine + "\n" + prevLine);
                    cursor = prevLineStart + std::min(col, curLine.size());
                    selectionAnchor = cursor;
                    changed = true;
                }
            } else {
                if (curLineEnd < text.size()) {
                    SaveUndo();
                    const size_t nextLineStart = curLineEnd + 1;
                    size_t nextLineEnd = text.find('\n', nextLineStart);
                    if (nextLineEnd == std::string::npos) nextLineEnd = text.size();
                    std::string curLine = text.substr(curLineStart, curLineEnd - curLineStart);
                    std::string nextLine = text.substr(nextLineStart, nextLineEnd - nextLineStart);
                    text.replace(curLineStart, nextLineEnd - curLineStart, nextLine + "\n" + curLine);
                    cursor = curLineStart + nextLine.size() + 1 + std::min(col, curLine.size());
                    selectionAnchor = cursor;
                    changed = true;
                }
            }
            return true;
        }

        case KeyAction::SelectAll:
            SelectAll();
            return true;

        case KeyAction::Copy:
            if (HasSelection()) {
                ctx.SetClipboardText(SelectedText());
            }
            return true;

        case KeyAction::Cut:
            if (HasSelection()) {
                ctx.SetClipboardText(SelectedText());
                if (!readOnly) {
                    DeleteSelection();
                }
            }
            return true;

        case KeyAction::Paste:
            if (!readOnly) {
                const std::string clip = ctx.GetClipboardText();
                if (!clip.empty()) {
                    Insert(clip);
                }
            }
            return true;

        case KeyAction::Undo:
            if (!readOnly) {
                Undo();
            }
            return true;

        case KeyAction::Redo:
            if (!readOnly) {
                Redo();
            }
            return true;

        case KeyAction::DuplicateLine: {
            if (!multiline || readOnly) return false;
            if (HasSelection()) {
                const std::string sel(SelectedText());
                if (!sel.empty()) {
                    size_t nextPos = text.find(sel, SelectionEnd());
                    if (nextPos == std::string::npos) {
                        nextPos = text.find(sel, 0);
                    }
                    if (nextPos != std::string::npos && nextPos != SelectionStart()) {
                        selectionAnchor = nextPos;
                        cursor = nextPos + sel.size();
                        return true;
                    }
                }
            }
            const size_t safeCur = std::min(cursor, text.size());
            const size_t prevNl = (safeCur > 0) ? text.rfind('\n', safeCur - 1) : std::string::npos;
            const size_t curLineStart = (prevNl == std::string::npos) ? 0 : (prevNl + 1);
            size_t curLineEnd = text.find('\n', safeCur);
            if (curLineEnd == std::string::npos) curLineEnd = text.size();
            const size_t col = safeCur - curLineStart;
            const std::string lineCopy = text.substr(curLineStart, curLineEnd - curLineStart);
            SaveUndo();
            text.insert(curLineEnd, "\n" + lineCopy);
            cursor = curLineEnd + 1 + col;
            selectionAnchor = cursor;
            changed = true;
            return true;
        }

        case KeyAction::DeleteLine: {
            if (!multiline || readOnly) return false;
            SaveUndo();
            const size_t safeCur = std::min(cursor, text.size());
            const size_t prevNl = (safeCur > 0) ? text.rfind('\n', safeCur - 1) : std::string::npos;
            const size_t curLineStart = (prevNl == std::string::npos) ? 0 : (prevNl + 1);
            size_t curLineEnd = text.find('\n', safeCur);
            if (curLineEnd != std::string::npos) {
                text.erase(curLineStart, curLineEnd - curLineStart + 1);
                cursor = std::min(curLineStart, text.size());
            } else if (curLineStart > 0) {
                text.erase(curLineStart - 1, text.size() - (curLineStart - 1));
                cursor = text.size();
            } else {
                text.clear();
                cursor = 0;
            }
            selectionAnchor = cursor;
            changed = true;
            return true;
        }

        case KeyAction::DeletePrevChar:
        case KeyAction::DeletePrevWord: {
            if (readOnly) return true;
            const bool byWord = (action == KeyAction::DeletePrevWord);
            if (HasSelection()) {
                DeleteSelection();
            } else if (cursor > 0 && !text.empty()) {
                SaveUndo();
                const size_t safeCur = std::min(cursor, text.size());
                if (autoCloseBrackets && multiline && !byWord && safeCur < text.size()) {
                    const char prevC = text[safeCur - 1];
                    const char nextC = text[safeCur];
                    if ((prevC == '(' && nextC == ')') ||
                        (prevC == '[' && nextC == ']') ||
                        (prevC == '{' && nextC == '}') ||
                        (prevC == '"' && nextC == '"') ||
                        (prevC == '\'' && nextC == '\'')) {
                        text.erase(safeCur - 1, 2);
                        cursor = safeCur - 1;
                        selectionAnchor = cursor;
                        changed = true;
                        return true;
                    }
                }
                const size_t prev = byWord
                    ? TextUtil::PrevWordBoundary(text, safeCur)
                    : TextUtil::PrevCharBoundary(text, safeCur);
                if (safeCur > prev) {
                    text.erase(prev, safeCur - prev);
                    cursor = prev;
                    selectionAnchor = cursor;
                    changed = true;
                }
            }
            return true;
        }

        case KeyAction::DeleteNextChar:
        case KeyAction::DeleteNextWord: {
            if (readOnly) return true;
            const bool byWord = (action == KeyAction::DeleteNextWord);
            if (HasSelection()) {
                DeleteSelection();
            } else if (cursor < text.size()) {
                SaveUndo();
                const size_t next = byWord
                    ? TextUtil::NextWordBoundary(text, cursor)
                    : TextUtil::NextCharBoundary(text, cursor);
                if (next > cursor) {
                    text.erase(cursor, next - cursor);
                    selectionAnchor = cursor;
                    changed = true;
                }
            }
            return true;
        }

        case KeyAction::MoveLeft:
        case KeyAction::SelectLeft:
        case KeyAction::MoveWordLeft:
        case KeyAction::SelectWordLeft: {
            const bool extendSel = (action == KeyAction::SelectLeft || action == KeyAction::SelectWordLeft);
            const bool byWord = (action == KeyAction::MoveWordLeft || action == KeyAction::SelectWordLeft);
            if (!extendSel && HasSelection()) {
                if (byWord) {
                    cursor = TextUtil::PrevWordBoundary(text, SelectionStart());
                } else {
                    cursor = SelectionStart();
                }
                selectionAnchor = cursor;
            } else {
                cursor = byWord
                    ? TextUtil::PrevWordBoundary(text, cursor)
                    : TextUtil::PrevCharBoundary(text, cursor);
                if (!extendSel) {
                    selectionAnchor = cursor;
                }
            }
            return true;
        }

        case KeyAction::MoveRight:
        case KeyAction::SelectRight:
        case KeyAction::MoveWordRight:
        case KeyAction::SelectWordRight: {
            const bool extendSel = (action == KeyAction::SelectRight || action == KeyAction::SelectWordRight);
            const bool byWord = (action == KeyAction::MoveWordRight || action == KeyAction::SelectWordRight);
            if (!extendSel && HasSelection()) {
                if (byWord) {
                    cursor = TextUtil::NextWordBoundary(text, SelectionEnd());
                } else {
                    cursor = SelectionEnd();
                }
                selectionAnchor = cursor;
            } else {
                cursor = byWord
                    ? TextUtil::NextWordBoundary(text, cursor)
                    : TextUtil::NextCharBoundary(text, cursor);
                if (!extendSel) {
                    selectionAnchor = cursor;
                }
            }
            return true;
        }

        case KeyAction::MoveUp:
            return moveLinesVertically(-1, false);
        case KeyAction::SelectUp:
            return moveLinesVertically(-1, true);
        case KeyAction::MoveDown:
            return moveLinesVertically(1, false);
        case KeyAction::SelectDown:
            return moveLinesVertically(1, true);
        case KeyAction::MovePageUp:
            return moveLinesVertically(-20, false);
        case KeyAction::SelectPageUp:
            return moveLinesVertically(-20, true);
        case KeyAction::MovePageDown:
            return moveLinesVertically(20, false);
        case KeyAction::SelectPageDown:
            return moveLinesVertically(20, true);

        case KeyAction::MoveLineStart:
        case KeyAction::SelectLineStart:
        case KeyAction::MoveDocStart:
        case KeyAction::SelectDocStart: {
            const bool extendSel = (action == KeyAction::SelectLineStart || action == KeyAction::SelectDocStart);
            const bool docStart = (action == KeyAction::MoveDocStart || action == KeyAction::SelectDocStart);
            if (multiline && !docStart) {
                const size_t safeCur = std::min(cursor, text.size());
                const size_t prevNl = (safeCur > 0) ? text.rfind('\n', safeCur - 1) : std::string::npos;
                const size_t lineStart = (prevNl == std::string::npos) ? 0 : prevNl + 1;
                size_t firstNonWs = lineStart;
                while (firstNonWs < text.size() && (text[firstNonWs] == ' ' || text[firstNonWs] == '\t')) {
                    ++firstNonWs;
                }
                if (safeCur == lineStart && firstNonWs > lineStart && autoCloseBrackets) {
                    cursor = firstNonWs;
                } else if (safeCur > firstNonWs && firstNonWs > lineStart && autoCloseBrackets) {
                    cursor = firstNonWs;
                } else {
                    cursor = lineStart;
                }
            } else {
                cursor = 0;
            }
            if (!extendSel) {
                selectionAnchor = cursor;
            }
            return true;
        }

        case KeyAction::MoveLineEnd:
        case KeyAction::SelectLineEnd:
        case KeyAction::MoveDocEnd:
        case KeyAction::SelectDocEnd: {
            const bool extendSel = (action == KeyAction::SelectLineEnd || action == KeyAction::SelectDocEnd);
            const bool docEnd = (action == KeyAction::MoveDocEnd || action == KeyAction::SelectDocEnd);
            if (multiline && !docEnd) {
                const size_t nextNl = text.find('\n', cursor);
                cursor = (nextNl == std::string::npos) ? text.size() : nextNl;
            } else {
                cursor = text.size();
            }
            if (!extendSel) {
                selectionAnchor = cursor;
            }
            return true;
        }

        case KeyAction::Indent:
        case KeyAction::Unindent: {
            if (multiline && !readOnly) {
                const bool unindent = (action == KeyAction::Unindent);
                const bool multiLineSel = HasSelection() && (SelectedText().find('\n') != std::string_view::npos);
                if (unindent || multiLineSel) {
                    const size_t selS = std::min(SelectionStart(), text.size());
                    size_t selE = std::min(SelectionEnd(), text.size());
                    if (selE > selS && text[selE - 1] == '\n') {
                        --selE;
                    }
                    const size_t prevNl = (selS > 0) ? text.rfind('\n', selS - 1) : std::string::npos;
                    const size_t blockStart = (prevNl == std::string::npos) ? 0 : (prevNl + 1);
                    size_t blockEnd = text.find('\n', selE);
                    if (blockEnd == std::string::npos) blockEnd = text.size();

                    SaveUndo();
                    size_t pos = blockStart;
                    bool anyModified = false;
                    while (pos <= blockEnd && pos <= text.size()) {
                        size_t lineEnd = text.find('\n', pos);
                        if (lineEnd == std::string::npos) lineEnd = text.size();

                        if (!unindent) {
                            text.insert(pos, "    ");
                            if (cursor >= pos) cursor += 4;
                            if (selectionAnchor >= pos) selectionAnchor += 4;
                            blockEnd += 4;
                            lineEnd += 4;
                            anyModified = true;
                        } else {
                            size_t rem = 0;
                            if (pos < lineEnd && text[pos] == '\t') {
                                rem = 1;
                            } else {
                                while (rem < 4 && (pos + rem) < lineEnd && text[pos + rem] == ' ') {
                                    ++rem;
                                }
                            }
                            if (rem > 0) {
                                text.erase(pos, rem);
                                if (cursor > pos) {
                                    cursor = (cursor >= pos + rem) ? (cursor - rem) : pos;
                                }
                                if (selectionAnchor > pos) {
                                    selectionAnchor = (selectionAnchor >= pos + rem) ? (selectionAnchor - rem) : pos;
                                }
                                blockEnd -= rem;
                                lineEnd -= rem;
                                anyModified = true;
                            }
                        }

                        if (lineEnd >= blockEnd || lineEnd >= text.size()) {
                            break;
                        }
                        pos = lineEnd + 1;
                    }
                    if (anyModified) {
                        changed = true;
                    }
                    return true;
                }
                Insert("    ");
                return true;
            }
            return false;
        }

        case KeyAction::Confirm:
            if (multiline) {
                if (!readOnly) {
                    const size_t safeCur = std::min(cursor, text.size());
                    const size_t prevNl = (safeCur > 0) ? text.rfind('\n', safeCur - 1) : std::string::npos;
                    const size_t curLineStart = (prevNl == std::string::npos) ? 0 : (prevNl + 1);
                    std::string indent;
                    for (size_t i = curLineStart; i < safeCur && (text[i] == ' ' || text[i] == '\t'); ++i) {
                        indent.push_back(text[i]);
                    }
                    if (autoCloseBrackets && !HasSelection() && safeCur > 0 && safeCur < text.size()) {
                        const char prevC = text[safeCur - 1];
                        const char nextC = text[safeCur];
                        if ((prevC == '{' && nextC == '}') ||
                            (prevC == '[' && nextC == ']') ||
                            (prevC == '(' && nextC == ')')) {
                            SaveUndo();
                            const std::string mid = "\n" + indent + "    ";
                            const std::string tail = "\n" + indent;
                            text.insert(safeCur, mid + tail);
                            cursor = safeCur + mid.size();
                            selectionAnchor = cursor;
                            changed = true;
                            return true;
                        }
                    }
                    if (autoCloseBrackets && !HasSelection() && safeCur > curLineStart) {
                        size_t checkPos = safeCur;
                        while (checkPos > curLineStart && (text[checkPos - 1] == ' ' || text[checkPos - 1] == '\t')) {
                            --checkPos;
                        }
                        if (checkPos > curLineStart && (text[checkPos - 1] == '{' || text[checkPos - 1] == '[' || text[checkPos - 1] == '(')) {
                            indent += "    ";
                        }
                    }
                    Insert("\n" + indent);
                }
                return true;
            }
            submitted = true;
            outLostFocus = true;
            return true;

        case KeyAction::Cancel:
            if (HasSelection()) {
                selectionAnchor = cursor;
                return true;
            }
            outLostFocus = true;
            id = Id::Null();
            return true;

        default:
            return false;
    }
}

void Context::RequestRepaint(int frames) noexcept {
    m_repaintFrames = std::max(m_repaintFrames, frames);
}

bool Context::NeedsRepaint() const noexcept {
    if (m_repaintFrames > 0) return true;
    if (m_mouseDown || m_rightMouseDown || m_middleMouseDown) return true;
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
    const Vec2 newPos = pos / scale;
    if (std::abs(newPos.x - m_mousePos.x) < 0.25f && std::abs(newPos.y - m_mousePos.y) < 0.25f) {
        return;
    }
    m_mousePos = newPos;
    RequestRepaint(1);
    if (m_inspectModeEnabled || m_anyScopedInspectActive) {
        InvalidateAll();
    }
}

void Context::OnMouseDown(int button) {
    if (button == 1) { // Left mouse button
        m_mouseDown = true;
        m_mouseClicked = true;
        const double now = m_currentTimeSec;
        const float dx = m_mousePos.x - m_lastClickPos.x;
        const float dy = m_mousePos.y - m_lastClickPos.y;
        if (m_lastClickTimeSec >= 0.0 && (now - m_lastClickTimeSec) <= 0.36 && (dx * dx + dy * dy) <= 64.0f) {
            m_mouseDoubleClicked = true;
            m_lastClickTimeSec = -1.0;
        } else {
            m_lastClickTimeSec = now;
            m_lastClickPos = m_mousePos;
        }
        ResetCursorBlink();
        RequestRepaint(3);
    } else if (button == 2) { // Middle mouse button
        m_middleMouseDown = true;
        m_middleMouseClicked = true;
        RequestRepaint(3);
    } else if (button == 3) { // Right mouse button
        m_rightMouseDown = true;
        m_rightMouseClicked = true;
        RequestRepaint(3);
    }
}

void Context::OnMouseUp(int button) {
    if (button == 1) { // Left mouse button
        m_mouseDown = false;
        m_mouseReleased = true;
        RequestRepaint(3);
    } else if (button == 2) { // Middle mouse button
        m_middleMouseDown = false;
        m_middleMouseReleased = true;
        RequestRepaint(3);
    } else if (button == 3) { // Right mouse button
        m_rightMouseDown = false;
        m_rightMouseReleased = true;
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
    if (!m_suspendDirectEditInput && !m_mouseClicked && m_focusedId.IsValid() && m_editState.id == m_focusedId) {
        m_editState.Insert(utf8Text);
    } else {
        m_pendingTextInput.emplace_back(utf8Text);
    }
}

void Context::OnKeyDown(Key key, KeyModifiers mods) {
    m_keyMods = mods;
    ResetCursorBlink();
    RequestRepaint(3);

    if (m_keymap.Matches(KeyAction::Cancel, key, mods) && m_openPopupId.IsValid()) {
        ClosePopup();
    }

    if (!m_suspendDirectEditInput && !m_mouseClicked && m_focusedId.IsValid() && m_editState.id == m_focusedId) {
        bool lostFocus = false;
        if (m_editState.HandleKey(*this, key, mods, lostFocus)) {
            if (lostFocus) {
                m_focusedId = Id::Null();
            }
            return;
        }
    }
    m_pendingKeyEvents.push_back({key, mods});
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

void Context::SetTooltip(UiText tooltip, Rect anchorRect) {
    if (tooltip.empty() || m_openPopupId.IsValid()) {
        return;
    }
    m_activeTooltip = AllocText(tooltip);
    m_activeTooltipAnchor = anchorRect;
}

void Context::RegisterFocusable(Id id, bool disabled) {
    if (!disabled && id.IsValid()) {
        m_currFocusableIds.push_back(id);
    }
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

bool Context::ConsumeKey(Key key, KeyModifiers& outMods) {
    for (auto it = m_pendingKeyEvents.begin(); it != m_pendingKeyEvents.end(); ++it) {
        if (it->key == key) {
            outMods = it->mods;
            m_pendingKeyEvents.erase(it);
            return true;
        }
    }
    return false;
}

bool Context::ConsumeShortcut(Key key, bool ctrl, bool shift, bool alt) {
    for (auto it = m_pendingKeyEvents.begin(); it != m_pendingKeyEvents.end(); ++it) {
        if (it->key == key && it->mods.ctrl == ctrl && it->mods.shift == shift && it->mods.alt == alt) {
            m_pendingKeyEvents.erase(it);
            return true;
        }
    }
    return false;
}

bool Context::ConsumeAction(KeyAction action) {
    if (action == KeyAction::None) {
        return false;
    }
    for (auto it = m_pendingKeyEvents.begin(); it != m_pendingKeyEvents.end(); ++it) {
        if (m_keymap.Matches(action, it->key, it->mods)) {
            m_pendingKeyEvents.erase(it);
            return true;
        }
    }
    return false;
}

std::vector<std::string> Context::ConsumePendingTextInput() {
    std::vector<std::string> res = std::move(m_pendingTextInput);
    m_pendingTextInput.clear();
    return res;
}

std::string_view Context::EnsureTextEdit(
    Id id,
    std::string_view initialText,
    bool multiline,
    bool readOnly
) {
    if (m_editState.id != id) {
        m_editState.id = id;
        m_editState.text = initialText;
        m_editState.cursor = m_editState.text.size();
        m_editState.selectionAnchor = m_editState.cursor;
        m_editState.undoStack.clear();
        m_editState.redoStack.clear();
        m_editState.preferredColumn.reset();
        m_editState.changed = false;
        m_editState.submitted = false;
        m_editState.multiline = multiline;
        m_editState.readOnly = readOnly;
    } else {
        m_editState.multiline = multiline;
        m_editState.readOnly = readOnly;
        if ((multiline || readOnly) && !m_editState.changed && m_editState.text != initialText) {
            m_editState.text = initialText;
            m_editState.cursor = std::min(m_editState.cursor, m_editState.text.size());
            m_editState.selectionAnchor = std::min(m_editState.selectionAnchor, m_editState.text.size());
            m_editState.undoStack.clear();
            m_editState.redoStack.clear();
            m_editState.preferredColumn.reset();
        }
    }
    return m_editState.text;
}

std::string_view Context::GetOrStartTextEdit(
    Id id,
    std::string_view initialText,
    size_t& outCursor,
    bool& outChanged,
    bool& outSubmitted,
    bool multiline
) {
    size_t anchor = 0;
    return GetOrStartTextEdit(id, initialText, outCursor, anchor, outChanged, outSubmitted, multiline, false);
}

std::string_view Context::GetOrStartTextEdit(
    Id id,
    std::string_view initialText,
    size_t& outCursor,
    size_t& outSelectionAnchor,
    bool& outChanged,
    bool& outSubmitted,
    bool multiline,
    bool readOnly
) {
    EnsureTextEdit(id, initialText, multiline, readOnly);

    for (const auto& txt : m_pendingTextInput) {
        m_editState.Insert(txt);
    }
    m_pendingTextInput.clear();

    for (auto it = m_pendingKeyEvents.begin(); it != m_pendingKeyEvents.end(); ) {
        bool lostFocus = false;
        if (m_editState.HandleKey(*this, it->key, it->mods, lostFocus)) {
            if (lostFocus) {
                m_focusedId = Id::Null();
            }
            it = m_pendingKeyEvents.erase(it);
        } else {
            ++it;
        }
    }

    m_editState.cursor = std::min(m_editState.cursor, m_editState.text.size());
    m_editState.selectionAnchor = std::min(m_editState.selectionAnchor, m_editState.text.size());

    outCursor = m_editState.cursor;
    outSelectionAnchor = m_editState.selectionAnchor;
    outChanged = m_editState.changed;
    outSubmitted = m_editState.submitted;

    m_editState.changed = false;
    m_editState.submitted = false;
    if (outSubmitted) {
        m_editState.id = Id::Null();
    }

    return m_editState.text;
}

void Context::SetTextEditBuffer(Id id, std::string_view text) {
    if (!id.IsValid()) return;
    m_editState.id = id;
    m_editState.text.assign(text);
    m_editState.cursor = m_editState.text.size();
    m_editState.selectionAnchor = m_editState.cursor;
    m_editState.preferredColumn.reset();
    m_editState.changed = false;
    m_editState.submitted = false;
    ResetCursorBlink();
}

void Context::SetTextEditCursor(Id id, size_t cursor, size_t selectionAnchor) noexcept {
    if (m_editState.id == id && id.IsValid()) {
        m_editState.cursor = std::min(cursor, m_editState.text.size());
        m_editState.selectionAnchor = std::min(selectionAnchor, m_editState.text.size());
        m_editState.preferredColumn.reset();
        ResetCursorBlink();
    }
}

void Context::SetTextEditAutoClose(Id id, bool enabled) noexcept {
    if (m_editState.id == id && id.IsValid()) {
        m_editState.autoCloseBrackets = enabled;
    }
}

void Context::ApplyTextEditChange(Id id, std::string_view newText, size_t newCursor, size_t newSelectionAnchor) {
    if (!id.IsValid() || m_editState.id != id || m_editState.readOnly) return;
    m_editState.SaveUndo();
    m_editState.text.assign(newText);
    m_editState.cursor = std::min(newCursor, m_editState.text.size());
    m_editState.selectionAnchor = std::min(newSelectionAnchor, m_editState.text.size());
    m_editState.preferredColumn.reset();
    m_editState.changed = true;
    ResetCursorBlink();
}

bool Context::UndoTextEdit(Id id) {
    if (m_editState.id == id && id.IsValid() && !m_editState.readOnly) {
        if (m_editState.Undo()) {
            ResetCursorBlink();
            return true;
        }
    }
    return false;
}

bool Context::RedoTextEdit(Id id) {
    if (m_editState.id == id && id.IsValid() && !m_editState.readOnly) {
        if (m_editState.Redo()) {
            ResetCursorBlink();
            return true;
        }
    }
    return false;
}

bool Context::GetTextEditSelection(Id id, size_t& outStart, size_t& outEnd) const noexcept {
    if (m_editState.id == id && id.IsValid()) {
        outStart = m_editState.SelectionStart();
        outEnd = m_editState.SelectionEnd();
        return m_editState.HasSelection();
    }
    outStart = 0;
    outEnd = 0;
    return false;
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

static Rect GetNodeContentClipRect(const Node* node, const UiTheme& theme) {
    if (!node) return Rect();
    if (node->type == NodeType::ScrollArea || (node->type == NodeType::Sidebar && node->sidebarScrollable)) {
        return ScrollArea::GetContentClipRect(node, theme);
    }
    if (node->type == NodeType::Scene) {
        return Scene::GetViewportRect(node, nullptr, theme);
    }
    if (node->type == NodeType::Carousel) {
        return Carousel::GetContentClipRect(node);
    }
    if (node->type == NodeType::Table) {
        return Table::GetBodyClipRect(node, theme);
    }
    return node->computedRect;
}

static void CollectBoundsRecursive(
    Node* node,
    std::unordered_map<Id, Rect>& outBounds,
    std::unordered_map<Id, Rect>& outUnclippedBounds,
    Rect parentClip,
    const UiTheme& theme
) {
    if (!node) return;

    const UiTheme& activeTheme = (node->theme != nullptr) ? *node->theme : theme;

    if (node->id.IsValid()) {
        if ((node->computedRect.w > 0.0f || node->type == NodeType::Sidebar) && node->computedRect.h > 0.0f) {
            outUnclippedBounds[node->id] = node->computedRect;
        }
        const Rect clipped = node->computedRect.Intersect(parentClip);
        if ((clipped.w > 0.0f || node->type == NodeType::Sidebar) && clipped.h > 0.0f) {
            outBounds[node->id] = clipped;
        }

        if (node->type == NodeType::Carousel) {
            Carousel::CollectBounds(node, outBounds, parentClip);
        }
    }

    Rect effectiveClip = parentClip;
    if (node->clipChildren) {
        effectiveClip = parentClip.Intersect(GetNodeContentClipRect(node, activeTheme));
    }

    for (Node* child = node->firstChild; child != nullptr; child = child->nextSibling) {
        CollectBoundsRecursive(child, outBounds, outUnclippedBounds, effectiveClip, activeTheme);
    }
}

bool Context::UpdateScrollAreasRecursive(Node* node) {
    if (!node) return false;

    bool scrollChanged = false;
    const Vec2 prevScroll = node->scrollOffset;

    if (node->type == NodeType::ScrollArea || (node->type == NodeType::Sidebar && node->sidebarScrollable)) {
        ScrollArea::Update(*this, node);
        if (std::abs(node->scrollOffset.x - prevScroll.x) > 0.01f ||
            std::abs(node->scrollOffset.y - prevScroll.y) > 0.01f) {
            scrollChanged = true;
        }
    } else if (node->type == NodeType::Scene) {
        Scene::Update(*this, node);
    } else if (node->type == NodeType::Carousel) {
        Carousel::Update(*this, node);
        if (std::abs(node->scrollOffset.x - prevScroll.x) > 0.01f ||
            std::abs(node->scrollOffset.y - prevScroll.y) > 0.01f) {
            scrollChanged = true;
        }
    } else if (node->type == NodeType::Table) {
        Table::Update(*this, node);
    }

    for (Node* child = node->firstChild; child != nullptr; child = child->nextSibling) {
        if (UpdateScrollAreasRecursive(child)) {
            scrollChanged = true;
        }
    }
    return scrollChanged;
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
        if ((p->type == NodeType::ScrollArea || (p->type == NodeType::Sidebar && p->sidebarScrollable)) && p->id.IsValid()) {
            ScrollArea::ScrollToChild(*this, p, target);
            break;
        } else if (p->type == NodeType::Carousel && p->id.IsValid()) {
            Carousel::ScrollToChild(*this, p, target);
            break;
        }
    }
}

void Context::EndFrame(ICanvas& canvas) {
    m_lastCanvas = &canvas;

    // Balance any unclosed container scopes
    while (m_containerStack.size() > 1) {
        m_containerStack.pop_back();
    }

    const Vec2 logicalSize = m_viewportSize / ((m_dpiScale > 0.0f) ? m_dpiScale : 1.0f);

    // Resolve geometry using canvas font metrics
    Solver::Measure(m_root, &canvas, m_theme);
    Solver::Arrange(m_root, Rect(0.0f, 0.0f, logicalSize.x, logicalSize.y), m_theme, &canvas);

    // Update interactive scrollbars; re-arrange only if a scrollbar drag or track click modified scrollOffset
    if (UpdateScrollAreasRecursive(m_root)) {
        Solver::Arrange(m_root, Rect(0.0f, 0.0f, logicalSize.x, logicalSize.y), m_theme, &canvas);
    }

    // Cache computed bounds and focusable widget list for next frame
    CollectBoundsRecursive(m_root, m_currFrameBounds, m_currUnclippedBounds, Rect(0.0f, 0.0f, logicalSize.x, logicalSize.y), m_theme);
    m_prevFrameBounds = std::move(m_currFrameBounds);
    m_prevUnclippedBounds = std::move(m_currUnclippedBounds);
    m_prevFocusableIds = std::move(m_currFocusableIds);

    // Reset single-frame mouse impulse states
    m_mouseClicked = false;
    m_mouseReleased = false;
    m_mouseDoubleClicked = false;
    m_rightMouseClicked = false;
    m_rightMouseReleased = false;
    m_middleMouseClicked = false;
    m_middleMouseReleased = false;
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
    bool anyDebugBounds = m_debugBoundsEnabled;
    bool anyInspectMode = m_inspectModeEnabled;

    std::function<void(Node*, Rect, const UiTheme&)> collectSnapshots =
        [&](Node* n, Rect parentClip, const UiTheme& parentTheme) {
        if (!n) return;
        const UiTheme& activeTheme = (n->theme != nullptr) ? *n->theme : parentTheme;
        if (n->debugBoundsOverride.value_or(false)) {
            anyDebugBounds = true;
        }
        if (n->inspectModeOverride.value_or(false)) {
            anyInspectMode = true;
        }
        if (n->id.IsValid()) {
            WidgetSnapshot snap;
            snap.rect = n->computedRect.Intersect(parentClip);
            snap.flags = (n->hovered ? 1u : 0u) |
                         (n->pressed ? 2u : 0u) |
                         (n->disabled ? 4u : 0u) |
                         (n->checked ? 8u : 0u) |
                         (n->active ? 64u : 0u) |
                         (n->focused ? 128u : 0u) |
                         (n->scrollbarHovered ? 256u : 0u) |
                         (n->scrollbarActive ? 512u : 0u) |
                         (n->scrollbarHoveredX ? 1024u : 0u) |
                         (n->scrollbarActiveX ? 2048u : 0u);
            snap.bgColor = n->backgroundColor.ToUint32();
            snap.borderColor = n->borderColor.ToUint32();

            uint64_t h = n->visualHash != 0 ? n->visualHash : detail::HashStringView(n->text);
            h = detail::HashArgs(
                h,
                n->computedRect.x,
                n->computedRect.y,
                n->computedRect.w,
                n->computedRect.h,
                n->scrollOffset.x,
                n->scrollOffset.y
            );
            if (m_editState.id.IsValid() && n->id == m_editState.id) {
                snap.flags |= (cursorVis ? 16u : 0u) | 32u;
                snap.cursor = m_editState.cursor;
                snap.selectionAnchor = m_editState.selectionAnchor;
                h = detail::HashStringView(m_editState.text, h);
            }
            snap.visualHash = h;

            auto it = m_prevSnapshots.find(n->id);
            if (it != m_prevSnapshots.end()) {
                if (it->second != snap) {
                    const bool curValid = (snap.rect.w > 0.0f && snap.rect.h > 0.0f);
                    const bool prevValid = (it->second.rect.w > 0.0f && it->second.rect.h > 0.0f);
                    if (curValid && prevValid) {
                        addDamage(snap.rect.Union(it->second.rect));
                    } else if (curValid) {
                        addDamage(snap.rect);
                    } else if (prevValid) {
                        addDamage(it->second.rect);
                    }
                }
            } else {
                addDamage(snap.rect);
            }

            currSnapshots.emplace(n->id, snap);
        }

        Rect effectiveClip = parentClip;
        if (n->clipChildren) {
            effectiveClip = parentClip.Intersect(GetNodeContentClipRect(n, activeTheme));
        }

        for (Node* c = n->firstChild; c != nullptr; c = c->nextSibling) {
            collectSnapshots(c, effectiveClip, activeTheme);
        }
    };

    collectSnapshots(m_root, Rect(0.0f, 0.0f, logicalSize.x, logicalSize.y), m_theme);
    m_anyScopedInspectActive = anyInspectMode;

    // Detect removed widgets
    for (const auto& [id, prevSnap] : m_prevSnapshots) {
        if (!currSnapshots.contains(id)) {
            addDamage(prevSnap.rect);
        }
    }

    const bool hasTooltip = !m_activeTooltip.empty();

    // Determine whether full repaint or partial dirty rect is required
    m_dirtyRects.clear();
    const float totalViewportArea = logicalSize.x * logicalSize.y;
    if (!m_dirtyRectEnabled || m_forceFullRepaint || m_prevSnapshots.empty() || m_openPopupId.IsValid() ||
        hasTooltip || m_hadTooltipLastFrame || anyDebugBounds || anyInspectMode) {
        m_isFullRepaint = true;
        m_hasDirtyDamage = true;
        m_dirtyRect = Rect(0.0f, 0.0f, logicalSize.x, logicalSize.y);
        m_dirtyRects.push_back(m_dirtyRect);
    } else if (damageRects.empty()) {
        m_isFullRepaint = false;
        m_hasDirtyDamage = false;
        m_dirtyRect = Rect{0.0f, 0.0f, 0.0f, 0.0f};
    } else {
        // Expand each damage rect by 2.0px margin to cover anti-aliasing edges and coalesce overlapping regions
        const float pad = 2.0f;
        for (const Rect& raw : damageRects) {
            const float x1 = std::max(0.0f, std::floor(raw.x - pad));
            const float y1 = std::max(0.0f, std::floor(raw.y - pad));
            const float x2 = std::min(logicalSize.x, std::ceil(raw.Right() + pad));
            const float y2 = std::min(logicalSize.y, std::ceil(raw.Bottom() + pad));
            if (x2 > x1 && y2 > y1) {
                m_dirtyRects.emplace_back(x1, y1, x2 - x1, y2 - y1);
            }
        }

        bool mergedAny = true;
        while (mergedAny && m_dirtyRects.size() > 1) {
            mergedAny = false;
            for (size_t i = 0; i < m_dirtyRects.size() && !mergedAny; ++i) {
                for (size_t j = i + 1; j < m_dirtyRects.size(); ++j) {
                    if (m_dirtyRects[i].Intersects(m_dirtyRects[j])) {
                        m_dirtyRects[i] = m_dirtyRects[i].Union(m_dirtyRects[j]);
                        m_dirtyRects.erase(m_dirtyRects.begin() + static_cast<ptrdiff_t>(j));
                        mergedAny = true;
                        break;
                    }
                }
            }
        }

        if (m_dirtyRects.empty()) {
            m_isFullRepaint = false;
            m_hasDirtyDamage = false;
            m_dirtyRect = Rect{0.0f, 0.0f, 0.0f, 0.0f};
        } else {
            Rect unionRect = m_dirtyRects[0];
            float totalDamageArea = m_dirtyRects[0].Area();
            for (size_t i = 1; i < m_dirtyRects.size(); ++i) {
                unionRect = unionRect.Union(m_dirtyRects[i]);
                totalDamageArea += m_dirtyRects[i].Area();
            }

            if (totalDamageArea >= 0.96f * totalViewportArea || m_dirtyRects.size() > 8) {
                m_isFullRepaint = true;
                m_dirtyRect = Rect(0.0f, 0.0f, logicalSize.x, logicalSize.y);
                m_dirtyRects.assign(1, m_dirtyRect);
            } else {
                m_isFullRepaint = false;
                m_dirtyRect = unionRect;
            }
            m_hasDirtyDamage = true;
        }
    }

    m_forceFullRepaint = false;
    m_hadTooltipLastFrame = hasTooltip;
    m_prevSnapshots = std::move(currSnapshots);

    // If damage exists, clear target buffer and rasterize scene graph
    if (m_hasDirtyDamage) {
        canvas.Begin(static_cast<uint32_t>(m_viewportSize.x), static_cast<uint32_t>(m_viewportSize.y), m_dpiScale);

        if (m_isFullRepaint) {
            canvas.PushClip(m_dirtyRect);
            canvas.Clear(m_clearColor);
            RenderNodeRecursive(m_root, canvas, m_dirtyRect, m_theme);

            for (const auto& overlayFn : m_overlayPasses) {
                overlayFn(canvas);
            }
            if (hasTooltip) {
                RenderTooltipOverlay(canvas, logicalSize);
            }
            if (anyDebugBounds) {
                RenderDebugBoundsRecursive(m_root, canvas, m_debugBoundsEnabled);
            }
            if (anyInspectMode) {
                RenderInspectOverlay(m_root, canvas, logicalSize);
            }

            canvas.PopClip();
            canvas.End();
        } else {
            for (const Rect& dr : m_dirtyRects) {
                canvas.PushClip(dr);
                canvas.Clear(m_clearColor, dr);
                RenderNodeRecursive(m_root, canvas, dr, m_theme);
                for (const auto& overlayFn : m_overlayPasses) {
                    overlayFn(canvas);
                }
                canvas.PopClip();
                canvas.End();
            }
        }
    }

    m_pendingKeyEvents.clear();
    m_pendingTextInput.clear();

    // Periodically prune stale persistent states (every 600 frames = ~10s at 60fps)
    if (m_frameIndex % 600 == 0 && m_frameIndex > 0) {
        for (auto it = m_customStates.begin(); it != m_customStates.end(); ) {
            if (it->second.lastFrameActive + 600 < m_frameIndex &&
                it->first.id != m_focusedId &&
                it->first.id != m_activeId) {
                it = m_customStates.erase(it);
            } else {
                ++it;
            }
        }
    }

    if (m_repaintFrames > 0) {
        --m_repaintFrames;
    }
}

void Context::RenderNodeRecursive(Node* node, ICanvas& canvas, const Rect& dirtyRect, const UiTheme& parentTheme) {
    if (!node) return;
    if (node->computedRect.w <= 0.0f || node->computedRect.h <= 0.0f) return;
    if (!node->computedRect.Intersects(dirtyRect)) return;

    const UiTheme& activeTheme = (node->theme != nullptr) ? *node->theme : parentTheme;

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
        node->renderFn(node, canvas, activeTheme);
    }

    if (node->clipChildren) {
        canvas.PushClip(GetNodeContentClipRect(node, activeTheme));
    }

    // Recurse on children
    for (Node* child = node->firstChild; child != nullptr; child = child->nextSibling) {
        RenderNodeRecursive(child, canvas, dirtyRect, activeTheme);
    }

    if (node->clipChildren) {
        canvas.PopClip();
    }

    // Render post-pass overlay contents (after children and outside clip)
    if (node->postRenderFn) {
        node->postRenderFn(node, canvas, activeTheme);
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

void Context::RenderDebugBoundsRecursive(Node* node, ICanvas& canvas, bool inheritedDebugBounds) {
    if (!node) return;
    if (node->computedRect.w <= 0.0f || node->computedRect.h <= 0.0f) return;

    const bool activeDebug = node->debugBoundsOverride.value_or(inheritedDebugBounds);

    if (node != m_root && activeDebug) {
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

    if (node->clipChildren) {
        const UiTheme* resolvedTheme = &m_theme;
        for (const Node* curr = node; curr != nullptr; curr = curr->parent) {
            if (curr->theme != nullptr) {
                resolvedTheme = curr->theme;
                break;
            }
        }
        canvas.PushClip(GetNodeContentClipRect(node, *resolvedTheme));
    }

    for (Node* child = node->firstChild; child != nullptr; child = child->nextSibling) {
        RenderDebugBoundsRecursive(child, canvas, activeDebug);
    }

    if (node->clipChildren) {
        canvas.PopClip();
    }
}

Node* Context::FindDeepestNodeAt(Node* root, Vec2 pos, Rect clipRect) {
    if (!root) return nullptr;
    if (root->computedRect.w <= 0.0f || root->computedRect.h <= 0.0f) return nullptr;

    const Rect visibleRect = root->computedRect.Intersect(clipRect);
    if (!visibleRect.Contains(pos)) {
        return nullptr;
    }

    Rect effectiveClip = clipRect;
    if (root->clipChildren) {
        const UiTheme* resolvedTheme = &m_theme;
        for (const Node* curr = root; curr != nullptr; curr = curr->parent) {
            if (curr->theme != nullptr) {
                resolvedTheme = curr->theme;
                break;
            }
        }
        effectiveClip = clipRect.Intersect(GetNodeContentClipRect(root, *resolvedTheme));
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

    bool inspectActive = m_inspectModeEnabled;
    for (const Node* curr = target; curr != nullptr; curr = curr->parent) {
        if (curr->inspectModeOverride.has_value()) {
            inspectActive = *curr->inspectModeOverride;
            break;
        }
    }
    if (!inspectActive) return;

    // Intersect ancestor clipChildren rects so inspected scrolled children don't bleed outside ScrollArea
    Rect ancestorClip(0.0f, 0.0f, logicalSize.x, logicalSize.y);
    for (const Node* p = target->parent; p != nullptr; p = p->parent) {
        if (p->clipChildren) {
            const UiTheme* resolvedTheme = &m_theme;
            for (const Node* curr = p; curr != nullptr; curr = curr->parent) {
                if (curr->theme != nullptr) {
                    resolvedTheme = curr->theme;
                    break;
                }
            }
            ancestorClip = ancestorClip.Intersect(GetNodeContentClipRect(p, *resolvedTheme));
        }
    }
    canvas.PushClip(ancestorClip);

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
    canvas.PopClip();

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
            if (!h->label.empty()) {
                labelStr = std::string(h->label.text.substr(0, 24));
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

void Context::RenderTooltipOverlay(ICanvas& canvas, Vec2 logicalSize) {
    if (m_activeTooltip.empty()) return;

    const float fontSize = m_theme.ResolveFontSize(m_activeTooltip, TextRole::LabelSmall, UiSize::Sm);
    const float lineH = canvas.GetLineHeight(fontSize);

    const float maxTextW = 280.0f;
    const float natW = TextUtil::MeasureWidth(m_activeTooltip, fontSize, &canvas);
    const float contentW = std::min(natW, maxTextW);
    const float contentH = TextUtil::MeasureUiTextHeight(
        m_activeTooltip, std::max(1.0f, contentW), fontSize, lineH, &canvas);

    const float padX = 10.0f;
    const float padY = 6.0f;
    const float boxW = std::max(28.0f, contentW + padX * 2.0f);
    const float boxH = std::max(24.0f, contentH + padY * 2.0f);

    float bx = m_mousePos.x + 12.0f;
    float by = m_mousePos.y + 18.0f;
    if (m_activeTooltipAnchor.w > 0.0f && m_activeTooltipAnchor.h > 0.0f) {
        bx = m_activeTooltipAnchor.x + (m_activeTooltipAnchor.w - boxW) * 0.5f;
        by = m_activeTooltipAnchor.y - boxH - 6.0f;
        if (by < 8.0f) {
            by = m_activeTooltipAnchor.y + m_activeTooltipAnchor.h + 6.0f;
        }
    }

    bx = std::clamp(bx, 8.0f, std::max(8.0f, logicalSize.x - boxW - 8.0f));
    by = std::clamp(by, 8.0f, std::max(8.0f, logicalSize.y - boxH - 8.0f));

    const Rect boxRect(bx, by, boxW, boxH);
    canvas.DrawRoundRect(Rect(bx, by + 2.0f, boxW, boxH), 6.0f, Color(0, 0, 0, 60), Color::Transparent(), 0.0f);
    canvas.DrawRoundRect(boxRect, 6.0f, Color(15, 23, 42, 245), Color(71, 85, 105, 220), 1.0f);

    const Color textColor = m_theme.ResolveTextColor(m_activeTooltip, Color(248, 250, 252, 255));

    TextUtil::DrawUiText(
        canvas, m_activeTooltip,
        Rect(bx + padX, by + padY, contentW + 2.0f, contentH),
        fontSize, lineH, textColor, 0.0f, 0.5f);
}

} // namespace guikit
