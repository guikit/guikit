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
        for (auto it = m_pendingKeyEvents.begin(); it != m_pendingKeyEvents.end(); ) {
            if (it->key == Key::Tab) {
                const bool reverse = it->mods.shift;
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
            }
        } else {
            if (m_editState.id == m_focusedId) {
                m_editState.id = Id::Null();
            }
            m_focusedId = Id::Null();
        }
    }

    m_arena.Reset();
    m_containerStack.clear();
    m_sizeStack.clear();
    m_disabledStack.clear();
    m_themeStack.clear();
    m_scaleStack.clear();
    m_currFrameBounds.clear();

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

ContainerScope Context::Sidebar(SidebarOptions opts, std::source_location loc) {
    return Open<guikit::Sidebar>(opts, loc);
}

SceneScope Context::Scene(SceneOptions opts, std::source_location loc) {
    return Open<guikit::Scene>(opts, loc);
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

Response Context::Element(ElementOptions opts, std::source_location loc) {
    return Run<guikit::Element>(opts, loc);
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

Response Context::InputField(std::string_view value, InputFieldOptions opts, std::source_location loc) {
    return Run<guikit::InputField>(nullptr, nullptr, nullptr, nullptr, value, opts, loc);
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
        "Def", "Pri", "Sec", "Suc", "Wrn", "Dgr", "Inf", "Ghst"
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
    if (!opts.valueText.variant.has_value() && variant != UiVariant::Default) {
        opts.valueText.variant = variant;
    }
    opts.variant = variant;

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
    if (mods.ctrl) {
        switch (key) {
            case Key::A:
                SelectAll();
                return true;
            case Key::C:
                if (HasSelection()) {
                    ctx.SetClipboardText(SelectedText());
                }
                return true;
            case Key::X:
                if (HasSelection()) {
                    ctx.SetClipboardText(SelectedText());
                    if (!readOnly) {
                        DeleteSelection();
                    }
                }
                return true;
            case Key::V:
                if (!readOnly) {
                    const std::string clip = ctx.GetClipboardText();
                    if (!clip.empty()) {
                        Insert(clip);
                    }
                }
                return true;
            case Key::Z:
                if (!readOnly && undoText != text) {
                    std::swap(text, undoText);
                    cursor = std::min(undoCursor, text.size());
                    selectionAnchor = cursor;
                    changed = true;
                }
                return true;
            default:
                break;
        }
    }

    switch (key) {
        case Key::Backspace: {
            if (readOnly) {
                return true;
            }
            if (HasSelection()) {
                DeleteSelection();
            } else if (cursor > 0 && !text.empty()) {
                SaveUndo();
                const size_t safeCur = std::min(cursor, text.size());
                const size_t prev = mods.ctrl
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
        case Key::Delete: {
            if (readOnly) {
                return true;
            }
            if (HasSelection()) {
                DeleteSelection();
            } else if (cursor < text.size()) {
                SaveUndo();
                const size_t next = mods.ctrl
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
        case Key::Left: {
            if (!mods.shift && HasSelection()) {
                if (mods.ctrl) {
                    cursor = TextUtil::PrevWordBoundary(text, SelectionStart());
                } else {
                    cursor = SelectionStart();
                }
                selectionAnchor = cursor;
            } else {
                cursor = mods.ctrl
                    ? TextUtil::PrevWordBoundary(text, cursor)
                    : TextUtil::PrevCharBoundary(text, cursor);
                if (!mods.shift) {
                    selectionAnchor = cursor;
                }
            }
            return true;
        }
        case Key::Right: {
            if (!mods.shift && HasSelection()) {
                if (mods.ctrl) {
                    cursor = TextUtil::NextWordBoundary(text, SelectionEnd());
                } else {
                    cursor = SelectionEnd();
                }
                selectionAnchor = cursor;
            } else {
                cursor = mods.ctrl
                    ? TextUtil::NextWordBoundary(text, cursor)
                    : TextUtil::NextCharBoundary(text, cursor);
                if (!mods.shift) {
                    selectionAnchor = cursor;
                }
            }
            return true;
        }
        case Key::Home: {
            if (multiline && !mods.ctrl) {
                const size_t prevNl = (cursor > 0) ? text.rfind('\n', cursor - 1) : std::string::npos;
                cursor = (prevNl == std::string::npos) ? 0 : prevNl + 1;
            } else {
                cursor = 0;
            }
            if (!mods.shift) {
                selectionAnchor = cursor;
            }
            return true;
        }
        case Key::End: {
            if (multiline && !mods.ctrl) {
                const size_t nextNl = text.find('\n', cursor);
                cursor = (nextNl == std::string::npos) ? text.size() : nextNl;
            } else {
                cursor = text.size();
            }
            if (!mods.shift) {
                selectionAnchor = cursor;
            }
            return true;
        }
        case Key::Enter:
            if (multiline) {
                if (!readOnly) {
                    Insert("\n");
                }
                return true;
            }
            submitted = true;
            outLostFocus = true;
            return true;
        case Key::Escape:
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
    if (m_inspectModeEnabled || m_anyScopedInspectActive) {
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
    if (!m_mouseClicked && m_focusedId.IsValid() && m_editState.id == m_focusedId) {
        m_editState.Insert(utf8Text);
    } else {
        m_pendingTextInput.emplace_back(utf8Text);
    }
}

void Context::OnKeyDown(Key key, KeyModifiers mods) {
    m_keyMods = mods;
    ResetCursorBlink();
    RequestRepaint(3);

    if (key == Key::Escape && m_openPopupId.IsValid()) {
        ClosePopup();
    }

    if (!m_mouseClicked && m_focusedId.IsValid() && m_editState.id == m_focusedId) {
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
        m_editState.undoText = m_editState.text;
        m_editState.undoCursor = m_editState.cursor;
        m_editState.changed = false;
        m_editState.submitted = false;
        m_editState.multiline = multiline;
        m_editState.readOnly = readOnly;
    } else {
        m_editState.multiline = multiline;
        m_editState.readOnly = readOnly;
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
    m_editState.changed = false;
    m_editState.submitted = false;
    ResetCursorBlink();
}

void Context::SetTextEditCursor(Id id, size_t cursor, size_t selectionAnchor) noexcept {
    if (m_editState.id == id && id.IsValid()) {
        m_editState.cursor = std::min(cursor, m_editState.text.size());
        m_editState.selectionAnchor = std::min(selectionAnchor, m_editState.text.size());
        ResetCursorBlink();
    }
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
    return node->computedRect;
}

static void CollectBoundsRecursive(Node* node, std::unordered_map<Id, Rect>& outBounds, Rect parentClip, const UiTheme& theme) {
    if (!node) return;

    const UiTheme& activeTheme = (node->theme != nullptr) ? *node->theme : theme;

    if (node->id.IsValid()) {
        const Rect clipped = node->computedRect.Intersect(parentClip);
        if (clipped.w > 0.0f && clipped.h > 0.0f) {
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
        CollectBoundsRecursive(child, outBounds, effectiveClip, activeTheme);
    }
}

void Context::UpdateScrollAreasRecursive(Node* node) {
    if (!node) return;

    if (node->type == NodeType::ScrollArea || (node->type == NodeType::Sidebar && node->sidebarScrollable)) {
        ScrollArea::Update(*this, node);
    } else if (node->type == NodeType::Scene) {
        Scene::Update(*this, node);
    } else if (node->type == NodeType::Carousel) {
        Carousel::Update(*this, node);
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
    LayoutSolver::Measure(m_root, &canvas, m_theme);
    LayoutSolver::Arrange(m_root, Rect(0.0f, 0.0f, logicalSize.x, logicalSize.y), m_theme, &canvas);

    // Update interactive scroll areas
    UpdateScrollAreasRecursive(m_root);

    // Cache computed bounds and focusable widget list for next frame
    CollectBoundsRecursive(m_root, m_currFrameBounds, Rect(0.0f, 0.0f, logicalSize.x, logicalSize.y), m_theme);
    m_prevFrameBounds = std::move(m_currFrameBounds);
    m_prevFocusableIds = std::move(m_currFocusableIds);

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
    bool anyDebugBounds = m_debugBoundsEnabled;
    bool anyInspectMode = m_inspectModeEnabled;

    std::function<void(Node*)> collectSnapshots = [&](Node* n) {
        if (!n) return;
        if (n->debugBoundsOverride.value_or(false)) {
            anyDebugBounds = true;
        }
        if (n->inspectModeOverride.value_or(false)) {
            anyInspectMode = true;
        }
        if (n->id.IsValid()) {
            WidgetSnapshot snap;
            snap.rect = n->computedRect;
            snap.flags = (n->hovered ? 1u : 0u) |
                         (n->pressed ? 2u : 0u) |
                         (n->disabled ? 4u : 0u) |
                         (n->checked ? 8u : 0u) |
                         (n->active ? 64u : 0u) |
                         (n->focused ? 128u : 0u);
            snap.bgColor = n->backgroundColor.ToUint32();
            snap.borderColor = n->borderColor.ToUint32();

            uint64_t h = n->visualHash != 0 ? n->visualHash : detail::HashStringView(n->text);
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
    m_anyScopedInspectActive = anyInspectMode;

    // Detect removed widgets
    for (const auto& [id, prevSnap] : m_prevSnapshots) {
        if (!currSnapshots.contains(id)) {
            addDamage(prevSnap.rect);
        }
    }

    const bool hasTooltip = !m_activeTooltip.empty();

    // Determine whether full repaint or partial dirty rect is required
    const float totalViewportArea = logicalSize.x * logicalSize.y;
    if (!m_dirtyRectEnabled || m_forceFullRepaint || m_prevSnapshots.empty() || m_openPopupId.IsValid() ||
        hasTooltip || m_hadTooltipLastFrame || anyDebugBounds || anyInspectMode) {
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
    m_hadTooltipLastFrame = hasTooltip;
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

        RenderNodeRecursive(m_root, canvas, m_dirtyRect, m_theme);

        // Render floating overlay passes (e.g. dropdown popups)
        for (const auto& overlayFn : m_overlayPasses) {
            overlayFn(canvas);
        }

        // Render floating tooltip overlay
        if (hasTooltip) {
            RenderTooltipOverlay(canvas, logicalSize);
        }

        // Developer Debug Pass: Element bounds and padding
        if (anyDebugBounds) {
            RenderDebugBoundsRecursive(m_root, canvas, m_debugBoundsEnabled);
        }

        // Developer Debug Pass: Chrome DevTools-style hover inspector & floating badge
        if (anyInspectMode) {
            RenderInspectOverlay(m_root, canvas, logicalSize);
        }

        canvas.PopClip();
        canvas.End();
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

    for (Node* child = node->firstChild; child != nullptr; child = child->nextSibling) {
        RenderDebugBoundsRecursive(child, canvas, activeDebug);
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
                labelStr = std::string(h->title.text.substr(0, 24));
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

    Color textColor(248, 250, 252, 255);
    if (m_activeTooltip.variant.has_value()) {
        textColor = m_theme.GetTextColor(*m_activeTooltip.variant);
    }

    TextUtil::DrawUiText(
        canvas, m_activeTooltip,
        Rect(bx + padX, by + padY, contentW + 2.0f, contentH),
        fontSize, lineH, textColor, 0.0f, 0.5f);
}

} // namespace guikit
