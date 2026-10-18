#include "guikit/widgets/BpmnEditor.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <format>
#include "guikit/Context.hpp"
#include "guikit/core/Element.hpp"

namespace guikit {

namespace {

constexpr float kPi = 3.14159265358979323846f;

[[nodiscard]] ControlSpec MakeBpmnControlSpec(const BpmnEditorOptions& opts) noexcept {
    ControlSpec c = opts.control;
    if (!c.width.has_value()) c.width = opts.width;
    if (!c.height.has_value()) c.height = opts.height;
    c.stretchable = true;
    c.stretchHeightWhenStandalone = (opts.heightMode == SizeMode::Stretch);
    c.hasIntrinsicBox = true;
    return c;
}

[[nodiscard]] ElementSpec MakeBpmnElementSpec(
    const BpmnEditorOptions& opts,
    UiSize size,
    const UiTheme& theme,
    bool disabled
) noexcept {
    const ControlSpec ctrlSpec = MakeBpmnControlSpec(opts);
    return ElementSpec{
        .label = opts.label,
        .description = opts.description,
        .icon = opts.icon,
        .badge = opts.badge,
        .layout = opts.layout,
        .inlineOrder = opts.inlineOrder,
        .align = opts.align,
        .stretch = opts.stretch,
        .width = ctrlSpec.width,
        .height = ctrlSpec.height,
        .control = ctrlSpec,
        .background = opts.background,
        .border = {.width = opts.borderWidth, .radius = opts.borderRadius},
        .widgetSize = size,
        .labelGap = theme.Scale(6.0f),
        .descGap = theme.Scale(4.0f),
        .inlineGap = theme.Scale(12.0f),
        .stretchableControl = true,
        .stretchHeightWhenStandalone = (opts.heightMode == SizeMode::Stretch),
        .disabled = disabled
    };
}

struct BpmnPalette {
    Color canvasBg{};
    Color canvasBorder{};
    Color gridDot{};
    Color gridMajor{};
    Color laneBgEven{};
    Color laneBgOdd{};
    Color poolHeaderBg{};
    Color laneHeaderBg{};
    Color laneBorder{};
    Color laneText{};

    Color nodeFill{};
    Color nodeStroke{};
    Color nodeShadow{};
    Color textPrimary{};
    Color textSecondary{};
    Color textMuted{};

    Color startEventFill{};
    Color startEventStroke{};
    Color interEventFill{};
    Color interEventStroke{};
    Color endEventFill{};
    Color endEventStroke{};
    Color gatewayFill{};
    Color gatewayStroke{};
    Color taskAccent{};
    Color annotationBg{};
    Color dataFill{};

    Color edgeStroke{};
    Color edgeMessage{};
    Color edgeLabelBg{};

    Color selectAccent{};
    Color selectGlow{};
    Color hoverAccent{};
    Color guideLaser{};

    Color hudBg{};
    Color hudBorder{};
    Color hudHover{};
    Color hudActiveBg{};
    Color simActiveGreen{};
};

[[nodiscard]] BpmnPalette ResolveBpmnPalette(BpmnThemePreset preset, const UiTheme& theme) noexcept {
    BpmnThemePreset eff = preset;
    if (eff == BpmnThemePreset::Auto) {
        eff = (theme.mode == ThemeMode::Light) ? BpmnThemePreset::BpmnIoClassic : BpmnThemePreset::CamundaDark;
    }

    BpmnPalette p{};
    if (eff == BpmnThemePreset::BpmnIoClassic) {
        p.canvasBg         = Color(248, 250, 252, 255);
        p.canvasBorder     = Color(203, 213, 225, 255);
        p.gridDot          = Color(148, 163, 184, 110);
        p.gridMajor        = Color(203, 213, 225, 130);
        p.laneBgEven       = Color(255, 255, 255, 242);
        p.laneBgOdd        = Color(244, 247, 251, 242);
        p.poolHeaderBg     = Color(214, 224, 236, 255);
        p.laneHeaderBg     = Color(229, 236, 245, 255);
        p.laneBorder       = Color(148, 163, 184, 230);
        p.laneText         = Color(30, 41, 59, 255);

        p.nodeFill         = Color(255, 255, 255, 255);
        p.nodeStroke       = Color(30, 41, 59, 255);
        p.nodeShadow       = Color(15, 23, 42, 26);
        p.textPrimary      = Color(15, 23, 42, 255);
        p.textSecondary    = Color(51, 65, 85, 255);
        p.textMuted        = Color(100, 116, 139, 235);

        p.startEventFill   = Color(236, 253, 245, 255);
        p.startEventStroke = Color(5, 150, 105, 255);
        p.interEventFill   = Color(255, 251, 235, 255);
        p.interEventStroke = Color(217, 119, 6, 255);
        p.endEventFill     = Color(254, 242, 242, 255);
        p.endEventStroke   = Color(220, 38, 38, 255);
        p.gatewayFill      = Color(255, 251, 235, 255);
        p.gatewayStroke    = Color(180, 83, 9, 255);
        p.taskAccent       = Color(37, 99, 235, 255);
        p.annotationBg     = Color(254, 252, 232, 240);
        p.dataFill         = Color(241, 245, 249, 255);

        p.edgeStroke       = Color(51, 65, 85, 255);
        p.edgeMessage      = Color(37, 99, 235, 235);
        p.edgeLabelBg      = Color(255, 255, 255, 242);

        p.selectAccent     = Color(37, 99, 235, 255);
        p.selectGlow       = Color(59, 130, 246, 55);
        p.hoverAccent      = Color(14, 165, 233, 220);
        p.guideLaser       = Color(236, 72, 153, 220);

        p.hudBg            = Color(255, 255, 255, 246);
        p.hudBorder        = Color(203, 213, 225, 245);
        p.hudHover         = Color(241, 245, 249, 255);
        p.hudActiveBg      = Color(219, 234, 254, 255);
        p.simActiveGreen   = Color(16, 185, 129, 255);
    } else if (eff == BpmnThemePreset::Blueprint) {
        p.canvasBg         = Color(7, 22, 44, 255);
        p.canvasBorder     = Color(30, 78, 138, 255);
        p.gridDot          = Color(56, 189, 248, 70);
        p.gridMajor        = Color(30, 64, 115, 120);
        p.laneBgEven       = Color(10, 31, 60, 225);
        p.laneBgOdd        = Color(13, 38, 72, 225);
        p.poolHeaderBg     = Color(12, 40, 78, 255);
        p.laneHeaderBg     = Color(17, 50, 94, 250);
        p.laneBorder       = Color(56, 189, 248, 155);
        p.laneText         = Color(224, 242, 254, 255);

        p.nodeFill         = Color(12, 38, 72, 252);
        p.nodeStroke       = Color(125, 211, 252, 255);
        p.nodeShadow       = Color(2, 8, 20, 125);
        p.textPrimary      = Color(240, 249, 255, 255);
        p.textSecondary    = Color(186, 230, 253, 240);
        p.textMuted        = Color(125, 211, 252, 195);

        p.startEventFill   = Color(8, 48, 62, 255);
        p.startEventStroke = Color(52, 211, 153, 255);
        p.interEventFill   = Color(45, 38, 22, 255);
        p.interEventStroke = Color(251, 191, 36, 255);
        p.endEventFill     = Color(58, 22, 38, 255);
        p.endEventStroke   = Color(251, 113, 133, 255);
        p.gatewayFill      = Color(16, 44, 82, 255);
        p.gatewayStroke    = Color(250, 204, 21, 255);
        p.taskAccent       = Color(56, 189, 248, 255);
        p.annotationBg     = Color(14, 42, 78, 235);
        p.dataFill         = Color(15, 45, 84, 255);

        p.edgeStroke       = Color(186, 230, 253, 240);
        p.edgeMessage      = Color(56, 189, 248, 240);
        p.edgeLabelBg      = Color(9, 28, 54, 242);

        p.selectAccent     = Color(34, 211, 238, 255);
        p.selectGlow       = Color(34, 211, 238, 70);
        p.hoverAccent      = Color(125, 211, 252, 220);
        p.guideLaser       = Color(244, 114, 182, 230);

        p.hudBg            = Color(9, 28, 54, 246);
        p.hudBorder        = Color(38, 98, 168, 235);
        p.hudHover         = Color(18, 52, 96, 255);
        p.hudActiveBg      = Color(14, 116, 144, 215);
        p.simActiveGreen   = Color(16, 185, 129, 255);
    } else {
        // CamundaDark (default dark theme)
        p.canvasBg         = Color(11, 15, 25, 255);
        p.canvasBorder     = Color(38, 50, 72, 235);
        p.gridDot          = Color(71, 85, 105, 100);
        p.gridMajor        = Color(30, 41, 59, 140);
        p.laneBgEven       = Color(15, 22, 36, 230);
        p.laneBgOdd        = Color(18, 26, 43, 230);
        p.poolHeaderBg     = Color(21, 31, 50, 255);
        p.laneHeaderBg     = Color(26, 38, 60, 252);
        p.laneBorder       = Color(56, 74, 102, 220);
        p.laneText         = Color(226, 232, 240, 255);

        p.nodeFill         = Color(24, 35, 56, 255);
        p.nodeStroke       = Color(164, 180, 204, 245);
        p.nodeShadow       = Color(2, 6, 14, 135);
        p.textPrimary      = Color(248, 250, 252, 255);
        p.textSecondary    = Color(196, 209, 228, 245);
        p.textMuted        = Color(132, 150, 178, 225);

        p.startEventFill   = Color(12, 44, 36, 255);
        p.startEventStroke = Color(16, 185, 129, 255);
        p.interEventFill   = Color(48, 35, 14, 255);
        p.interEventStroke = Color(245, 158, 11, 255);
        p.endEventFill     = Color(54, 19, 30, 255);
        p.endEventStroke   = Color(244, 63, 94, 255);
        p.gatewayFill      = Color(40, 31, 18, 255);
        p.gatewayStroke    = Color(251, 191, 36, 255);
        p.taskAccent       = Color(56, 189, 248, 255);
        p.annotationBg     = Color(28, 40, 62, 235);
        p.dataFill         = Color(28, 39, 60, 255);

        p.edgeStroke       = Color(162, 178, 202, 240);
        p.edgeMessage      = Color(56, 189, 248, 240);
        p.edgeLabelBg      = Color(15, 23, 42, 244);

        p.selectAccent     = Color(34, 211, 238, 255);
        p.selectGlow       = Color(34, 211, 238, 64);
        p.hoverAccent      = Color(56, 189, 248, 215);
        p.guideLaser       = Color(244, 114, 182, 225);

        p.hudBg            = Color(17, 24, 39, 246);
        p.hudBorder        = Color(51, 65, 85, 240);
        p.hudHover         = Color(30, 41, 59, 255);
        p.hudActiveBg      = Color(14, 116, 144, 205);
        p.simActiveGreen   = Color(16, 185, 129, 255);
    }
    return p;
}

struct AlignmentGuide {
    bool vertical{true};
    float worldCoord{0.0f};
    float worldSpanMin{0.0f};
    float worldSpanMax{0.0f};
};

struct BpmnEditorState {
    bool initialized{false};
    size_t lastDiagramNodeCount{0};
    uint32_t lastFirstNodeId{0};

    Vec2 pan{60.0f, 44.0f};
    float zoom{1.0f};

    BpmnTool activeTool{BpmnTool::Select};

    // Selection state
    std::vector<uint32_t> selectedNodes{};
    uint32_t primaryNodeId{0};
    uint32_t selectedEdgeId{0};
    uint32_t selectedLaneId{0};

    uint32_t hoveredNodeId{0};
    uint32_t hoveredEdgeId{0};
    uint32_t hoveredLaneId{0};
    uint32_t hoveredLabelNodeId{0};
    uint32_t hoveredLabelEdgeId{0};

    int hoveredPaletteIdx{-1};
    int hoveredContextPadBtn{-1};
    int hoveredReplaceMenuItem{-1};
    int hoveredResizeCorner{-1};   // 0=NW, 1=NE, 2=SE, 3=SW
    int hoveredEdgeSegIdx{-1};     // Hovered orthogonal segment index on selected/hovered edge
    int hoveredEdgeEndpoint{0};    // 0=none, 1=source, 2=target

    // Replace Menu (`🔧 Change Type`) popover
    bool replaceMenuOpen{false};

    // Dragging state
    bool panningCanvas{false};
    Vec2 panStartMouse{0.0f, 0.0f};
    Vec2 panStartOffset{0.0f, 0.0f};

    bool draggingNodes{false};
    bool dragUndoRecorded{false};
    Vec2 dragStartWorld{0.0f, 0.0f};
    std::vector<std::pair<uint32_t, Vec2>> dragInitialNodePos{};
    uint32_t dragBoundaryHostId{0};

    // Node corner resize (`Task`, `SubProcess`, `TextAnnotation`)
    bool resizingNode{false};
    int resizeCorner{-1};
    Vec2 resizeStartWorld{0.0f, 0.0f};
    Rect resizeInitialBounds{};

    // External label dragging (`Event`, `Gateway`, `DataObject`, `DataStore`, `BpmnEdge`)
    bool draggingLabel{false};
    uint32_t dragLabelNodeId{0};
    uint32_t dragLabelEdgeId{0};
    Vec2 dragLabelStartWorld{0.0f, 0.0f};
    Vec2 dragLabelInitOffset{0.0f, 0.0f};

    // Orthogonal Edge Segment & Endpoint Reconnect dragging (`ConnectionSegmentMove` / `BendpointMove`)
    bool draggingEdgeSegment{false};
    uint32_t dragEdgeId{0};
    int dragEdgeSegIdx{-1};
    bool dragEdgeSegHorizontal{true};
    Vec2 dragEdgeStartWorld{0.0f, 0.0f};
    std::vector<Vec2> dragEdgeInitWaypoints{};

    bool reconnectingEdge{false};
    uint32_t reconnectEdgeId{0};
    int reconnectEndpoint{0}; // 1=fromNodeId, 2=toNodeId
    Vec2 reconnectEndWorld{0.0f, 0.0f};
    uint32_t reconnectSnapNodeId{0};

    // Palette Drag-to-Create + Live Ghost Preview Smart Alignment
    bool paletteDragCreating{false};
    Vec2 ghostSnappedPos{0.0f, 0.0f};

    bool draggingLane{false};
    int laneResizeEdge{0}; // 0 = move, 1 = right edge, 2 = bottom edge
    Vec2 laneDragStartWorld{0.0f, 0.0f};
    Rect laneInitialRect{};
    std::vector<std::pair<uint32_t, Vec2>> laneContainedNodeOffsets{};

    // Lasso box selection
    bool lassoActive{false};
    Vec2 lassoStartWorld{0.0f, 0.0f};
    Vec2 lassoEndWorld{0.0f, 0.0f};

    // Space Tool (`S`)
    bool spaceToolDragging{false};
    bool spaceToolAxisLocked{false};
    bool spaceToolHorizontal{true};
    Vec2 spaceStartWorld{0.0f, 0.0f};
    Vec2 spaceCurrentWorld{0.0f, 0.0f};
    std::vector<std::pair<uint32_t, Vec2>> spaceInitialNodes{};
    std::vector<std::pair<uint32_t, Rect>> spaceInitialLanes{};

    // Interactive wire connection
    bool connectingWire{false};
    uint32_t connectFromNodeId{0};
    BpmnPortSide connectFromPort{BpmnPortSide::Auto};
    Vec2 connectEndWorld{0.0f, 0.0f};
    uint32_t connectSnapNodeId{0};

    // Inline text editing (double-click)
    uint32_t editingNodeId{0};
    uint32_t editingEdgeId{0};
    uint32_t editingLaneId{0};

    // Smart alignment guides active during node drag or ghost creation preview
    std::vector<AlignmentGuide> activeGuides{};

    // Undo / Redo stacks
    std::vector<BpmnDiagram> undoStack{};
    std::vector<BpmnDiagram> redoStack{};

    // Clipboard for Copy / Paste
    std::vector<BpmnNode> clipboardNodes{};
    std::vector<BpmnEdge> clipboardEdges{};

    // Token simulation timing
    double lastSimTickSec{0.0};
    double spawnCooldownSec{0.0};
    uint32_t nextTokenId{1};
    uint32_t nextInstanceId{1};
    bool simPaused{false};

    [[nodiscard]] bool IsNodeSelected(uint32_t id) const noexcept {
        if (id == 0) return false;
        if (primaryNodeId == id) return true;
        return std::find(selectedNodes.begin(), selectedNodes.end(), id) != selectedNodes.end();
    }

    void SelectSingleNode(uint32_t id) {
        selectedNodes.clear();
        if (id != 0) selectedNodes.push_back(id);
        primaryNodeId = id;
        selectedEdgeId = 0;
        selectedLaneId = 0;
        replaceMenuOpen = false;
    }

    void ToggleNodeSelection(uint32_t id) {
        if (id == 0) return;
        auto it = std::find(selectedNodes.begin(), selectedNodes.end(), id);
        if (it != selectedNodes.end()) {
            selectedNodes.erase(it);
            primaryNodeId = selectedNodes.empty() ? 0 : selectedNodes.back();
        } else {
            selectedNodes.push_back(id);
            primaryNodeId = id;
        }
        selectedEdgeId = 0;
        selectedLaneId = 0;
        replaceMenuOpen = false;
    }

    void ClearSelection() noexcept {
        selectedNodes.clear();
        primaryNodeId = 0;
        selectedEdgeId = 0;
        selectedLaneId = 0;
        replaceMenuOpen = false;
    }

    void PushUndo(const BpmnDiagram& d) {
        if (undoStack.size() >= 48) {
            undoStack.erase(undoStack.begin());
        }
        undoStack.push_back(d);
        redoStack.clear();
    }

    bool Undo(BpmnDiagram& d) {
        if (undoStack.empty()) return false;
        redoStack.push_back(d);
        d = std::move(undoStack.back());
        undoStack.pop_back();
        ClearSelection();
        return true;
    }

    bool Redo(BpmnDiagram& d) {
        if (redoStack.empty()) return false;
        undoStack.push_back(d);
        d = std::move(redoStack.back());
        redoStack.pop_back();
        ClearSelection();
        return true;
    }
};

struct BpmnEditorPayload {
    BpmnEditorOptions opts{};
    BpmnEditorResponse resp{};
    BpmnDiagram diagramSnapshot{};
    BpmnEditorState stateSnapshot{};
    std::string activeEditBuffer{};
    size_t activeEditCursor{0};
    bool cursorBlinkOn{true};
    float animPhase{0.0f};
};

// ============================================================================
// Multi-Line Word-Wrapping & Rotated Swimlane Typography Helpers
// ============================================================================

void DrawWrappedTextBox(
    ICanvas& canvas,
    std::string_view text,
    Rect bounds,
    float fontSize,
    Color color,
    FontStyle style = FontStyle::Regular,
    float alignX = 0.5f,
    float alignY = 0.5f,
    size_t maxLines = 3
) {
    if (text.empty() || color.a == 0 || bounds.w <= 4.0f || bounds.h <= 4.0f || maxLines == 0) return;

    std::vector<std::string> lines;
    lines.reserve(maxLines);

    size_t pos = 0;
    while (pos <= text.size() && lines.size() < maxLines) {
        const size_t nl = text.find('\n', pos);
        const std::string_view para = (nl == std::string_view::npos)
            ? text.substr(pos)
            : text.substr(pos, nl - pos);

        // Word-wrap `para` into `bounds.w`
        size_t wPos = 0;
        std::string currentLine;
        while (wPos < para.size() && lines.size() < maxLines) {
            while (wPos < para.size() && para[wPos] == ' ') ++wPos;
            if (wPos >= para.size()) break;
            const size_t nextSpace = para.find(' ', wPos);
            const std::string_view word = (nextSpace == std::string_view::npos)
                ? para.substr(wPos)
                : para.substr(wPos, nextSpace - wPos);

            std::string candidate = currentLine.empty()
                ? std::string(word)
                : (currentLine + " " + std::string(word));

            const float candW = canvas.MeasureTextStyled(candidate, fontSize, style).x;
            if (!currentLine.empty() && candW > bounds.w && lines.size() + 1 < maxLines) {
                lines.push_back(std::move(currentLine));
                currentLine = std::string(word);
            } else {
                currentLine = std::move(candidate);
            }
            if (nextSpace == std::string_view::npos) break;
            wPos = nextSpace + 1;
        }
        if (!currentLine.empty() && lines.size() < maxLines) {
            lines.push_back(std::move(currentLine));
        }

        if (nl == std::string_view::npos) break;
        pos = nl + 1;
    }

    if (lines.empty()) return;

    const float lineH = std::max(fontSize * 1.18f, canvas.GetLineHeight(fontSize) * 0.94f);
    const float totalH = static_cast<float>(lines.size()) * lineH;
    float startY = bounds.y + (bounds.h - totalH) * alignY;

    for (const std::string& line : lines) {
        const Rect lr(bounds.x, startY, bounds.w, lineH);
        canvas.DrawTextBox(line, lr, fontSize, color, alignX, 0.5f);
        startY += lineH;
    }
}

void DrawVerticalRotatedHeaderText(
    ICanvas& canvas,
    std::string_view text,
    Rect stripRect,
    float fontSize,
    Color color,
    FontStyle style = FontStyle::Bold
) {
    if (text.empty() || stripRect.w <= 4.0f || stripRect.h <= 16.0f) return;
    canvas.PushClip(stripRect);
    const Vec2 tm = canvas.MeasureTextStyled(text, fontSize, style);
    const float tx = stripRect.x + (stripRect.w - tm.y) * 0.5f;
    const float ty = stripRect.y + (stripRect.h + std::min(tm.x, stripRect.h - 8.0f)) * 0.5f;
    canvas.DrawTextStyled(text, {tx, ty}, fontSize, color, style, -90.0f);
    canvas.PopClip();
}

// ============================================================================
// Coordinate & Geometry Helpers
// ============================================================================

[[nodiscard]] Vec2 WorldToScreen(Vec2 w, const Rect& viewRect, Vec2 pan, float zoom) noexcept {
    return Vec2{
        viewRect.x + pan.x + w.x * zoom,
        viewRect.y + pan.y + w.y * zoom
    };
}

[[nodiscard]] Vec2 ScreenToWorld(Vec2 s, const Rect& viewRect, Vec2 pan, float zoom) noexcept {
    const float z = std::max(0.05f, zoom);
    return Vec2{
        (s.x - viewRect.x - pan.x) / z,
        (s.y - viewRect.y - pan.y) / z
    };
}

[[nodiscard]] Rect WorldToScreenRect(const Rect& wr, const Rect& viewRect, Vec2 pan, float zoom) noexcept {
    const Vec2 tl = WorldToScreen({wr.x, wr.y}, viewRect, pan, zoom);
    return Rect(tl.x, tl.y, wr.w * zoom, wr.h * zoom);
}

[[nodiscard]] float SnapValue(float v, float step) noexcept {
    if (step <= 1.0f) return v;
    return std::round(v / step) * step;
}

[[nodiscard]] Rect ComputeDiagramWorldBounds(const BpmnDiagram& diagram) noexcept {
    bool any = false;
    float minX = 0.0f, minY = 0.0f, maxX = 1200.0f, maxY = 700.0f;
    auto includeRect = [&](const Rect& r) {
        if (!any) {
            minX = r.x;
            minY = r.y;
            maxX = r.Right();
            maxY = r.Bottom();
            any = true;
        } else {
            minX = std::min(minX, r.x);
            minY = std::min(minY, r.y);
            maxX = std::max(maxX, r.Right());
            maxY = std::max(maxY, r.Bottom());
        }
    };
    for (const auto& l : diagram.lanes) includeRect(l.rect);
    for (const auto& n : diagram.nodes) includeRect(n.Bounds());
    if (!any) return Rect(0.0f, 0.0f, 1200.0f, 700.0f);
    return Rect(minX, minY, std::max(200.0f, maxX - minX), std::max(140.0f, maxY - minY));
}

void FitDiagramToViewport(const BpmnDiagram& diagram, const Rect& viewRect, float leftInset, BpmnEditorState& state) noexcept {
    if (viewRect.w <= 80.0f || viewRect.h <= 80.0f) return;
    const Rect wb = ComputeDiagramWorldBounds(diagram);
    const float padLeft = leftInset + 24.0f;
    const float padRight = 24.0f;
    const float padTop = 54.0f;
    const float padBottom = 34.0f;
    const float availW = std::max(120.0f, viewRect.w - padLeft - padRight);
    const float availH = std::max(120.0f, viewRect.h - padTop - padBottom);
    const float zx = availW / std::max(100.0f, wb.w);
    const float zy = availH / std::max(100.0f, wb.h);
    state.zoom = std::clamp(std::min(zx, zy), 0.35f, 1.25f);
    state.pan.x = padLeft + (availW - wb.w * state.zoom) * 0.5f - wb.x * state.zoom;
    state.pan.y = padTop + (availH - wb.h * state.zoom) * 0.5f - wb.y * state.zoom;
}

[[nodiscard]] Vec2 GetNodePortWorldPos(const BpmnNode& node, BpmnPortSide side) noexcept {
    const Rect b = node.Bounds();
    switch (side) {
        case BpmnPortSide::Left:   return {b.x, b.y + b.h * 0.5f};
        case BpmnPortSide::Right:  return {b.Right(), b.y + b.h * 0.5f};
        case BpmnPortSide::Top:    return {b.x + b.w * 0.5f, b.y};
        case BpmnPortSide::Bottom: return {b.x + b.w * 0.5f, b.Bottom()};
        case BpmnPortSide::Auto:   break;
    }
    return b.Center();
}

[[nodiscard]] Vec2 PortOutwardNormal(BpmnPortSide side) noexcept {
    switch (side) {
        case BpmnPortSide::Left:   return {-1.0f, 0.0f};
        case BpmnPortSide::Right:  return {1.0f, 0.0f};
        case BpmnPortSide::Top:    return {0.0f, -1.0f};
        case BpmnPortSide::Bottom: return {0.0f, 1.0f};
        default:                   return {1.0f, 0.0f};
    }
}

void ResolveAutoPorts(
    const BpmnNode& fromNode,
    const BpmnNode& toNode,
    BpmnPortSide reqFrom,
    BpmnPortSide reqTo,
    BpmnPortSide& outFrom,
    BpmnPortSide& outTo
) noexcept {
    outFrom = reqFrom;
    outTo = reqTo;
    const Vec2 fc = fromNode.Center();
    const Vec2 tc = toNode.Center();
    const float dx = tc.x - fc.x;
    const float dy = tc.y - fc.y;

    if (fromNode.type == BpmnNodeType::BoundaryEvent && outFrom == BpmnPortSide::Auto) {
        outFrom = BpmnPortSide::Bottom;
    }

    if (outFrom == BpmnPortSide::Auto && outTo == BpmnPortSide::Auto) {
        // Special gateway fork/join ergonomics across lanes
        if (fromNode.type == BpmnNodeType::Gateway && std::abs(dy) > 58.0f && dx > 16.0f) {
            outFrom = (dy > 0.0f) ? BpmnPortSide::Bottom : BpmnPortSide::Top;
            outTo = BpmnPortSide::Left;
            return;
        }
        if (toNode.type == BpmnNodeType::Gateway && std::abs(dy) > 58.0f && dx > 16.0f) {
            outFrom = BpmnPortSide::Right;
            outTo = (dy > 0.0f) ? BpmnPortSide::Top : BpmnPortSide::Bottom;
            return;
        }
    }

    if (outFrom == BpmnPortSide::Auto) {
        if (std::abs(dx) >= std::abs(dy) * 0.58f) {
            outFrom = (dx >= 0.0f) ? BpmnPortSide::Right : BpmnPortSide::Left;
        } else {
            outFrom = (dy >= 0.0f) ? BpmnPortSide::Bottom : BpmnPortSide::Top;
        }
    }
    if (outTo == BpmnPortSide::Auto) {
        if (fromNode.type == BpmnNodeType::BoundaryEvent) {
            if (dx > 50.0f) {
                outTo = BpmnPortSide::Left;
            } else if (dy < -30.0f) {
                outTo = BpmnPortSide::Bottom;
            } else {
                outTo = BpmnPortSide::Top;
            }
        } else if (std::abs(dx) >= std::abs(dy) * 0.58f) {
            outTo = (dx >= 0.0f) ? BpmnPortSide::Left : BpmnPortSide::Right;
        } else {
            outTo = (dy >= 0.0f) ? BpmnPortSide::Top : BpmnPortSide::Bottom;
        }
    }
}

void SimplifyOrthogonalPoints(std::vector<Vec2>& pts) {
    if (pts.size() < 3) return;
    std::vector<Vec2> out;
    out.reserve(pts.size());
    out.push_back(pts.front());
    for (size_t i = 1; i < pts.size(); ++i) {
        if (std::hypot(pts[i].x - out.back().x, pts[i].y - out.back().y) > 0.8f) {
            out.push_back(pts[i]);
        }
    }
    if (out.size() < 3) {
        pts = std::move(out);
        return;
    }
    std::vector<Vec2> clean;
    clean.reserve(out.size());
    clean.push_back(out.front());
    for (size_t i = 1; i + 1 < out.size(); ++i) {
        const Vec2 a = clean.back();
        const Vec2 b = out[i];
        const Vec2 c = out[i + 1];
        const bool sameX = (std::abs(a.x - b.x) <= 0.8f && std::abs(b.x - c.x) <= 0.8f);
        const bool sameY = (std::abs(a.y - b.y) <= 0.8f && std::abs(b.y - c.y) <= 0.8f);
        if (!sameX && !sameY) {
            clean.push_back(b);
        }
    }
    clean.push_back(out.back());
    pts = std::move(clean);
}

[[nodiscard]] std::vector<Vec2> BuildEdgeWorldPoints(
    const BpmnDiagram& diagram,
    const BpmnNode& fromNode,
    const BpmnNode& toNode,
    const BpmnEdge& edge,
    BpmnRoutingMode routingMode
) {
    BpmnPortSide fromSide = BpmnPortSide::Right;
    BpmnPortSide toSide = BpmnPortSide::Left;
    ResolveAutoPorts(fromNode, toNode, edge.fromPort, edge.toPort, fromSide, toSide);

    const Vec2 p0 = GetNodePortWorldPos(fromNode, fromSide);
    const Vec2 p1 = GetNodePortWorldPos(toNode, toSide);

    std::vector<Vec2> pts;
    pts.reserve(8);
    pts.push_back(p0);

    if (!edge.waypoints.empty()) {
        std::vector<Vec2> customWps = edge.waypoints;
        if (routingMode == BpmnRoutingMode::Orthogonal) {
            const bool fromH = (fromSide == BpmnPortSide::Left || fromSide == BpmnPortSide::Right);
            const bool toH = (toSide == BpmnPortSide::Left || toSide == BpmnPortSide::Right);
            if (customWps.size() == 1) {
                if (fromH && !toH) customWps[0] = {p1.x, p0.y};
                else if (!fromH && toH) customWps[0] = {p0.x, p1.y};
            } else if (customWps.size() >= 2) {
                if (fromH) customWps.front().y = p0.y;
                else       customWps.front().x = p0.x;
                if (toH)   customWps.back().y = p1.y;
                else       customWps.back().x = p1.x;
            }
        }
        for (const Vec2& wp : customWps) {
            if (std::hypot(wp.x - pts.back().x, wp.y - pts.back().y) > 0.5f) {
                pts.push_back(wp);
            }
        }
        if (std::hypot(p1.x - pts.back().x, p1.y - pts.back().y) > 0.5f || pts.size() == 1) {
            pts.push_back(p1);
        }
        return pts;
    }

    // In bpmn-js, Associations (DataObject / DataStore / TextAnnotation) can use direct or 1-bend dotted lines
    if (edge.flowType == BpmnFlowType::Association) {
        if (std::abs(p0.x - p1.x) > 8.0f && std::abs(p0.y - p1.y) > 8.0f && routingMode == BpmnRoutingMode::Orthogonal) {
            const bool fromHoriz = (fromSide == BpmnPortSide::Left || fromSide == BpmnPortSide::Right);
            pts.push_back(fromHoriz ? Vec2{p1.x, p0.y} : Vec2{p0.x, p1.y});
        }
        pts.push_back(p1);
        return pts;
    }

    const Vec2 d0 = PortOutwardNormal(fromSide);
    const Vec2 d1 = PortOutwardNormal(toSide);

    if (routingMode == BpmnRoutingMode::Bezier) {
        const float dist = std::hypot(p1.x - p0.x, p1.y - p0.y);
        const float tangent = std::clamp(dist * 0.42f, 28.0f, 150.0f);
        const Vec2 c0{p0.x + d0.x * tangent, p0.y + d0.y * tangent};
        const Vec2 c1{p1.x + d1.x * tangent, p1.y + d1.y * tangent};
        constexpr int kSteps = 18;
        for (int i = 1; i < kSteps; ++i) {
            const float t = static_cast<float>(i) / static_cast<float>(kSteps);
            const float u = 1.0f - t;
            const float b0 = u * u * u;
            const float b1 = 3.0f * u * u * t;
            const float b2 = 3.0f * u * t * t;
            const float b3 = t * t * t;
            pts.push_back({
                b0 * p0.x + b1 * c0.x + b2 * c1.x + b3 * p1.x,
                b0 * p0.y + b1 * c0.y + b2 * c1.y + b3 * p1.y
            });
        }
        pts.push_back(p1);
        return pts;
    }

    // Orthogonal Manhattan Router with Port Lead-Out Stubs & Full Obstacle Clearance (`bpmn-js` ManhattanLayout)
    Rect srcBox = fromNode.Bounds();
    if (fromNode.type == BpmnNodeType::BoundaryEvent && fromNode.attachedToNodeId != 0) {
        if (const BpmnNode* host = diagram.FindNode(fromNode.attachedToNodeId)) {
            srcBox = srcBox.Union(host->Bounds());
        }
    }
    constexpr float kBoxPad = 10.0f;
    srcBox = Rect(srcBox.x - kBoxPad, srcBox.y - kBoxPad, srcBox.w + kBoxPad * 2.0f, srcBox.h + kBoxPad * 2.0f);
    const Rect dstBox(toNode.pos.x - kBoxPad, toNode.pos.y - kBoxPad, toNode.size.x + kBoxPad * 2.0f, toNode.size.y + kBoxPad * 2.0f);

    constexpr float kStub = 20.0f;
    Vec2 s0{p0.x + d0.x * kStub, p0.y + d0.y * kStub};
    if (fromSide == BpmnPortSide::Left)   s0.x = std::min(s0.x, srcBox.x - 4.0f);
    if (fromSide == BpmnPortSide::Right)  s0.x = std::max(s0.x, srcBox.Right() + 4.0f);
    if (fromSide == BpmnPortSide::Top)    s0.y = std::min(s0.y, srcBox.y - 4.0f);
    if (fromSide == BpmnPortSide::Bottom) s0.y = std::max(s0.y, srcBox.Bottom() + 4.0f);

    Vec2 s1{p1.x + d1.x * kStub, p1.y + d1.y * kStub};
    if (toSide == BpmnPortSide::Left)   s1.x = std::min(s1.x, dstBox.x - 4.0f);
    if (toSide == BpmnPortSide::Right)  s1.x = std::max(s1.x, dstBox.Right() + 4.0f);
    if (toSide == BpmnPortSide::Top)    s1.y = std::min(s1.y, dstBox.y - 4.0f);
    if (toSide == BpmnPortSide::Bottom) s1.y = std::max(s1.y, dstBox.Bottom() + 4.0f);

    auto segHitsRect = [](Vec2 a, Vec2 b, const Rect& r) noexcept -> bool {
        const float minX = std::min(a.x, b.x);
        const float maxX = std::max(a.x, b.x);
        const float minY = std::min(a.y, b.y);
        const float maxY = std::max(a.y, b.y);
        return (maxX > r.x + 0.5f && minX < r.Right() - 0.5f &&
                maxY > r.y + 0.5f && minY < r.Bottom() - 0.5f);
    };

    auto isPathClear = [&](std::initializer_list<Vec2> chain, bool checkOtherNodes = true) -> bool {
        Vec2 prev{};
        bool first = true;
        for (Vec2 pt : chain) {
            if (!first) {
                if (segHitsRect(prev, pt, srcBox) || segHitsRect(prev, pt, dstBox)) {
                    return false;
                }
                if (checkOtherNodes) {
                    for (const auto& other : diagram.nodes) {
                        if (other.id == fromNode.id || other.id == toNode.id ||
                            other.id == fromNode.attachedToNodeId || other.type == BpmnNodeType::BoundaryEvent) {
                            continue;
                        }
                        const Rect ob(other.pos.x - 6.0f, other.pos.y - 6.0f, other.size.x + 12.0f, other.size.y + 12.0f);
                        if (segHitsRect(prev, pt, ob)) {
                            return false;
                        }
                    }
                }
            }
            prev = pt;
            first = false;
        }
        return true;
    };

    auto adjustMidYClearOfLanes = [&](float yVal) -> float {
        for (const auto& lane : diagram.lanes) {
            if (std::abs(yVal - lane.rect.y) < 16.0f) return lane.rect.y - 20.0f;
            if (std::abs(yVal - lane.rect.Bottom()) < 16.0f) return lane.rect.Bottom() - 20.0f;
        }
        return yVal;
    };

    const bool fromHoriz = (fromSide == BpmnPortSide::Left || fromSide == BpmnPortSide::Right);
    const bool toHoriz = (toSide == BpmnPortSide::Left || toSide == BpmnPortSide::Right);

    if (fromHoriz && toHoriz) {
        const bool facing =
            (fromSide == BpmnPortSide::Right && toSide == BpmnPortSide::Left && s0.x <= s1.x + 4.0f) ||
            (fromSide == BpmnPortSide::Left && toSide == BpmnPortSide::Right && s0.x >= s1.x - 4.0f);

        if (facing && std::abs(p0.y - p1.y) <= 1.5f && isPathClear({s0, s1})) {
            // Direct horizontal line
        } else {
            bool routed = false;
            if (facing) {
                const std::array<float, 5> candMidX = {
                    (s0.x + s1.x) * 0.5f,
                    s0.x,
                    s1.x,
                    srcBox.Right() + 14.0f,
                    dstBox.x - 14.0f
                };
                for (float mx : candMidX) {
                    if ((mx - p0.x) * d0.x >= 10.0f && (mx - p1.x) * d1.x >= 10.0f &&
                        isPathClear({s0, {mx, p0.y}, {mx, p1.y}, s1})) {
                        pts.push_back({mx, p0.y});
                        pts.push_back({mx, p1.y});
                        routed = true;
                        break;
                    }
                }
            }
            if (!routed) {
                const float detourY = (p1.y >= p0.y)
                    ? std::max(srcBox.Bottom(), dstBox.Bottom()) + 16.0f
                    : std::min(srcBox.y, dstBox.y) - 16.0f;
                pts.push_back(s0);
                pts.push_back({s0.x, detourY});
                pts.push_back({s1.x, detourY});
                pts.push_back(s1);
            }
        }
    } else if (!fromHoriz && !toHoriz) {
        const bool facing =
            (fromSide == BpmnPortSide::Bottom && toSide == BpmnPortSide::Top && s0.y <= s1.y + 4.0f) ||
            (fromSide == BpmnPortSide::Top && toSide == BpmnPortSide::Bottom && s0.y >= s1.y - 4.0f);

        if (facing && std::abs(p0.x - p1.x) <= 1.5f && isPathClear({s0, s1})) {
            // Direct vertical line
        } else {
            bool routed = false;
            if (facing) {
                const float midY = adjustMidYClearOfLanes((s0.y + s1.y) * 0.5f);
                if (isPathClear({s0, {p0.x, midY}, {p1.x, midY}, s1})) {
                    pts.push_back({p0.x, midY});
                    pts.push_back({p1.x, midY});
                    routed = true;
                }
            }
            if (!routed) {
                pts.push_back(s0);
                const std::array<float, 6> candX = {
                    s1.x,
                    s0.x,
                    (srcBox.Right() + dstBox.x) * 0.5f,
                    srcBox.Right() + 16.0f,
                    std::max(srcBox.Right(), dstBox.Right()) + 16.0f,
                    std::min(srcBox.x, dstBox.x) - 16.0f
                };
                for (int pass = 0; pass < 2 && !routed; ++pass) {
                    for (float dx : candX) {
                        if (isPathClear({s0, {dx, s0.y}, {dx, s1.y}, s1}, pass == 0)) {
                            pts.push_back({dx, s0.y});
                            pts.push_back({dx, s1.y});
                            pts.push_back(s1);
                            routed = true;
                            break;
                        }
                    }
                }
                if (!routed) {
                    const float detourX = std::max(srcBox.Right(), dstBox.Right()) + 18.0f;
                    pts.push_back({detourX, s0.y});
                    pts.push_back({detourX, s1.y});
                    pts.push_back(s1);
                }
            }
        }
    } else if (!fromHoriz && toHoriz) {
        // Vertical (Top/Bottom) -> Horizontal (Left/Right)
        const Vec2 corner{p0.x, p1.y};
        const bool forwardFrom = ((corner.y - p0.y) * d0.y >= 12.0f);
        const bool forwardTo = ((corner.x - p1.x) * d1.x >= 12.0f);

        if (forwardFrom && forwardTo && isPathClear({s0, corner, s1})) {
            pts.push_back(corner);
        } else {
            pts.push_back(s0);
            bool routed = false;
            const std::array<float, 6> candMidX = {
                (srcBox.Right() + dstBox.x) * 0.5f,
                srcBox.Right() + 14.0f,
                s1.x,
                srcBox.x - 14.0f,
                std::max(srcBox.Right(), dstBox.Right()) + 18.0f,
                std::min(srcBox.x, dstBox.x) - 18.0f
            };
            for (int pass = 0; pass < 2 && !routed; ++pass) {
                for (float mx : candMidX) {
                    if ((mx - p1.x) * d1.x >= 10.0f &&
                        isPathClear({s0, {mx, s0.y}, {mx, p1.y}, s1}, pass == 0)) {
                        pts.push_back({mx, s0.y});
                        pts.push_back({mx, p1.y});
                        routed = true;
                        break;
                    }
                }
            }
            if (!routed) {
                const float midY = adjustMidYClearOfLanes((s0.y + p1.y) * 0.5f);
                pts.push_back({s0.x, midY});
                pts.push_back({s1.x, midY});
                pts.push_back(s1);
            }
        }
    } else {
        // Horizontal (Left/Right) -> Vertical (Top/Bottom)
        const Vec2 corner{p1.x, p0.y};
        const bool forwardFrom = ((corner.x - p0.x) * d0.x >= 12.0f);
        const bool forwardTo = ((corner.y - p1.y) * d1.y >= 12.0f);

        if (forwardFrom && forwardTo && isPathClear({s0, corner, s1})) {
            pts.push_back(corner);
        } else {
            pts.push_back(s0);
            bool routed = false;
            const std::array<float, 5> candMidY = {
                adjustMidYClearOfLanes((s0.y + s1.y) * 0.5f),
                s1.y,
                srcBox.Bottom() + 14.0f,
                srcBox.y - 14.0f,
                std::max(srcBox.Bottom(), dstBox.Bottom()) + 18.0f
            };
            for (int pass = 0; pass < 2 && !routed; ++pass) {
                for (float my : candMidY) {
                    if ((my - p1.y) * d1.y >= 10.0f &&
                        isPathClear({s0, {s0.x, my}, {p1.x, my}, s1}, pass == 0)) {
                        pts.push_back({s0.x, my});
                        pts.push_back({p1.x, my});
                        routed = true;
                        break;
                    }
                }
            }
            if (!routed) {
                const float midX = (s0.x + s1.x) * 0.5f;
                pts.push_back({midX, s0.y});
                pts.push_back({midX, s1.y});
                pts.push_back(s1);
            }
        }
    }

    pts.push_back(p1);
    SimplifyOrthogonalPoints(pts);
    return pts;
}

[[nodiscard]] std::vector<Vec2> BuildRoundedPolyline(const std::vector<Vec2>& pts, float cornerRadius) {
    if (pts.size() < 3 || cornerRadius <= 1.0f) return pts;
    std::vector<Vec2> out;
    out.reserve(pts.size() * 4);
    out.push_back(pts.front());

    for (size_t i = 1; i + 1 < pts.size(); ++i) {
        const Vec2 prev = pts[i - 1];
        const Vec2 curr = pts[i];
        const Vec2 next = pts[i + 1];

        const float dx1 = curr.x - prev.x;
        const float dy1 = curr.y - prev.y;
        const float len1 = std::hypot(dx1, dy1);

        const float dx2 = next.x - curr.x;
        const float dy2 = next.y - curr.y;
        const float len2 = std::hypot(dx2, dy2);

        if (len1 < 3.0f || len2 < 3.0f) {
            out.push_back(curr);
            continue;
        }

        const float r = std::min({cornerRadius, len1 * 0.42f, len2 * 0.42f});
        const Vec2 u1{dx1 / len1, dy1 / len1};
        const Vec2 u2{dx2 / len2, dy2 / len2};

        const Vec2 pEnter{curr.x - u1.x * r, curr.y - u1.y * r};
        const Vec2 pExit{curr.x + u2.x * r, curr.y + u2.y * r};

        out.push_back(pEnter);
        constexpr int kArcSteps = 4;
        for (int s = 1; s < kArcSteps; ++s) {
            const float t = static_cast<float>(s) / static_cast<float>(kArcSteps);
            const float omt = 1.0f - t;
            out.push_back({
                omt * omt * pEnter.x + 2.0f * omt * t * curr.x + t * t * pExit.x,
                omt * omt * pEnter.y + 2.0f * omt * t * curr.y + t * t * pExit.y
            });
        }
        out.push_back(pExit);
    }

    out.push_back(pts.back());
    return out;
}

[[nodiscard]] float PointToSegmentDistanceSq(Vec2 p, Vec2 a, Vec2 b) noexcept {
    const float vx = b.x - a.x;
    const float vy = b.y - a.y;
    const float lenSq = vx * vx + vy * vy;
    if (lenSq <= 1e-4f) {
        const float dx = p.x - a.x;
        const float dy = p.y - a.y;
        return dx * dx + dy * dy;
    }
    const float t = std::clamp(((p.x - a.x) * vx + (p.y - a.y) * vy) / lenSq, 0.0f, 1.0f);
    const float projX = a.x + t * vx;
    const float projY = a.y + t * vy;
    const float dx = p.x - projX;
    const float dy = p.y - projY;
    return dx * dx + dy * dy;
}

[[nodiscard]] Vec2 SamplePolylineAtProgress(
    const std::vector<Vec2>& pts,
    float progress,
    Vec2* outMidpoint = nullptr,
    bool* outHorizontalSeg = nullptr
) noexcept {
    if (pts.empty()) return {0.0f, 0.0f};
    if (pts.size() == 1) {
        if (outMidpoint) *outMidpoint = pts[0];
        if (outHorizontalSeg) *outHorizontalSeg = true;
        return pts[0];
    }
    float totalLen = 0.0f;
    size_t longestIdx = 0;
    float longestSeg = -1.0f;
    for (size_t i = 0; i + 1 < pts.size(); ++i) {
        const float seg = std::hypot(pts[i + 1].x - pts[i].x, pts[i + 1].y - pts[i].y);
        totalLen += seg;
        if (seg > longestSeg) {
            longestSeg = seg;
            longestIdx = i;
        }
    }
    if (totalLen <= 1e-4f) {
        if (outMidpoint) *outMidpoint = pts[0];
        if (outHorizontalSeg) *outHorizontalSeg = true;
        return pts[0];
    }

    if (outMidpoint) {
        *outMidpoint = {
            (pts[longestIdx].x + pts[longestIdx + 1].x) * 0.5f,
            (pts[longestIdx].y + pts[longestIdx + 1].y) * 0.5f
        };
    }
    if (outHorizontalSeg) {
        *outHorizontalSeg = std::abs(pts[longestIdx + 1].x - pts[longestIdx].x) >=
                            std::abs(pts[longestIdx + 1].y - pts[longestIdx].y);
    }

    const float targetDist = std::clamp(progress, 0.0f, 1.0f) * totalLen;
    float accum = 0.0f;
    for (size_t i = 0; i + 1 < pts.size(); ++i) {
        const float seg = std::hypot(pts[i + 1].x - pts[i].x, pts[i + 1].y - pts[i].y);
        if (accum + seg >= targetDist) {
            const float localT = (seg > 1e-4f) ? (targetDist - accum) / seg : 0.0f;
            return {
                pts[i].x + (pts[i + 1].x - pts[i].x) * localT,
                pts[i].y + (pts[i + 1].y - pts[i].y) * localT
            };
        }
        accum += seg;
    }
    return pts.back();
}

// ============================================================================
// Vector Drawing Primitives for BPMN 2.0 Symbols
// ============================================================================

void DrawDashedLine(ICanvas& canvas, Vec2 a, Vec2 b, Color col, float width, float dashLen, float gapLen) {
    const float dx = b.x - a.x;
    const float dy = b.y - a.y;
    const float len = std::hypot(dx, dy);
    if (len <= 0.5f) return;
    const float ux = dx / len;
    const float uy = dy / len;
    const float step = std::max(2.0f, dashLen + gapLen);
    for (float d = 0.0f; d < len; d += step) {
        const float dEnd = std::min(len, d + dashLen);
        canvas.DrawLine(
            {a.x + ux * d, a.y + uy * d},
            {a.x + ux * dEnd, a.y + uy * dEnd},
            col,
            width
        );
    }
}

void DrawDashedCircle(ICanvas& canvas, Vec2 center, float r, Color stroke, float strokeWidth, int segments = 16) {
    for (int i = 0; i < segments; ++i) {
        const float a0 = (static_cast<float>(i) * 2.0f * kPi) / static_cast<float>(segments);
        const float a1 = ((static_cast<float>(i) + 0.58f) * 2.0f * kPi) / static_cast<float>(segments);
        canvas.DrawLine(
            {center.x + std::cos(a0) * r, center.y + std::sin(a0) * r},
            {center.x + std::cos(a1) * r, center.y + std::sin(a1) * r},
            stroke,
            strokeWidth
        );
    }
}

void DrawDiamond(ICanvas& canvas, Vec2 center, float halfW, float halfH, Color fill, Color stroke, float strokeWidth) {
    const Vec2 top{center.x, center.y - halfH};
    const Vec2 right{center.x + halfW, center.y};
    const Vec2 bottom{center.x, center.y + halfH};
    const Vec2 left{center.x - halfW, center.y};

    if (fill.a > 0) {
        canvas.DrawTriangle(top, right, bottom, fill);
        canvas.DrawTriangle(top, bottom, left, fill);
    }
    if (stroke.a > 0 && strokeWidth > 0.0f) {
        canvas.DrawLine(top, right, stroke, strokeWidth);
        canvas.DrawLine(right, bottom, stroke, strokeWidth);
        canvas.DrawLine(bottom, left, stroke, strokeWidth);
        canvas.DrawLine(left, top, stroke, strokeWidth);
    }
}

void DrawEventDefinitionSymbol(
    ICanvas& canvas,
    Vec2 center,
    float radius,
    BpmnEventDefinition def,
    bool filledThrow,
    Color strokeColor,
    Color bgColor,
    float s
) {
    const float sw = std::max(1.2f, 1.4f * s);
    switch (def) {
        case BpmnEventDefinition::None:
            break;
        case BpmnEventDefinition::Message: {
            const float ew = radius * 1.02f;
            const float eh = radius * 0.70f;
            const Rect envRect(center.x - ew * 0.5f, center.y - eh * 0.5f, ew, eh);
            canvas.DrawRoundRect(
                envRect,
                1.5f * s,
                filledThrow ? strokeColor : Color(0, 0, 0, 0),
                strokeColor,
                sw
            );
            const Color flapCol = filledThrow ? bgColor : strokeColor;
            canvas.DrawLine({envRect.x, envRect.y}, {center.x, center.y + eh * 0.06f}, flapCol, sw);
            canvas.DrawLine({envRect.Right(), envRect.y}, {center.x, center.y + eh * 0.06f}, flapCol, sw);
            break;
        }
        case BpmnEventDefinition::Timer: {
            const float cr = radius * 0.54f;
            canvas.DrawRoundRect(
                Rect(center.x - cr, center.y - cr, cr * 2.0f, cr * 2.0f),
                cr,
                Color(0, 0, 0, 0),
                strokeColor,
                sw
            );
            for (int i = 0; i < 4; ++i) {
                const float ang = static_cast<float>(i) * (kPi * 0.5f);
                canvas.DrawLine(
                    {center.x + std::cos(ang) * (cr * 0.72f), center.y + std::sin(ang) * (cr * 0.72f)},
                    {center.x + std::cos(ang) * cr, center.y + std::sin(ang) * cr},
                    strokeColor,
                    sw
                );
            }
            canvas.DrawLine(center, {center.x, center.y - cr * 0.58f}, strokeColor, sw);
            canvas.DrawLine(center, {center.x + cr * 0.45f, center.y + cr * 0.12f}, strokeColor, sw);
            break;
        }
        case BpmnEventDefinition::Error: {
            const float r = radius * 0.50f;
            const Vec2 p0{center.x - r * 0.65f, center.y + r * 0.65f};
            const Vec2 p1{center.x - r * 0.15f, center.y - r * 0.65f};
            const Vec2 p2{center.x + r * 0.20f, center.y + r * 0.18f};
            const Vec2 p3{center.x + r * 0.70f, center.y - r * 0.65f};
            canvas.DrawLine(p0, p1, strokeColor, sw * 1.25f);
            canvas.DrawLine(p1, p2, strokeColor, sw * 1.25f);
            canvas.DrawLine(p2, p3, strokeColor, sw * 1.25f);
            break;
        }
        case BpmnEventDefinition::Signal: {
            const float r = radius * 0.50f;
            const Vec2 p0{center.x, center.y - r};
            const Vec2 p1{center.x + r * 0.92f, center.y + r * 0.72f};
            const Vec2 p2{center.x - r * 0.92f, center.y + r * 0.72f};
            canvas.DrawTriangle(
                p0, p1, p2,
                filledThrow ? strokeColor : Color(0, 0, 0, 0),
                strokeColor,
                sw
            );
            break;
        }
        case BpmnEventDefinition::Escalation: {
            const float r = radius * 0.48f;
            const Vec2 top{center.x, center.y - r};
            const Vec2 right{center.x + r * 0.75f, center.y + r * 0.68f};
            const Vec2 mid{center.x, center.y + r * 0.15f};
            const Vec2 left{center.x - r * 0.75f, center.y + r * 0.68f};
            canvas.DrawLine(top, right, strokeColor, sw * 1.2f);
            canvas.DrawLine(right, mid, strokeColor, sw * 1.2f);
            canvas.DrawLine(mid, left, strokeColor, sw * 1.2f);
            canvas.DrawLine(left, top, strokeColor, sw * 1.2f);
            break;
        }
        case BpmnEventDefinition::Terminate: {
            const float tr = radius * 0.54f;
            canvas.DrawRoundRect(
                Rect(center.x - tr, center.y - tr, tr * 2.0f, tr * 2.0f),
                tr,
                strokeColor
            );
            break;
        }
    }
}

void DrawCogwheel(ICanvas& canvas, Vec2 c, float r, Color col, Color bgCol, float sw) {
    for (int i = 0; i < 4; ++i) {
        const float ang = static_cast<float>(i) * (kPi * 0.25f);
        canvas.DrawLine(
            {c.x - std::cos(ang) * r, c.y - std::sin(ang) * r},
            {c.x + std::cos(ang) * r, c.y + std::sin(ang) * r},
            col,
            sw * 1.3f
        );
    }
    const float outerR = r * 0.68f;
    const float innerR = r * 0.30f;
    canvas.DrawRoundRect(Rect(c.x - outerR, c.y - outerR, outerR * 2.0f, outerR * 2.0f), outerR, bgCol, col, sw);
    canvas.DrawRoundRect(Rect(c.x - innerR, c.y - innerR, innerR * 2.0f, innerR * 2.0f), innerR, bgCol, col, sw * 0.85f);
}

void DrawTaskTypeMarker(ICanvas& canvas, Rect iconRect, BpmnTaskType taskType, Color col, Color bgCol, float s) {
    const Vec2 c = iconRect.Center();
    const float r = std::min(iconRect.w, iconRect.h) * 0.46f;
    const float sw = std::max(1.15f, 1.35f * s);

    switch (taskType) {
        case BpmnTaskType::None:
        case BpmnTaskType::CallActivity:
            break;
        case BpmnTaskType::User: {
            const float hr = r * 0.42f;
            canvas.DrawRoundRect(
                Rect(c.x - hr, c.y - r * 0.82f, hr * 2.0f, hr * 2.0f),
                hr,
                bgCol,
                col,
                sw
            );
            const Rect bust(c.x - r * 0.84f, c.y + r * 0.08f, r * 1.68f, r * 0.80f);
            canvas.DrawRoundRect(bust, r * 0.38f, bgCol, col, sw);
            canvas.DrawLine({c.x - r * 0.32f, bust.y + bust.h * 0.35f}, {c.x - r * 0.32f, bust.Bottom()}, col, sw * 0.85f);
            canvas.DrawLine({c.x + r * 0.32f, bust.y + bust.h * 0.35f}, {c.x + r * 0.32f, bust.Bottom()}, col, sw * 0.85f);
            break;
        }
        case BpmnTaskType::Service: {
            // Signature `bpmn-js` two interlocking gears
            DrawCogwheel(canvas, {c.x - r * 0.28f, c.y - r * 0.28f}, r * 0.58f, col, bgCol, sw * 0.9f);
            DrawCogwheel(canvas, {c.x + r * 0.22f, c.y + r * 0.22f}, r * 0.72f, col, bgCol, sw);
            break;
        }
        case BpmnTaskType::Send:
        case BpmnTaskType::Receive: {
            const bool filled = (taskType == BpmnTaskType::Send);
            const Rect er(c.x - r * 0.92f, c.y - r * 0.62f, r * 1.84f, r * 1.24f);
            canvas.DrawRoundRect(er, 1.5f * s, filled ? col : bgCol, col, sw);
            const Color flap = filled ? bgCol : col;
            canvas.DrawLine({er.x, er.y}, {c.x, c.y + r * 0.08f}, flap, sw);
            canvas.DrawLine({er.Right(), er.y}, {c.x, c.y + r * 0.08f}, flap, sw);
            break;
        }
        case BpmnTaskType::Script: {
            const Rect sr(c.x - r * 0.72f, c.y - r * 0.85f, r * 1.44f, r * 1.70f);
            canvas.DrawRoundRect(sr, 2.5f * s, bgCol, col, sw);
            for (int i = 0; i < 3; ++i) {
                const float ly = sr.y + sr.h * (0.28f + static_cast<float>(i) * 0.22f);
                canvas.DrawLine({sr.x + sr.w * 0.22f, ly}, {sr.Right() - sr.w * 0.22f, ly}, col, sw);
            }
            break;
        }
        case BpmnTaskType::Manual: {
            const Rect palm(c.x - r * 0.78f, c.y - r * 0.45f, r * 1.56f, r * 0.95f);
            canvas.DrawRoundRect(palm, 2.5f * s, bgCol, col, sw);
            canvas.DrawLine({palm.x + palm.w * 0.33f, palm.y}, {palm.x + palm.w * 0.33f, palm.Bottom()}, col, sw);
            canvas.DrawLine({palm.x + palm.w * 0.66f, palm.y}, {palm.x + palm.w * 0.66f, palm.Bottom()}, col, sw);
            break;
        }
        case BpmnTaskType::BusinessRule: {
            const Rect tbl(c.x - r * 0.88f, c.y - r * 0.68f, r * 1.76f, r * 1.36f);
            canvas.DrawRoundRect(tbl, 1.5f * s, bgCol, col, sw);
            canvas.DrawRect(Rect(tbl.x, tbl.y, tbl.w, tbl.h * 0.34f), col.WithAlpha(85));
            canvas.DrawLine({tbl.x, tbl.y + tbl.h * 0.34f}, {tbl.Right(), tbl.y + tbl.h * 0.34f}, col, sw);
            canvas.DrawLine({tbl.x, tbl.y + tbl.h * 0.67f}, {tbl.Right(), tbl.y + tbl.h * 0.67f}, col, sw);
            canvas.DrawLine({tbl.x + tbl.w * 0.38f, tbl.y + tbl.h * 0.34f}, {tbl.x + tbl.w * 0.38f, tbl.Bottom()}, col, sw);
            break;
        }
    }
}

void DrawGatewayMarker(ICanvas& canvas, Vec2 center, float halfSize, BpmnGatewayType gwType, Color col, float s) {
    const float sw = std::max(1.9f, 2.4f * s);
    switch (gwType) {
        case BpmnGatewayType::Exclusive: {
            const float d = halfSize * 0.34f;
            canvas.DrawLine({center.x - d, center.y - d}, {center.x + d, center.y + d}, col, sw);
            canvas.DrawLine({center.x + d, center.y - d}, {center.x - d, center.y + d}, col, sw);
            break;
        }
        case BpmnGatewayType::Parallel: {
            const float d = halfSize * 0.42f;
            canvas.DrawLine({center.x - d, center.y}, {center.x + d, center.y}, col, sw);
            canvas.DrawLine({center.x, center.y - d}, {center.x, center.y + d}, col, sw);
            break;
        }
        case BpmnGatewayType::Inclusive: {
            const float r = halfSize * 0.40f;
            canvas.DrawRoundRect(
                Rect(center.x - r, center.y - r, r * 2.0f, r * 2.0f),
                r,
                Color(0, 0, 0, 0),
                col,
                sw
            );
            break;
        }
        case BpmnGatewayType::EventBased: {
            const float r1 = halfSize * 0.50f;
            const float r2 = halfSize * 0.39f;
            const float swThin = std::max(1.1f, 1.3f * s);
            canvas.DrawRoundRect(Rect(center.x - r1, center.y - r1, r1 * 2.0f, r1 * 2.0f), r1, Color(0, 0, 0, 0), col, swThin);
            canvas.DrawRoundRect(Rect(center.x - r2, center.y - r2, r2 * 2.0f, r2 * 2.0f), r2, Color(0, 0, 0, 0), col, swThin);
            const float pr = halfSize * 0.24f;
            for (int i = 0; i < 5; ++i) {
                const float a0 = -kPi * 0.5f + static_cast<float>(i) * (2.0f * kPi / 5.0f);
                const float a1 = -kPi * 0.5f + static_cast<float>((i + 1) % 5) * (2.0f * kPi / 5.0f);
                canvas.DrawLine(
                    {center.x + std::cos(a0) * pr, center.y + std::sin(a0) * pr},
                    {center.x + std::cos(a1) * pr, center.y + std::sin(a1) * pr},
                    col,
                    swThin
                );
            }
            break;
        }
    }
}

[[nodiscard]] bool HasPortConnection(const BpmnDiagram& diagram, const BpmnNode& node, BpmnPortSide targetSide) noexcept {
    for (const auto& e : diagram.edges) {
        if (e.fromNodeId != node.id && e.toNodeId != node.id) continue;
        const BpmnNode* fn = diagram.FindNode(e.fromNodeId);
        const BpmnNode* tn = diagram.FindNode(e.toNodeId);
        if (!fn || !tn) continue;
        BpmnPortSide fs = BpmnPortSide::Right;
        BpmnPortSide ts = BpmnPortSide::Left;
        ResolveAutoPorts(*fn, *tn, e.fromPort, e.toPort, fs, ts);
        if (e.fromNodeId == node.id && fs == targetSide) return true;
        if (e.toNodeId == node.id && ts == targetSide) return true;
    }
    return false;
}

[[nodiscard]] Rect ComputeExternalNodeLabelScreenRect(
    const BpmnDiagram& diagram,
    const BpmnNode& node,
    const Rect& sr,
    float zoom,
    float s,
    float* outAlignX = nullptr,
    float* outAlignY = nullptr
) noexcept {
    const float zs = std::clamp(zoom, 0.42f, 2.0f) * s;
    const Vec2 center = sr.Center();
    const float offX = node.labelOffset.x * zoom;
    const float offY = node.labelOffset.y * zoom;

    if (node.type == BpmnNodeType::BoundaryEvent) {
        if (outAlignX) *outAlignX = 0.5f;
        if (outAlignY) *outAlignY = 0.5f;
        return Rect(sr.Right() + 4.0f * zs + offX, center.y + 1.0f * zs + offY, 84.0f * zs, 18.0f * zs);
    }

    const bool bottomBusy = HasPortConnection(diagram, node, BpmnPortSide::Bottom);
    const bool topBusy = HasPortConnection(diagram, node, BpmnPortSide::Top);

    Rect lblRect(center.x - 64.0f * zs, sr.Bottom() + 4.0f * zs, 128.0f * zs, 28.0f * zs);
    float alignX = 0.5f;
    float alignY = 0.0f;

    if (bottomBusy && !topBusy) {
        lblRect = Rect(center.x - 64.0f * zs, sr.y - 30.0f * zs, 128.0f * zs, 26.0f * zs);
        alignY = 1.0f;
    } else if (bottomBusy && topBusy) {
        lblRect = Rect(sr.x - 94.0f * zs, sr.Bottom() - 2.0f * zs, 88.0f * zs, 28.0f * zs);
        alignX = 1.0f;
        alignY = 0.0f;
    }

    lblRect.x += offX;
    lblRect.y += offY;
    if (outAlignX) *outAlignX = alignX;
    if (outAlignY) *outAlignY = alignY;
    return lblRect;
}

[[nodiscard]] bool IsExternalLabelNode(BpmnNodeType type) noexcept {
    return type == BpmnNodeType::StartEvent ||
           type == BpmnNodeType::IntermediateEvent ||
           type == BpmnNodeType::BoundaryEvent ||
           type == BpmnNodeType::EndEvent ||
           type == BpmnNodeType::Gateway ||
           type == BpmnNodeType::DataObject ||
           type == BpmnNodeType::DataStore;
}

[[nodiscard]] bool IsResizableNode(BpmnNodeType type) noexcept {
    return type == BpmnNodeType::Task ||
           type == BpmnNodeType::SubProcess ||
           type == BpmnNodeType::TextAnnotation;
}

[[nodiscard]] std::array<Rect, 4> GetNodeResizeHandleRects(const Rect& sr, float zoom, float s) noexcept {
    const float zs = std::clamp(zoom, 0.35f, 2.2f) * s;
    const float pad = 5.0f * zs;
    const Rect outline(sr.x - pad, sr.y - pad, sr.w + pad * 2.0f, sr.h + pad * 2.0f);
    const float hs = 6.5f * s;
    const float hh = hs * 0.5f;
    return {{
        Rect(outline.x - hh, outline.y - hh, hs, hs),             // 0: NW
        Rect(outline.Right() - hh, outline.y - hh, hs, hs),       // 1: NE
        Rect(outline.Right() - hh, outline.Bottom() - hh, hs, hs),// 2: SE
        Rect(outline.x - hh, outline.Bottom() - hh, hs, hs)       // 3: SW
    }};
}

void DrawExternalNodeLabel(
    ICanvas& canvas,
    const BpmnDiagram& diagram,
    const BpmnNode& node,
    const Rect& sr,
    float zoom,
    float s,
    const BpmnPalette& pal,
    bool isNodeSelected = false,
    bool isLabelHovered = false
) {
    if (node.label.empty() || zoom < 0.42f) return;
    const float zs = std::clamp(zoom, 0.42f, 2.0f) * s;
    const float fs = std::clamp(11.0f * zs, 8.5f * s, 13.5f * s);
    float alignX = 0.5f;
    float alignY = 0.0f;
    Rect lblRect = ComputeExternalNodeLabelScreenRect(diagram, node, sr, zoom, s, &alignX, &alignY);

    if (node.type == BpmnNodeType::BoundaryEvent) {
        const Vec2 tm = canvas.MeasureText(node.label, fs);
        lblRect.w = tm.x + 8.0f * zs;
        lblRect.h = tm.y + 4.0f * zs;
        canvas.DrawRoundRect(lblRect, 3.0f * zs, pal.canvasBg.WithAlpha(225));
        canvas.DrawTextBox(node.label, lblRect, fs, pal.textPrimary, 0.5f, 0.5f);
    } else {
        DrawWrappedTextBox(canvas, node.label, lblRect, fs, pal.textPrimary, FontStyle::Regular, alignX, alignY, 2);
    }

    if (isLabelHovered || (isNodeSelected && (std::abs(node.labelOffset.x) > 0.5f || std::abs(node.labelOffset.y) > 0.5f))) {
        const Color c = isLabelHovered ? pal.selectAccent : pal.hoverAccent.WithAlpha(160);
        const float sw = std::max(1.0f, 1.15f * s);
        DrawDashedLine(canvas, {lblRect.x, lblRect.y}, {lblRect.Right(), lblRect.y}, c, sw, 3.5f * s, 2.5f * s);
        DrawDashedLine(canvas, {lblRect.Right(), lblRect.y}, {lblRect.Right(), lblRect.Bottom()}, c, sw, 3.5f * s, 2.5f * s);
        DrawDashedLine(canvas, {lblRect.Right(), lblRect.Bottom()}, {lblRect.x, lblRect.Bottom()}, c, sw, 3.5f * s, 2.5f * s);
        DrawDashedLine(canvas, {lblRect.x, lblRect.Bottom()}, {lblRect.x, lblRect.y}, c, sw, 3.5f * s, 2.5f * s);
    }
}

void DrawNodeVisual(
    ICanvas& canvas,
    const BpmnDiagram& diagram,
    const BpmnNode& node,
    const Rect& sr,
    float zoom,
    float s,
    const BpmnPalette& pal,
    bool isSelected,
    bool isHovered,
    bool simulationActive,
    bool showSlaHeatmap,
    float animPhase,
    bool isLabelHovered = false,
    bool showResizeHandles = false,
    int hoveredResizeCorner = -1
) {
    const float zs = std::clamp(zoom, 0.35f, 2.2f) * s;
    const Vec2 center = sr.Center();

    // Optional SLA Heatmap halo on activities
    if (showSlaHeatmap && (node.type == BpmnNodeType::Task || node.type == BpmnNodeType::SubProcess)) {
        const float slaNorm = std::clamp((node.slaDurationMs - 100.0f) / 1400.0f, 0.0f, 1.0f);
        const Color heatCol = (slaNorm > 0.66f)
            ? Color(244, 63, 94, 58)
            : ((slaNorm > 0.33f) ? Color(245, 158, 11, 52) : Color(16, 185, 129, 44));
        const float exp = 6.0f * zs;
        canvas.DrawRoundRect(
            Rect(sr.x - exp, sr.y - exp, sr.w + exp * 2.0f, sr.h + exp * 2.0f),
            12.0f * zs,
            heatCol
        );
    }

    // Active Token simulation pulse
    if (node.activeTokensInside > 0) {
        const float pad = (4.0f + 2.5f * std::sin(animPhase * 2.0f * kPi)) * zs;
        const float rad = (node.type == BpmnNodeType::StartEvent || node.type == BpmnNodeType::IntermediateEvent ||
                           node.type == BpmnNodeType::BoundaryEvent || node.type == BpmnNodeType::EndEvent)
            ? (sr.w * 0.5f + pad)
            : 10.0f * zs;
        canvas.DrawRoundRect(
            Rect(sr.x - pad, sr.y - pad, sr.w + pad * 2.0f, sr.h + pad * 2.0f),
            rad,
            pal.simActiveGreen.WithAlpha(80),
            pal.simActiveGreen,
            std::max(1.5f, 1.8f * s)
        );
    }

    switch (node.type) {
        case BpmnNodeType::StartEvent:
        case BpmnNodeType::IntermediateEvent:
        case BpmnNodeType::BoundaryEvent:
        case BpmnNodeType::EndEvent: {
            const float r = std::min(sr.w, sr.h) * 0.5f;
            Color fill = pal.startEventFill;
            Color stroke = pal.startEventStroke;
            float strokeW = std::max(1.6f, 2.0f * zs);

            if (node.type == BpmnNodeType::IntermediateEvent || node.type == BpmnNodeType::BoundaryEvent) {
                fill = pal.interEventFill;
                stroke = pal.interEventStroke;
                strokeW = std::max(1.4f, 1.75f * zs);
            } else if (node.type == BpmnNodeType::EndEvent) {
                fill = pal.endEventFill;
                stroke = pal.endEventStroke;
                strokeW = std::max(2.8f, 3.6f * zs);
            }
            if (node.customColor.a > 0) {
                stroke = node.customColor;
            }

            canvas.DrawRoundRect(
                Rect(center.x - r, center.y - r + 1.8f * zs, r * 2.0f, r * 2.0f),
                r,
                pal.nodeShadow
            );

            if (node.nonInterrupting && (node.type == BpmnNodeType::IntermediateEvent || node.type == BpmnNodeType::BoundaryEvent)) {
                canvas.DrawRoundRect(Rect(center.x - r, center.y - r, r * 2.0f, r * 2.0f), r, fill);
                DrawDashedCircle(canvas, center, r, stroke, strokeW, 14);
                DrawDashedCircle(canvas, center, r * 0.78f, stroke, std::max(1.1f, 1.3f * zs), 12);
            } else {
                canvas.DrawRoundRect(Rect(center.x - r, center.y - r, r * 2.0f, r * 2.0f), r, fill, stroke, strokeW);
                if (node.type == BpmnNodeType::IntermediateEvent || node.type == BpmnNodeType::BoundaryEvent) {
                    const float rInner = r * 0.78f;
                    canvas.DrawRoundRect(
                        Rect(center.x - rInner, center.y - rInner, rInner * 2.0f, rInner * 2.0f),
                        rInner,
                        Color(0, 0, 0, 0),
                        stroke,
                        std::max(1.1f, 1.35f * zs)
                    );
                }
            }

            const bool filledIcon = (node.type == BpmnNodeType::EndEvent);
            DrawEventDefinitionSymbol(canvas, center, r, node.eventDef, filledIcon, stroke, fill, zs);
            DrawExternalNodeLabel(canvas, diagram, node, sr, zoom, s, pal, isSelected, isLabelHovered);
            break;
        }

        case BpmnNodeType::Gateway: {
            const float hw = sr.w * 0.5f;
            const float hh = sr.h * 0.5f;
            const Color stroke = (node.customColor.a > 0) ? node.customColor : pal.gatewayStroke;
            DrawDiamond(canvas, {center.x, center.y + 1.8f * zs}, hw, hh, pal.nodeShadow, Color(0, 0, 0, 0), 0.0f);
            DrawDiamond(canvas, center, hw, hh, pal.gatewayFill, stroke, std::max(1.6f, 2.0f * zs));
            DrawGatewayMarker(canvas, center, std::min(hw, hh), node.gatewayType, stroke, zs);
            DrawExternalNodeLabel(canvas, diagram, node, sr, zoom, s, pal, isSelected, isLabelHovered);
            break;
        }

        case BpmnNodeType::Task:
        case BpmnNodeType::SubProcess: {
            const float rad = 10.0f * zs;
            const bool isCall = (node.type == BpmnNodeType::Task && node.taskType == BpmnTaskType::CallActivity);
            const bool isExpSub = (node.type == BpmnNodeType::SubProcess && node.isExpanded);
            const float strokeW = isCall ? std::max(2.8f, 3.6f * zs) : std::max(1.5f, 1.9f * zs);
            const Color strokeCol = (node.customColor.a > 0) ? node.customColor : pal.nodeStroke;
            const Color fillCol = isExpSub ? pal.nodeFill.WithAlpha(220) : pal.nodeFill;

            // Crisp `bpmn-js` Task / SubProcess rounded rectangle
            canvas.DrawRoundRect(Rect(sr.x, sr.y + 2.0f * zs, sr.w, sr.h), rad, pal.nodeShadow);
            canvas.DrawRoundRect(sr, rad, fillCol, strokeCol, strokeW);

            if (isExpSub) {
                const float hdrH = 26.0f * zs;
                canvas.DrawLine({sr.x, sr.y + hdrH}, {sr.Right(), sr.y + hdrH}, strokeCol.WithAlpha(90), 1.0f * s);
                if (!node.label.empty() && zoom >= 0.35f) {
                    const float fs = std::clamp(11.5f * zs, 8.5f * s, 13.5f * s);
                    canvas.DrawTextBox(node.label, Rect(sr.x + 10.0f * zs, sr.y + 3.0f * zs, sr.w - 20.0f * zs, hdrH - 4.0f * zs), fs, pal.textPrimary, 0.5f, 0.5f);
                }
            } else {
                // Top-left BPMN 2.0 Task Type Marker Icon
                if (node.type == BpmnNodeType::Task && node.taskType != BpmnTaskType::None) {
                    const Rect iconRect(sr.x + 6.5f * zs, sr.y + 6.0f * zs, 16.0f * zs, 16.0f * zs);
                    DrawTaskTypeMarker(canvas, iconRect, node.taskType, strokeCol, pal.nodeFill, zs);
                }

                // Centered multi-line wrapped Task label (`bpmn-js` style, up to 3 lines)
                if (!node.label.empty() && zoom >= 0.35f) {
                    const float fs = std::clamp(11.5f * zs, 8.5f * s, 13.5f * s);
                    const float topPad = 14.0f * zs;
                    const float botPad = (node.type == BpmnNodeType::SubProcess || node.parallelMultiInstance || node.loopMarker ||
                                          (showSlaHeatmap && !node.documentation.empty()))
                        ? 18.0f * zs
                        : 10.0f * zs;
                    const Rect textRect(
                        sr.x + 8.0f * zs,
                        sr.y + topPad,
                        std::max(16.0f, sr.w - 16.0f * zs),
                        std::max(14.0f, sr.h - topPad - botPad)
                    );
                    DrawWrappedTextBox(canvas, node.label, textRect, fs, pal.textPrimary, FontStyle::Regular, 0.5f, 0.5f, 3);
                }
            }

            // Active token progress bar or optional SLA telemetry when heatmap is enabled
            if (node.activeTokensInside > 0) {
                const Rect progBg(sr.x + 12.0f * zs, sr.Bottom() - 10.0f * zs, sr.w - 24.0f * zs, 4.0f * zs);
                canvas.DrawRoundRect(progBg, 2.0f * zs, pal.simActiveGreen.WithAlpha(55));
                const float fillW = progBg.w * (0.25f + 0.75f * std::abs(std::sin(animPhase * kPi)));
                canvas.DrawRoundRect(Rect(progBg.x, progBg.y, fillW, progBg.h), 2.0f * zs, pal.simActiveGreen);
            } else if (showSlaHeatmap && !node.documentation.empty() && zoom >= 0.55f && node.type == BpmnNodeType::Task && !node.parallelMultiInstance && !node.loopMarker) {
                const float subFs = std::clamp(9.0f * zs, 7.5f * s, 10.5f * s);
                const Rect subRect(sr.x + 8.0f * zs, sr.Bottom() - 15.0f * zs, sr.w - 16.0f * zs, 12.0f * zs);
                canvas.DrawTextBox(node.documentation, subRect, subFs, pal.textMuted, 0.5f, 0.5f);
            }

            // SubProcess [+]/[-] marker or Loop / Parallel Multi-Instance markers at bottom center
            if (node.type == BpmnNodeType::SubProcess) {
                const float ms = 13.0f * zs;
                const Rect mr(center.x - ms * 0.5f, sr.Bottom() - ms - 3.5f * zs, ms, ms);
                canvas.DrawRoundRect(mr, 2.0f * zs, pal.nodeFill, strokeCol, std::max(1.1f, 1.3f * zs));
                const Vec2 mc = mr.Center();
                canvas.DrawLine({mc.x - ms * 0.28f, mc.y}, {mc.x + ms * 0.28f, mc.y}, strokeCol, std::max(1.1f, 1.35f * zs));
                if (!isExpSub) {
                    canvas.DrawLine({mc.x, mc.y - ms * 0.28f}, {mc.x, mc.y + ms * 0.28f}, strokeCol, std::max(1.1f, 1.35f * zs));
                }
            } else if (node.parallelMultiInstance) {
                const float mh = 9.0f * zs;
                const float my = sr.Bottom() - mh - 4.0f * zs;
                for (int i = -1; i <= 1; ++i) {
                    const float mx = center.x + static_cast<float>(i) * 3.5f * zs;
                    canvas.DrawLine({mx, my}, {mx, my + mh}, strokeCol, std::max(1.2f, 1.45f * zs));
                }
            } else if (node.loopMarker) {
                const float lr = 4.5f * zs;
                const Vec2 lc{center.x, sr.Bottom() - lr - 4.0f * zs};
                canvas.DrawRoundRect(Rect(lc.x - lr, lc.y - lr, lr * 2.0f, lr * 2.0f), lr, Color(0, 0, 0, 0), strokeCol, std::max(1.1f, 1.35f * zs));
            }
            break;
        }

        case BpmnNodeType::DataObject: {
            const float fold = std::min(sr.w, sr.h) * 0.26f;
            const float sw = std::max(1.3f, 1.6f * zs);
            canvas.DrawRoundRect(sr, 2.0f * zs, pal.dataFill, pal.nodeStroke, sw);
            canvas.DrawLine({sr.Right() - fold, sr.y}, {sr.Right() - fold, sr.y + fold}, pal.nodeStroke, sw);
            canvas.DrawLine({sr.Right() - fold, sr.y + fold}, {sr.Right(), sr.y + fold}, pal.nodeStroke, sw);
            DrawExternalNodeLabel(canvas, diagram, node, sr, zoom, s, pal, isSelected, isLabelHovered);
            break;
        }

        case BpmnNodeType::DataStore: {
            const float sw = std::max(1.3f, 1.6f * zs);
            const float capH = sr.h * 0.28f;
            canvas.DrawRoundRect(sr, 6.0f * zs, pal.dataFill, pal.nodeStroke, sw);
            canvas.DrawRoundRect(Rect(sr.x, sr.y, sr.w, capH), capH * 0.5f, pal.laneHeaderBg, pal.nodeStroke, sw);
            canvas.DrawLine({sr.x, sr.y + capH * 1.45f}, {sr.Right(), sr.y + capH * 1.45f}, pal.nodeStroke.WithAlpha(160), sw * 0.85f);
            DrawExternalNodeLabel(canvas, diagram, node, sr, zoom, s, pal, isSelected, isLabelHovered);
            break;
        }

        case BpmnNodeType::TextAnnotation: {
            const float sw = std::max(1.4f, 1.8f * zs);
            const float bw = 10.0f * zs;
            canvas.DrawLine({sr.x + bw, sr.y}, {sr.x, sr.y}, pal.nodeStroke, sw);
            canvas.DrawLine({sr.x, sr.y}, {sr.x, sr.Bottom()}, pal.nodeStroke, sw);
            canvas.DrawLine({sr.x, sr.Bottom()}, {sr.x + bw, sr.Bottom()}, pal.nodeStroke, sw);
            if (!node.label.empty() && zoom >= 0.38f) {
                const float fs = std::clamp(10.5f * zs, 8.0f * s, 12.5f * s);
                const Rect tr(sr.x + 8.0f * zs, sr.y + 2.0f * zs, std::max(8.0f, sr.w - 10.0f * zs), std::max(8.0f, sr.h - 4.0f * zs));
                DrawWrappedTextBox(canvas, node.label, tr, fs, pal.textSecondary, FontStyle::Regular, 0.0f, 0.5f, 3);
            }
            break;
        }
    }

    // Authentic `bpmn-js` / `diagram-js` rectangular `djs-outline` bounding box on selection or hover
    if (isSelected || isHovered) {
        const float pad = 5.0f * zs;
        const Rect outlineRect(sr.x - pad, sr.y - pad, sr.w + pad * 2.0f, sr.h + pad * 2.0f);
        if (isSelected) {
            canvas.DrawRoundRect(
                outlineRect,
                3.0f * s,
                pal.selectGlow.WithAlpha(24),
                pal.selectAccent,
                std::max(1.4f, 1.7f * s)
            );
            if (showResizeHandles && IsResizableNode(node.type)) {
                const auto handles = GetNodeResizeHandleRects(sr, zoom, s);
                for (int c = 0; c < 4; ++c) {
                    const bool isCornerHov = (hoveredResizeCorner == c);
                    canvas.DrawRoundRect(
                        handles[static_cast<size_t>(c)],
                        1.5f * s,
                        isCornerHov ? pal.selectAccent : pal.nodeFill,
                        pal.selectAccent,
                        1.4f * s
                    );
                }
            }
        } else {
            const float osw = std::max(1.1f, 1.3f * s);
            DrawDashedLine(canvas, {outlineRect.x, outlineRect.y}, {outlineRect.Right(), outlineRect.y}, pal.hoverAccent, osw, 4.0f * s, 3.0f * s);
            DrawDashedLine(canvas, {outlineRect.Right(), outlineRect.y}, {outlineRect.Right(), outlineRect.Bottom()}, pal.hoverAccent, osw, 4.0f * s, 3.0f * s);
            DrawDashedLine(canvas, {outlineRect.Right(), outlineRect.Bottom()}, {outlineRect.x, outlineRect.Bottom()}, pal.hoverAccent, osw, 4.0f * s, 3.0f * s);
            DrawDashedLine(canvas, {outlineRect.x, outlineRect.Bottom()}, {outlineRect.x, outlineRect.y}, pal.hoverAccent, osw, 4.0f * s, 3.0f * s);
        }
    }

    // Token execution counter badge in Token Simulation mode
    if (simulationActive && (node.tokenExecutionCount > 0 || node.activeTokensInside > 0) && zoom >= 0.45f) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%u", node.tokenExecutionCount + node.activeTokensInside);
        const float bFs = std::clamp(9.5f * zs, 8.0f * s, 11.5f * s);
        const float bw = std::max(18.0f * zs, canvas.MeasureText(buf, bFs).x + 8.0f * zs);
        const float bh = 16.0f * zs;
        const Rect badgeRect(sr.Right() - bw * 0.65f, sr.y - bh * 0.45f, bw, bh);
        const Color badgeBg = (node.activeTokensInside > 0) ? pal.simActiveGreen : pal.selectAccent;
        canvas.DrawRoundRect(badgeRect, bh * 0.5f, badgeBg, pal.canvasBg, 1.2f * s);
        canvas.DrawTextBox(buf, badgeRect, bFs, Color(255, 255, 255, 255), 0.5f, 0.5f);
    }
}

[[nodiscard]] Rect ComputeEdgeLabelScreenRect(
    ICanvas* canvas,
    const std::vector<Vec2>& rawScreenPts,
    const BpmnEdge& edge,
    float zoom,
    float s
) noexcept {
    if (edge.label.empty() || rawScreenPts.size() < 2) return Rect{};
    const float zs = std::clamp(zoom, 0.4f, 2.0f) * s;
    Vec2 mid{0.0f, 0.0f};
    bool isHorizSeg = true;
    (void)SamplePolylineAtProgress(rawScreenPts, 0.5f, &mid, &isHorizSeg);
    const float fs = std::clamp(10.2f * zs, 8.0f * s, 12.5f * s);
    const Vec2 tm = canvas
        ? canvas->MeasureText(edge.label, fs)
        : Vec2{static_cast<float>(edge.label.size()) * fs * 0.56f, fs * 1.2f};
    const float labelX = mid.x + edge.labelOffset.x * zoom;
    const float labelY = (isHorizSeg ? (mid.y - tm.y * 0.5f - 10.5f * zs) : mid.y) + edge.labelOffset.y * zoom;
    return Rect(
        labelX - tm.x * 0.5f - 4.5f * zs,
        labelY - tm.y * 0.5f - 2.0f * zs,
        tm.x + 9.0f * zs,
        tm.y + 4.0f * zs
    );
}

void DrawEdgeVisual(
    ICanvas& canvas,
    const std::vector<Vec2>& rawScreenPts,
    const BpmnEdge& edge,
    BpmnRoutingMode routingMode,
    float zoom,
    float s,
    const BpmnPalette& pal,
    bool isSelected,
    bool isHovered,
    bool isSimActiveBranch,
    int hoveredSegIdx = -1,
    int hoveredEndpoint = 0,
    bool isLabelHovered = false
) {
    if (rawScreenPts.size() < 2) return;
    const float zs = std::clamp(zoom, 0.4f, 2.0f) * s;
    Color col = (edge.flowType == BpmnFlowType::MessageFlow) ? pal.edgeMessage : pal.edgeStroke;
    if (isSimActiveBranch) col = pal.simActiveGreen;
    if (isHovered) col = pal.hoverAccent;
    if (isSelected) col = pal.selectAccent;

    const float sw = (isSelected || isHovered || isSimActiveBranch)
        ? std::max(2.0f, 2.3f * zs)
        : std::max(1.5f, 1.8f * zs);

    const bool useRoundedCorners =
        (routingMode == BpmnRoutingMode::Orthogonal) &&
        (edge.flowType != BpmnFlowType::MessageFlow && edge.flowType != BpmnFlowType::Association);

    const std::vector<Vec2> drawPts = useRoundedCorners
        ? BuildRoundedPolyline(rawScreenPts, 6.0f * zs)
        : rawScreenPts;

    if (isSelected) {
        for (size_t i = 0; i + 1 < drawPts.size(); ++i) {
            canvas.DrawLine(drawPts[i], drawPts[i + 1], pal.selectGlow, sw + 4.5f * zs);
        }
    }

    for (size_t i = 0; i + 1 < drawPts.size(); ++i) {
        const Vec2 a = drawPts[i];
        const Vec2 b = drawPts[i + 1];
        if (edge.flowType == BpmnFlowType::MessageFlow) {
            DrawDashedLine(canvas, a, b, col, sw, 6.0f * zs, 4.0f * zs);
        } else if (edge.flowType == BpmnFlowType::Association) {
            DrawDashedLine(canvas, a, b, col, sw, 2.5f * zs, 3.5f * zs);
        } else {
            canvas.DrawLine(a, b, col, sw);
        }
    }

    // Source decorators (Default slash `\`, Conditional mini-diamond, Message open circle)
    const Vec2 p0 = rawScreenPts.front();
    const Vec2 pNext = rawScreenPts[1];
    const float sdx = pNext.x - p0.x;
    const float sdy = pNext.y - p0.y;
    const float slen = std::hypot(sdx, sdy);
    if (slen > 4.0f) {
        const float ux = sdx / slen;
        const float uy = sdy / slen;
        const float nx = -uy;
        const float ny = ux;
        if (edge.flowType == BpmnFlowType::DefaultFlow) {
            const Vec2 tc{p0.x + ux * 10.5f * zs, p0.y + uy * 10.5f * zs};
            canvas.DrawLine(
                {tc.x - ux * 3.5f * zs - nx * 5.0f * zs, tc.y - uy * 3.5f * zs - ny * 5.0f * zs},
                {tc.x + ux * 3.5f * zs + nx * 5.0f * zs, tc.y + uy * 3.5f * zs + ny * 5.0f * zs},
                col,
                sw * 1.15f
            );
        } else if (edge.flowType == BpmnFlowType::ConditionalFlow) {
            const Vec2 dc{p0.x + ux * 7.5f * zs, p0.y + uy * 7.5f * zs};
            DrawDiamond(canvas, dc, 5.5f * zs, 4.0f * zs, pal.nodeFill, col, sw);
        } else if (edge.flowType == BpmnFlowType::MessageFlow) {
            const float mr = 4.0f * zs;
            canvas.DrawRoundRect(Rect(p0.x - mr, p0.y - mr, mr * 2.0f, mr * 2.0f), mr, pal.nodeFill, col, sw);
        }
    }

    // Target arrowhead (for Sequence, Default, Conditional, Message flows)
    if (edge.flowType != BpmnFlowType::Association) {
        const Vec2 tip = rawScreenPts.back();
        const Vec2 prev = rawScreenPts[rawScreenPts.size() - 2];
        const float dx = tip.x - prev.x;
        const float dy = tip.y - prev.y;
        const float len = std::hypot(dx, dy);
        if (len > 1.0f) {
            const float ux = dx / len;
            const float uy = dy / len;
            const float nx = -uy;
            const float ny = ux;
            const float al = 9.0f * zs;
            const float aw = 4.6f * zs;
            const Vec2 b1{tip.x - ux * al + nx * aw, tip.y - uy * al + ny * aw};
            const Vec2 b2{tip.x - ux * al - nx * aw, tip.y - uy * al - ny * aw};
            canvas.DrawTriangle(
                tip, b1, b2,
                (edge.flowType == BpmnFlowType::MessageFlow) ? pal.nodeFill : col,
                col,
                1.0f * s
            );
        }
    }

    // Interactive `bpmn-js` Segment Drag Handles (`ConnectionSegmentMove`) & Endpoint Reconnect Handles (`BendpointMove`)
    if ((isSelected || isHovered) && routingMode == BpmnRoutingMode::Orthogonal) {
        for (size_t i = 0; i + 1 < rawScreenPts.size(); ++i) {
            const Vec2 a = rawScreenPts[i];
            const Vec2 b = rawScreenPts[i + 1];
            const float segLen = std::hypot(b.x - a.x, b.y - a.y);
            if (segLen < 22.0f * s) continue;
            const bool isHoriz = std::abs(b.x - a.x) >= std::abs(b.y - a.y);
            const Vec2 mid{(a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f};
            const float hw = isHoriz ? 7.0f * s : 3.2f * s;
            const float hh = isHoriz ? 3.2f * s : 7.0f * s;
            const bool isSegHov = (hoveredSegIdx == static_cast<int>(i));
            const Rect pillR(mid.x - hw, mid.y - hh, hw * 2.0f, hh * 2.0f);
            canvas.DrawRoundRect(
                pillR,
                3.0f * s,
                isSegHov ? pal.selectAccent : pal.nodeFill,
                pal.selectAccent,
                1.3f * s
            );
        }
    }
    if (isSelected) {
        const float er = 4.5f * s;
        const Vec2 ep0 = rawScreenPts.front();
        const Vec2 ep1 = rawScreenPts.back();
        canvas.DrawRoundRect(
            Rect(ep0.x - er, ep0.y - er, er * 2.0f, er * 2.0f),
            er,
            (hoveredEndpoint == 1) ? pal.selectAccent : pal.nodeFill,
            pal.selectAccent,
            1.5f * s
        );
        canvas.DrawRoundRect(
            Rect(ep1.x - er, ep1.y - er, er * 2.0f, er * 2.0f),
            er,
            (hoveredEndpoint == 2) ? pal.selectAccent : pal.nodeFill,
            pal.selectAccent,
            1.5f * s
        );
    }

    // Clean `bpmn-js` Edge Label placement (above horizontal segments so the wire stays unbroken, or knockout halo on vertical segments)
    if (!edge.label.empty() && zoom >= 0.45f) {
        const float fs = std::clamp(10.2f * zs, 8.0f * s, 12.5f * s);
        const Rect pill = ComputeEdgeLabelScreenRect(&canvas, rawScreenPts, edge, zoom, s);
        canvas.DrawRoundRect(
            pill,
            3.0f * zs,
            pal.canvasBg.WithAlpha(232),
            (isSelected || isLabelHovered) ? pal.selectAccent : Color(0, 0, 0, 0),
            (isSelected || isLabelHovered) ? 1.1f * s : 0.0f
        );
        canvas.DrawTextBox(edge.label, pill, fs, (isSelected || isLabelHovered) ? pal.selectAccent : pal.textPrimary, 0.5f, 0.5f);
    }
}

// ============================================================================
// Left Tool Palette & Context Pad Layout Helpers (`bpmn-js` Vector Icons)
// ============================================================================

struct PaletteItemDesc {
    BpmnTool tool;
    std::string_view shortTag;
    std::string_view tooltip;
    bool separatorAfter{false};
};

inline constexpr std::array<PaletteItemDesc, 15> kPaletteItems = {{
    {BpmnTool::Select,                  "V",   "Activate the select / move tool (V)"},
    {BpmnTool::Hand,                    "H",   "Activate the hand tool (H)"},
    {BpmnTool::Lasso,                   "L",   "Activate the lasso tool (L)"},
    {BpmnTool::Space,                   "S",   "Activate the create/remove space tool (S)"},
    {BpmnTool::GlobalConnect,           "C",   "Activate the global connect tool (C)", true},
    {BpmnTool::CreateStartEvent,        "○",   "Create StartEvent"},
    {BpmnTool::CreateIntermediateEvent, "◎",   "Create Intermediate/Boundary Event"},
    {BpmnTool::CreateEndEvent,          "●",   "Create EndEvent", true},
    {BpmnTool::CreateGateway,           "◇",   "Create Gateway"},
    {BpmnTool::CreateTask,              "▭",   "Create Task"},
    {BpmnTool::CreateSubProcess,        "⊞",   "Create expanded/collapsed Sub-Process", true},
    {BpmnTool::CreateDataObject,        "▯",   "Create DataObjectReference"},
    {BpmnTool::CreateDataStore,         "⛁",   "Create DataStoreReference"},
    {BpmnTool::CreateAnnotation,        "[T",  "Create TextAnnotation"},
    {BpmnTool::CreateLane,              "☰",   "Create Pool/Participant"}
}};

inline constexpr std::array<std::string_view, 8> kContextPadTooltips = {{
    "Append Task",
    "Append Gateway",
    "Append EndEvent",
    "Append Intermediate/Boundary Event",
    "Connect using Sequence/Message/Association",
    "Change element type (Replace Menu)",
    "Append TextAnnotation",
    "Remove element (Del)"
}};

[[nodiscard]] std::vector<int> GetContextPadActionsForNode(const BpmnNode& node) {
    switch (node.type) {
        case BpmnNodeType::EndEvent:
            // In BPMN 2.0, EndEvent cannot have outgoing SequenceFlow
            return {4, 5, 6, 7};
        case BpmnNodeType::DataObject:
        case BpmnNodeType::DataStore:
        case BpmnNodeType::TextAnnotation:
            // Artifacts only support Association connect, Annotation append, and Delete
            return {4, 6, 7};
        default:
            return {0, 1, 2, 3, 4, 5, 6, 7};
    }
}

void DrawContextPadButtonIcon(ICanvas& canvas, Rect r, int btnIdx, Color col, const BpmnPalette& pal, float s) {
    const Vec2 c = r.Center();
    const float sw = std::max(1.25f, 1.45f * s);
    switch (btnIdx) {
        case 0: { // Append Task (rounded rectangle)
            canvas.DrawRoundRect(Rect(c.x - 6.8f * s, c.y - 4.8f * s, 13.6f * s, 9.6f * s), 2.4f * s, Color(0, 0, 0, 0), col, sw);
            break;
        }
        case 1: { // Append Gateway (diamond)
            DrawDiamond(canvas, c, 6.4f * s, 6.4f * s, Color(0, 0, 0, 0), pal.gatewayStroke, sw * 1.1f);
            break;
        }
        case 2: { // Append EndEvent (thick circle)
            const float rad = 5.8f * s;
            canvas.DrawRoundRect(Rect(c.x - rad, c.y - rad, rad * 2.0f, rad * 2.0f), rad, Color(0, 0, 0, 0), pal.endEventStroke, sw * 1.85f);
            break;
        }
        case 3: { // Append IntermediateEvent (concentric double circle)
            const float r1 = 6.0f * s;
            const float r2 = 4.0f * s;
            canvas.DrawRoundRect(Rect(c.x - r1, c.y - r1, r1 * 2.0f, r1 * 2.0f), r1, Color(0, 0, 0, 0), pal.interEventStroke, sw);
            canvas.DrawRoundRect(Rect(c.x - r2, c.y - r2, r2 * 2.0f, r2 * 2.0f), r2, Color(0, 0, 0, 0), pal.interEventStroke, sw * 0.85f);
            break;
        }
        case 4: { // Connect using Sequence/MessageFlow (diagonal arrow)
            const Vec2 a{c.x - 5.2f * s, c.y + 5.2f * s};
            const Vec2 b{c.x + 5.2f * s, c.y - 5.2f * s};
            canvas.DrawLine(a, b, col, sw);
            canvas.DrawTriangle(
                b,
                {b.x - 5.0f * s, b.y + 1.2f * s},
                {b.x - 1.2f * s, b.y + 5.0f * s},
                col
            );
            canvas.DrawRoundRect(Rect(a.x - 2.0f * s, a.y - 2.0f * s, 4.0f * s, 4.0f * s), 2.0f * s, col);
            break;
        }
        case 5: { // Replace Element Type (Wrench `🔧` icon)
            const Vec2 head{c.x + 2.5f * s, c.y - 2.5f * s};
            const Vec2 tail{c.x - 5.2f * s, c.y + 5.2f * s};
            canvas.DrawLine({c.x + 0.5f * s, c.y - 0.5f * s}, tail, col, sw * 1.45f);
            const float hr = 3.6f * s;
            canvas.DrawRoundRect(Rect(head.x - hr, head.y - hr, hr * 2.0f, hr * 2.0f), hr, Color(0, 0, 0, 0), col, sw * 1.15f);
            canvas.DrawRoundRect(Rect(tail.x - 1.6f * s, tail.y - 1.6f * s, 3.2f * s, 3.2f * s), 1.6f * s, col);
            break;
        }
        case 6: { // Append TextAnnotation (`[` bracket with text lines)
            canvas.DrawLine({c.x - 1.5f * s, c.y - 5.5f * s}, {c.x - 5.5f * s, c.y - 5.5f * s}, col, sw);
            canvas.DrawLine({c.x - 5.5f * s, c.y - 5.5f * s}, {c.x - 5.5f * s, c.y + 5.5f * s}, col, sw);
            canvas.DrawLine({c.x - 5.5f * s, c.y + 5.5f * s}, {c.x - 1.5f * s, c.y + 5.5f * s}, col, sw);
            canvas.DrawLine({c.x - 2.0f * s, c.y - 1.8f * s}, {c.x + 5.5f * s, c.y - 1.8f * s}, col, sw);
            canvas.DrawLine({c.x - 2.0f * s, c.y + 1.8f * s}, {c.x + 4.0f * s, c.y + 1.8f * s}, col, sw);
            break;
        }
        case 7: { // Delete (Trash bin `🗑` icon)
            const Rect bin(c.x - 4.4f * s, c.y - 3.0f * s, 8.8f * s, 9.0f * s);
            canvas.DrawRoundRect(bin, 1.5f * s, Color(0, 0, 0, 0), col, sw);
            canvas.DrawLine({c.x - 6.0f * s, c.y - 3.2f * s}, {c.x + 6.0f * s, c.y - 3.2f * s}, col, sw * 1.15f);
            canvas.DrawLine({c.x - 2.2f * s, c.y - 5.2f * s}, {c.x + 2.2f * s, c.y - 5.2f * s}, col, sw);
            canvas.DrawLine({c.x - 1.6f * s, bin.y + 2.2f * s}, {c.x - 1.6f * s, bin.Bottom() - 2.0f * s}, col, sw * 0.85f);
            canvas.DrawLine({c.x + 1.6f * s, bin.y + 2.2f * s}, {c.x + 1.6f * s, bin.Bottom() - 2.0f * s}, col, sw * 0.85f);
            break;
        }
    }
}

void DrawPaletteButtonIcon(ICanvas& canvas, Rect r, BpmnTool tool, Color col, const BpmnPalette& pal, float s) {
    const Vec2 c = r.Center();
    const float sw = std::max(1.25f, 1.45f * s);
    switch (tool) {
        case BpmnTool::Select: {
            const Vec2 p0{c.x - 4.5f * s, c.y - 6.0f * s};
            const Vec2 p1{c.x - 4.5f * s, c.y + 5.5f * s};
            const Vec2 p2{c.x + 5.0f * s, c.y + 1.0f * s};
            canvas.DrawTriangle(p0, p1, p2, col);
            break;
        }
        case BpmnTool::Hand: {
            const Rect palm(c.x - 5.0f * s, c.y - 2.0f * s, 10.0f * s, 8.0f * s);
            canvas.DrawRoundRect(palm, 2.5f * s, Color(0, 0, 0, 0), col, sw);
            for (int i = -1; i <= 1; ++i) {
                const float fx = c.x + static_cast<float>(i) * 3.0f * s;
                canvas.DrawLine({fx, c.y - 6.5f * s}, {fx, c.y - 2.0f * s}, col, sw);
            }
            break;
        }
        case BpmnTool::Lasso: {
            const Rect box(c.x - 6.5f * s, c.y - 5.5f * s, 13.0f * s, 11.0f * s);
            DrawDashedLine(canvas, {box.x, box.y}, {box.Right(), box.y}, col, sw, 3.0f * s, 2.0f * s);
            DrawDashedLine(canvas, {box.Right(), box.y}, {box.Right(), box.Bottom()}, col, sw, 3.0f * s, 2.0f * s);
            DrawDashedLine(canvas, {box.Right(), box.Bottom()}, {box.x, box.Bottom()}, col, sw, 3.0f * s, 2.0f * s);
            DrawDashedLine(canvas, {box.x, box.Bottom()}, {box.x, box.y}, col, sw, 3.0f * s, 2.0f * s);
            break;
        }
        case BpmnTool::Space: {
            canvas.DrawLine({c.x, c.y - 7.0f * s}, {c.x, c.y + 7.0f * s}, col, sw);
            canvas.DrawLine({c.x - 6.5f * s, c.y}, {c.x + 6.5f * s, c.y}, col, sw);
            canvas.DrawTriangle({c.x - 7.5f * s, c.y}, {c.x - 3.5f * s, c.y - 3.0f * s}, {c.x - 3.5f * s, c.y + 3.0f * s}, col);
            canvas.DrawTriangle({c.x + 7.5f * s, c.y}, {c.x + 3.5f * s, c.y - 3.0f * s}, {c.x + 3.5f * s, c.y + 3.0f * s}, col);
            break;
        }
        case BpmnTool::GlobalConnect: {
            const Vec2 a{c.x - 5.5f * s, c.y + 4.5f * s};
            const Vec2 b{c.x + 5.5f * s, c.y - 4.5f * s};
            canvas.DrawLine(a, b, col, sw);
            canvas.DrawRoundRect(Rect(a.x - 2.5f * s, a.y - 2.5f * s, 5.0f * s, 5.0f * s), 2.5f * s, col);
            canvas.DrawRoundRect(Rect(b.x - 2.5f * s, b.y - 2.5f * s, 5.0f * s, 5.0f * s), 2.5f * s, col);
            break;
        }
        case BpmnTool::CreateStartEvent: {
            const float rad = 6.5f * s;
            canvas.DrawRoundRect(Rect(c.x - rad, c.y - rad, rad * 2.0f, rad * 2.0f), rad, Color(0, 0, 0, 0), pal.startEventStroke, sw * 1.15f);
            break;
        }
        case BpmnTool::CreateIntermediateEvent: {
            const float r1 = 6.8f * s;
            const float r2 = 4.6f * s;
            canvas.DrawRoundRect(Rect(c.x - r1, c.y - r1, r1 * 2.0f, r1 * 2.0f), r1, Color(0, 0, 0, 0), pal.interEventStroke, sw);
            canvas.DrawRoundRect(Rect(c.x - r2, c.y - r2, r2 * 2.0f, r2 * 2.0f), r2, Color(0, 0, 0, 0), pal.interEventStroke, sw * 0.85f);
            break;
        }
        case BpmnTool::CreateEndEvent: {
            const float rad = 6.5f * s;
            canvas.DrawRoundRect(Rect(c.x - rad, c.y - rad, rad * 2.0f, rad * 2.0f), rad, Color(0, 0, 0, 0), pal.endEventStroke, sw * 1.9f);
            break;
        }
        case BpmnTool::CreateGateway: {
            DrawDiamond(canvas, c, 7.2f * s, 7.2f * s, Color(0, 0, 0, 0), pal.gatewayStroke, sw * 1.15f);
            break;
        }
        case BpmnTool::CreateTask: {
            canvas.DrawRoundRect(Rect(c.x - 7.5f * s, c.y - 5.2f * s, 15.0f * s, 10.4f * s), 2.5f * s, Color(0, 0, 0, 0), col, sw);
            break;
        }
        case BpmnTool::CreateSubProcess: {
            const Rect br(c.x - 7.5f * s, c.y - 5.5f * s, 15.0f * s, 11.0f * s);
            canvas.DrawRoundRect(br, 2.5f * s, Color(0, 0, 0, 0), col, sw);
            canvas.DrawLine({c.x - 2.5f * s, c.y + 1.5f * s}, {c.x + 2.5f * s, c.y + 1.5f * s}, col, sw);
            canvas.DrawLine({c.x, c.y - 1.0f * s}, {c.x, c.y + 4.0f * s}, col, sw);
            break;
        }
        case BpmnTool::CreateDataObject: {
            const Rect dr(c.x - 5.2f * s, c.y - 6.5f * s, 10.4f * s, 13.0f * s);
            canvas.DrawRoundRect(dr, 1.5f * s, Color(0, 0, 0, 0), col, sw);
            canvas.DrawLine({dr.Right() - 3.5f * s, dr.y}, {dr.Right(), dr.y + 3.5f * s}, col, sw);
            break;
        }
        case BpmnTool::CreateDataStore: {
            const Rect dr(c.x - 6.2f * s, c.y - 6.0f * s, 12.4f * s, 12.0f * s);
            canvas.DrawRoundRect(dr, 3.0f * s, Color(0, 0, 0, 0), col, sw);
            canvas.DrawLine({dr.x, dr.y + 4.0f * s}, {dr.Right(), dr.y + 4.0f * s}, col, sw);
            break;
        }
        case BpmnTool::CreateAnnotation: {
            canvas.DrawLine({c.x - 2.0f * s, c.y - 6.0f * s}, {c.x - 6.5f * s, c.y - 6.0f * s}, col, sw);
            canvas.DrawLine({c.x - 6.5f * s, c.y - 6.0f * s}, {c.x - 6.5f * s, c.y + 6.0f * s}, col, sw);
            canvas.DrawLine({c.x - 6.5f * s, c.y + 6.0f * s}, {c.x - 2.0f * s, c.y + 6.0f * s}, col, sw);
            canvas.DrawLine({c.x - 2.5f * s, c.y - 2.0f * s}, {c.x + 5.5f * s, c.y - 2.0f * s}, col, sw);
            canvas.DrawLine({c.x - 2.5f * s, c.y + 2.0f * s}, {c.x + 4.0f * s, c.y + 2.0f * s}, col, sw);
            break;
        }
        case BpmnTool::CreateLane: {
            const Rect lr(c.x - 7.5f * s, c.y - 5.5f * s, 15.0f * s, 11.0f * s);
            canvas.DrawRoundRect(lr, 1.5f * s, Color(0, 0, 0, 0), col, sw);
            canvas.DrawLine({lr.x + 4.0f * s, lr.y}, {lr.x + 4.0f * s, lr.Bottom()}, col, sw);
            canvas.DrawLine({lr.x, c.y}, {lr.Right(), c.y}, col, sw);
            break;
        }
    }
}

[[nodiscard]] Rect ComputeLeftPaletteRect(const Rect& viewRect, bool showTopBar, float s) noexcept {
    const float x = viewRect.x + 12.0f * s;
    const float y = viewRect.y + (showTopBar ? 50.0f : 12.0f) * s;
    const float w = 42.0f * s;
    const float h = std::min(std::max(120.0f, viewRect.h - (y - viewRect.y) - 52.0f * s), 462.0f * s);
    return Rect(x, y, w, h);
}

[[nodiscard]] Rect ComputeTopBarRect(const Rect& viewRect, float s) noexcept {
    return Rect(viewRect.x + 12.0f * s, viewRect.y + 10.0f * s, std::max(200.0f, viewRect.w - 24.0f * s), 32.0f * s);
}

[[nodiscard]] Rect ComputeMinimapRect(const Rect& viewRect, float s) noexcept {
    const float mw = 188.0f * s;
    const float mh = 116.0f * s;
    return Rect(viewRect.Right() - mw - 12.0f * s, viewRect.Bottom() - mh - 12.0f * s, mw, mh);
}

[[nodiscard]] Rect ComputeZoomHudRect(const Rect& viewRect, float s) noexcept {
    const float zw = 178.0f * s;
    const float zh = 28.0f * s;
    return Rect(viewRect.x + 12.0f * s, viewRect.Bottom() - zh - 12.0f * s, zw, zh);
}

// ============================================================================
// Token Simulation Step Engine (`bpmn-js-token-simulation`)
// ============================================================================

void SpawnTokenFromStartEvent(BpmnDiagram& diagram, const BpmnNode& startNode, BpmnEditorState& state) {
    static constexpr std::array<Color, 5> kTokenColors = {{
        Color(16, 185, 129, 255),
        Color(56, 189, 248, 255),
        Color(245, 158, 11, 255),
        Color(168, 85, 247, 255),
        Color(244, 63, 94, 255)
    }};
    const uint32_t instId = state.nextInstanceId++;
    const Color col = kTokenColors[(instId - 1) % kTokenColors.size()];
    if (BpmnNode* mutStart = diagram.FindNode(startNode.id)) {
        mutStart->tokenExecutionCount++;
    }
    for (const auto& edge : diagram.edges) {
        if (edge.fromNodeId == startNode.id && edge.flowType != BpmnFlowType::Association) {
            BpmnToken tok{};
            tok.id = state.nextTokenId++;
            tok.edgeId = edge.id;
            tok.nodeId = 0;
            tok.progress = 0.0f;
            tok.color = col;
            tok.instanceId = instId;
            diagram.tokens.push_back(tok);
        }
    }
}

void StepTokenSimulation(BpmnDiagram& diagram, BpmnEditorState& state, float dtSec, float speedMultiplier, bool autoSpawn) {
    if (state.simPaused) return;
    const float dt = std::clamp(dtSec, 0.001f, 0.12f) * std::clamp(speedMultiplier, 0.2f, 4.0f);

    if (autoSpawn && diagram.tokens.size() < 24) {
        state.spawnCooldownSec -= dt;
        if (state.spawnCooldownSec <= 0.0) {
            for (const auto& n : diagram.nodes) {
                if (n.type == BpmnNodeType::StartEvent) {
                    SpawnTokenFromStartEvent(diagram, n, state);
                    break;
                }
            }
            state.spawnCooldownSec = 2.6;
        }
    }

    for (auto& n : diagram.nodes) n.activeTokensInside = 0;

    std::vector<BpmnToken> spawnedNext{};
    std::vector<size_t> removeIndices{};

    for (size_t i = 0; i < diagram.tokens.size(); ++i) {
        BpmnToken& tok = diagram.tokens[i];
        if (tok.edgeId != 0) {
            tok.progress += dt * 0.95f;
            if (tok.progress >= 1.0f) {
                const BpmnEdge* edge = diagram.FindEdge(tok.edgeId);
                BpmnNode* target = edge ? diagram.FindNode(edge->toNodeId) : nullptr;
                removeIndices.push_back(i);
                if (!target) continue;

                if (target->type == BpmnNodeType::EndEvent) {
                    target->tokenExecutionCount++;
                    if (target->eventDef == BpmnEventDefinition::Terminate) {
                        const uint32_t termInst = tok.instanceId;
                        for (size_t k = 0; k < diagram.tokens.size(); ++k) {
                            if (diagram.tokens[k].instanceId == termInst) {
                                removeIndices.push_back(k);
                            }
                        }
                    }
                } else if (target->type == BpmnNodeType::Task || target->type == BpmnNodeType::SubProcess ||
                           target->type == BpmnNodeType::IntermediateEvent) {
                    BpmnToken dwellTok = tok;
                    dwellTok.edgeId = 0;
                    dwellTok.nodeId = target->id;
                    dwellTok.progress = 0.0f;
                    spawnedNext.push_back(dwellTok);
                } else if (target->type == BpmnNodeType::Gateway) {
                    target->tokenExecutionCount++;
                    std::vector<const BpmnEdge*> outEdges;
                    for (const auto& e : diagram.edges) {
                        if (e.fromNodeId == target->id && e.flowType != BpmnFlowType::Association) {
                            outEdges.push_back(&e);
                        }
                    }
                    if (!outEdges.empty()) {
                        if (target->gatewayType == BpmnGatewayType::Exclusive || target->gatewayType == BpmnGatewayType::EventBased) {
                            const BpmnEdge* chosen = outEdges.front();
                            for (const BpmnEdge* oe : outEdges) {
                                if (oe->id == target->activeOutgoingEdgeId) {
                                    chosen = oe;
                                    break;
                                }
                            }
                            BpmnToken nextTok = tok;
                            nextTok.id = state.nextTokenId++;
                            nextTok.edgeId = chosen->id;
                            nextTok.nodeId = 0;
                            nextTok.progress = 0.0f;
                            spawnedNext.push_back(nextTok);
                        } else {
                            for (const BpmnEdge* oe : outEdges) {
                                BpmnToken nextTok = tok;
                                nextTok.id = state.nextTokenId++;
                                nextTok.edgeId = oe->id;
                                nextTok.nodeId = 0;
                                nextTok.progress = 0.0f;
                                spawnedNext.push_back(nextTok);
                            }
                        }
                    }
                }
            }
        } else if (tok.nodeId != 0) {
            BpmnNode* host = diagram.FindNode(tok.nodeId);
            if (!host) {
                removeIndices.push_back(i);
                continue;
            }
            const float dwellSec = std::clamp(host->slaDurationMs / 1000.0f, 0.25f, 2.0f);
            tok.progress += dt / dwellSec;
            if (tok.progress >= 1.0f) {
                host->tokenExecutionCount++;
                removeIndices.push_back(i);
                for (const auto& e : diagram.edges) {
                    if (e.fromNodeId == host->id && e.flowType != BpmnFlowType::Association) {
                        BpmnToken nextTok = tok;
                        nextTok.id = state.nextTokenId++;
                        nextTok.edgeId = e.id;
                        nextTok.nodeId = 0;
                        nextTok.progress = 0.0f;
                        spawnedNext.push_back(nextTok);
                    }
                }
            } else {
                host->activeTokensInside++;
            }
        }
    }

    if (!removeIndices.empty()) {
        std::sort(removeIndices.begin(), removeIndices.end());
        removeIndices.erase(std::unique(removeIndices.begin(), removeIndices.end()), removeIndices.end());
        for (auto it = removeIndices.rbegin(); it != removeIndices.rend(); ++it) {
            if (*it < diagram.tokens.size()) {
                diagram.tokens.erase(diagram.tokens.begin() + static_cast<std::ptrdiff_t>(*it));
            }
        }
    }
    for (const auto& st : spawnedNext) {
        if (diagram.tokens.size() < 48) {
            diagram.tokens.push_back(st);
            if (st.nodeId != 0) {
                if (BpmnNode* n = diagram.FindNode(st.nodeId)) {
                    n->activeTokensInside++;
                }
            }
        }
    }
}

// ============================================================================
// Render Node Function (`RenderBpmnEditorNode`)
// ============================================================================

void RenderBpmnEditorNode(const Node* node, ICanvas& canvas, const UiTheme& theme) {
    const auto* payload = node->As<BpmnEditorPayload>();
    if (!payload) return;

    const BpmnEditorOptions& opts = payload->opts;
    const BpmnDiagram& diagram = payload->diagramSnapshot;
    const BpmnEditorState& state = payload->stateSnapshot;
    const float s = theme.Scale(1.0f);

    const ElementSpec elemSpec = MakeBpmnElementSpec(opts, node->uiSize, theme, node->disabled);
    const Vec2 rawSize = {node->computedRect.w, node->computedRect.h};
    const ElementGeometry elemGeom = Element::Resolve(node->computedRect, rawSize, elemSpec, &canvas, theme);
    Element::RenderText(canvas, elemGeom, elemSpec, theme);

    const Rect viewRect = (elemGeom.innerControlRect.w > 1.0f && elemGeom.innerControlRect.h > 1.0f)
        ? elemGeom.innerControlRect
        : node->computedRect;
    if (viewRect.w <= 2.0f || viewRect.h <= 2.0f) return;

    const BpmnPalette pal = ResolveBpmnPalette(opts.themePreset, theme);
    const float radius = opts.borderRadius.value_or(8.0f) * s;

    // 1. Canvas Background & Border
    canvas.DrawRoundRect(viewRect, radius, pal.canvasBg, pal.canvasBorder, std::max(1.0f, opts.borderWidth > 0.0f ? opts.borderWidth * s : 1.0f * s));
    canvas.PushClip(viewRect);

    const Vec2 pan = state.pan;
    const float zoom = state.zoom;
    const float zs = std::clamp(zoom, 0.35f, 2.2f) * s;

    // 2. World Grid
    if (opts.showGrid && opts.gridStep >= 8.0f) {
        const float screenStep = opts.gridStep * zoom;
        if (screenStep >= 9.0f) {
            const Vec2 wTL = ScreenToWorld({viewRect.x, viewRect.y}, viewRect, pan, zoom);
            const Vec2 wBR = ScreenToWorld({viewRect.Right(), viewRect.Bottom()}, viewRect, pan, zoom);
            const float startX = std::floor(wTL.x / opts.gridStep) * opts.gridStep;
            const float startY = std::floor(wTL.y / opts.gridStep) * opts.gridStep;
            const int maxCols = static_cast<int>((wBR.x - startX) / opts.gridStep) + 2;
            const int maxRows = static_cast<int>((wBR.y - startY) / opts.gridStep) + 2;
            if (maxCols * maxRows <= 6500) {
                for (int r = 0; r < maxRows; ++r) {
                    const float wy = startY + static_cast<float>(r) * opts.gridStep;
                    const float sy = viewRect.y + pan.y + wy * zoom;
                    for (int c = 0; c < maxCols; ++c) {
                        const float wx = startX + static_cast<float>(c) * opts.gridStep;
                        const float sx = viewRect.x + pan.x + wx * zoom;
                        canvas.DrawRect(Rect(sx - 0.8f * s, sy - 0.8f * s, 1.6f * s, 1.6f * s), pal.gridDot);
                    }
                }
            }
        }
    }

    // 3. Swimlanes & Participant Pools (`BpmnLane`) - Authentic `bpmn.io` Two-Column Vertical Header Strip
    const float poolStripW = std::clamp(28.0f * zs, 18.0f * s, 36.0f * s);
    const float laneStripW = std::clamp(28.0f * zs, 18.0f * s, 36.0f * s);

    // Draw individual lane bands and vertical lane headers first
    for (size_t i = 0; i < diagram.lanes.size(); ++i) {
        const BpmnLane& lane = diagram.lanes[i];
        const Rect sr = WorldToScreenRect(lane.rect, viewRect, pan, zoom);
        if (sr.Right() < viewRect.x || sr.x > viewRect.Right() || sr.Bottom() < viewRect.y || sr.y > viewRect.Bottom()) {
            continue;
        }
        const bool hasPool = !lane.poolName.empty();
        const float leftOffset = hasPool ? poolStripW : 0.0f;

        const bool isLaneSel = (state.selectedLaneId == lane.id);
        const bool isLaneHov = (state.hoveredLaneId == lane.id);
        const Color laneBg = (i % 2 == 0) ? pal.laneBgEven : pal.laneBgOdd;
        const Color borderCol = isLaneSel ? pal.selectAccent : (isLaneHov ? pal.hoverAccent : pal.laneBorder);
        const float bw = isLaneSel ? std::max(1.8f, 2.2f * s) : std::max(1.0f, 1.2f * s);

        canvas.DrawRect(sr, laneBg);
        canvas.DrawLine({sr.x + leftOffset, sr.y}, {sr.Right(), sr.y}, borderCol, bw);
        canvas.DrawLine({sr.x + leftOffset, sr.Bottom()}, {sr.Right(), sr.Bottom()}, borderCol, bw);
        canvas.DrawLine({sr.Right(), sr.y}, {sr.Right(), sr.Bottom()}, borderCol, bw);

        const Rect laneHdrRect(sr.x + leftOffset, sr.y, std::min(laneStripW, std::max(0.0f, sr.w - leftOffset)), sr.h);
        const Color hdrAccent = (lane.accentColor.a > 0) ? lane.accentColor : pal.laneHeaderBg;
        canvas.DrawRect(laneHdrRect, hdrAccent);
        canvas.DrawLine({laneHdrRect.x, laneHdrRect.y}, {laneHdrRect.x, laneHdrRect.Bottom()}, borderCol, bw);
        canvas.DrawLine({laneHdrRect.Right(), laneHdrRect.y}, {laneHdrRect.Right(), laneHdrRect.Bottom()}, borderCol, bw);

        if (zoom >= 0.35f) {
            const float fs = std::clamp(11.0f * zs, 8.0f * s, 13.0f * s);
            DrawVerticalRotatedHeaderText(canvas, lane.name, laneHdrRect, fs, pal.laneText, FontStyle::Bold);
        }
    }

    // Draw unified outer Participant Pool vertical header strip for contiguous lanes sharing `poolName`
    size_t groupStart = 0;
    while (groupStart < diagram.lanes.size()) {
        const std::string& pName = diagram.lanes[groupStart].poolName;
        size_t groupEnd = groupStart + 1;
        Rect unionWorld = diagram.lanes[groupStart].rect;
        while (groupEnd < diagram.lanes.size() && diagram.lanes[groupEnd].poolName == pName) {
            unionWorld = unionWorld.Union(diagram.lanes[groupEnd].rect);
            ++groupEnd;
        }
        if (!pName.empty()) {
            const Rect poolSr = WorldToScreenRect(unionWorld, viewRect, pan, zoom);
            const Rect poolHdrRect(poolSr.x, poolSr.y, std::min(poolStripW, poolSr.w), poolSr.h);
            canvas.DrawRect(poolHdrRect, pal.poolHeaderBg);
            canvas.DrawRoundRect(poolSr, 3.0f * s, Color(0, 0, 0, 0), pal.laneBorder, std::max(1.2f, 1.5f * s));
            canvas.DrawLine({poolHdrRect.Right(), poolHdrRect.y}, {poolHdrRect.Right(), poolHdrRect.Bottom()}, pal.laneBorder, std::max(1.2f, 1.4f * s));
            if (zoom >= 0.35f) {
                const float fs = std::clamp(11.5f * zs, 8.5f * s, 13.5f * s);
                DrawVerticalRotatedHeaderText(canvas, pName, poolHdrRect, fs, pal.laneText, FontStyle::Bold);
            }
        }
        groupStart = groupEnd;
    }

    // 4. Edges / Flows (`BpmnEdge`)
    for (const auto& edge : diagram.edges) {
        const BpmnNode* fromNode = diagram.FindNode(edge.fromNodeId);
        const BpmnNode* toNode = diagram.FindNode(edge.toNodeId);
        if (!fromNode || !toNode) continue;

        const std::vector<Vec2> worldPts = BuildEdgeWorldPoints(diagram, *fromNode, *toNode, edge, opts.routingMode);
        std::vector<Vec2> screenPts;
        screenPts.reserve(worldPts.size());
        for (const Vec2& wp : worldPts) {
            screenPts.push_back(WorldToScreen(wp, viewRect, pan, zoom));
        }

        bool isSimActiveBranch = false;
        if (opts.tokenSimulation && fromNode->type == BpmnNodeType::Gateway &&
            fromNode->gatewayType == BpmnGatewayType::Exclusive) {
            if (fromNode->activeOutgoingEdgeId == edge.id) {
                isSimActiveBranch = true;
            }
        }

        const bool isEdgeSel = (state.selectedEdgeId == edge.id);
        const bool isEdgeHov = (state.hoveredEdgeId == edge.id);
        DrawEdgeVisual(
            canvas,
            screenPts,
            edge,
            opts.routingMode,
            zoom,
            s,
            pal,
            isEdgeSel,
            isEdgeHov,
            isSimActiveBranch,
            (isEdgeSel || isEdgeHov) ? state.hoveredEdgeSegIdx : -1,
            isEdgeSel ? state.hoveredEdgeEndpoint : 0,
            state.hoveredLabelEdgeId == edge.id
        );
    }

    // Interactive wire connection or endpoint reconnect preview
    if (state.connectingWire && state.connectFromNodeId != 0) {
        if (const BpmnNode* src = diagram.FindNode(state.connectFromNodeId)) {
            const Vec2 w0 = (state.connectFromPort == BpmnPortSide::Auto)
                ? src->Center()
                : GetNodePortWorldPos(*src, state.connectFromPort);
            const Vec2 s0 = WorldToScreen(w0, viewRect, pan, zoom);
            const Vec2 s1 = WorldToScreen(state.connectEndWorld, viewRect, pan, zoom);
            DrawDashedLine(canvas, s0, s1, pal.selectAccent, 2.2f * s, 6.0f * s, 4.0f * s);
            canvas.DrawRoundRect(Rect(s1.x - 4.5f * s, s1.y - 4.5f * s, 9.0f * s, 9.0f * s), 4.5f * s, pal.selectAccent);
        }
    } else if (state.reconnectingEdge && state.reconnectEdgeId != 0) {
        if (const BpmnEdge* re = diagram.FindEdge(state.reconnectEdgeId)) {
            const BpmnNode* fixedNode = diagram.FindNode(state.reconnectEndpoint == 1 ? re->toNodeId : re->fromNodeId);
            if (fixedNode) {
                const Vec2 s0 = WorldToScreen(fixedNode->Center(), viewRect, pan, zoom);
                const Vec2 s1 = WorldToScreen(state.reconnectEndWorld, viewRect, pan, zoom);
                DrawDashedLine(canvas, s0, s1, pal.selectAccent, 2.2f * s, 6.0f * s, 4.0f * s);
                canvas.DrawRoundRect(Rect(s1.x - 4.5f * s, s1.y - 4.5f * s, 9.0f * s, 9.0f * s), 4.5f * s, pal.selectAccent);
            }
        }
    }

    // 5. Nodes (`BpmnNode`) - Pass 0: Expanded SubProcess containers, Pass 1: Standard nodes, Pass 2: BoundaryEvents
    for (int pass = 0; pass < 3; ++pass) {
        for (const auto& n : diagram.nodes) {
            const bool isBoundary = (n.type == BpmnNodeType::BoundaryEvent);
            const bool isExpSub = (n.type == BpmnNodeType::SubProcess && n.isExpanded);
            const int nodePass = isExpSub ? 0 : (isBoundary ? 2 : 1);
            if (nodePass != pass) continue;

            const Rect sr = WorldToScreenRect(n.Bounds(), viewRect, pan, zoom);
            if (sr.Right() < viewRect.x - 100.0f || sr.x > viewRect.Right() + 100.0f ||
                sr.Bottom() < viewRect.y - 100.0f || sr.y > viewRect.Bottom() + 100.0f) {
                continue;
            }
            const bool isSel = state.IsNodeSelected(n.id);
            const bool isHov = (state.hoveredNodeId == n.id || state.connectSnapNodeId == n.id || state.reconnectSnapNodeId == n.id);
            const bool showResize = (isSel && state.selectedNodes.size() <= 1 && !opts.readOnly && !opts.tokenSimulation);
            DrawNodeVisual(
                canvas,
                diagram,
                n,
                sr,
                zoom,
                s,
                pal,
                isSel,
                isHov,
                opts.tokenSimulation,
                opts.showSlaHeatmap,
                payload->animPhase,
                state.hoveredLabelNodeId == n.id,
                showResize,
                showResize ? state.hoveredResizeCorner : -1
            );

            // Token Simulation Triggers on StartEvent / BoundaryEvent / Exclusive Gateway
            if (opts.tokenSimulation && zoom >= 0.45f) {
                if (n.type == BpmnNodeType::StartEvent || n.type == BpmnNodeType::BoundaryEvent) {
                    const float btnW = 22.0f * s;
                    const float btnH = 18.0f * s;
                    const Rect trigRect(sr.x - btnW - 4.0f * s, sr.Center().y - btnH * 0.5f, btnW, btnH);
                    const Color trigCol = (n.type == BpmnNodeType::StartEvent) ? pal.simActiveGreen : pal.interEventStroke;
                    canvas.DrawRoundRect(trigRect, 4.0f * s, trigCol, pal.canvasBg, 1.2f * s);
                    const Vec2 tc = trigRect.Center();
                    canvas.DrawTriangle(
                        {tc.x - 3.0f * s, tc.y - 4.2f * s},
                        {tc.x - 3.0f * s, tc.y + 4.2f * s},
                        {tc.x + 4.5f * s, tc.y},
                        Color(255, 255, 255, 255)
                    );
                }
            }
        }
    }

    // 6. Live Animated Process Tokens (`BpmnToken`)
    if (opts.tokenSimulation) {
        for (const auto& tok : diagram.tokens) {
            if (tok.edgeId != 0) {
                const BpmnEdge* edge = diagram.FindEdge(tok.edgeId);
                if (!edge) continue;
                const BpmnNode* fromNode = diagram.FindNode(edge->fromNodeId);
                const BpmnNode* toNode = diagram.FindNode(edge->toNodeId);
                if (!fromNode || !toNode) continue;

                const std::vector<Vec2> worldPts = BuildEdgeWorldPoints(diagram, *fromNode, *toNode, *edge, opts.routingMode);
                const Vec2 wPos = SamplePolylineAtProgress(worldPts, tok.progress);
                const Vec2 sPos = WorldToScreen(wPos, viewRect, pan, zoom);
                const float tr = std::clamp(8.5f * zoom * s, 6.0f * s, 12.0f * s);
                canvas.DrawRoundRect(Rect(sPos.x - tr * 1.4f, sPos.y - tr * 1.4f, tr * 2.8f, tr * 2.8f), tr * 1.4f, tok.color.WithAlpha(75));
                canvas.DrawRoundRect(Rect(sPos.x - tr, sPos.y - tr, tr * 2.0f, tr * 2.0f), tr, tok.color, Color(255, 255, 255, 240), 1.5f * s);
                char ibuf[16];
                std::snprintf(ibuf, sizeof(ibuf), "%u", tok.instanceId);
                canvas.DrawTextBox(ibuf, Rect(sPos.x - tr, sPos.y - tr, tr * 2.0f, tr * 2.0f), tr * 1.05f, Color(255, 255, 255, 255), 0.5f, 0.5f);
            }
        }
    }

    // 7. Smart Alignment Laser Guides (active during node drag AND during Create Tool Ghost Preview!)
    if (opts.smartGuides && (state.draggingNodes || state.activeTool >= BpmnTool::CreateStartEvent) && !state.activeGuides.empty()) {
        for (const auto& g : state.activeGuides) {
            if (g.vertical) {
                const Vec2 a = WorldToScreen({g.worldCoord, g.worldSpanMin}, viewRect, pan, zoom);
                const Vec2 b = WorldToScreen({g.worldCoord, g.worldSpanMax}, viewRect, pan, zoom);
                DrawDashedLine(canvas, a, b, pal.guideLaser, 1.4f * s, 5.0f * s, 3.0f * s);
            } else {
                const Vec2 a = WorldToScreen({g.worldSpanMin, g.worldCoord}, viewRect, pan, zoom);
                const Vec2 b = WorldToScreen({g.worldSpanMax, g.worldCoord}, viewRect, pan, zoom);
                DrawDashedLine(canvas, a, b, pal.guideLaser, 1.4f * s, 5.0f * s, 3.0f * s);
            }
        }
    }

    // 8. Space Tool (`S`) Split Axis Line
    if (state.spaceToolDragging && state.spaceToolAxisLocked) {
        const Vec2 sSplit = WorldToScreen(state.spaceStartWorld, viewRect, pan, zoom);
        const Vec2 sCurr = WorldToScreen(state.spaceCurrentWorld, viewRect, pan, zoom);
        if (state.spaceToolHorizontal) {
            DrawDashedLine(canvas, {sSplit.x, viewRect.y}, {sSplit.x, viewRect.Bottom()}, pal.selectAccent, 1.8f * s, 6.0f * s, 4.0f * s);
            DrawDashedLine(canvas, {sCurr.x, viewRect.y}, {sCurr.x, viewRect.Bottom()}, pal.guideLaser, 1.8f * s, 6.0f * s, 4.0f * s);
            canvas.DrawRect(Rect(std::min(sSplit.x, sCurr.x), viewRect.y, std::abs(sCurr.x - sSplit.x), viewRect.h), pal.selectGlow);
        } else {
            DrawDashedLine(canvas, {viewRect.x, sSplit.y}, {viewRect.Right(), sSplit.y}, pal.selectAccent, 1.8f * s, 6.0f * s, 4.0f * s);
            DrawDashedLine(canvas, {viewRect.x, sCurr.y}, {viewRect.Right(), sCurr.y}, pal.guideLaser, 1.8f * s, 6.0f * s, 4.0f * s);
            canvas.DrawRect(Rect(viewRect.x, std::min(sSplit.y, sCurr.y), viewRect.w, std::abs(sCurr.y - sSplit.y)), pal.selectGlow);
        }
    }

    // 9. Lasso Marquee Box
    if (state.lassoActive) {
        const Vec2 a = WorldToScreen(state.lassoStartWorld, viewRect, pan, zoom);
        const Vec2 b = WorldToScreen(state.lassoEndWorld, viewRect, pan, zoom);
        const Rect lr(std::min(a.x, b.x), std::min(a.y, b.y), std::abs(b.x - a.x), std::abs(b.y - a.y));
        canvas.DrawRoundRect(lr, 3.0f * s, pal.selectGlow, pal.selectAccent, 1.4f * s);
    }

    // 10. Authentic `bpmn-js` CreatePreview (Full Vector Node Silhouette under Cursor)
    if (!opts.readOnly && !opts.tokenSimulation && payload->resp.hovered && state.activeTool >= BpmnTool::CreateStartEvent && state.hoveredPaletteIdx < 0) {
        if (state.activeTool == BpmnTool::CreateLane) {
            Rect laneGhost(state.ghostSnappedPos.x, state.ghostSnappedPos.y, 1280.0f, 190.0f);
            if (!diagram.lanes.empty()) {
                laneGhost.x = diagram.lanes.back().rect.x;
                laneGhost.y = diagram.lanes.back().rect.Bottom();
                laneGhost.w = diagram.lanes.back().rect.w;
            }
            const Rect gsr = WorldToScreenRect(laneGhost, viewRect, pan, zoom);
            canvas.DrawRoundRect(gsr, 4.0f * s, pal.selectGlow, pal.selectAccent, 1.6f * s);
        } else {
            BpmnNode ghost{};
            switch (state.activeTool) {
                case BpmnTool::CreateStartEvent:        ghost.type = BpmnNodeType::StartEvent; ghost.label = "Start"; break;
                case BpmnTool::CreateIntermediateEvent: ghost.type = (state.dragBoundaryHostId != 0) ? BpmnNodeType::BoundaryEvent : BpmnNodeType::IntermediateEvent; ghost.eventDef = BpmnEventDefinition::Timer; ghost.label = (state.dragBoundaryHostId != 0) ? "Boundary" : "Timer"; break;
                case BpmnTool::CreateEndEvent:          ghost.type = BpmnNodeType::EndEvent; ghost.label = "End"; break;
                case BpmnTool::CreateGateway:           ghost.type = BpmnNodeType::Gateway; ghost.label = "Gateway?"; break;
                case BpmnTool::CreateTask:              ghost.type = BpmnNodeType::Task; ghost.label = "Task"; break;
                case BpmnTool::CreateSubProcess:        ghost.type = BpmnNodeType::SubProcess; ghost.label = "Sub-Process"; break;
                case BpmnTool::CreateDataObject:        ghost.type = BpmnNodeType::DataObject; ghost.label = "Payload"; break;
                case BpmnTool::CreateDataStore:         ghost.type = BpmnNodeType::DataStore; ghost.label = "Database"; break;
                case BpmnTool::CreateAnnotation:        ghost.type = BpmnNodeType::TextAnnotation; ghost.label = "Text annotation"; break;
                default: break;
            }
            ghost.size = BpmnDiagram::DefaultNodeSize(ghost.type);
            ghost.pos = state.ghostSnappedPos;
            const Rect gsr = WorldToScreenRect(ghost.Bounds(), viewRect, pan, zoom);
            DrawNodeVisual(canvas, diagram, ghost, gsr, zoom, s, pal, false, true, false, false, 0.0f);
        }
    }

    // 11. Context-Sensitive `bpmn.io` Context Pad (Vector Icons) + `🔧 Replace Menu` Popover
    if (opts.showContextPad && !opts.readOnly && !opts.tokenSimulation && state.editingNodeId == 0 && state.editingEdgeId == 0) {
        if (state.primaryNodeId != 0 && state.selectedNodes.size() <= 1) {
            if (const BpmnNode* selNode = diagram.FindNode(state.primaryNodeId)) {
                const std::vector<int> actions = GetContextPadActionsForNode(*selNode);
                const int cols = std::min(4, static_cast<int>(actions.size()));
                const int rows = (static_cast<int>(actions.size()) + 3) / 4;
                const Rect nsr = WorldToScreenRect(selNode->Bounds(), viewRect, pan, zoom);
                const float btnS = 26.0f * s;
                const float gap = 4.0f * s;
                const float padW = (btnS + gap) * static_cast<float>(cols) + 4.0f * s;
                const float padH = (btnS + gap) * static_cast<float>(rows) + 4.0f * s;
                const float padX = std::min(viewRect.Right() - padW - 12.0f * s, nsr.Right() + 10.0f * s);
                const float padY = std::clamp(nsr.y - 4.0f * s, viewRect.y + 48.0f * s, viewRect.Bottom() - padH - 12.0f * s);
                const Rect padFrame(padX - 4.0f * s, padY - 4.0f * s, padW, padH);
                canvas.DrawRoundRect(padFrame, 6.0f * s, pal.hudBg, pal.hudBorder, 1.2f * s);

                for (size_t slot = 0; slot < actions.size(); ++slot) {
                    const int actIdx = actions[slot];
                    const int row = static_cast<int>(slot) / 4;
                    const int col = static_cast<int>(slot) % 4;
                    const Rect br(padX + static_cast<float>(col) * (btnS + gap), padY + static_cast<float>(row) * (btnS + gap), btnS, btnS);
                    const bool isActiveBtn = (actIdx == 5 && state.replaceMenuOpen);
                    const bool isHovBtn = (state.hoveredContextPadBtn == actIdx);
                    const Color bBg = isActiveBtn
                        ? pal.hudActiveBg
                        : (isHovBtn ? ((actIdx == 7) ? pal.endEventFill : pal.hudActiveBg) : ((actIdx == 7) ? pal.endEventFill.WithAlpha(160) : pal.hudHover));
                    const Color bFg = (actIdx == 7)
                        ? pal.endEventStroke
                        : ((isActiveBtn || isHovBtn) ? pal.selectAccent : pal.textPrimary);
                    canvas.DrawRoundRect(br, 4.0f * s, bBg, isHovBtn ? pal.selectAccent : pal.hudBorder, 1.0f * s);
                    DrawContextPadButtonIcon(canvas, br, actIdx, bFg, pal, s);
                }

                // Floating tooltip for hovered Context Pad button
                if (state.hoveredContextPadBtn >= 0 && state.hoveredContextPadBtn < 8 && !state.replaceMenuOpen) {
                    const std::string_view tip = kContextPadTooltips[static_cast<size_t>(state.hoveredContextPadBtn)];
                    const float tfs = 10.0f * s;
                    const Vec2 tm = canvas.MeasureText(tip, tfs);
                    const Rect tipRect(padFrame.x, padFrame.y - tm.y - 10.0f * s, tm.x + 12.0f * s, tm.y + 6.0f * s);
                    canvas.DrawRoundRect(tipRect, 4.0f * s, pal.hudBg, pal.selectAccent, 1.0f * s);
                    canvas.DrawTextBox(tip, tipRect, tfs, pal.textPrimary, 0.5f, 0.5f);
                }

                if (state.replaceMenuOpen) {
                    std::array<std::string_view, 8> items{};
                    size_t itemCount = 0;
                    if (selNode->type == BpmnNodeType::Task || selNode->type == BpmnNodeType::SubProcess) {
                        items = {
                            "User Task", "Service Task", "Send Task", "Receive Task",
                            "Script Task", "Business Rule Task", "Sub-Process (Collapsed)", "Sub-Process (Expanded)"
                        };
                        itemCount = 8;
                    } else if (selNode->type == BpmnNodeType::Gateway) {
                        items = {"Exclusive Gateway", "Parallel Gateway", "Inclusive Gateway", "Event-Based Gateway", "", "", "", ""};
                        itemCount = 4;
                    } else {
                        items = {"Blank Event", "Message Event", "Timer Event", "Error Event", "Signal Event", "Escalation Event", "Terminate End Event", ""};
                        itemCount = 7;
                    }
                    const float rowH = 24.0f * s;
                    const float popH = 30.0f * s + static_cast<float>(itemCount) * rowH;
                    const Rect popRect(padFrame.x, padFrame.Bottom() + 6.0f * s, 204.0f * s, popH);
                    canvas.DrawRoundRect(popRect, 7.0f * s, pal.hudBg, pal.selectAccent, 1.4f * s);
                    canvas.DrawText("CHANGE ELEMENT TYPE", {popRect.x + 10.0f * s, popRect.y + 7.0f * s}, 9.5f * s, pal.selectAccent);
                    canvas.DrawLine({popRect.x + 6.0f * s, popRect.y + 23.0f * s}, {popRect.Right() - 6.0f * s, popRect.y + 23.0f * s}, pal.hudBorder, 1.0f * s);

                    for (size_t k = 0; k < itemCount; ++k) {
                        const Rect ir(popRect.x + 6.0f * s, popRect.y + 27.0f * s + static_cast<float>(k) * rowH, popRect.w - 12.0f * s, rowH - 2.0f * s);
                        const bool isRowHov = (state.hoveredReplaceMenuItem == static_cast<int>(k));
                        canvas.DrawRoundRect(ir, 4.0f * s, isRowHov ? pal.hudActiveBg : pal.hudHover);

                        const Rect iconBox(ir.x + 5.0f * s, ir.y + 3.0f * s, 16.0f * s, 16.0f * s);
                        if (selNode->type == BpmnNodeType::Task || selNode->type == BpmnNodeType::SubProcess) {
                            if (k >= 6) {
                                DrawPaletteButtonIcon(canvas, iconBox, BpmnTool::CreateSubProcess, pal.textPrimary, pal, s * 0.85f);
                            } else {
                                static constexpr std::array<BpmnTaskType, 6> kRowTasks = {
                                    BpmnTaskType::User, BpmnTaskType::Service, BpmnTaskType::Send,
                                    BpmnTaskType::Receive, BpmnTaskType::Script, BpmnTaskType::BusinessRule
                                };
                                DrawTaskTypeMarker(canvas, iconBox, kRowTasks[k], pal.textPrimary, pal.hudHover, s * 0.9f);
                            }
                        } else if (selNode->type == BpmnNodeType::Gateway) {
                            DrawGatewayMarker(canvas, iconBox.Center(), 7.0f * s, static_cast<BpmnGatewayType>(k), pal.gatewayStroke, s * 0.8f);
                        } else {
                            DrawEventDefinitionSymbol(canvas, iconBox.Center(), 7.0f * s, static_cast<BpmnEventDefinition>(k), selNode->type == BpmnNodeType::EndEvent, pal.textPrimary, pal.hudHover, s * 0.85f);
                        }

                        canvas.DrawTextBox(items[k], Rect(ir.x + 26.0f * s, ir.y, ir.w - 30.0f * s, ir.h), 10.0f * s, isRowHov ? pal.selectAccent : pal.textPrimary, 0.0f, 0.5f);
                    }
                }
            }
        } else if (state.selectedEdgeId != 0) {
            if (const BpmnEdge* selEdge = diagram.FindEdge(state.selectedEdgeId)) {
                const BpmnNode* fn = diagram.FindNode(selEdge->fromNodeId);
                const BpmnNode* tn = diagram.FindNode(selEdge->toNodeId);
                if (fn && tn) {
                    const auto wpts = BuildEdgeWorldPoints(diagram, *fn, *tn, *selEdge, opts.routingMode);
                    Vec2 wMid{0.0f, 0.0f};
                    (void)SamplePolylineAtProgress(wpts, 0.5f, &wMid);
                    const Vec2 sMid = WorldToScreen(wMid, viewRect, pan, zoom);
                    const Rect bar(sMid.x - 110.0f * s, sMid.y - 38.0f * s, 220.0f * s, 26.0f * s);
                    canvas.DrawRoundRect(bar, 5.0f * s, pal.hudBg, pal.selectAccent, 1.2f * s);
                    static constexpr std::array<std::string_view, 6> kEdgeActions = {"Seq", "Def \\", "Cond", "Msg", "Auto", "Del"};
                    const float bw = (bar.w - 8.0f * s) / 6.0f;
                    for (int i = 0; i < 6; ++i) {
                        const Rect br(bar.x + 4.0f * s + static_cast<float>(i) * bw, bar.y + 3.0f * s, bw - 2.0f * s, bar.h - 6.0f * s);
                        canvas.DrawRoundRect(br, 3.0f * s, (i == 5) ? pal.endEventFill : pal.hudHover);
                        canvas.DrawTextBox(kEdgeActions[static_cast<size_t>(i)], br, 8.6f * s, (i == 5) ? pal.endEventStroke : pal.textPrimary, 0.5f, 0.5f);
                    }
                }
            }
        } else if (state.selectedLaneId != 0) {
            if (const BpmnLane* selLane = diagram.FindLane(state.selectedLaneId)) {
                const Rect lsr = WorldToScreenRect(selLane->rect, viewRect, pan, zoom);
                const Rect bar(std::max(viewRect.x + 58.0f * s, lsr.x + 44.0f * s), std::max(viewRect.y + 48.0f * s, lsr.y - 30.0f * s), 232.0f * s, 26.0f * s);
                canvas.DrawRoundRect(bar, 5.0f * s, pal.hudBg, pal.selectAccent, 1.2f * s);
                static constexpr std::array<std::string_view, 4> kLaneActions = {"+Above", "+Below", "Split /2", "Del Lane"};
                const float bw = (bar.w - 8.0f * s) / 4.0f;
                for (int i = 0; i < 4; ++i) {
                    const Rect br(bar.x + 4.0f * s + static_cast<float>(i) * bw, bar.y + 3.0f * s, bw - 2.0f * s, bar.h - 6.0f * s);
                    canvas.DrawRoundRect(br, 3.0f * s, (i == 3) ? pal.endEventFill : pal.hudHover);
                    canvas.DrawTextBox(kLaneActions[static_cast<size_t>(i)], br, 9.0f * s, (i == 3) ? pal.endEventStroke : pal.textPrimary, 0.5f, 0.5f);
                }
            }
        }
    }

    // 12. Inline Label Editor Overlay (`Double-Click` / `F2`)
    if (state.editingNodeId != 0 || state.editingEdgeId != 0 || state.editingLaneId != 0) {
        Rect editBox(viewRect.Center().x - 90.0f * s, viewRect.Center().y - 16.0f * s, 180.0f * s, 30.0f * s);
        if (const BpmnNode* en = diagram.FindNode(state.editingNodeId)) {
            const Rect nsr = WorldToScreenRect(en->Bounds(), viewRect, pan, zoom);
            editBox = Rect(nsr.Center().x - 86.0f * s, nsr.Center().y - 15.0f * s, 172.0f * s, 30.0f * s);
        } else if (const BpmnEdge* ee = diagram.FindEdge(state.editingEdgeId)) {
            const BpmnNode* fn = diagram.FindNode(ee->fromNodeId);
            const BpmnNode* tn = diagram.FindNode(ee->toNodeId);
            if (fn && tn) {
                const auto wpts = BuildEdgeWorldPoints(diagram, *fn, *tn, *ee, opts.routingMode);
                Vec2 wMid{0.0f, 0.0f};
                (void)SamplePolylineAtProgress(wpts, 0.5f, &wMid);
                const Vec2 sMid = WorldToScreen(wMid, viewRect, pan, zoom);
                editBox = Rect(sMid.x - 80.0f * s, sMid.y - 15.0f * s, 160.0f * s, 30.0f * s);
            }
        } else if (const BpmnLane* el = diagram.FindLane(state.editingLaneId)) {
            const Rect lsr = WorldToScreenRect(el->rect, viewRect, pan, zoom);
            editBox = Rect(lsr.x + 48.0f * s, lsr.y + 6.0f * s, 180.0f * s, 28.0f * s);
        }
        canvas.DrawRoundRect(editBox, 5.0f * s, pal.hudBg, pal.selectAccent, 2.0f * s);
        const std::string displayStr = payload->cursorBlinkOn
            ? (payload->activeEditBuffer + "|")
            : payload->activeEditBuffer;
        canvas.DrawTextBox(displayStr, Rect(editBox.x + 8.0f * s, editBox.y, editBox.w - 16.0f * s, editBox.h), 11.5f * s, pal.textPrimary, 0.0f, 0.5f);
    }

    // 13. Left Floating `bpmn.io` Tool Palette
    if (opts.showLeftPalette && !opts.readOnly) {
        const Rect palRect = ComputeLeftPaletteRect(viewRect, opts.showTopBar, s);
        canvas.DrawRoundRect(palRect, 7.0f * s, pal.hudBg, pal.hudBorder, 1.2f * s);
        float cy = palRect.y + 5.0f * s;
        const float btnH = 25.0f * s;
        for (size_t idx = 0; idx < kPaletteItems.size(); ++idx) {
            const auto& item = kPaletteItems[idx];
            if (cy + btnH > palRect.Bottom() - 2.0f * s) break;
            const Rect br(palRect.x + 4.0f * s, cy, palRect.w - 8.0f * s, btnH);
            const bool isActive = (state.activeTool == item.tool);
            const bool isHov = (state.hoveredPaletteIdx == static_cast<int>(idx));
            if (isActive || isHov) {
                canvas.DrawRoundRect(br, 4.0f * s, isActive ? pal.hudActiveBg : pal.hudHover, isActive ? pal.selectAccent : pal.hudBorder, 1.1f * s);
            }
            DrawPaletteButtonIcon(canvas, br, item.tool, (isActive || isHov) ? pal.selectAccent : pal.textPrimary, pal, s);
            if (isHov) {
                const float tfs = 10.0f * s;
                const Vec2 tm = canvas.MeasureText(item.tooltip, tfs);
                const Rect tipRect(palRect.Right() + 8.0f * s, br.Center().y - tm.y * 0.5f - 3.5f * s, tm.x + 12.0f * s, tm.y + 7.0f * s);
                canvas.DrawRoundRect(tipRect, 4.0f * s, pal.hudBg, pal.selectAccent, 1.0f * s);
                canvas.DrawTextBox(item.tooltip, tipRect, tfs, pal.textPrimary, 0.5f, 0.5f);
            }
            cy += btnH + 2.0f * s;
            if (item.separatorAfter) {
                canvas.DrawLine({palRect.x + 6.0f * s, cy + 1.5f * s}, {palRect.Right() - 6.0f * s, cy + 1.5f * s}, pal.hudBorder, 1.0f * s);
                cy += 5.0f * s;
            }
        }
    }

    // 14. Top Floating BPMN Status & Token Simulation Bar
    if (opts.showTopBar) {
        const Rect topBar = ComputeTopBarRect(viewRect, s);
        canvas.DrawRoundRect(topBar, 6.0f * s, pal.hudBg, pal.hudBorder, 1.2f * s);

        const std::string headerStr = std::format(
            "BPMN 2.0  |  {}   ({} nodes, {} flows, {} lanes)",
            diagram.name,
            diagram.nodes.size(),
            diagram.edges.size(),
            diagram.lanes.size()
        );
        canvas.DrawTextBox(
            headerStr,
            Rect(topBar.x + 12.0f * s, topBar.y, topBar.w * 0.48f, topBar.h),
            11.0f * s,
            pal.textPrimary,
            0.0f,
            0.5f
        );

        float rx = topBar.Right() - 6.0f * s;
        auto drawTopPill = [&](std::string_view text, float w, bool active, Color activeCol) -> Rect {
            rx -= w;
            const Rect pr(rx, topBar.y + 4.0f * s, w, topBar.h - 8.0f * s);
            rx -= 5.0f * s;
            canvas.DrawRoundRect(
                pr,
                4.0f * s,
                active ? activeCol.WithAlpha(42) : pal.hudHover,
                active ? activeCol : pal.hudBorder,
                1.1f * s
            );
            canvas.DrawTextBox(text, pr, 10.0f * s, active ? activeCol : pal.textPrimary, 0.5f, 0.5f);
            return pr;
        };

        if (opts.tokenSimulation) {
            (void)drawTopPill("+ Trigger Token", 96.0f * s, true, pal.simActiveGreen);
        }
        (void)drawTopPill(
            opts.tokenSimulation ? "● Token Sim: ON" : "▶ Token Sim",
            106.0f * s,
            opts.tokenSimulation,
            pal.simActiveGreen
        );
        (void)drawTopPill(
            (opts.routingMode == BpmnRoutingMode::Orthogonal) ? "Route: Ortho" : "Route: Bezier",
            86.0f * s,
            false,
            pal.selectAccent
        );
        (void)drawTopPill("Redo", 44.0f * s, !state.redoStack.empty(), pal.selectAccent);
        (void)drawTopPill("Undo", 44.0f * s, !state.undoStack.empty(), pal.selectAccent);
        (void)drawTopPill("XML", 44.0f * s, false, pal.selectAccent);
    }

    // 15. Bottom-Left Zoom & Fit HUD
    if (opts.showZoomControls) {
        const Rect zHud = ComputeZoomHudRect(viewRect, s);
        canvas.DrawRoundRect(zHud, 5.0f * s, pal.hudBg, pal.hudBorder, 1.2f * s);
        const float btnW = 28.0f * s;
        const Rect minusR(zHud.x + 2.0f * s, zHud.y + 2.0f * s, btnW, zHud.h - 4.0f * s);
        const Rect pctR(minusR.Right() + 2.0f * s, zHud.y + 2.0f * s, 58.0f * s, zHud.h - 4.0f * s);
        const Rect plusR(pctR.Right() + 2.0f * s, zHud.y + 2.0f * s, btnW, zHud.h - 4.0f * s);
        const Rect fitR(plusR.Right() + 4.0f * s, zHud.y + 2.0f * s, zHud.Right() - plusR.Right() - 6.0f * s, zHud.h - 4.0f * s);

        canvas.DrawRoundRect(minusR, 3.0f * s, pal.hudHover);
        canvas.DrawTextBox("-", minusR, 12.0f * s, pal.textPrimary, 0.5f, 0.5f);
        char zBuf[24];
        std::snprintf(zBuf, sizeof(zBuf), "%.0f%%", zoom * 100.0f);
        canvas.DrawTextBox(zBuf, pctR, 10.5f * s, pal.textSecondary, 0.5f, 0.5f);
        canvas.DrawRoundRect(plusR, 3.0f * s, pal.hudHover);
        canvas.DrawTextBox("+", plusR, 12.0f * s, pal.textPrimary, 0.5f, 0.5f);
        canvas.DrawRoundRect(fitR, 3.0f * s, pal.hudHover);
        canvas.DrawTextBox("Fit", fitR, 10.0f * s, pal.selectAccent, 0.5f, 0.5f);
    }

    // 16. Bottom-Right Interactive Minimap
    if (opts.showMinimap && viewRect.w >= 420.0f * s && viewRect.h >= 260.0f * s) {
        const Rect mm = ComputeMinimapRect(viewRect, s);
        canvas.DrawRoundRect(mm, 6.0f * s, pal.hudBg, pal.hudBorder, 1.2f * s);
        canvas.PushClip(mm);

        const Rect wb = ComputeDiagramWorldBounds(diagram);
        const float pad = 8.0f * s;
        const float sx = (mm.w - pad * 2.0f) / std::max(100.0f, wb.w);
        const float sy = (mm.h - pad * 2.0f) / std::max(100.0f, wb.h);
        const float ms = std::min(sx, sy);
        const float ox = mm.x + (mm.w - wb.w * ms) * 0.5f - wb.x * ms;
        const float oy = mm.y + (mm.h - wb.h * ms) * 0.5f - wb.y * ms;

        for (const auto& l : diagram.lanes) {
            canvas.DrawRect(Rect(ox + l.rect.x * ms, oy + l.rect.y * ms, l.rect.w * ms, l.rect.h * ms), pal.laneHeaderBg.WithAlpha(110));
        }
        for (const auto& n : diagram.nodes) {
            Color nc = pal.taskAccent;
            if (n.type == BpmnNodeType::StartEvent) nc = pal.startEventStroke;
            else if (n.type == BpmnNodeType::EndEvent) nc = pal.endEventStroke;
            else if (n.type == BpmnNodeType::Gateway) nc = pal.gatewayStroke;
            canvas.DrawRect(Rect(ox + n.pos.x * ms, oy + n.pos.y * ms, std::max(2.5f, n.size.x * ms), std::max(2.5f, n.size.y * ms)), nc);
        }

        const Vec2 camTL = ScreenToWorld({viewRect.x, viewRect.y}, viewRect, pan, zoom);
        const Vec2 camBR = ScreenToWorld({viewRect.Right(), viewRect.Bottom()}, viewRect, pan, zoom);
        const Rect camRect(
            ox + camTL.x * ms,
            oy + camTL.y * ms,
            (camBR.x - camTL.x) * ms,
            (camBR.y - camTL.y) * ms
        );
        canvas.DrawRoundRect(camRect, 2.0f * s, pal.selectGlow, pal.selectAccent, 1.2f * s);
        canvas.PopClip();
    }

    canvas.PopClip();
}

} // namespace

Vec2 BpmnEditor::Measure(
    const BpmnEditorOptions& opts,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme,
    float availableWidth
) {
    const ElementSpec spec = MakeBpmnElementSpec(opts, size, theme, opts.disabled);
    const float defaultW = spec.control.width.has_value()
        ? theme.Scale(*spec.control.width)
        : (availableWidth > 0.0f ? availableWidth : theme.Scale(880.0f));
    const float defaultH = spec.control.height.has_value()
        ? theme.Scale(*spec.control.height)
        : theme.Scale(520.0f);
    const Vec2 ctrlSize = Control::Measure({defaultW, defaultH}, spec.control, opts.stretch, size, theme);
    return Element::Measure(ctrlSize, spec, canvas, theme, availableWidth);
}

BpmnEditorResponse BpmnEditor::Run(
    Context& ctx,
    BpmnDiagram& diagram,
    const BpmnEditorOptions& opts,
    std::source_location loc
) {
    const Id id = ctx.MakeChildId(loc);
    const bool isDisabled = opts.disabled || ctx.IsDisabled();
    const UiSize effectiveSize = ctx.GetCurrentSize();
    const UiTheme& theme = ctx.GetTheme();
    const float s = theme.Scale(1.0f);
    ICanvas* canvas = ctx.GetCanvas();

    diagram.SyncBoundaryEvents();

    Response baseRes = ctx.Interact(id);
    ctx.RegisterFocusable(id, isDisabled);

    BpmnEditorResponse resp{};
    static_cast<Response&>(resp) = baseRes;

    auto& state = ctx.GetOrCreateState<BpmnEditorState>(id);

    BpmnEditorOptions effOpts = opts;
    if (opts.tokenSimulationPtr) effOpts.tokenSimulation = *opts.tokenSimulationPtr;
    if (opts.routingModePtr) effOpts.routingMode = *opts.routingModePtr;
    if (opts.selectedNodeIdPtr && *opts.selectedNodeIdPtr != state.primaryNodeId) {
        if (diagram.FindNode(*opts.selectedNodeIdPtr)) {
            state.SelectSingleNode(*opts.selectedNodeIdPtr);
        }
    }

    Rect prevBounds = baseRes.rect;
    if (prevBounds.w <= 1.0f || prevBounds.h <= 1.0f) {
        prevBounds = ctx.GetParentPrevFrameBounds();
    }
    const ElementSpec elemSpec = MakeBpmnElementSpec(effOpts, effectiveSize, theme, isDisabled);
    const ElementGeometry prevElemGeom = Element::Resolve(prevBounds, {prevBounds.w, prevBounds.h}, elemSpec, canvas, theme);
    const Rect viewRect = (prevElemGeom.innerControlRect.w > 1.0f && prevElemGeom.innerControlRect.h > 1.0f)
        ? prevElemGeom.innerControlRect
        : prevBounds;
    resp.canvasRect = viewRect;

    const uint32_t firstNodeId = diagram.nodes.empty() ? 0 : diagram.nodes.front().id;
    if ((!state.initialized || state.lastDiagramNodeCount != diagram.nodes.size() || state.lastFirstNodeId != firstNodeId) &&
        viewRect.w > 100.0f && viewRect.h > 100.0f) {
        if (!state.initialized || state.lastFirstNodeId != firstNodeId) {
            FitDiagramToViewport(diagram, viewRect, effOpts.showLeftPalette ? 54.0f * s : 0.0f, state);
            state.initialized = true;
        }
        state.lastDiagramNodeCount = diagram.nodes.size();
        state.lastFirstNodeId = firstNodeId;
    }

    for (auto& n : diagram.nodes) {
        if (n.type == BpmnNodeType::Gateway && n.gatewayType == BpmnGatewayType::Exclusive) {
            bool valid = false;
            uint32_t firstOut = 0;
            for (const auto& e : diagram.edges) {
                if (e.fromNodeId == n.id && e.flowType != BpmnFlowType::Association) {
                    if (firstOut == 0) firstOut = e.id;
                    if (e.id == n.activeOutgoingEdgeId) valid = true;
                }
            }
            if (!valid) n.activeOutgoingEdgeId = firstOut;
        }
    }

    const double nowSec = ctx.GetCurrentTime();
    if (effOpts.tokenSimulation) {
        const float dtSec = (state.lastSimTickSec > 0.0)
            ? static_cast<float>(std::clamp(nowSec - state.lastSimTickSec, 0.004, 0.08))
            : 0.016f;
        state.lastSimTickSec = nowSec;
        StepTokenSimulation(diagram, state, dtSec, effOpts.simulationSpeed, effOpts.autoSpawnTokens);
        ctx.RequestRepaint(2);
    } else {
        state.lastSimTickSec = nowSec;
        if (!diagram.tokens.empty()) {
            diagram.ResetSimulationStats();
        }
    }

    std::string activeEditStr{};
    size_t activeEditCursor = 0;
    const Id editId = Id(id.value ^ 0x9E3779B97F4A7C15ULL);
    if (state.editingNodeId != 0 || state.editingEdgeId != 0 || state.editingLaneId != 0) {
        std::string initialText{};
        if (const BpmnNode* en = diagram.FindNode(state.editingNodeId)) initialText = en->label;
        else if (const BpmnEdge* ee = diagram.FindEdge(state.editingEdgeId)) initialText = ee->label;
        else if (const BpmnLane* el = diagram.FindLane(state.editingLaneId)) initialText = el->name;

        bool textChanged = false;
        bool textSubmitted = false;
        const std::string_view bufView = ctx.GetOrStartTextEdit(editId, initialText, activeEditCursor, textChanged, textSubmitted, false);
        activeEditStr = std::string(bufView);
        if (textChanged) {
            if (BpmnNode* en = diagram.FindNode(state.editingNodeId)) en->label = activeEditStr;
            else if (BpmnEdge* ee = diagram.FindEdge(state.editingEdgeId)) ee->label = activeEditStr;
            else if (BpmnLane* el = diagram.FindLane(state.editingLaneId)) el->name = activeEditStr;
            resp.changed = true;
            resp.diagramModified = true;
        }
        if (textSubmitted || ctx.ConsumeKey(Key::Escape) || !ctx.HasActiveTextEdit(editId)) {
            state.editingNodeId = 0;
            state.editingEdgeId = 0;
            state.editingLaneId = 0;
            ctx.ClearFocus();
        }
    }

    if (viewRect.w > 1.0f && viewRect.h > 1.0f && !isDisabled) {
        const Vec2 mouse = ctx.GetMousePos();
        const bool hovered = viewRect.Contains(mouse);
        resp.hovered = hovered;
        resp.mouseWorldPos = ScreenToWorld(mouse, viewRect, state.pan, state.zoom);

        const bool lmbClicked = hovered && ctx.IsMouseClicked();
        const bool lmbDown = ctx.IsMouseDown();
        const bool lmbReleased = ctx.IsMouseReleased();
        const bool doubleClicked = hovered && baseRes.doubleClicked;
        const KeyModifiers mods = ctx.GetKeyModifiers();

        bool hudConsumedClick = false;

        // A. Top Bar interactions
        if (effOpts.showTopBar && lmbClicked) {
            const Rect topBar = ComputeTopBarRect(viewRect, s);
            if (topBar.Contains(mouse)) {
                hudConsumedClick = true;
                float rx = topBar.Right() - 6.0f * s;
                auto hitTopPill = [&](float w) -> bool {
                    rx -= w;
                    const Rect pr(rx, topBar.y + 4.0f * s, w, topBar.h - 8.0f * s);
                    rx -= 5.0f * s;
                    return pr.Contains(mouse);
                };
                if (effOpts.tokenSimulation && hitTopPill(96.0f * s)) {
                    for (const auto& n : diagram.nodes) {
                        if (n.type == BpmnNodeType::StartEvent) {
                            SpawnTokenFromStartEvent(diagram, n, state);
                            break;
                        }
                    }
                    ctx.RequestRepaint(2);
                } else if (hitTopPill(106.0f * s)) {
                    effOpts.tokenSimulation = !effOpts.tokenSimulation;
                    if (opts.tokenSimulationPtr) *opts.tokenSimulationPtr = effOpts.tokenSimulation;
                    if (effOpts.tokenSimulation) {
                        state.ClearSelection();
                        for (const auto& n : diagram.nodes) {
                            if (n.type == BpmnNodeType::StartEvent) {
                                SpawnTokenFromStartEvent(diagram, n, state);
                                break;
                            }
                        }
                    } else {
                        diagram.ResetSimulationStats();
                    }
                    resp.changed = true;
                    ctx.RequestRepaint(2);
                } else if (hitTopPill(86.0f * s)) {
                    effOpts.routingMode = (effOpts.routingMode == BpmnRoutingMode::Orthogonal)
                        ? BpmnRoutingMode::Bezier
                        : BpmnRoutingMode::Orthogonal;
                    if (opts.routingModePtr) *opts.routingModePtr = effOpts.routingMode;
                    resp.changed = true;
                    ctx.RequestRepaint(2);
                } else if (hitTopPill(44.0f * s)) {
                    if (state.Redo(diagram)) {
                        resp.changed = true;
                        resp.diagramModified = true;
                    }
                } else if (hitTopPill(44.0f * s)) {
                    if (state.Undo(diagram)) {
                        resp.changed = true;
                        resp.diagramModified = true;
                    }
                } else if (hitTopPill(44.0f * s)) {
                    ctx.SetClipboardText(BpmnEditor::ExportBpmnXml(diagram));
                }
            }
        }

        // B. Left Palette hover, click & drag-to-canvas interactions
        state.hoveredPaletteIdx = -1;
        state.hoveredContextPadBtn = -1;
        state.hoveredReplaceMenuItem = -1;

        const Rect palRect = (effOpts.showLeftPalette && !effOpts.readOnly)
            ? ComputeLeftPaletteRect(viewRect, effOpts.showTopBar, s)
            : Rect(0.0f, 0.0f, 0.0f, 0.0f);

        if (hovered && effOpts.showLeftPalette && !effOpts.readOnly) {
            if (palRect.Contains(mouse)) {
                float cy = palRect.y + 5.0f * s;
                const float btnH = 25.0f * s;
                for (size_t i = 0; i < kPaletteItems.size(); ++i) {
                    const auto& item = kPaletteItems[i];
                    const Rect br(palRect.x + 4.0f * s, cy, palRect.w - 8.0f * s, btnH);
                    if (br.Contains(mouse)) {
                        state.hoveredPaletteIdx = static_cast<int>(i);
                        if (!hudConsumedClick && lmbClicked) {
                            hudConsumedClick = true;
                            state.activeTool = item.tool;
                            state.paletteDragCreating = (item.tool >= BpmnTool::CreateStartEvent);
                            state.replaceMenuOpen = false;
                            ctx.RequestRepaint(2);
                        }
                        break;
                    }
                    cy += btnH + 2.0f * s;
                    if (item.separatorAfter) cy += 5.0f * s;
                }
                if (lmbClicked) hudConsumedClick = true;
            }
        }
        if (state.paletteDragCreating && lmbReleased && palRect.Contains(mouse)) {
            state.paletteDragCreating = false;
        }

        // C. Bottom-Left Zoom HUD interactions
        if (!hudConsumedClick && effOpts.showZoomControls && lmbClicked) {
            const Rect zHud = ComputeZoomHudRect(viewRect, s);
            if (zHud.Contains(mouse)) {
                hudConsumedClick = true;
                const float btnW = 28.0f * s;
                const Rect minusR(zHud.x + 2.0f * s, zHud.y + 2.0f * s, btnW, zHud.h - 4.0f * s);
                const Rect pctR(minusR.Right() + 2.0f * s, zHud.y + 2.0f * s, 58.0f * s, zHud.h - 4.0f * s);
                const Rect plusR(pctR.Right() + 2.0f * s, zHud.y + 2.0f * s, btnW, zHud.h - 4.0f * s);
                const Rect fitR(plusR.Right() + 4.0f * s, zHud.y + 2.0f * s, zHud.Right() - plusR.Right() - 6.0f * s, zHud.h - 4.0f * s);

                const Vec2 centerScreen = viewRect.Center();
                const Vec2 centerWorld = ScreenToWorld(centerScreen, viewRect, state.pan, state.zoom);
                if (minusR.Contains(mouse)) {
                    state.zoom = std::clamp(state.zoom / 1.18f, 0.25f, 2.5f);
                    state.pan.x = centerScreen.x - viewRect.x - centerWorld.x * state.zoom;
                    state.pan.y = centerScreen.y - viewRect.y - centerWorld.y * state.zoom;
                } else if (plusR.Contains(mouse)) {
                    state.zoom = std::clamp(state.zoom * 1.18f, 0.25f, 2.5f);
                    state.pan.x = centerScreen.x - viewRect.x - centerWorld.x * state.zoom;
                    state.pan.y = centerScreen.y - viewRect.y - centerWorld.y * state.zoom;
                } else if (pctR.Contains(mouse)) {
                    state.zoom = 1.0f;
                    state.pan.x = centerScreen.x - viewRect.x - centerWorld.x * state.zoom;
                    state.pan.y = centerScreen.y - viewRect.y - centerWorld.y * state.zoom;
                } else if (fitR.Contains(mouse)) {
                    FitDiagramToViewport(diagram, viewRect, effOpts.showLeftPalette ? 54.0f * s : 0.0f, state);
                }
                ctx.RequestRepaint(2);
            }
        }

        // D. Bottom-Right Minimap click/drag navigation
        if (!hudConsumedClick && effOpts.showMinimap && viewRect.w >= 420.0f * s && viewRect.h >= 260.0f * s) {
            const Rect mm = ComputeMinimapRect(viewRect, s);
            if (mm.Contains(mouse) && (lmbClicked || (lmbDown && !state.draggingNodes && !state.panningCanvas && !state.resizingNode && !state.draggingEdgeSegment))) {
                hudConsumedClick = true;
                const Rect wb = ComputeDiagramWorldBounds(diagram);
                const float pad = 8.0f * s;
                const float sx = (mm.w - pad * 2.0f) / std::max(100.0f, wb.w);
                const float sy = (mm.h - pad * 2.0f) / std::max(100.0f, wb.h);
                const float ms = std::min(sx, sy);
                const float ox = mm.x + (mm.w - wb.w * ms) * 0.5f - wb.x * ms;
                const float oy = mm.y + (mm.h - wb.h * ms) * 0.5f - wb.y * ms;
                const float targetWorldX = (mouse.x - ox) / std::max(1e-4f, ms);
                const float targetWorldY = (mouse.y - oy) / std::max(1e-4f, ms);
                state.pan.x = viewRect.w * 0.5f - targetWorldX * state.zoom;
                state.pan.y = viewRect.h * 0.5f - targetWorldY * state.zoom;
                ctx.RequestRepaint(2);
            }
        }

        // E. Context Pad & Replace Menu hover + click interactions
        if (hovered && effOpts.showContextPad && !effOpts.readOnly && !effOpts.tokenSimulation) {
            if (state.primaryNodeId != 0 && state.selectedNodes.size() <= 1) {
                if (BpmnNode* selNode = diagram.FindNode(state.primaryNodeId)) {
                    const Rect nsr = WorldToScreenRect(selNode->Bounds(), viewRect, state.pan, state.zoom);
                    const auto padActions = GetContextPadActionsForNode(*selNode);
                    const int numCols = std::min(4, static_cast<int>(padActions.size()));
                    const int numRows = (static_cast<int>(padActions.size()) + 3) / 4;
                    const float padX = std::min(viewRect.Right() - 156.0f * s, nsr.Right() + 10.0f * s);
                    const float padY = std::clamp(nsr.y - 4.0f * s, viewRect.y + 48.0f * s, viewRect.Bottom() - 100.0f * s);
                    const float btnS = 26.0f * s;
                    const float gap = 4.0f * s;
                    const Rect padFrame(
                        padX - 4.0f * s,
                        padY - 4.0f * s,
                        (btnS + gap) * static_cast<float>(numCols) + 4.0f * s,
                        (btnS + gap) * static_cast<float>(numRows) + 4.0f * s
                    );

                    if (state.replaceMenuOpen) {
                        const Rect popRect(padFrame.x, padFrame.Bottom() + 6.0f * s, 210.0f * s, 224.0f * s);
                        if (popRect.Contains(mouse)) {
                            const float rowH = 23.0f * s;
                            const int idx = static_cast<int>((mouse.y - (popRect.y + 27.0f * s)) / rowH);
                            const int maxItems = (selNode->type == BpmnNodeType::Task || selNode->type == BpmnNodeType::SubProcess) ? 8
                                               : (selNode->type == BpmnNodeType::Gateway ? 4 : 7);
                            if (idx >= 0 && idx < maxItems) {
                                state.hoveredReplaceMenuItem = idx;
                                if (!hudConsumedClick && lmbClicked) {
                                    hudConsumedClick = true;
                                    state.PushUndo(diagram);
                                    BpmnNode* mutNode = diagram.FindNode(state.primaryNodeId);
                                    if (mutNode) {
                                        if (mutNode->type == BpmnNodeType::Task || mutNode->type == BpmnNodeType::SubProcess) {
                                            const Vec2 oldCenter = mutNode->Center();
                                            if (idx == 6) {
                                                mutNode->type = BpmnNodeType::SubProcess;
                                                mutNode->isExpanded = false;
                                                mutNode->size = BpmnDiagram::DefaultNodeSize(BpmnNodeType::SubProcess);
                                                mutNode->pos = {oldCenter.x - mutNode->size.x * 0.5f, oldCenter.y - mutNode->size.y * 0.5f};
                                            } else if (idx == 7) {
                                                mutNode->type = BpmnNodeType::SubProcess;
                                                mutNode->isExpanded = true;
                                                mutNode->size = {320.0f, 180.0f};
                                                mutNode->pos = {oldCenter.x - mutNode->size.x * 0.5f, oldCenter.y - mutNode->size.y * 0.5f};
                                            } else {
                                                mutNode->type = BpmnNodeType::Task;
                                                mutNode->isExpanded = false;
                                                mutNode->size = BpmnDiagram::DefaultNodeSize(BpmnNodeType::Task);
                                                mutNode->pos = {oldCenter.x - mutNode->size.x * 0.5f, oldCenter.y - mutNode->size.y * 0.5f};
                                                static constexpr std::array<BpmnTaskType, 6> kTaskTypes = {
                                                    BpmnTaskType::User, BpmnTaskType::Service, BpmnTaskType::Send,
                                                    BpmnTaskType::Receive, BpmnTaskType::Script, BpmnTaskType::BusinessRule
                                                };
                                                mutNode->taskType = kTaskTypes[static_cast<size_t>(idx)];
                                            }
                                            diagram.SyncBoundaryEvents();
                                        } else if (mutNode->type == BpmnNodeType::Gateway && idx < 4) {
                                            mutNode->gatewayType = static_cast<BpmnGatewayType>(idx);
                                        } else if (idx < 7) {
                                            mutNode->eventDef = static_cast<BpmnEventDefinition>(idx);
                                        }
                                        resp.changed = true;
                                        resp.diagramModified = true;
                                    }
                                    state.replaceMenuOpen = false;
                                }
                            }
                            if (lmbClicked) hudConsumedClick = true;
                        }
                    }

                    if (padFrame.Contains(mouse)) {
                        for (size_t idx = 0; idx < padActions.size(); ++idx) {
                            const int actionId = padActions[idx];
                            const int row = static_cast<int>(idx) / 4;
                            const int col = static_cast<int>(idx) % 4;
                            const Rect br(padX + static_cast<float>(col) * (btnS + gap), padY + static_cast<float>(row) * (btnS + gap), btnS, btnS);
                            if (!br.Contains(mouse)) continue;
                            state.hoveredContextPadBtn = actionId;
                            if (!hudConsumedClick && lmbClicked) {
                                hudConsumedClick = true;
                                const uint32_t srcId = selNode->id;
                                const Vec2 srcPos = selNode->pos;
                                const Vec2 srcSize = selNode->size;
                                const Vec2 srcCenter = selNode->Center();
                                const uint32_t srcLane = selNode->laneId;

                                auto appendQuickNode = [&](BpmnNodeType newType, std::string label, BpmnFlowType flow) {
                                    state.PushUndo(diagram);
                                    BpmnNode nn{};
                                    nn.type = newType;
                                    nn.label = std::move(label);
                                    nn.size = BpmnDiagram::DefaultNodeSize(newType);
                                    if (newType == BpmnNodeType::TextAnnotation) {
                                        const float targetCX = SnapValue(srcCenter.x + srcSize.x * 0.5f + 70.0f, effOpts.gridStep);
                                        const float targetCY = SnapValue(srcPos.y - 45.0f, effOpts.gridStep);
                                        nn.pos = {targetCX - nn.size.x * 0.5f, targetCY - nn.size.y * 0.5f};
                                    } else {
                                        const float gapX = (newType == BpmnNodeType::Task || newType == BpmnNodeType::SubProcess) ? 60.0f : 50.0f;
                                        const float targetCX = SnapValue(srcPos.x + srcSize.x + gapX + nn.size.x * 0.5f, effOpts.gridStep);
                                        nn.pos = {targetCX - nn.size.x * 0.5f, srcCenter.y - nn.size.y * 0.5f};
                                    }
                                    nn.laneId = srcLane;
                                    if (newType == BpmnNodeType::IntermediateEvent) nn.eventDef = BpmnEventDefinition::Timer;
                                    const uint32_t newId = diagram.AddNode(std::move(nn));
                                    diagram.AddEdge(srcId, newId, flow);
                                    state.SelectSingleNode(newId);
                                    resp.changed = true;
                                    resp.diagramModified = true;
                                };

                                if (actionId == 0) appendQuickNode(BpmnNodeType::Task, "Next Task", BpmnFlowType::SequenceFlow);
                                else if (actionId == 1) appendQuickNode(BpmnNodeType::Gateway, "Decision?", BpmnFlowType::SequenceFlow);
                                else if (actionId == 2) appendQuickNode(BpmnNodeType::EndEvent, "Completed", BpmnFlowType::SequenceFlow);
                                else if (actionId == 3) appendQuickNode(BpmnNodeType::IntermediateEvent, "Wait Timer", BpmnFlowType::SequenceFlow);
                                else if (actionId == 4) {
                                    state.connectingWire = true;
                                    state.connectFromNodeId = srcId;
                                    state.connectFromPort = BpmnPortSide::Auto;
                                    state.connectEndWorld = resp.mouseWorldPos;
                                } else if (actionId == 5) {
                                    state.replaceMenuOpen = !state.replaceMenuOpen;
                                } else if (actionId == 6) {
                                    appendQuickNode(BpmnNodeType::TextAnnotation, "Text annotation", BpmnFlowType::Association);
                                } else if (actionId == 7) {
                                    state.PushUndo(diagram);
                                    diagram.RemoveNode(srcId);
                                    state.ClearSelection();
                                    resp.changed = true;
                                    resp.diagramModified = true;
                                }
                                ctx.RequestRepaint(2);
                            }
                            break;
                        }
                        if (lmbClicked) hudConsumedClick = true;
                    }
                }
            } else if (!hudConsumedClick && lmbClicked && state.selectedEdgeId != 0) {
                if (BpmnEdge* selEdge = diagram.FindEdge(state.selectedEdgeId)) {
                    const BpmnNode* fn = diagram.FindNode(selEdge->fromNodeId);
                    const BpmnNode* tn = diagram.FindNode(selEdge->toNodeId);
                    if (fn && tn) {
                        const auto wpts = BuildEdgeWorldPoints(diagram, *fn, *tn, *selEdge, effOpts.routingMode);
                        Vec2 wMid{0.0f, 0.0f};
                        (void)SamplePolylineAtProgress(wpts, 0.5f, &wMid);
                        const Vec2 sMid = WorldToScreen(wMid, viewRect, state.pan, state.zoom);
                        const Rect bar(sMid.x - 110.0f * s, sMid.y - 36.0f * s, 220.0f * s, 26.0f * s);
                        if (bar.Contains(mouse)) {
                            hudConsumedClick = true;
                            const float bw = (bar.w - 8.0f * s) / 6.0f;
                            const int idx = std::clamp(static_cast<int>((mouse.x - (bar.x + 4.0f * s)) / bw), 0, 5);
                            state.PushUndo(diagram);
                            if (idx == 0) selEdge->flowType = BpmnFlowType::SequenceFlow;
                            else if (idx == 1) selEdge->flowType = BpmnFlowType::DefaultFlow;
                            else if (idx == 2) selEdge->flowType = BpmnFlowType::ConditionalFlow;
                            else if (idx == 3) selEdge->flowType = BpmnFlowType::MessageFlow;
                            else if (idx == 4) {
                                selEdge->waypoints.clear();
                                selEdge->labelOffset = {0.0f, 0.0f};
                            } else if (idx == 5) {
                                diagram.RemoveEdge(selEdge->id);
                                state.ClearSelection();
                            }
                            resp.changed = true;
                            resp.diagramModified = true;
                            ctx.RequestRepaint(2);
                        }
                    }
                }
            } else if (!hudConsumedClick && lmbClicked && state.selectedLaneId != 0) {
                if (BpmnLane* selLane = diagram.FindLane(state.selectedLaneId)) {
                    const Rect lsr = WorldToScreenRect(selLane->rect, viewRect, state.pan, state.zoom);
                    const Rect bar(std::max(viewRect.x + 58.0f * s, lsr.x + 44.0f * s), std::max(viewRect.y + 48.0f * s, lsr.y - 30.0f * s), 232.0f * s, 26.0f * s);
                    if (bar.Contains(mouse)) {
                        hudConsumedClick = true;
                        const float bw = (bar.w - 8.0f * s) / 4.0f;
                        const int idx = std::clamp(static_cast<int>((mouse.x - (bar.x + 4.0f * s)) / bw), 0, 3);
                        state.PushUndo(diagram);
                        const Rect curR = selLane->rect;
                        const std::string poolName = selLane->poolName;
                        if (idx == 0) {
                            const uint32_t newId = diagram.AddLane(poolName, "Upper Lane", Rect(curR.x, curR.y - 180.0f, curR.w, 180.0f));
                            state.selectedLaneId = newId;
                        } else if (idx == 1) {
                            const uint32_t newId = diagram.AddLane(poolName, "Lower Lane", Rect(curR.x, curR.Bottom(), curR.w, 180.0f));
                            state.selectedLaneId = newId;
                        } else if (idx == 2 && curR.h >= 140.0f) {
                            const float halfH = curR.h * 0.5f;
                            selLane->rect.h = halfH;
                            diagram.AddLane(poolName, selLane->name + " B", Rect(curR.x, curR.y + halfH, curR.w, halfH));
                        } else if (idx == 3) {
                            diagram.RemoveLane(selLane->id);
                            state.ClearSelection();
                        }
                        resp.changed = true;
                        resp.diagramModified = true;
                        ctx.RequestRepaint(2);
                    }
                }
            }
        }

        // F. Mouse Wheel Zoom (centered at cursor)
        if (hovered) {
            const Vec2 wheel = ctx.GetMouseWheel();
            if (std::abs(wheel.y) > 0.01f) {
                const Vec2 worldBefore = ScreenToWorld(mouse, viewRect, state.pan, state.zoom);
                const float factor = (wheel.y > 0.0f) ? 1.12f : (1.0f / 1.12f);
                state.zoom = std::clamp(state.zoom * factor, 0.25f, 2.5f);
                state.pan.x = mouse.x - viewRect.x - worldBefore.x * state.zoom;
                state.pan.y = mouse.y - viewRect.y - worldBefore.y * state.zoom;
                ctx.ConsumeMouseWheel();
                ctx.RequestRepaint(2);
            }
        }

        // G. Hit-Testing World Elements (Resize Handles, Edge Endpoints/Segments, External Labels, Nodes, Edges, Lanes)
        state.hoveredNodeId = 0;
        state.hoveredEdgeId = 0;
        state.hoveredLaneId = 0;
        state.hoveredLabelNodeId = 0;
        state.hoveredLabelEdgeId = 0;
        state.hoveredResizeCorner = -1;
        state.hoveredEdgeSegIdx = -1;
        state.hoveredEdgeEndpoint = 0;
        state.dragBoundaryHostId = 0;

        if (hovered && !hudConsumedClick) {
            const Vec2 wp = resp.mouseWorldPos;

            // G1. Check 4-Corner Resize Handles on Selected Resizable Node
            if (!effOpts.readOnly && !effOpts.tokenSimulation && state.primaryNodeId != 0 && state.selectedNodes.size() == 1) {
                if (const BpmnNode* selNode = diagram.FindNode(state.primaryNodeId)) {
                    if (IsResizableNode(selNode->type)) {
                        const Rect nsr = WorldToScreenRect(selNode->Bounds(), viewRect, state.pan, state.zoom);
                        const auto corners = GetNodeResizeHandleRects(nsr, state.zoom, s);
                        for (int c = 0; c < 4; ++c) {
                            const Rect& cr = corners[static_cast<size_t>(c)];
                            const Rect hitR(cr.x - 3.0f * s, cr.y - 3.0f * s, cr.w + 6.0f * s, cr.h + 6.0f * s);
                            if (hitR.Contains(mouse)) {
                                state.hoveredResizeCorner = c;
                                break;
                            }
                        }
                    }
                }
            }

            // G2. Check Selected Edge Endpoint Reconnect Circles & Orthogonal Segment Handles
            if (!effOpts.readOnly && !effOpts.tokenSimulation && state.hoveredResizeCorner < 0 && state.selectedEdgeId != 0) {
                if (const BpmnEdge* selEdge = diagram.FindEdge(state.selectedEdgeId)) {
                    const BpmnNode* fn = diagram.FindNode(selEdge->fromNodeId);
                    const BpmnNode* tn = diagram.FindNode(selEdge->toNodeId);
                    if (fn && tn) {
                        const auto wpts = BuildEdgeWorldPoints(diagram, *fn, *tn, *selEdge, effOpts.routingMode);
                        if (wpts.size() >= 2) {
                            const Vec2 pStart = WorldToScreen(wpts.front(), viewRect, state.pan, state.zoom);
                            const Vec2 pEnd = WorldToScreen(wpts.back(), viewRect, state.pan, state.zoom);
                            const float endR = 8.5f * s;
                            if ((mouse.x - pStart.x) * (mouse.x - pStart.x) + (mouse.y - pStart.y) * (mouse.y - pStart.y) <= endR * endR) {
                                state.hoveredEdgeEndpoint = 1;
                                state.hoveredEdgeId = selEdge->id;
                            } else if ((mouse.x - pEnd.x) * (mouse.x - pEnd.x) + (mouse.y - pEnd.y) * (mouse.y - pEnd.y) <= endR * endR) {
                                state.hoveredEdgeEndpoint = 2;
                                state.hoveredEdgeId = selEdge->id;
                            } else if (effOpts.routingMode == BpmnRoutingMode::Orthogonal) {
                                const float segTolSq = (7.0f * s) * (7.0f * s);
                                for (size_t i = 0; i + 1 < wpts.size(); ++i) {
                                    const Vec2 sp0 = WorldToScreen(wpts[i], viewRect, state.pan, state.zoom);
                                    const Vec2 sp1 = WorldToScreen(wpts[i + 1], viewRect, state.pan, state.zoom);
                                    const float segLen = std::hypot(sp1.x - sp0.x, sp1.y - sp0.y);
                                    if (segLen >= 18.0f * s && PointToSegmentDistanceSq(mouse, sp0, sp1) <= segTolSq) {
                                        state.hoveredEdgeSegIdx = static_cast<int>(i);
                                        state.hoveredEdgeId = selEdge->id;
                                        break;
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // G3. Check External Labels (Events, Gateways, DataObjects, DataStores, and Edges)
            if (state.hoveredResizeCorner < 0 && state.hoveredEdgeEndpoint == 0 && state.hoveredEdgeSegIdx < 0) {
                for (auto it = diagram.nodes.rbegin(); it != diagram.nodes.rend(); ++it) {
                    if (!IsExternalLabelNode(it->type) || it->label.empty()) continue;
                    const Rect nsr = WorldToScreenRect(it->Bounds(), viewRect, state.pan, state.zoom);
                    const Rect lsr = ComputeExternalNodeLabelScreenRect(diagram, *it, nsr, state.zoom, s);
                    if (lsr.Contains(mouse)) {
                        state.hoveredLabelNodeId = it->id;
                        state.hoveredNodeId = it->id;
                        break;
                    }
                }
                if (state.hoveredLabelNodeId == 0) {
                    for (auto it = diagram.edges.rbegin(); it != diagram.edges.rend(); ++it) {
                        if (it->label.empty()) continue;
                        const BpmnNode* fn = diagram.FindNode(it->fromNodeId);
                        const BpmnNode* tn = diagram.FindNode(it->toNodeId);
                        if (!fn || !tn) continue;
                        const auto wpts = BuildEdgeWorldPoints(diagram, *fn, *tn, *it, effOpts.routingMode);
                        std::vector<Vec2> spts;
                        spts.reserve(wpts.size());
                        for (const auto& pt : wpts) spts.push_back(WorldToScreen(pt, viewRect, state.pan, state.zoom));
                        const Rect elr = ComputeEdgeLabelScreenRect(canvas, spts, *it, state.zoom, s);
                        if (elr.w > 0.0f && elr.Contains(mouse)) {
                            state.hoveredLabelEdgeId = it->id;
                            state.hoveredEdgeId = it->id;
                            break;
                        }
                    }
                }
            }

            // G4. Check Node Bounds (Pass 0: BoundaryEvents, Pass 1: Standard Nodes, Pass 2: Expanded SubProcess Containers)
            if (state.hoveredResizeCorner < 0 && state.hoveredEdgeEndpoint == 0 && state.hoveredEdgeSegIdx < 0 &&
                state.hoveredLabelNodeId == 0 && state.hoveredLabelEdgeId == 0) {
                for (int pass = 0; pass < 3 && state.hoveredNodeId == 0; ++pass) {
                    for (auto it = diagram.nodes.rbegin(); it != diagram.nodes.rend(); ++it) {
                        const bool isBoundary = (it->type == BpmnNodeType::BoundaryEvent);
                        const bool isExpandedSub = (it->type == BpmnNodeType::SubProcess && it->isExpanded);
                        if (pass == 0 && !isBoundary) continue;
                        if (pass == 1 && (isBoundary || isExpandedSub)) continue;
                        if (pass == 2 && !isExpandedSub) continue;
                        if (it->Bounds().Contains(wp)) {
                            state.hoveredNodeId = it->id;
                            break;
                        }
                    }
                }
            }

            if (state.activeTool == BpmnTool::CreateIntermediateEvent) {
                for (const auto& n : diagram.nodes) {
                    if (n.type == BpmnNodeType::Task || n.type == BpmnNodeType::SubProcess) {
                        const Rect b = n.Bounds();
                        if (wp.x >= b.x && wp.x <= b.Right() && std::abs(wp.y - b.Bottom()) <= 18.0f) {
                            state.dragBoundaryHostId = n.id;
                            break;
                        }
                    }
                }
            }

            if (state.hoveredNodeId == 0 && state.hoveredEdgeId == 0) {
                const float tolWorld = 7.5f / std::max(0.25f, state.zoom);
                const float tolSq = tolWorld * tolWorld;
                for (const auto& edge : diagram.edges) {
                    const BpmnNode* fn = diagram.FindNode(edge.fromNodeId);
                    const BpmnNode* tn = diagram.FindNode(edge.toNodeId);
                    if (!fn || !tn) continue;
                    const auto wpts = BuildEdgeWorldPoints(diagram, *fn, *tn, edge, effOpts.routingMode);
                    for (size_t i = 0; i + 1 < wpts.size(); ++i) {
                        if (PointToSegmentDistanceSq(wp, wpts[i], wpts[i + 1]) <= tolSq) {
                            state.hoveredEdgeId = edge.id;
                            break;
                        }
                    }
                    if (state.hoveredEdgeId != 0) break;
                }
            }

            for (auto it = diagram.lanes.rbegin(); it != diagram.lanes.rend(); ++it) {
                if (it->rect.Contains(wp)) {
                    state.hoveredLaneId = it->id;
                    break;
                }
            }
        }

        // G5. Live Ghost Preview Snapping & Smart Alignment Guides during Creation Tools
        if (hovered && !effOpts.readOnly && !effOpts.tokenSimulation &&
            state.activeTool >= BpmnTool::CreateStartEvent && state.activeTool != BpmnTool::CreateLane) {
            BpmnNodeType gType = BpmnNodeType::Task;
            switch (state.activeTool) {
                case BpmnTool::CreateStartEvent:        gType = BpmnNodeType::StartEvent; break;
                case BpmnTool::CreateIntermediateEvent: gType = (state.dragBoundaryHostId != 0) ? BpmnNodeType::BoundaryEvent : BpmnNodeType::IntermediateEvent; break;
                case BpmnTool::CreateEndEvent:          gType = BpmnNodeType::EndEvent; break;
                case BpmnTool::CreateGateway:           gType = BpmnNodeType::Gateway; break;
                case BpmnTool::CreateTask:              gType = BpmnNodeType::Task; break;
                case BpmnTool::CreateSubProcess:        gType = BpmnNodeType::SubProcess; break;
                case BpmnTool::CreateDataObject:        gType = BpmnNodeType::DataObject; break;
                case BpmnTool::CreateDataStore:         gType = BpmnNodeType::DataStore; break;
                case BpmnTool::CreateAnnotation:        gType = BpmnNodeType::TextAnnotation; break;
                default: break;
            }
            const Vec2 gSize = BpmnDiagram::DefaultNodeSize(gType);
            Vec2 gCenter = resp.mouseWorldPos;
            if (effOpts.snapToGrid && !mods.alt) {
                gCenter.x = SnapValue(gCenter.x, effOpts.gridStep);
                gCenter.y = SnapValue(gCenter.y, effOpts.gridStep);
            }
            Vec2 gPos{gCenter.x - gSize.x * 0.5f, gCenter.y - gSize.y * 0.5f};
            state.activeGuides.clear();
            if (effOpts.smartGuides && state.dragBoundaryHostId == 0) {
                const float snapTol = 6.0f;
                for (const auto& other : diagram.nodes) {
                    if (other.type == BpmnNodeType::BoundaryEvent) continue;
                    const Vec2 oc = other.Center();
                    if (std::abs((gPos.x + gSize.x * 0.5f) - oc.x) <= snapTol) {
                        gPos.x = oc.x - gSize.x * 0.5f;
                        state.activeGuides.push_back(AlignmentGuide{
                            true, oc.x,
                            std::min(gPos.y, other.pos.y) - 20.0f,
                            std::max(gPos.y + gSize.y, other.pos.y + other.size.y) + 20.0f
                        });
                    }
                    if (std::abs((gPos.y + gSize.y * 0.5f) - oc.y) <= snapTol) {
                        gPos.y = oc.y - gSize.y * 0.5f;
                        state.activeGuides.push_back(AlignmentGuide{
                            false, oc.y,
                            std::min(gPos.x, other.pos.x) - 20.0f,
                            std::max(gPos.x + gSize.x, other.pos.x + other.size.x) + 20.0f
                        });
                    }
                }
            }
            state.ghostSnappedPos = gPos;
            ctx.RequestRepaint(2);
        } else if (!state.draggingNodes) {
            state.activeGuides.clear();
        }

        // H. Token Simulation Click Triggers on StartEvent / BoundaryEvent / Exclusive Gateway
        if (!hudConsumedClick && effOpts.tokenSimulation && lmbClicked) {
            for (auto& n : diagram.nodes) {
                const Rect sr = WorldToScreenRect(n.Bounds(), viewRect, state.pan, state.zoom);
                if (n.type == BpmnNodeType::StartEvent || n.type == BpmnNodeType::BoundaryEvent) {
                    const float btnW = 22.0f * s;
                    const float btnH = 18.0f * s;
                    const Rect trigRect(sr.x - btnW - 4.0f * s, sr.Center().y - btnH * 0.5f, btnW, btnH);
                    if (trigRect.Contains(mouse) || sr.Contains(mouse)) {
                        SpawnTokenFromStartEvent(diagram, n, state);
                        hudConsumedClick = true;
                        ctx.RequestRepaint(2);
                        break;
                    }
                } else if (n.type == BpmnNodeType::Gateway && n.gatewayType == BpmnGatewayType::Exclusive && sr.Contains(mouse)) {
                    std::vector<uint32_t> outIds;
                    for (const auto& e : diagram.edges) {
                        if (e.fromNodeId == n.id && e.flowType != BpmnFlowType::Association) {
                            outIds.push_back(e.id);
                        }
                    }
                    if (!outIds.empty()) {
                        auto it = std::find(outIds.begin(), outIds.end(), n.activeOutgoingEdgeId);
                        if (it == outIds.end() || ++it == outIds.end()) {
                            n.activeOutgoingEdgeId = outIds.front();
                        } else {
                            n.activeOutgoingEdgeId = *it;
                        }
                        hudConsumedClick = true;
                        ctx.RequestRepaint(2);
                        break;
                    }
                }
            }
        }

        // I. Double-Click Inline Label Editing
        if (!hudConsumedClick && !effOpts.readOnly && !effOpts.tokenSimulation && doubleClicked) {
            state.editingNodeId = 0;
            state.editingEdgeId = 0;
            state.editingLaneId = 0;
            if (state.hoveredNodeId != 0) {
                state.editingNodeId = state.hoveredNodeId;
                if (const BpmnNode* n = diagram.FindNode(state.editingNodeId)) {
                    (void)ctx.EnsureTextEdit(editId, n->label, false, false);
                }
            } else if (state.hoveredEdgeId != 0) {
                state.editingEdgeId = state.hoveredEdgeId;
                if (const BpmnEdge* e = diagram.FindEdge(state.editingEdgeId)) {
                    (void)ctx.EnsureTextEdit(editId, e->label, false, false);
                }
            } else if (state.hoveredLaneId != 0) {
                state.editingLaneId = state.hoveredLaneId;
                if (const BpmnLane* l = diagram.FindLane(state.editingLaneId)) {
                    (void)ctx.EnsureTextEdit(editId, l->name, false, false);
                }
            }
        }

        // J. Primary Canvas Tool Interactions
        if (!hudConsumedClick && hovered && (ctx.IsMiddleMouseClicked() || ctx.IsRightMouseClicked() || (lmbClicked && state.activeTool == BpmnTool::Hand))) {
            state.panningCanvas = true;
            state.panStartMouse = mouse;
            state.panStartOffset = state.pan;
        }

        if (state.panningCanvas) {
            if (ctx.IsMiddleMouseDown() || ctx.IsRightMouseDown() || (lmbDown && state.activeTool == BpmnTool::Hand)) {
                state.pan.x = state.panStartOffset.x + (mouse.x - state.panStartMouse.x);
                state.pan.y = state.panStartOffset.y + (mouse.y - state.panStartMouse.y);
                ctx.RequestRepaint(2);
            } else {
                state.panningCanvas = false;
            }
        }

        const bool triggerCreateOnCanvas =
            (!hudConsumedClick && !effOpts.readOnly && !effOpts.tokenSimulation &&
             state.activeTool >= BpmnTool::CreateStartEvent &&
             (lmbClicked || (state.paletteDragCreating && lmbReleased && hovered && !palRect.Contains(mouse))));

        if (triggerCreateOnCanvas) {
            ctx.RequestFocus(id);
            state.PushUndo(diagram);
            if (state.activeTool == BpmnTool::CreateLane) {
                float newY = SnapValue(resp.mouseWorldPos.y, effOpts.gridStep);
                float newX = SnapValue(resp.mouseWorldPos.x - 120.0f, effOpts.gridStep);
                float newW = 1280.0f;
                if (!diagram.lanes.empty()) {
                    newX = diagram.lanes.back().rect.x;
                    newY = diagram.lanes.back().rect.Bottom();
                    newW = diagram.lanes.back().rect.w;
                }
                const uint32_t lid = diagram.AddLane("Order Fulfillment Pool", "New Swimlane", Rect(newX, newY, newW, 190.0f));
                state.ClearSelection();
                state.selectedLaneId = lid;
            } else {
                BpmnNode nn{};
                switch (state.activeTool) {
                    case BpmnTool::CreateStartEvent:        nn.type = BpmnNodeType::StartEvent; nn.label = "Start"; break;
                    case BpmnTool::CreateIntermediateEvent:
                        if (state.dragBoundaryHostId != 0) {
                            nn.type = BpmnNodeType::BoundaryEvent;
                            nn.attachedToNodeId = state.dragBoundaryHostId;
                            nn.eventDef = BpmnEventDefinition::Error;
                            nn.label = "Exception";
                            if (const BpmnNode* host = diagram.FindNode(state.dragBoundaryHostId)) {
                                nn.boundaryRatioX = std::clamp((resp.mouseWorldPos.x - host->pos.x) / std::max(1.0f, host->size.x), 0.15f, 0.85f);
                            }
                        } else {
                            nn.type = BpmnNodeType::IntermediateEvent;
                            nn.eventDef = BpmnEventDefinition::Timer;
                            nn.label = "Wait Timer";
                        }
                        break;
                    case BpmnTool::CreateEndEvent:          nn.type = BpmnNodeType::EndEvent; nn.label = "End"; break;
                    case BpmnTool::CreateGateway:           nn.type = BpmnNodeType::Gateway; nn.label = "Gateway?"; break;
                    case BpmnTool::CreateTask:              nn.type = BpmnNodeType::Task; nn.label = "Task"; break;
                    case BpmnTool::CreateSubProcess:        nn.type = BpmnNodeType::SubProcess; nn.label = "Sub-Process"; break;
                    case BpmnTool::CreateDataObject:        nn.type = BpmnNodeType::DataObject; nn.label = "Payload"; break;
                    case BpmnTool::CreateDataStore:         nn.type = BpmnNodeType::DataStore; nn.label = "Database"; break;
                    case BpmnTool::CreateAnnotation:        nn.type = BpmnNodeType::TextAnnotation; nn.label = "Note / SLA"; break;
                    default: break;
                }
                nn.size = BpmnDiagram::DefaultNodeSize(nn.type);
                nn.pos = state.ghostSnappedPos;
                nn.laneId = state.hoveredLaneId;
                const uint32_t newId = diagram.AddNode(std::move(nn));
                state.SelectSingleNode(newId);
            }
            state.activeTool = BpmnTool::Select;
            state.paletteDragCreating = false;
            state.activeGuides.clear();
            resp.changed = true;
            resp.diagramModified = true;
            ctx.RequestRepaint(2);
        } else if (!hudConsumedClick && lmbClicked && state.activeTool == BpmnTool::GlobalConnect) {
            ctx.RequestFocus(id);
            if (state.hoveredNodeId != 0) {
                state.connectingWire = true;
                state.connectFromNodeId = state.hoveredNodeId;
                state.connectFromPort = BpmnPortSide::Auto;
                state.connectEndWorld = resp.mouseWorldPos;
            }
        } else if (!hudConsumedClick && lmbClicked && state.activeTool == BpmnTool::Space && !effOpts.readOnly) {
            ctx.RequestFocus(id);
            state.PushUndo(diagram);
            state.spaceToolDragging = true;
            state.spaceToolAxisLocked = false;
            state.spaceStartWorld = resp.mouseWorldPos;
            state.spaceCurrentWorld = resp.mouseWorldPos;
            state.spaceInitialNodes.clear();
            state.spaceInitialLanes.clear();
            for (const auto& n : diagram.nodes) state.spaceInitialNodes.emplace_back(n.id, n.pos);
            for (const auto& l : diagram.lanes) state.spaceInitialLanes.emplace_back(l.id, l.rect);
        } else if (!hudConsumedClick && lmbClicked && state.activeTool == BpmnTool::Lasso) {
            ctx.RequestFocus(id);
            state.lassoActive = true;
            state.lassoStartWorld = resp.mouseWorldPos;
            state.lassoEndWorld = resp.mouseWorldPos;
            if (!mods.shift && !mods.ctrl) state.ClearSelection();
        } else if (!hudConsumedClick && lmbClicked && state.activeTool == BpmnTool::Select) {
            ctx.RequestFocus(id);
            // 1. Check 4-Corner Node Resize Handle Click
            if (!effOpts.readOnly && !effOpts.tokenSimulation && state.hoveredResizeCorner >= 0 && state.primaryNodeId != 0) {
                if (const BpmnNode* rn = diagram.FindNode(state.primaryNodeId)) {
                    state.PushUndo(diagram);
                    state.resizingNode = true;
                    state.resizeCorner = state.hoveredResizeCorner;
                    state.resizeStartWorld = resp.mouseWorldPos;
                    state.resizeInitialBounds = rn->Bounds();
                }
            }
            // 2. Check Edge Endpoint Reconnect Handle Click
            else if (!effOpts.readOnly && !effOpts.tokenSimulation && state.hoveredEdgeEndpoint > 0 && state.selectedEdgeId != 0) {
                state.reconnectingEdge = true;
                state.reconnectEdgeId = state.selectedEdgeId;
                state.reconnectEndpoint = state.hoveredEdgeEndpoint;
                state.reconnectEndWorld = resp.mouseWorldPos;
                state.reconnectSnapNodeId = 0;
            }
            // 3. Check Orthogonal Edge Segment Drag Handle Click
            else if (!effOpts.readOnly && !effOpts.tokenSimulation && state.hoveredEdgeSegIdx >= 0 && state.selectedEdgeId != 0) {
                if (BpmnEdge* se = diagram.FindEdge(state.selectedEdgeId)) {
                    const BpmnNode* fn = diagram.FindNode(se->fromNodeId);
                    const BpmnNode* tn = diagram.FindNode(se->toNodeId);
                    if (fn && tn) {
                        state.PushUndo(diagram);
                        const auto fullPts = BuildEdgeWorldPoints(diagram, *fn, *tn, *se, effOpts.routingMode);
                        if (fullPts.size() >= 2) {
                            int segIdx = state.hoveredEdgeSegIdx;
                            const bool isHoriz = std::abs(fullPts[static_cast<size_t>(segIdx) + 1].y - fullPts[static_cast<size_t>(segIdx)].y) <=
                                                 std::abs(fullPts[static_cast<size_t>(segIdx) + 1].x - fullPts[static_cast<size_t>(segIdx)].x);
                            std::vector<Vec2> inner;
                            for (size_t k = 1; k + 1 < fullPts.size(); ++k) inner.push_back(fullPts[k]);
                            if (inner.empty()) {
                                const Vec2 mid{(fullPts.front().x + fullPts.back().x) * 0.5f, (fullPts.front().y + fullPts.back().y) * 0.5f};
                                inner.push_back(mid);
                                inner.push_back(mid);
                                segIdx = 1;
                            } else if (segIdx == 0) {
                                inner.insert(inner.begin(), inner.front());
                                segIdx = 1;
                            } else if (segIdx >= static_cast<int>(fullPts.size()) - 2) {
                                inner.push_back(inner.back());
                                segIdx = static_cast<int>(inner.size()) - 1;
                            }
                            se->waypoints = inner;
                            state.draggingEdgeSegment = true;
                            state.dragEdgeId = se->id;
                            state.dragEdgeSegIdx = segIdx;
                            state.dragEdgeSegHorizontal = isHoriz;
                            state.dragEdgeStartWorld = resp.mouseWorldPos;
                            state.dragEdgeInitWaypoints = se->waypoints;
                        }
                    }
                }
            }
            // 4. Check External Label Drag Click (Node or Edge)
            else if (!effOpts.readOnly && !effOpts.tokenSimulation && (state.hoveredLabelNodeId != 0 || state.hoveredLabelEdgeId != 0)) {
                state.PushUndo(diagram);
                state.draggingLabel = true;
                state.dragLabelNodeId = state.hoveredLabelNodeId;
                state.dragLabelEdgeId = state.hoveredLabelEdgeId;
                state.dragLabelStartWorld = resp.mouseWorldPos;
                if (state.dragLabelNodeId != 0) {
                    state.SelectSingleNode(state.dragLabelNodeId);
                    if (const BpmnNode* ln = diagram.FindNode(state.dragLabelNodeId)) {
                        state.dragLabelInitOffset = ln->labelOffset;
                    }
                } else if (state.dragLabelEdgeId != 0) {
                    state.ClearSelection();
                    state.selectedEdgeId = state.dragLabelEdgeId;
                    if (const BpmnEdge* le = diagram.FindEdge(state.dragLabelEdgeId)) {
                        state.dragLabelInitOffset = le->labelOffset;
                    }
                }
                resp.changed = true;
            }
            // 5. Check Node Click (including SubProcess [+]/[-] collapse/expand marker)
            else if (state.hoveredNodeId != 0) {
                BpmnNode* clickedNode = diagram.FindNode(state.hoveredNodeId);
                bool toggledExpand = false;
                if (!effOpts.readOnly && !effOpts.tokenSimulation && clickedNode && clickedNode->type == BpmnNodeType::SubProcess) {
                    const Rect nsr = WorldToScreenRect(clickedNode->Bounds(), viewRect, state.pan, state.zoom);
                    const Rect markerR(nsr.Center().x - 10.0f * s, nsr.Bottom() - 18.0f * s, 20.0f * s, 17.0f * s);
                    if (markerR.Contains(mouse)) {
                        state.PushUndo(diagram);
                        const Vec2 oldC = clickedNode->Center();
                        clickedNode->isExpanded = !clickedNode->isExpanded;
                        clickedNode->size = clickedNode->isExpanded ? Vec2{320.0f, 180.0f} : BpmnDiagram::DefaultNodeSize(BpmnNodeType::SubProcess);
                        clickedNode->pos = {oldC.x - clickedNode->size.x * 0.5f, oldC.y - clickedNode->size.y * 0.5f};
                        diagram.SyncBoundaryEvents();
                        toggledExpand = true;
                        resp.changed = true;
                        resp.diagramModified = true;
                        ctx.RequestRepaint(2);
                    }
                }
                if (!toggledExpand) {
                    if (mods.shift || mods.ctrl) {
                        state.ToggleNodeSelection(state.hoveredNodeId);
                    } else if (!state.IsNodeSelected(state.hoveredNodeId)) {
                        state.SelectSingleNode(state.hoveredNodeId);
                    }
                    if (!effOpts.readOnly && !effOpts.tokenSimulation) {
                        state.draggingNodes = true;
                        state.dragUndoRecorded = false;
                        state.dragStartWorld = resp.mouseWorldPos;
                        state.dragInitialNodePos.clear();
                        for (uint32_t selId : state.selectedNodes) {
                            if (const BpmnNode* sn = diagram.FindNode(selId)) {
                                state.dragInitialNodePos.emplace_back(selId, sn->pos);
                                if (sn->type == BpmnNodeType::SubProcess && sn->isExpanded) {
                                    const Rect sb = sn->Bounds();
                                    for (const auto& child : diagram.nodes) {
                                        if (child.id != sn->id && !state.IsNodeSelected(child.id) && sb.Contains(child.Center())) {
                                            state.dragInitialNodePos.emplace_back(child.id, child.pos);
                                        }
                                    }
                                }
                            }
                        }
                    }
                    resp.changed = true;
                }
            } else if (state.hoveredEdgeId != 0) {
                state.ClearSelection();
                state.selectedEdgeId = state.hoveredEdgeId;
                resp.changed = true;
            } else if (state.hoveredLaneId != 0) {
                state.ClearSelection();
                state.selectedLaneId = state.hoveredLaneId;
                if (!effOpts.readOnly && !effOpts.tokenSimulation) {
                    if (const BpmnLane* l = diagram.FindLane(state.hoveredLaneId)) {
                        const float headerWorldW = 64.0f;
                        const float edgeTol = 12.0f / std::max(0.25f, state.zoom);
                        if (std::abs(resp.mouseWorldPos.x - l->rect.Right()) <= edgeTol) {
                            state.draggingLane = true;
                            state.laneResizeEdge = 1;
                        } else if (std::abs(resp.mouseWorldPos.y - l->rect.Bottom()) <= edgeTol) {
                            state.draggingLane = true;
                            state.laneResizeEdge = 2;
                        } else if (resp.mouseWorldPos.x <= l->rect.x + headerWorldW) {
                            state.draggingLane = true;
                            state.laneResizeEdge = 0;
                        }
                        if (state.draggingLane) {
                            state.PushUndo(diagram);
                            state.laneDragStartWorld = resp.mouseWorldPos;
                            state.laneInitialRect = l->rect;
                            state.laneContainedNodeOffsets.clear();
                            for (const auto& n : diagram.nodes) {
                                if (l->rect.Contains(n.Center())) {
                                    state.laneContainedNodeOffsets.emplace_back(n.id, n.pos);
                                }
                            }
                        } else {
                            if (mods.shift) {
                                state.lassoActive = true;
                                state.lassoStartWorld = resp.mouseWorldPos;
                                state.lassoEndWorld = resp.mouseWorldPos;
                            } else {
                                state.panningCanvas = true;
                                state.panStartMouse = mouse;
                                state.panStartOffset = state.pan;
                            }
                        }
                    }
                }
                resp.changed = true;
            } else {
                state.ClearSelection();
                if (mods.shift) {
                    state.lassoActive = true;
                    state.lassoStartWorld = resp.mouseWorldPos;
                    state.lassoEndWorld = resp.mouseWorldPos;
                } else {
                    state.panningCanvas = true;
                    state.panStartMouse = mouse;
                    state.panStartOffset = state.pan;
                }
            }
        }

        // Ongoing 4-Corner Node Resize (`Resize`)
        if (state.resizingNode) {
            if (lmbDown) {
                if (BpmnNode* rn = diagram.FindNode(state.primaryNodeId)) {
                    const Vec2 delta{
                        resp.mouseWorldPos.x - state.resizeStartWorld.x,
                        resp.mouseWorldPos.y - state.resizeStartWorld.y
                    };
                    const float minW = (rn->type == BpmnNodeType::SubProcess && rn->isExpanded) ? 200.0f : 80.0f;
                    const float minH = (rn->type == BpmnNodeType::SubProcess && rn->isExpanded) ? 120.0f : 50.0f;
                    float left = state.resizeInitialBounds.x;
                    float top = state.resizeInitialBounds.y;
                    float right = state.resizeInitialBounds.Right();
                    float bottom = state.resizeInitialBounds.Bottom();

                    if (state.resizeCorner == 0 || state.resizeCorner == 3) {
                        left = std::min(right - minW, left + delta.x);
                        if (effOpts.snapToGrid && !mods.alt) left = std::min(right - minW, SnapValue(left, effOpts.gridStep));
                    } else {
                        right = std::max(left + minW, right + delta.x);
                        if (effOpts.snapToGrid && !mods.alt) right = std::max(left + minW, SnapValue(right, effOpts.gridStep));
                    }
                    if (state.resizeCorner == 0 || state.resizeCorner == 1) {
                        top = std::min(bottom - minH, top + delta.y);
                        if (effOpts.snapToGrid && !mods.alt) top = std::min(bottom - minH, SnapValue(top, effOpts.gridStep));
                    } else {
                        bottom = std::max(top + minH, bottom + delta.y);
                        if (effOpts.snapToGrid && !mods.alt) bottom = std::max(top + minH, SnapValue(bottom, effOpts.gridStep));
                    }
                    rn->pos = {left, top};
                    rn->size = {right - left, bottom - top};
                    diagram.SyncBoundaryEvents();
                    resp.changed = true;
                    resp.diagramModified = true;
                    ctx.RequestRepaint(2);
                }
            } else {
                state.resizingNode = false;
                state.resizeCorner = -1;
            }
        }

        // Ongoing Orthogonal Edge Segment Drag (`ConnectionSegmentMove` + `BendpointMove`)
        if (state.draggingEdgeSegment) {
            if (lmbDown) {
                if (BpmnEdge* se = diagram.FindEdge(state.dragEdgeId)) {
                    const Vec2 delta{
                        resp.mouseWorldPos.x - state.dragEdgeStartWorld.x,
                        resp.mouseWorldPos.y - state.dragEdgeStartWorld.y
                    };
                    se->waypoints = state.dragEdgeInitWaypoints;
                    const int wpIdx0 = state.dragEdgeSegIdx - 1;
                    const int wpIdx1 = state.dragEdgeSegIdx;
                    if (state.dragEdgeSegHorizontal) {
                        float newY = ((wpIdx0 >= 0 && wpIdx0 < static_cast<int>(se->waypoints.size())) ? se->waypoints[static_cast<size_t>(wpIdx0)].y : resp.mouseWorldPos.y) + delta.y;
                        if (effOpts.snapToGrid && !mods.alt) newY = SnapValue(newY, effOpts.gridStep);
                        if (wpIdx0 >= 0 && wpIdx0 < static_cast<int>(se->waypoints.size())) se->waypoints[static_cast<size_t>(wpIdx0)].y = newY;
                        if (wpIdx1 >= 0 && wpIdx1 < static_cast<int>(se->waypoints.size())) se->waypoints[static_cast<size_t>(wpIdx1)].y = newY;
                    } else {
                        float newX = ((wpIdx0 >= 0 && wpIdx0 < static_cast<int>(se->waypoints.size())) ? se->waypoints[static_cast<size_t>(wpIdx0)].x : resp.mouseWorldPos.x) + delta.x;
                        if (effOpts.snapToGrid && !mods.alt) newX = SnapValue(newX, effOpts.gridStep);
                        if (wpIdx0 >= 0 && wpIdx0 < static_cast<int>(se->waypoints.size())) se->waypoints[static_cast<size_t>(wpIdx0)].x = newX;
                        if (wpIdx1 >= 0 && wpIdx1 < static_cast<int>(se->waypoints.size())) se->waypoints[static_cast<size_t>(wpIdx1)].x = newX;
                    }
                    resp.changed = true;
                    resp.diagramModified = true;
                    ctx.RequestRepaint(2);
                }
            } else {
                state.draggingEdgeSegment = false;
                state.dragEdgeId = 0;
                state.dragEdgeSegIdx = -1;
            }
        }

        // Ongoing Edge Endpoint Reconnect (`Reconnect`)
        if (state.reconnectingEdge) {
            state.reconnectEndWorld = resp.mouseWorldPos;
            state.reconnectSnapNodeId = state.hoveredNodeId;
            if (state.reconnectSnapNodeId != 0) {
                if (const BpmnNode* target = diagram.FindNode(state.reconnectSnapNodeId)) {
                    state.reconnectEndWorld = target->Center();
                }
            }
            if (lmbDown) {
                ctx.RequestRepaint(2);
            } else {
                if (BpmnEdge* re = diagram.FindEdge(state.reconnectEdgeId)) {
                    if (state.reconnectSnapNodeId != 0) {
                        const uint32_t otherId = (state.reconnectEndpoint == 1) ? re->toNodeId : re->fromNodeId;
                        if (state.reconnectSnapNodeId != otherId) {
                            state.PushUndo(diagram);
                            if (state.reconnectEndpoint == 1) {
                                re->fromNodeId = state.reconnectSnapNodeId;
                                re->fromPort = BpmnPortSide::Auto;
                            } else {
                                re->toNodeId = state.reconnectSnapNodeId;
                                re->toPort = BpmnPortSide::Auto;
                            }
                            re->waypoints.clear();
                            resp.changed = true;
                            resp.diagramModified = true;
                        }
                    }
                }
                state.reconnectingEdge = false;
                state.reconnectEdgeId = 0;
                state.reconnectEndpoint = 0;
                state.reconnectSnapNodeId = 0;
                ctx.RequestRepaint(2);
            }
        }

        // Ongoing External Label Dragging (`ExternalLabelMove`)
        if (state.draggingLabel) {
            if (lmbDown) {
                const Vec2 delta{
                    resp.mouseWorldPos.x - state.dragLabelStartWorld.x,
                    resp.mouseWorldPos.y - state.dragLabelStartWorld.y
                };
                if (state.dragLabelNodeId != 0) {
                    if (BpmnNode* ln = diagram.FindNode(state.dragLabelNodeId)) {
                        ln->labelOffset = {state.dragLabelInitOffset.x + delta.x, state.dragLabelInitOffset.y + delta.y};
                        resp.changed = true;
                        resp.diagramModified = true;
                        ctx.RequestRepaint(2);
                    }
                } else if (state.dragLabelEdgeId != 0) {
                    if (BpmnEdge* le = diagram.FindEdge(state.dragLabelEdgeId)) {
                        le->labelOffset = {state.dragLabelInitOffset.x + delta.x, state.dragLabelInitOffset.y + delta.y};
                        resp.changed = true;
                        resp.diagramModified = true;
                        ctx.RequestRepaint(2);
                    }
                }
            } else {
                state.draggingLabel = false;
                state.dragLabelNodeId = 0;
                state.dragLabelEdgeId = 0;
            }
        }

        // Ongoing Node Dragging + Smart Alignment Guides
        if (state.draggingNodes) {
            if (lmbDown) {
                const Vec2 delta{
                    resp.mouseWorldPos.x - state.dragStartWorld.x,
                    resp.mouseWorldPos.y - state.dragStartWorld.y
                };
                if (!state.dragUndoRecorded && (std::abs(delta.x) > 2.0f || std::abs(delta.y) > 2.0f)) {
                    BpmnDiagram beforeSnap = diagram;
                    for (const auto& [nid, initPos] : state.dragInitialNodePos) {
                        if (BpmnNode* n = beforeSnap.FindNode(nid)) n->pos = initPos;
                    }
                    state.PushUndo(beforeSnap);
                    state.dragUndoRecorded = true;
                }

                state.activeGuides.clear();
                for (const auto& [nid, initPos] : state.dragInitialNodePos) {
                    BpmnNode* n = diagram.FindNode(nid);
                    if (!n) continue;
                    if (n->type == BpmnNodeType::BoundaryEvent && n->attachedToNodeId != 0) {
                        if (const BpmnNode* host = diagram.FindNode(n->attachedToNodeId)) {
                            const float newCenterX = initPos.x + n->size.x * 0.5f + delta.x;
                            n->boundaryRatioX = std::clamp((newCenterX - host->pos.x) / std::max(1.0f, host->size.x), 0.15f, 0.85f);
                        }
                        continue;
                    }
                    Vec2 targetPos{initPos.x + delta.x, initPos.y + delta.y};
                    if (effOpts.snapToGrid && !mods.alt) {
                        const float cx = SnapValue(targetPos.x + n->size.x * 0.5f, effOpts.gridStep);
                        const float cy = SnapValue(targetPos.y + n->size.y * 0.5f, effOpts.gridStep);
                        targetPos.x = cx - n->size.x * 0.5f;
                        targetPos.y = cy - n->size.y * 0.5f;
                    }

                    if (effOpts.smartGuides && nid == state.primaryNodeId) {
                        const float snapTol = 6.0f;
                        const float myCX = targetPos.x + n->size.x * 0.5f;
                        const float myCY = targetPos.y + n->size.y * 0.5f;
                        for (const auto& other : diagram.nodes) {
                            if (state.IsNodeSelected(other.id) || other.type == BpmnNodeType::BoundaryEvent) continue;
                            const Vec2 oc = other.Center();
                            if (std::abs(myCX - oc.x) <= snapTol) {
                                targetPos.x = oc.x - n->size.x * 0.5f;
                                state.activeGuides.push_back(AlignmentGuide{
                                    true, oc.x,
                                    std::min(targetPos.y, other.pos.y) - 20.0f,
                                    std::max(targetPos.y + n->size.y, other.pos.y + other.size.y) + 20.0f
                                });
                            }
                            if (std::abs(myCY - oc.y) <= snapTol) {
                                targetPos.y = oc.y - n->size.y * 0.5f;
                                state.activeGuides.push_back(AlignmentGuide{
                                    false, oc.y,
                                    std::min(targetPos.x, other.pos.x) - 20.0f,
                                    std::max(targetPos.x + n->size.x, other.pos.x + other.size.x) + 20.0f
                                });
                            }
                        }
                    }

                    n->pos = targetPos;
                }
                diagram.SyncBoundaryEvents();
                resp.changed = true;
                resp.diagramModified = true;
                ctx.RequestRepaint(2);
            } else {
                state.draggingNodes = false;
                state.activeGuides.clear();
            }
        }

        // Ongoing Lane Move / Resize
        if (state.draggingLane) {
            if (lmbDown) {
                if (BpmnLane* l = diagram.FindLane(state.selectedLaneId)) {
                    const Vec2 delta{
                        resp.mouseWorldPos.x - state.laneDragStartWorld.x,
                        resp.mouseWorldPos.y - state.laneDragStartWorld.y
                    };
                    if (state.laneResizeEdge == 1) {
                        l->rect.w = std::max(320.0f, SnapValue(state.laneInitialRect.w + delta.x, effOpts.gridStep));
                    } else if (state.laneResizeEdge == 2) {
                        l->rect.h = std::max(110.0f, SnapValue(state.laneInitialRect.h + delta.y, effOpts.gridStep));
                    } else {
                        l->rect.x = SnapValue(state.laneInitialRect.x + delta.x, effOpts.gridStep);
                        l->rect.y = SnapValue(state.laneInitialRect.y + delta.y, effOpts.gridStep);
                        const Vec2 appliedDelta{l->rect.x - state.laneInitialRect.x, l->rect.y - state.laneInitialRect.y};
                        for (const auto& [nid, initPos] : state.laneContainedNodeOffsets) {
                            if (BpmnNode* cn = diagram.FindNode(nid)) {
                                cn->pos = {initPos.x + appliedDelta.x, initPos.y + appliedDelta.y};
                            }
                        }
                        diagram.SyncBoundaryEvents();
                    }
                    resp.changed = true;
                    resp.diagramModified = true;
                    ctx.RequestRepaint(2);
                }
            } else {
                state.draggingLane = false;
            }
        }

        // Ongoing Space Tool (`S`) Dragging
        if (state.spaceToolDragging) {
            if (lmbDown) {
                state.spaceCurrentWorld = resp.mouseWorldPos;
                const float dx = state.spaceCurrentWorld.x - state.spaceStartWorld.x;
                const float dy = state.spaceCurrentWorld.y - state.spaceStartWorld.y;
                if (!state.spaceToolAxisLocked && (std::abs(dx) > 6.0f || std::abs(dy) > 6.0f)) {
                    state.spaceToolAxisLocked = true;
                    state.spaceToolHorizontal = (std::abs(dx) >= std::abs(dy));
                }
                if (state.spaceToolAxisLocked) {
                    const float shift = effOpts.snapToGrid
                        ? SnapValue(state.spaceToolHorizontal ? dx : dy, effOpts.gridStep)
                        : (state.spaceToolHorizontal ? dx : dy);
                    for (const auto& [nid, initPos] : state.spaceInitialNodes) {
                        BpmnNode* n = diagram.FindNode(nid);
                        if (!n || n->type == BpmnNodeType::BoundaryEvent) continue;
                        if (state.spaceToolHorizontal) {
                            n->pos.x = (initPos.x >= state.spaceStartWorld.x) ? (initPos.x + shift) : initPos.x;
                        } else {
                            n->pos.y = (initPos.y >= state.spaceStartWorld.y) ? (initPos.y + shift) : initPos.y;
                        }
                    }
                    for (const auto& [lid, initR] : state.spaceInitialLanes) {
                        BpmnLane* l = diagram.FindLane(lid);
                        if (!l) continue;
                        if (state.spaceToolHorizontal) {
                            if (initR.x < state.spaceStartWorld.x && initR.Right() > state.spaceStartWorld.x) {
                                l->rect.w = std::max(240.0f, initR.w + shift);
                            } else if (initR.x >= state.spaceStartWorld.x) {
                                l->rect.x = initR.x + shift;
                            }
                        } else {
                            if (initR.y < state.spaceStartWorld.y && initR.Bottom() > state.spaceStartWorld.y) {
                                l->rect.h = std::max(100.0f, initR.h + shift);
                            } else if (initR.y >= state.spaceStartWorld.y) {
                                l->rect.y = initR.y + shift;
                            }
                        }
                    }
                    diagram.SyncBoundaryEvents();
                    resp.changed = true;
                    resp.diagramModified = true;
                    ctx.RequestRepaint(2);
                }
            } else {
                state.spaceToolDragging = false;
                state.activeTool = BpmnTool::Select;
            }
        }

        // Ongoing Lasso Selection
        if (state.lassoActive) {
            if (lmbDown) {
                state.lassoEndWorld = resp.mouseWorldPos;
                const Rect box(
                    std::min(state.lassoStartWorld.x, state.lassoEndWorld.x),
                    std::min(state.lassoStartWorld.y, state.lassoEndWorld.y),
                    std::abs(state.lassoEndWorld.x - state.lassoStartWorld.x),
                    std::abs(state.lassoEndWorld.y - state.lassoStartWorld.y)
                );
                state.selectedNodes.clear();
                for (const auto& n : diagram.nodes) {
                    if (box.Intersects(n.Bounds())) {
                        state.selectedNodes.push_back(n.id);
                    }
                }
                state.primaryNodeId = state.selectedNodes.empty() ? 0 : state.selectedNodes.back();
                ctx.RequestRepaint(2);
            } else {
                state.lassoActive = false;
                if (state.activeTool == BpmnTool::Lasso) {
                    state.activeTool = BpmnTool::Select;
                }
            }
        }

        // Ongoing Wire Connection
        if (state.connectingWire) {
            state.connectEndWorld = resp.mouseWorldPos;
            state.connectSnapNodeId = (state.hoveredNodeId != 0 && state.hoveredNodeId != state.connectFromNodeId)
                ? state.hoveredNodeId
                : 0;
            if (state.connectSnapNodeId != 0) {
                if (const BpmnNode* target = diagram.FindNode(state.connectSnapNodeId)) {
                    state.connectEndWorld = target->Center();
                }
            }
            if (lmbReleased || (lmbClicked && state.connectSnapNodeId != 0)) {
                if (state.connectSnapNodeId != 0) {
                    state.PushUndo(diagram);
                    const BpmnNode* src = diagram.FindNode(state.connectFromNodeId);
                    const BpmnNode* dst = diagram.FindNode(state.connectSnapNodeId);
                    BpmnFlowType flow = BpmnFlowType::SequenceFlow;
                    if (src && dst) {
                        if (src->type == BpmnNodeType::DataObject || src->type == BpmnNodeType::DataStore || src->type == BpmnNodeType::TextAnnotation ||
                            dst->type == BpmnNodeType::DataObject || dst->type == BpmnNodeType::DataStore || dst->type == BpmnNodeType::TextAnnotation) {
                            flow = BpmnFlowType::Association;
                        } else if (src->laneId != 0 && dst->laneId != 0 && src->laneId != dst->laneId) {
                            const BpmnLane* l1 = diagram.FindLane(src->laneId);
                            const BpmnLane* l2 = diagram.FindLane(dst->laneId);
                            if (l1 && l2 && l1->poolName != l2->poolName) {
                                flow = BpmnFlowType::MessageFlow;
                            }
                        }
                    }
                    const uint32_t eid = diagram.AddEdge(state.connectFromNodeId, state.connectSnapNodeId, flow);
                    state.ClearSelection();
                    state.selectedEdgeId = eid;
                    resp.changed = true;
                    resp.diagramModified = true;
                }
                state.connectingWire = false;
                state.connectFromNodeId = 0;
                state.connectSnapNodeId = 0;
                if (state.activeTool == BpmnTool::GlobalConnect) {
                    state.activeTool = BpmnTool::Select;
                }
                ctx.RequestRepaint(2);
            }
        }

        // K. Keyboard Shortcuts when focused or hovered
        if ((ctx.IsFocused(id) || hovered) && state.editingNodeId == 0 && state.editingEdgeId == 0 && state.editingLaneId == 0 && !effOpts.readOnly) {
            if (ctx.ConsumeShortcut(Key::Z, true, false)) {
                if (state.Undo(diagram)) { resp.changed = true; resp.diagramModified = true; }
            } else if (ctx.ConsumeShortcut(Key::Y, true, false) || ctx.ConsumeShortcut(Key::Z, true, true)) {
                if (state.Redo(diagram)) { resp.changed = true; resp.diagramModified = true; }
            } else if (ctx.ConsumeShortcut(Key::A, true, false)) {
                state.selectedNodes.clear();
                for (const auto& n : diagram.nodes) state.selectedNodes.push_back(n.id);
                state.primaryNodeId = state.selectedNodes.empty() ? 0 : state.selectedNodes.back();
                resp.changed = true;
            } else if (ctx.ConsumeShortcut(Key::C, true, false)) {
                state.clipboardNodes.clear();
                state.clipboardEdges.clear();
                for (uint32_t nid : state.selectedNodes) {
                    if (const BpmnNode* n = diagram.FindNode(nid)) state.clipboardNodes.push_back(*n);
                }
                for (const auto& e : diagram.edges) {
                    if (state.IsNodeSelected(e.fromNodeId) && state.IsNodeSelected(e.toNodeId)) {
                        state.clipboardEdges.push_back(e);
                    }
                }
            } else if (ctx.ConsumeShortcut(Key::V, true, false) && !state.clipboardNodes.empty()) {
                state.PushUndo(diagram);
                std::unordered_map<uint32_t, uint32_t> idMap;
                state.selectedNodes.clear();
                for (BpmnNode copy : state.clipboardNodes) {
                    const uint32_t oldId = copy.id;
                    copy.id = 0;
                    copy.pos.x += 40.0f;
                    copy.pos.y += 40.0f;
                    const uint32_t newId = diagram.AddNode(std::move(copy));
                    idMap[oldId] = newId;
                    state.selectedNodes.push_back(newId);
                }
                for (const auto& e : state.clipboardEdges) {
                    if (idMap.contains(e.fromNodeId) && idMap.contains(e.toNodeId)) {
                        diagram.AddEdge(idMap[e.fromNodeId], idMap[e.toNodeId], e.flowType, e.label, e.fromPort, e.toPort);
                    }
                }
                state.primaryNodeId = state.selectedNodes.empty() ? 0 : state.selectedNodes.back();
                resp.changed = true;
                resp.diagramModified = true;
            } else if (ctx.ConsumeShortcut(Key::D, true, false) && !state.selectedNodes.empty()) {
                state.PushUndo(diagram);
                std::vector<uint32_t> newSel;
                for (uint32_t nid : state.selectedNodes) {
                    if (const BpmnNode* n = diagram.FindNode(nid)) {
                        BpmnNode copy = *n;
                        copy.id = 0;
                        copy.pos.x += 40.0f;
                        copy.pos.y += 40.0f;
                        newSel.push_back(diagram.AddNode(std::move(copy)));
                    }
                }
                state.selectedNodes = std::move(newSel);
                state.primaryNodeId = state.selectedNodes.empty() ? 0 : state.selectedNodes.back();
                resp.changed = true;
                resp.diagramModified = true;
            } else if (ctx.ConsumeKey(Key::Delete) || ctx.ConsumeKey(Key::Backspace)) {
                if (!state.selectedNodes.empty()) {
                    state.PushUndo(diagram);
                    const auto toDel = state.selectedNodes;
                    for (uint32_t nid : toDel) diagram.RemoveNode(nid);
                    state.ClearSelection();
                    resp.changed = true;
                    resp.diagramModified = true;
                } else if (state.selectedEdgeId != 0) {
                    state.PushUndo(diagram);
                    diagram.RemoveEdge(state.selectedEdgeId);
                    state.ClearSelection();
                    resp.changed = true;
                    resp.diagramModified = true;
                } else if (state.selectedLaneId != 0) {
                    state.PushUndo(diagram);
                    diagram.RemoveLane(state.selectedLaneId);
                    state.ClearSelection();
                    resp.changed = true;
                    resp.diagramModified = true;
                }
            } else if (ctx.ConsumeKey(Key::Escape)) {
                state.activeTool = BpmnTool::Select;
                state.connectingWire = false;
                state.reconnectingEdge = false;
                state.resizingNode = false;
                state.draggingEdgeSegment = false;
                state.draggingLabel = false;
                state.paletteDragCreating = false;
                state.replaceMenuOpen = false;
                state.ClearSelection();
            } else if (!mods.ctrl && !mods.alt) {
                if (ctx.ConsumeKey(Key::V)) state.activeTool = BpmnTool::Select;
                else if (ctx.ConsumeKey(Key::H)) state.activeTool = BpmnTool::Hand;
                else if (ctx.ConsumeKey(Key::C)) state.activeTool = BpmnTool::GlobalConnect;
                const auto typed = ctx.ConsumePendingTextInput();
                for (const auto& sIn : typed) {
                    if (sIn == "v" || sIn == "V") state.activeTool = BpmnTool::Select;
                    else if (sIn == "h" || sIn == "H") state.activeTool = BpmnTool::Hand;
                    else if (sIn == "l" || sIn == "L") state.activeTool = BpmnTool::Lasso;
                    else if (sIn == "s" || sIn == "S") state.activeTool = BpmnTool::Space;
                    else if (sIn == "c" || sIn == "C") state.activeTool = BpmnTool::GlobalConnect;
                }
            }
        }
    }

    resp.pan = state.pan;
    resp.zoom = state.zoom;
    resp.activeTool = state.activeTool;
    resp.selectedNodeId = state.primaryNodeId;
    resp.selectedEdgeId = state.selectedEdgeId;
    resp.selectedLaneId = state.selectedLaneId;
    resp.selectedNodeCount = state.selectedNodes.size();
    resp.hoveredNodeId = state.hoveredNodeId;
    resp.hoveredEdgeId = state.hoveredEdgeId;
    resp.simulationActive = effOpts.tokenSimulation;
    resp.activeTokenCount = diagram.tokens.size();
    resp.undoDepth = state.undoStack.size();
    resp.redoDepth = state.redoStack.size();

    if (opts.selectedNodeIdPtr) *opts.selectedNodeIdPtr = state.primaryNodeId;
    if (opts.selectedEdgeIdPtr) *opts.selectedEdgeIdPtr = state.selectedEdgeId;
    if (opts.selectedLaneIdPtr) *opts.selectedLaneIdPtr = state.selectedLaneId;

    if (resp.hovered && !opts.tooltip.empty()) {
        ctx.SetTooltip(opts.tooltip, baseRes.rect);
    }

    Node* node = ctx.CreateNode(NodeType::BpmnEditor, id);
    auto* payload = ctx.AllocPayload<BpmnEditorPayload>();
    payload->opts = effOpts;
    payload->opts.label = ctx.AllocText(opts.label);
    payload->opts.description = ctx.AllocText(opts.description);
    payload->opts.badge = ctx.AllocText(opts.badge);
    payload->opts.tooltip = ctx.AllocString(opts.tooltip);
    payload->resp = resp;
    payload->diagramSnapshot = diagram;
    payload->stateSnapshot = state;
    payload->activeEditBuffer = std::move(activeEditStr);
    payload->activeEditCursor = activeEditCursor;
    payload->cursorBlinkOn = ctx.IsCursorVisible();
    payload->animPhase = static_cast<float>(std::fmod(nowSec * 0.85, 1.0));

    node->text = payload->opts.label.text.empty() ? "BpmnEditor" : payload->opts.label.text;
    node->SetPayload(payload);

    uint64_t graphHash = detail::HashArgs(
        diagram.name,
        diagram.lanes.size(),
        diagram.nodes.size(),
        diagram.edges.size(),
        diagram.tokens.size()
    );
    for (const auto& l : diagram.lanes) {
        graphHash = detail::HashArgs(graphHash, l.id, l.name, l.poolName, l.rect.x, l.rect.y, l.rect.w, l.rect.h);
    }
    for (const auto& n : diagram.nodes) {
        graphHash = detail::HashArgs(
            graphHash, n.id, static_cast<uint8_t>(n.type), n.label, n.documentation,
            n.pos.x, n.pos.y, n.size.x, n.size.y,
            static_cast<uint8_t>(n.taskType), static_cast<uint8_t>(n.gatewayType), static_cast<uint8_t>(n.eventDef),
            n.nonInterrupting, n.isExpanded, n.labelOffset.x, n.labelOffset.y,
            n.activeOutgoingEdgeId, n.tokenExecutionCount, n.activeTokensInside,
            n.slaDurationMs, n.loopMarker, n.parallelMultiInstance
        );
    }
    for (const auto& e : diagram.edges) {
        graphHash = detail::HashArgs(
            graphHash, e.id, e.fromNodeId, e.toNodeId, static_cast<uint8_t>(e.flowType), e.label,
            e.labelOffset.x, e.labelOffset.y, e.waypoints.size()
        );
        for (const auto& wp : e.waypoints) {
            graphHash = detail::HashArgs(graphHash, wp.x, wp.y);
        }
    }
    for (const auto& t : diagram.tokens) {
        graphHash = detail::HashArgs(graphHash, t.id, t.edgeId, t.nodeId, static_cast<int>(t.progress * 200.0f));
    }

    node->visualHash = detail::HashArgs(
        graphHash,
        payload->opts.label,
        payload->opts.description,
        payload->opts.badge,
        static_cast<uint8_t>(payload->opts.routingMode),
        static_cast<uint8_t>(payload->opts.themePreset),
        payload->opts.showLeftPalette,
        payload->opts.showTopBar,
        payload->opts.showContextPad,
        payload->opts.showMinimap,
        payload->opts.showZoomControls,
        payload->opts.showGrid,
        payload->opts.gridStep,
        payload->opts.tokenSimulation,
        payload->opts.showSlaHeatmap,
        state.pan.x,
        state.pan.y,
        state.zoom,
        static_cast<uint8_t>(state.activeTool),
        state.primaryNodeId,
        state.selectedEdgeId,
        state.selectedLaneId,
        state.selectedNodes.size(),
        state.hoveredNodeId,
        state.hoveredEdgeId,
        state.hoveredLabelNodeId,
        state.hoveredLabelEdgeId,
        state.hoveredResizeCorner,
        state.hoveredEdgeSegIdx,
        state.hoveredEdgeEndpoint,
        state.hoveredPaletteIdx,
        state.hoveredContextPadBtn,
        state.hoveredReplaceMenuItem,
        state.replaceMenuOpen,
        state.connectingWire,
        state.reconnectingEdge,
        state.connectEndWorld.x,
        state.connectEndWorld.y,
        state.ghostSnappedPos.x,
        state.ghostSnappedPos.y,
        state.lassoActive,
        state.lassoEndWorld.x,
        state.lassoEndWorld.y,
        state.spaceToolDragging,
        state.spaceCurrentWorld.x,
        state.spaceCurrentWorld.y,
        state.editingNodeId,
        state.editingEdgeId,
        state.editingLaneId,
        payload->activeEditBuffer,
        payload->cursorBlinkOn,
        effOpts.tokenSimulation ? static_cast<int>(payload->animPhase * 60.0f) : 0,
        viewRect.w,
        viewRect.h,
        effectiveSize,
        isDisabled
    );

    node->measureFn = [](const Node* n, float availableWidth, ICanvas* c, const UiTheme& th) noexcept -> Vec2 {
        const auto* p = n->As<BpmnEditorPayload>();
        if (!p) return {0.0f, 0.0f};
        return BpmnEditor::Measure(p->opts, n->uiSize, c, th, availableWidth);
    };
    node->renderFn = &RenderBpmnEditorNode;

    node->uiSize = effectiveSize;
    node->disabled = isDisabled;
    node->hovered = resp.hovered;
    node->pressed = baseRes.pressed;
    node->widthMode = (opts.stretch != StretchMode::Compact) ? SizeMode::Stretch : SizeMode::Fit;
    node->heightMode = opts.heightMode;
    if (elemSpec.control.height.has_value() && *elemSpec.control.height > 0.0f &&
        !opts.label.HasContentOrReserve() && !opts.description.HasContentOrReserve() && !Element::HasOuterFrame(elemSpec)) {
        node->heightMode = SizeMode::Fixed;
        node->fixedHeight = theme.Scale(*elemSpec.control.height);
    }
    node->minHeight = elemSpec.control.height.has_value() ? theme.Scale(*elemSpec.control.height) : theme.Scale(320.0f);

    return resp;
}

// ============================================================================
// Standard BPMN 2.0 XML Serialization & Round-Trip Deserialization
// ============================================================================
namespace {

std::string EscapeXmlText(std::string_view s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        switch (c) {
            case '&':  out += "&amp;"; break;
            case '<':  out += "&lt;"; break;
            case '>':  out += "&gt;"; break;
            case '"':  out += "&quot;"; break;
            case '\'': out += "&apos;"; break;
            case '\n': out += "&#10;"; break;
            default:   out.push_back(c); break;
        }
    }
    return out;
}

std::string UnescapeXmlText(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '&') {
            if (s.substr(i, 5) == "&amp;")  { out.push_back('&');  i += 4; continue; }
            if (s.substr(i, 4) == "&lt;")   { out.push_back('<');  i += 3; continue; }
            if (s.substr(i, 4) == "&gt;")   { out.push_back('>');  i += 3; continue; }
            if (s.substr(i, 6) == "&quot;") { out.push_back('"');  i += 5; continue; }
            if (s.substr(i, 6) == "&apos;") { out.push_back('\''); i += 5; continue; }
            if (s.substr(i, 5) == "&#10;")  { out.push_back('\n'); i += 4; continue; }
        }
        out.push_back(s[i]);
    }
    return out;
}

std::string ExtractXmlAttr(std::string_view tag, std::string_view attrName) {
    const std::string needle = std::string(attrName) + "=\"";
    const size_t pos = tag.find(needle);
    if (pos == std::string_view::npos) return {};
    const size_t valStart = pos + needle.size();
    const size_t valEnd = tag.find('"', valStart);
    if (valEnd == std::string_view::npos) return {};
    return UnescapeXmlText(tag.substr(valStart, valEnd - valStart));
}

float ExtractXmlFloatAttr(std::string_view tag, std::string_view attrName, float fallback = 0.0f) {
    const std::string s = ExtractXmlAttr(tag, attrName);
    if (s.empty()) return fallback;
    try {
        return std::stof(s);
    } catch (...) {
        return fallback;
    }
}

uint32_t ParseTrailingIdNumber(std::string_view xmlId) {
    size_t i = xmlId.size();
    while (i > 0 && xmlId[i - 1] >= '0' && xmlId[i - 1] <= '9') --i;
    if (i == xmlId.size()) return 0;
    try {
        return static_cast<uint32_t>(std::stoul(std::string(xmlId.substr(i))));
    } catch (...) {
        return 0;
    }
}

} // namespace

std::string BpmnEditor::ExportBpmnXml(const BpmnDiagram& diagram) {
    std::string xml;
    xml.reserve(8192);
    xml += "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    xml += "<bpmn:definitions xmlns:bpmn=\"http://www.omg.org/spec/BPMN/20100524/MODEL\"\n";
    xml += "                  xmlns:bpmndi=\"http://www.omg.org/spec/BPMN/20100524/DI\"\n";
    xml += "                  xmlns:dc=\"http://www.omg.org/spec/DD/20100524/DC\"\n";
    xml += "                  xmlns:di=\"http://www.omg.org/spec/DD/20100524/DI\"\n";
    xml += "                  id=\"Definitions_1\" targetNamespace=\"http://bpmn.io/schema/bpmn\">\n";
    xml += "  <bpmn:process id=\"Process_1\" name=\"" + EscapeXmlText(diagram.name) + "\" isExecutable=\"true\">\n";

    if (!diagram.lanes.empty()) {
        xml += "    <bpmn:laneSet id=\"LaneSet_1\">\n";
        for (const auto& l : diagram.lanes) {
            xml += "      <bpmn:lane id=\"Lane_" + std::to_string(l.id) + "\" name=\"" + EscapeXmlText(l.name) + "\""
                   " poolName=\"" + EscapeXmlText(l.poolName) + "\" />\n";
        }
        xml += "    </bpmn:laneSet>\n";
    }

    for (const auto& n : diagram.nodes) {
        const char* tag = "bpmn:task";
        switch (n.type) {
            case BpmnNodeType::StartEvent:        tag = "bpmn:startEvent"; break;
            case BpmnNodeType::IntermediateEvent: tag = "bpmn:intermediateCatchEvent"; break;
            case BpmnNodeType::BoundaryEvent:     tag = "bpmn:boundaryEvent"; break;
            case BpmnNodeType::EndEvent:          tag = "bpmn:endEvent"; break;
            case BpmnNodeType::Task:
                switch (n.taskType) {
                    case BpmnTaskType::None:         tag = "bpmn:task"; break;
                    case BpmnTaskType::User:         tag = "bpmn:userTask"; break;
                    case BpmnTaskType::Service:      tag = "bpmn:serviceTask"; break;
                    case BpmnTaskType::Send:         tag = "bpmn:sendTask"; break;
                    case BpmnTaskType::Receive:      tag = "bpmn:receiveTask"; break;
                    case BpmnTaskType::Script:       tag = "bpmn:scriptTask"; break;
                    case BpmnTaskType::Manual:       tag = "bpmn:manualTask"; break;
                    case BpmnTaskType::BusinessRule: tag = "bpmn:businessRuleTask"; break;
                    case BpmnTaskType::CallActivity: tag = "bpmn:callActivity"; break;
                }
                break;
            case BpmnNodeType::SubProcess:        tag = "bpmn:subProcess"; break;
            case BpmnNodeType::Gateway:
                switch (n.gatewayType) {
                    case BpmnGatewayType::Exclusive:  tag = "bpmn:exclusiveGateway"; break;
                    case BpmnGatewayType::Parallel:   tag = "bpmn:parallelGateway"; break;
                    case BpmnGatewayType::Inclusive:  tag = "bpmn:inclusiveGateway"; break;
                    case BpmnGatewayType::EventBased: tag = "bpmn:eventBasedGateway"; break;
                }
                break;
            case BpmnNodeType::DataObject:        tag = "bpmn:dataObjectReference"; break;
            case BpmnNodeType::DataStore:         tag = "bpmn:dataStoreReference"; break;
            case BpmnNodeType::TextAnnotation:    tag = "bpmn:textAnnotation"; break;
        }
        xml += "    <" + std::string(tag) + " id=\"FlowNode_" + std::to_string(n.id) + "\" name=\"" + EscapeXmlText(n.label) + "\"";
        if (n.type == BpmnNodeType::BoundaryEvent && n.attachedToNodeId != 0) {
            xml += " attachedToRef=\"FlowNode_" + std::to_string(n.attachedToNodeId) + "\"";
            xml += std::string(" cancelActivity=\"") + (n.nonInterrupting ? "false" : "true") + "\"";
        }
        if (n.eventDef != BpmnEventDefinition::None &&
            (n.type == BpmnNodeType::StartEvent || n.type == BpmnNodeType::IntermediateEvent ||
             n.type == BpmnNodeType::BoundaryEvent || n.type == BpmnNodeType::EndEvent)) {
            const char* evTag = "bpmn:timerEventDefinition";
            switch (n.eventDef) {
                case BpmnEventDefinition::Message:    evTag = "bpmn:messageEventDefinition"; break;
                case BpmnEventDefinition::Timer:      evTag = "bpmn:timerEventDefinition"; break;
                case BpmnEventDefinition::Error:      evTag = "bpmn:errorEventDefinition"; break;
                case BpmnEventDefinition::Signal:     evTag = "bpmn:signalEventDefinition"; break;
                case BpmnEventDefinition::Escalation: evTag = "bpmn:escalationEventDefinition"; break;
                case BpmnEventDefinition::Terminate:  evTag = "bpmn:terminateEventDefinition"; break;
                case BpmnEventDefinition::None:       break;
            }
            xml += ">\n      <" + std::string(evTag) + " />\n    </" + std::string(tag) + ">\n";
        } else {
            xml += " />\n";
        }
    }

    for (const auto& e : diagram.edges) {
        const char* ftag = "bpmn:sequenceFlow";
        if (e.flowType == BpmnFlowType::MessageFlow) ftag = "bpmn:messageFlow";
        else if (e.flowType == BpmnFlowType::Association) ftag = "bpmn:association";
        xml += "    <" + std::string(ftag) + " id=\"Flow_" + std::to_string(e.id) +
               "\" sourceRef=\"FlowNode_" + std::to_string(e.fromNodeId) +
               "\" targetRef=\"FlowNode_" + std::to_string(e.toNodeId) + "\"";
        if (e.flowType == BpmnFlowType::DefaultFlow) xml += " isDefault=\"true\"";
        else if (e.flowType == BpmnFlowType::ConditionalFlow) xml += " isConditional=\"true\"";
        if (!e.label.empty()) xml += " name=\"" + EscapeXmlText(e.label) + "\"";
        xml += " />\n";
    }

    xml += "  </bpmn:process>\n";
    xml += "  <bpmndi:BPMNDiagram id=\"BPMNDiagram_1\">\n";
    xml += "    <bpmndi:BPMNPlane id=\"BPMNPlane_1\" bpmnElement=\"Process_1\">\n";

    for (const auto& l : diagram.lanes) {
        xml += "      <bpmndi:BPMNShape id=\"Lane_" + std::to_string(l.id) + "_di\" bpmnElement=\"Lane_" + std::to_string(l.id) + "\" isHorizontal=\"true\">\n";
        xml += "        <dc:Bounds x=\"" + std::to_string(static_cast<int>(l.rect.x)) +
               "\" y=\"" + std::to_string(static_cast<int>(l.rect.y)) +
               "\" width=\"" + std::to_string(static_cast<int>(l.rect.w)) +
               "\" height=\"" + std::to_string(static_cast<int>(l.rect.h)) + "\" />\n";
        xml += "      </bpmndi:BPMNShape>\n";
    }

    for (const auto& n : diagram.nodes) {
        xml += "      <bpmndi:BPMNShape id=\"FlowNode_" + std::to_string(n.id) + "_di\" bpmnElement=\"FlowNode_" + std::to_string(n.id) + "\"";
        if (n.type == BpmnNodeType::SubProcess) {
            xml += std::string(" isExpanded=\"") + (n.isExpanded ? "true" : "false") + "\"";
        }
        xml += ">\n";
        xml += "        <dc:Bounds x=\"" + std::to_string(static_cast<int>(n.pos.x)) +
               "\" y=\"" + std::to_string(static_cast<int>(n.pos.y)) +
               "\" width=\"" + std::to_string(static_cast<int>(n.size.x)) +
               "\" height=\"" + std::to_string(static_cast<int>(n.size.y)) + "\" />\n";
        if (std::abs(n.labelOffset.x) > 0.5f || std::abs(n.labelOffset.y) > 0.5f) {
            xml += "        <bpmndi:BPMNLabel>\n";
            xml += "          <dc:Bounds x=\"" + std::to_string(static_cast<int>(n.pos.x + n.labelOffset.x)) +
                   "\" y=\"" + std::to_string(static_cast<int>(n.pos.y + n.labelOffset.y)) +
                   "\" width=\"90\" height=\"20\" />\n";
            xml += "        </bpmndi:BPMNLabel>\n";
        }
        xml += "      </bpmndi:BPMNShape>\n";
    }

    for (const auto& e : diagram.edges) {
        const BpmnNode* fn = diagram.FindNode(e.fromNodeId);
        const BpmnNode* tn = diagram.FindNode(e.toNodeId);
        if (!fn || !tn) continue;
        const auto wpts = BuildEdgeWorldPoints(diagram, *fn, *tn, e, BpmnRoutingMode::Orthogonal);
        xml += "      <bpmndi:BPMNEdge id=\"Flow_" + std::to_string(e.id) + "_di\" bpmnElement=\"Flow_" + std::to_string(e.id) + "\">\n";
        for (const auto& pt : wpts) {
            xml += "        <di:waypoint x=\"" + std::to_string(static_cast<int>(pt.x)) +
                   "\" y=\"" + std::to_string(static_cast<int>(pt.y)) + "\" />\n";
        }
        if (std::abs(e.labelOffset.x) > 0.5f || std::abs(e.labelOffset.y) > 0.5f) {
            xml += "        <bpmndi:BPMNLabel>\n";
            xml += "          <dc:Bounds x=\"" + std::to_string(static_cast<int>(e.labelOffset.x)) +
                   "\" y=\"" + std::to_string(static_cast<int>(e.labelOffset.y)) +
                   "\" width=\"80\" height=\"20\" />\n";
            xml += "        </bpmndi:BPMNLabel>\n";
        }
        xml += "      </bpmndi:BPMNEdge>\n";
    }

    xml += "    </bpmndi:BPMNPlane>\n";
    xml += "  </bpmndi:BPMNDiagram>\n";
    xml += "</bpmn:definitions>\n";
    return xml;
}

bool BpmnEditor::ImportBpmnXml(std::string_view xml, BpmnDiagram& outDiagram) {
    if (xml.find("<bpmn:") == std::string_view::npos && xml.find("<definitions") == std::string_view::npos) {
        return false;
    }

    BpmnDiagram parsed;
    parsed.Clear();
    parsed.name = "Imported BPMN 2.0 Process";

    std::unordered_map<std::string, uint32_t> nodeIdMap;
    std::unordered_map<std::string, uint32_t> laneIdMap;
    std::unordered_map<std::string, uint32_t> edgeIdMap;
    std::vector<std::pair<uint32_t, std::string>> pendingBoundaryAttach;

    size_t pos = 0;
    while (pos < xml.size()) {
        const size_t lt = xml.find('<', pos);
        if (lt == std::string_view::npos) break;
        const size_t gt = xml.find('>', lt + 1);
        if (gt == std::string_view::npos) break;

        const std::string_view tagContent = xml.substr(lt + 1, gt - lt - 1);
        pos = gt + 1;
        if (tagContent.empty() || tagContent[0] == '?' || tagContent[0] == '!' || tagContent[0] == '/') continue;

        size_t nameEnd = 0;
        while (nameEnd < tagContent.size() && tagContent[nameEnd] != ' ' && tagContent[nameEnd] != '\t' &&
               tagContent[nameEnd] != '\r' && tagContent[nameEnd] != '\n' && tagContent[nameEnd] != '/') {
            ++nameEnd;
        }
        std::string_view tagName = tagContent.substr(0, nameEnd);
        const size_t colon = tagName.find(':');
        const std::string_view localName = (colon != std::string_view::npos) ? tagName.substr(colon + 1) : tagName;

        if (localName == "process") {
            const std::string procName = ExtractXmlAttr(tagContent, "name");
            if (!procName.empty()) parsed.name = procName;
        } else if (localName == "lane") {
            const std::string xmlId = ExtractXmlAttr(tagContent, "id");
            const std::string name = ExtractXmlAttr(tagContent, "name");
            std::string poolName = ExtractXmlAttr(tagContent, "poolName");
            if (poolName.empty()) poolName = parsed.name;
            BpmnLane l{};
            l.id = ParseTrailingIdNumber(xmlId);
            if (l.id == 0) l.id = parsed.nextId++;
            else parsed.nextId = std::max(parsed.nextId, l.id + 1);
            l.name = name.empty() ? "Lane" : name;
            l.poolName = poolName;
            l.rect = Rect(80.0f, 80.0f + static_cast<float>(parsed.lanes.size()) * 190.0f, 1280.0f, 190.0f);
            laneIdMap[xmlId] = l.id;
            parsed.lanes.push_back(std::move(l));
        } else if (localName == "startEvent" || localName == "intermediateCatchEvent" || localName == "intermediateThrowEvent" ||
                   localName == "boundaryEvent" || localName == "endEvent" ||
                   localName == "task" || localName == "userTask" || localName == "serviceTask" ||
                   localName == "sendTask" || localName == "receiveTask" || localName == "scriptTask" ||
                   localName == "businessRuleTask" || localName == "manualTask" || localName == "callActivity" ||
                   localName == "subProcess" ||
                   localName == "exclusiveGateway" || localName == "parallelGateway" ||
                   localName == "inclusiveGateway" || localName == "eventBasedGateway" ||
                   localName == "dataObjectReference" || localName == "dataStoreReference" || localName == "textAnnotation") {
            const std::string xmlId = ExtractXmlAttr(tagContent, "id");
            if (xmlId.empty()) continue;

            BpmnNode n{};
            n.id = ParseTrailingIdNumber(xmlId);
            if (n.id == 0) n.id = parsed.nextId++;
            else parsed.nextId = std::max(parsed.nextId, n.id + 1);
            n.label = ExtractXmlAttr(tagContent, "name");

            if (localName == "startEvent") n.type = BpmnNodeType::StartEvent;
            else if (localName == "intermediateCatchEvent" || localName == "intermediateThrowEvent") n.type = BpmnNodeType::IntermediateEvent;
            else if (localName == "boundaryEvent") {
                n.type = BpmnNodeType::BoundaryEvent;
                n.nonInterrupting = (ExtractXmlAttr(tagContent, "cancelActivity") == "false");
                const std::string attachedRef = ExtractXmlAttr(tagContent, "attachedToRef");
                if (!attachedRef.empty()) pendingBoundaryAttach.emplace_back(n.id, attachedRef);
            } else if (localName == "endEvent") n.type = BpmnNodeType::EndEvent;
            else if (localName == "subProcess") n.type = BpmnNodeType::SubProcess;
            else if (localName == "exclusiveGateway") { n.type = BpmnNodeType::Gateway; n.gatewayType = BpmnGatewayType::Exclusive; }
            else if (localName == "parallelGateway")  { n.type = BpmnNodeType::Gateway; n.gatewayType = BpmnGatewayType::Parallel; }
            else if (localName == "inclusiveGateway") { n.type = BpmnNodeType::Gateway; n.gatewayType = BpmnGatewayType::Inclusive; }
            else if (localName == "eventBasedGateway"){ n.type = BpmnNodeType::Gateway; n.gatewayType = BpmnGatewayType::EventBased; }
            else if (localName == "dataObjectReference") n.type = BpmnNodeType::DataObject;
            else if (localName == "dataStoreReference")  n.type = BpmnNodeType::DataStore;
            else if (localName == "textAnnotation")      n.type = BpmnNodeType::TextAnnotation;
            else {
                n.type = BpmnNodeType::Task;
                if (localName == "serviceTask")           n.taskType = BpmnTaskType::Service;
                else if (localName == "sendTask")         n.taskType = BpmnTaskType::Send;
                else if (localName == "receiveTask")      n.taskType = BpmnTaskType::Receive;
                else if (localName == "scriptTask")       n.taskType = BpmnTaskType::Script;
                else if (localName == "businessRuleTask") n.taskType = BpmnTaskType::BusinessRule;
                else if (localName == "manualTask")       n.taskType = BpmnTaskType::Manual;
                else if (localName == "callActivity")     n.taskType = BpmnTaskType::CallActivity;
                else                                      n.taskType = BpmnTaskType::User;
            }
            n.size = BpmnDiagram::DefaultNodeSize(n.type);
            n.pos = {140.0f + static_cast<float>(parsed.nodes.size()) * 160.0f, 180.0f};

            if (tagContent.back() != '/') {
                const std::string closeNeedle = "</" + std::string(tagName) + ">";
                const size_t closePos = xml.find(closeNeedle, pos);
                if (closePos != std::string_view::npos) {
                    const std::string_view body = xml.substr(pos, closePos - pos);
                    if (body.find("messageEventDefinition") != std::string_view::npos)        n.eventDef = BpmnEventDefinition::Message;
                    else if (body.find("timerEventDefinition") != std::string_view::npos)     n.eventDef = BpmnEventDefinition::Timer;
                    else if (body.find("errorEventDefinition") != std::string_view::npos)     n.eventDef = BpmnEventDefinition::Error;
                    else if (body.find("signalEventDefinition") != std::string_view::npos)    n.eventDef = BpmnEventDefinition::Signal;
                    else if (body.find("escalationEventDefinition") != std::string_view::npos) n.eventDef = BpmnEventDefinition::Escalation;
                    else if (body.find("terminateEventDefinition") != std::string_view::npos)  n.eventDef = BpmnEventDefinition::Terminate;
                    pos = closePos + closeNeedle.size();
                }
            }

            nodeIdMap[xmlId] = n.id;
            parsed.nodes.push_back(std::move(n));
        } else if (localName == "sequenceFlow" || localName == "messageFlow" || localName == "association") {
            const std::string xmlId = ExtractXmlAttr(tagContent, "id");
            const std::string srcRef = ExtractXmlAttr(tagContent, "sourceRef");
            const std::string dstRef = ExtractXmlAttr(tagContent, "targetRef");
            if (!nodeIdMap.contains(srcRef) || !nodeIdMap.contains(dstRef)) continue;

            BpmnEdge e{};
            e.id = ParseTrailingIdNumber(xmlId);
            if (e.id == 0) e.id = parsed.nextId++;
            else parsed.nextId = std::max(parsed.nextId, e.id + 1);
            e.fromNodeId = nodeIdMap[srcRef];
            e.toNodeId = nodeIdMap[dstRef];
            e.label = ExtractXmlAttr(tagContent, "name");
            if (localName == "messageFlow") e.flowType = BpmnFlowType::MessageFlow;
            else if (localName == "association") e.flowType = BpmnFlowType::Association;
            else if (ExtractXmlAttr(tagContent, "isDefault") == "true") e.flowType = BpmnFlowType::DefaultFlow;
            else if (ExtractXmlAttr(tagContent, "isConditional") == "true") e.flowType = BpmnFlowType::ConditionalFlow;
            else e.flowType = BpmnFlowType::SequenceFlow;

            if (!xmlId.empty()) edgeIdMap[xmlId] = e.id;
            parsed.edges.push_back(std::move(e));
        } else if (localName == "BPMNShape") {
            const std::string elemRef = ExtractXmlAttr(tagContent, "bpmnElement");
            const std::string expandedStr = ExtractXmlAttr(tagContent, "isExpanded");
            const size_t shapeEnd = xml.find("</bpmndi:BPMNShape>", pos);
            const size_t altEnd = (shapeEnd == std::string_view::npos) ? xml.find("</BPMNShape>", pos) : shapeEnd;
            if (altEnd != std::string_view::npos) {
                const std::string_view shapeBody = xml.substr(pos, altEnd - pos);
                const size_t bPos = shapeBody.find("Bounds");
                if (bPos != std::string_view::npos) {
                    const size_t bEnd = shapeBody.find('>', bPos);
                    if (bEnd != std::string_view::npos) {
                        const std::string_view bTag = shapeBody.substr(bPos, bEnd - bPos);
                        const float x = ExtractXmlFloatAttr(bTag, "x", 0.0f);
                        const float y = ExtractXmlFloatAttr(bTag, "y", 0.0f);
                        const float w = ExtractXmlFloatAttr(bTag, "width", 100.0f);
                        const float h = ExtractXmlFloatAttr(bTag, "height", 80.0f);
                        if (nodeIdMap.contains(elemRef)) {
                            if (BpmnNode* n = parsed.FindNode(nodeIdMap[elemRef])) {
                                n->pos = {x, y};
                                n->size = {w, h};
                                if (!expandedStr.empty()) n->isExpanded = (expandedStr == "true");
                            }
                        } else if (laneIdMap.contains(elemRef)) {
                            if (BpmnLane* l = parsed.FindLane(laneIdMap[elemRef])) {
                                l->rect = Rect(x, y, w, h);
                            }
                        }
                    }
                }
                pos = altEnd;
            }
        } else if (localName == "BPMNEdge") {
            const std::string elemRef = ExtractXmlAttr(tagContent, "bpmnElement");
            const size_t edgeEnd = xml.find("</bpmndi:BPMNEdge>", pos);
            const size_t altEnd = (edgeEnd == std::string_view::npos) ? xml.find("</BPMNEdge>", pos) : edgeEnd;
            if (altEnd != std::string_view::npos && edgeIdMap.contains(elemRef)) {
                const std::string_view edgeBody = xml.substr(pos, altEnd - pos);
                std::vector<Vec2> allPts;
                size_t wpScan = 0;
                while (wpScan < edgeBody.size()) {
                    const size_t wPos = edgeBody.find("waypoint", wpScan);
                    if (wPos == std::string_view::npos) break;
                    const size_t wEnd = edgeBody.find('>', wPos);
                    if (wEnd == std::string_view::npos) break;
                    const std::string_view wTag = edgeBody.substr(wPos, wEnd - wPos);
                    allPts.push_back(Vec2{ExtractXmlFloatAttr(wTag, "x", 0.0f), ExtractXmlFloatAttr(wTag, "y", 0.0f)});
                    wpScan = wEnd + 1;
                }
                if (allPts.size() > 2) {
                    if (BpmnEdge* e = parsed.FindEdge(edgeIdMap[elemRef])) {
                        e->waypoints.assign(allPts.begin() + 1, allPts.end() - 1);
                    }
                }
                pos = altEnd;
            }
        }
    }

    for (const auto& [bid, hostXmlId] : pendingBoundaryAttach) {
        if (nodeIdMap.contains(hostXmlId)) {
            BpmnNode* bn = parsed.FindNode(bid);
            const BpmnNode* host = parsed.FindNode(nodeIdMap[hostXmlId]);
            if (bn && host) {
                bn->attachedToNodeId = host->id;
                bn->boundaryRatioX = std::clamp((bn->Center().x - host->pos.x) / std::max(1.0f, host->size.x), 0.15f, 0.85f);
            }
        }
    }

    for (auto& n : parsed.nodes) {
        for (const auto& l : parsed.lanes) {
            if (l.rect.Contains(n.Center())) {
                n.laneId = l.id;
                break;
            }
        }
    }
    parsed.SyncBoundaryEvents();

    if (parsed.nodes.empty() && parsed.lanes.empty()) {
        return false;
    }

    outDiagram = std::move(parsed);
    return true;
}

} // namespace guikit

