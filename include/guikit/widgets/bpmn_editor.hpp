#pragma once

#include "guikit/core/control.hpp"
#include "guikit/core/response.hpp"
#include "guikit/core/theme.hpp"
#include "guikit/core/tokens.hpp"
#include "guikit/core/types.hpp"
#include "guikit/graphics/icon.hpp"
#include "keel/array.h"
#include "keel/core.h"
#include "keel/optional.h"
#include "keel/small_array.h"
#include "keel/string.h"

namespace guikit {

class Context;
class ICanvas;

/// Primary BPMN 2.0 element classification.
enum class BpmnNodeType : keel::u8 {
    StartEvent = 0,         ///< Thin single-circle Start Event (36-40px)
    IntermediateEvent,      ///< Double-ring Intermediate Catch/Throw Event
    BoundaryEvent,          ///< Double-ring Boundary Event attached to the border of a Task / SubProcess
    EndEvent,               ///< Thick-ring End Event
    Task,                   ///< Rounded rectangle Activity / Task (User, Service, Script, Send, Receive, etc.)
    SubProcess,             ///< Activity container with bottom-center [+] expansion marker
    Gateway,                ///< Diamond routing Gateway (Exclusive XOR, Parallel AND, Inclusive OR, EventBased)
    DataObject,             ///< Dog-eared document artifact
    DataStore,              ///< Cylindrical database storage artifact
    TextAnnotation          ///< Open-bracket `[` text comment artifact
};

/// Event trigger / result symbol drawn inside Start, Intermediate, Boundary, and End events.
enum class BpmnEventDefinition : keel::u8 {
    None = 0,               ///< Blank circle (Untyped Start / Intermediate / End)
    Message,                ///< Envelope icon (Message Catch / Throw)
    Timer,                  ///< Clock face icon (Timer cycle / timeout)
    Error,                  ///< Lightning bolt N-shape (Error boundary / end)
    Signal,                 ///< Triangle icon (Broadcast Signal)
    Escalation,             ///< Upward chevron arrow (SLA Escalation)
    Terminate               ///< Solid inner circle (Terminate Process End Event)
};

/// Activity specialization marker drawn in the top-left corner of a `BpmnNodeType::Task`.
enum class BpmnTaskType : keel::u8 {
    None = 0,               ///< Abstract / untyped task
    User,                   ///< Person bust icon (Human workflow step)
    Service,                ///< Gear / cogwheel icon (Automated microservice / API call)
    Send,                   ///< Filled dark envelope (Send message task)
    Receive,                ///< Outlined envelope (Receive message task)
    Script,                 ///< S-curved script page lines (Inline script execution)
    Manual,                 ///< Hand / physical action marker
    BusinessRule,           ///< Decision table grid header icon (DMN Business Rule)
    CallActivity            ///< Thick outer border reusable process call
};

/// Routing semantics marker drawn inside a `BpmnNodeType::Gateway` diamond.
enum class BpmnGatewayType : keel::u8 {
    Exclusive = 0,          ///< Data-based Exclusive Gateway (`XOR` with `×` marker)
    Parallel,               ///< Parallel fork/join Gateway (`AND` with `+` marker)
    Inclusive,              ///< Inclusive Gateway (`OR` with `○` ring marker)
    EventBased              ///< Event-Based Gateway (double circle + inner pentagon)
};

/// Connection semantics for BPMN 2.0 edges (`BpmnEdge`).
enum class BpmnFlowType : keel::u8 {
    SequenceFlow = 0,       ///< Solid directed line with filled arrowhead
    ConditionalFlow,        ///< Solid directed line with mini-diamond at source and filled arrowhead
    DefaultFlow,            ///< Solid directed line with slash tick `\` at source and filled arrowhead
    MessageFlow,            ///< Dashed line with open circle at source and arrowhead (cross-pool communication)
    Association             ///< Dotted line connecting DataObject / DataStore / TextAnnotation
};

/// Cardinal port attachment side on a BPMN node.
enum class BpmnPortSide : keel::u8 {
    Auto = 0,               ///< Automatically choose optimal side based on relative node geometry
    Left,                   ///< West port
    Right,                  ///< East port
    Top,                    ///< North port
    Bottom                  ///< South port
};

/// Active interactive cursor/creation tool in the `bpmn.io`-style left palette.
enum class BpmnTool : keel::u8 {
    Select = 0,             ///< Standard click/move/context-pad tool (`V`)
    Hand,                   ///< Canvas pan hand tool (`H`)
    Lasso,                  ///< Marquee multi-selection box tool (`L`)
    Space,                  ///< Signature `bpmn.io` Space Tool: splits canvas and pushes/pulls nodes & lanes (`S`)
    GlobalConnect,          ///< Interactive wire connection tool (`C`)
    CreateStartEvent,       ///< Click canvas to place a Start Event
    CreateIntermediateEvent,///< Click canvas to place an Intermediate Event (or hover Task edge for Boundary Event!)
    CreateEndEvent,         ///< Click canvas to place an End Event
    CreateGateway,          ///< Click canvas to place a Gateway
    CreateTask,             ///< Click canvas to place a Task
    CreateSubProcess,       ///< Click canvas to place a SubProcess
    CreateDataObject,       ///< Click canvas to place a DataObject
    CreateDataStore,        ///< Click canvas to place a DataStore
    CreateAnnotation,       ///< Click canvas to place a TextAnnotation
    CreateLane              ///< Click canvas to add a new Pool / Swimlane
};

/// Edge routing geometry algorithm.
enum class BpmnRoutingMode : keel::u8 {
    Orthogonal = 0,         ///< Manhattan H-V-H / V-H-V routing with rounded bends (classic BPMN 2.0 standard)
    Bezier                  ///< Smooth cubic Bezier curve routing
};

/// Visual color theme preset for the BPMN canvas.
enum class BpmnThemePreset : keel::u8 {
    Auto = 0,               ///< Adapts automatically to the active `UiTheme` (Dark / Light)
    BpmnIoClassic,          ///< Signature `bpmn.io` daylight paper canvas with crisp charcoal & semantic strokes
    CamundaDark,            ///< Deep slate dark modeler with vibrant semantic BPMN accents
    Blueprint               ///< Engineering CAD cobalt blueprint grid
};

/// Horizontal Participant Pool / Swimlane band (`BpmnLane`).
struct BpmnLane {
    keel::u32 id{0};
    keel::FixedString<128> poolName{"Order Fulfillment Pool"};
    keel::FixedString<128> name{"Lane"};
    Rect rect{80.0f, 80.0f, 1260.0f, 210.0f}; ///< World-space bounding rectangle
    Color accentColor{0, 0, 0, 0};            ///< Optional lane header bar accent color (alpha 0 = default)

    [[nodiscard]] BpmnLane clone() const noexcept { return *this; }
};

/// Individual BPMN 2.0 graphical node (`BpmnNode`).
struct BpmnNode {
    keel::u32 id{0};
    BpmnNodeType type{BpmnNodeType::Task};
    keel::FixedString<256> label{"Task"};
    keel::FixedString<256> documentation{};   ///< Optional SLA / condition / microservice metadata
    Vec2 pos{0.0f, 0.0f};                     ///< World-space top-left position
    Vec2 size{0.0f, 0.0f};                    ///< World-space dimensions (0,0 = auto-sized by BpmnNodeType)

    BpmnTaskType taskType{BpmnTaskType::User};
    BpmnGatewayType gatewayType{BpmnGatewayType::Exclusive};
    BpmnEventDefinition eventDef{BpmnEventDefinition::None};
    bool nonInterrupting{false};              ///< Dashed border for non-interrupting Boundary / Intermediate events

    keel::u32 attachedToNodeId{0};             ///< For `BoundaryEvent`: parent `Task` / `SubProcess` ID
    float boundaryRatioX{0.72f};              ///< Normalized position [0.15 .. 0.85] along parent bottom edge
    keel::u32 laneId{0};                       ///< Optional owning swimlane ID

    // --- Token Simulation & SLA Telemetry ---
    keel::u32 activeOutgoingEdgeId{0};         ///< Selected outgoing edge ID for Exclusive (XOR) gateway in simulation
    float slaDurationMs{420.0f};              ///< Nominal activity duration / SLA metric (ms)
    keel::u32 tokenExecutionCount{0};          ///< Total tokens that have passed through this node
    keel::u32 activeTokensInside{0};           ///< Live tokens currently dwelling inside this node

    // --- Activity Markers & Layout Offsets ---
    bool isExpanded{false};                   ///< True for expanded Sub-Process container (`320x180`)
    bool loopMarker{false};                   ///< Bottom-center circular loop arrow marker
    bool parallelMultiInstance{false};        ///< Bottom-center `|||` parallel multi-instance marker
    bool compensationMarker{false};           ///< Bottom-center `<<` compensation marker
    Vec2 labelOffset{0.0f, 0.0f};             ///< World-space offset for draggable external labels (Events/Gateways/Data)
    Color customColor{0, 0, 0, 0};            ///< Optional custom semantic tint (alpha 0 = default)

    [[nodiscard]] BpmnNode clone() const noexcept { return *this; }

    [[nodiscard]] Rect Bounds() const noexcept {
        return Rect(pos.x, pos.y, size.x, size.y);
    }
    [[nodiscard]] Vec2 Center() const noexcept {
        return Vec2{pos.x + size.x * 0.5f, pos.y + size.y * 0.5f};
    }
};

/// Directed BPMN 2.0 connection / flow (`BpmnEdge`).
struct BpmnEdge {
    keel::u32 id{0};
    keel::u32 fromNodeId{0};
    keel::u32 toNodeId{0};
    BpmnPortSide fromPort{BpmnPortSide::Auto};
    BpmnPortSide toPort{BpmnPortSide::Auto};
    BpmnFlowType flowType{BpmnFlowType::SequenceFlow};
    keel::FixedString<128> label{};
    Vec2 labelOffset{0.0f, 0.0f};             ///< World-space offset for draggable edge label
    keel::Array<Vec2> waypoints{};            ///< Optional intermediate waypoints in world space

    [[nodiscard]] BpmnEdge clone() const {
        BpmnEdge out{};
        out.id = id;
        out.fromNodeId = fromNodeId;
        out.toNodeId = toNodeId;
        out.fromPort = fromPort;
        out.toPort = toPort;
        out.flowType = flowType;
        out.label = label;
        out.labelOffset = labelOffset;
        out.waypoints = waypoints.clone();
        return out;
    }
};

/// Live animated process instance token for `bpmn-js-token-simulation` mode.
struct BpmnToken {
    keel::u32 id{0};
    keel::u32 edgeId{0};                       ///< Non-zero when token is traveling along an edge
    keel::u32 nodeId{0};                       ///< Non-zero when token is executing inside a Task / IntermediateEvent
    float progress{0.0f};                     ///< Normalized progress in [0.0 .. 1.0]
    Color color{16, 185, 129, 255};           ///< Token badge color
    keel::u32 instanceId{1};                   ///< Process instance identifier

    [[nodiscard]] BpmnToken clone() const noexcept { return *this; }
};

/// Complete BPMN 2.0 diagram document model (`BpmnDiagram`).
struct BpmnDiagram {
    keel::FixedString<128> name{"BPMN 2.0 Process"};
    keel::Array<BpmnLane> lanes{};
    keel::Array<BpmnNode> nodes{};
    keel::Array<BpmnEdge> edges{};
    keel::Array<BpmnToken> tokens{};
    keel::u32 nextId{1};

    [[nodiscard]] BpmnDiagram clone() const {
        BpmnDiagram out{};
        out.name = name;
        out.lanes = lanes.clone();
        out.nodes = nodes.clone();
        out.edges = edges.clone();
        out.tokens = tokens.clone();
        out.nextId = nextId;
        return out;
    }

    [[nodiscard]] keel::u32 AllocateId() noexcept {
        for (const auto& l : lanes) if (l.id >= nextId) nextId = l.id + 1;
        for (const auto& n : nodes) if (n.id >= nextId) nextId = n.id + 1;
        for (const auto& e : edges) if (e.id >= nextId) nextId = e.id + 1;
        return nextId++;
    }

    [[nodiscard]] BpmnNode* FindNode(keel::u32 id) noexcept {
        if (id == 0) return nullptr;
        for (auto& n : nodes) {
            if (n.id == id) return &n;
        }
        return nullptr;
    }

    [[nodiscard]] const BpmnNode* FindNode(keel::u32 id) const noexcept {
        if (id == 0) return nullptr;
        for (const auto& n : nodes) {
            if (n.id == id) return &n;
        }
        return nullptr;
    }

    [[nodiscard]] BpmnEdge* FindEdge(keel::u32 id) noexcept {
        if (id == 0) return nullptr;
        for (auto& e : edges) {
            if (e.id == id) return &e;
        }
        return nullptr;
    }

    [[nodiscard]] const BpmnEdge* FindEdge(keel::u32 id) const noexcept {
        if (id == 0) return nullptr;
        for (const auto& e : edges) {
            if (e.id == id) return &e;
        }
        return nullptr;
    }

    [[nodiscard]] BpmnLane* FindLane(keel::u32 id) noexcept {
        if (id == 0) return nullptr;
        for (auto& l : lanes) {
            if (l.id == id) return &l;
        }
        return nullptr;
    }

    [[nodiscard]] const BpmnLane* FindLane(keel::u32 id) const noexcept {
        if (id == 0) return nullptr;
        for (const auto& l : lanes) {
            if (l.id == id) return &l;
        }
        return nullptr;
    }

    [[nodiscard]] static constexpr Vec2 DefaultNodeSize(BpmnNodeType type) noexcept {
        switch (type) {
            case BpmnNodeType::StartEvent:
            case BpmnNodeType::IntermediateEvent:
            case BpmnNodeType::EndEvent:
                return {36.0f, 36.0f};
            case BpmnNodeType::BoundaryEvent:
                return {32.0f, 32.0f};
            case BpmnNodeType::Gateway:
                return {50.0f, 50.0f};
            case BpmnNodeType::Task:
                return {120.0f, 80.0f};
            case BpmnNodeType::SubProcess:
                return {140.0f, 84.0f};
            case BpmnNodeType::DataObject:
                return {36.0f, 50.0f};
            case BpmnNodeType::DataStore:
                return {50.0f, 50.0f};
            case BpmnNodeType::TextAnnotation:
                return {140.0f, 50.0f};
        }
        return {120.0f, 80.0f};
    }

    keel::u32 AddLane(keel::StringRef poolName, keel::StringRef laneName, Rect rect, Color accent = Color(0, 0, 0, 0)) {
        BpmnLane lane{};
        lane.id = AllocateId();
        lane.poolName = poolName;
        lane.name = laneName;
        lane.rect = rect;
        lane.accentColor = accent;
        lanes.push_back(keel::move(lane));
        return lanes.back().id;
    }

    keel::u32 AddNode(BpmnNode node) {
        if (node.id == 0) {
            node.id = AllocateId();
        } else if (node.id >= nextId) {
            nextId = node.id + 1;
        }
        if (node.size.x <= 1.0f || node.size.y <= 1.0f) {
            node.size = DefaultNodeSize(node.type);
        }
        nodes.push_back(keel::move(node));
        SyncBoundaryEvents();
        return nodes.back().id;
    }

    keel::u32 AddEdge(
        keel::u32 fromId,
        keel::u32 toId,
        BpmnFlowType type = BpmnFlowType::SequenceFlow,
        keel::StringRef label = {},
        BpmnPortSide fromPort = BpmnPortSide::Auto,
        BpmnPortSide toPort = BpmnPortSide::Auto
    ) {
        if (fromId == 0 || toId == 0 || fromId == toId) return 0;
        for (const auto& existing : edges) {
            if (existing.fromNodeId == fromId && existing.toNodeId == toId) {
                return existing.id;
            }
        }
        BpmnEdge edge{};
        edge.id = AllocateId();
        edge.fromNodeId = fromId;
        edge.toNodeId = toId;
        edge.flowType = type;
        edge.label = label;
        edge.fromPort = fromPort;
        edge.toPort = toPort;
        edges.push_back(keel::move(edge));
        return edges.back().id;
    }

    bool RemoveEdge(keel::u32 edgeId) noexcept {
        if (edgeId == 0) return false;
        const keel::usize before = edges.size();
        RemoveMatching(edges, [edgeId](const BpmnEdge& e) { return e.id == edgeId; });
        RemoveMatching(tokens, [edgeId](const BpmnToken& t) { return t.edgeId == edgeId; });
        return edges.size() != before;
    }

    bool RemoveNode(keel::u32 nodeId) noexcept {
        if (nodeId == 0) return false;
        keel::SmallArray<keel::u32, 8> toRemove;
        toRemove.push_back(nodeId);
        for (const auto& n : nodes) {
            if (n.type == BpmnNodeType::BoundaryEvent && n.attachedToNodeId == nodeId) {
                toRemove.push_back(n.id);
            }
        }
        const keel::usize before = nodes.size();
        for (keel::u32 remId : toRemove) {
            RemoveMatching(edges, [remId](const BpmnEdge& e) {
                return e.fromNodeId == remId || e.toNodeId == remId;
            });
            RemoveMatching(tokens, [remId](const BpmnToken& t) {
                return t.nodeId == remId;
            });
            RemoveMatching(nodes, [remId](const BpmnNode& n) {
                return n.id == remId;
            });
        }
        return nodes.size() != before;
    }

    bool RemoveLane(keel::u32 laneId) noexcept {
        if (laneId == 0) return false;
        const keel::usize before = lanes.size();
        RemoveMatching(lanes, [laneId](const BpmnLane& l) { return l.id == laneId; });
        for (auto& n : nodes) {
            if (n.laneId == laneId) n.laneId = 0;
        }
        return lanes.size() != before;
    }

    void SyncBoundaryEvents() noexcept {
        for (auto& n : nodes) {
            if (n.type != BpmnNodeType::BoundaryEvent || n.attachedToNodeId == 0) continue;
            const BpmnNode* host = FindNode(n.attachedToNodeId);
            if (!host) continue;
            const float ratio = (n.boundaryRatioX < 0.15f) ? 0.15f : ((n.boundaryRatioX > 0.85f) ? 0.85f : n.boundaryRatioX);
            n.boundaryRatioX = ratio;
            if (n.size.x <= 1.0f || n.size.y <= 1.0f) {
                n.size = DefaultNodeSize(BpmnNodeType::BoundaryEvent);
            }
            n.pos.x = host->pos.x + host->size.x * ratio - n.size.x * 0.5f;
            n.pos.y = host->pos.y + host->size.y - n.size.y * 0.5f;
        }
    }

    void ResetSimulationStats() noexcept {
        tokens.clear();
        for (auto& n : nodes) {
            n.tokenExecutionCount = 0;
            n.activeTokensInside = 0;
        }
    }

    void Clear() noexcept {
        lanes.clear();
        nodes.clear();
        edges.clear();
        tokens.clear();
        nextId = 1;
    }

private:
    template <typename T, typename Pred>
    static void RemoveMatching(keel::Array<T>& arr, Pred&& pred) noexcept {
        keel::usize writeIdx = 0;
        for (keel::usize readIdx = 0; readIdx < arr.size(); ++readIdx) {
            if (!pred(arr[readIdx])) {
                if (writeIdx != readIdx) {
                    arr[writeIdx] = keel::move(arr[readIdx]);
                }
                ++writeIdx;
            }
        }
        while (arr.size() > writeIdx) {
            arr.pop_back();
        }
    }
};

/// Declarative configuration options for the autonomous `BpmnEditor` widget (`ui.BpmnEditor(...)`).
struct BpmnEditorOptions {
    // --- BPMN Modeler UX & Visual Configuration ---
    BpmnRoutingMode routingMode{BpmnRoutingMode::Orthogonal};
    BpmnThemePreset themePreset{BpmnThemePreset::Auto};
    bool showLeftPalette{true};         ///< Floating `bpmn.io` left tool & element creation dock
    bool showTopBar{true};              ///< Floating top process breadcrumb, Undo/Redo, & Token Simulation bar
    bool showContextPad{true};          ///< Signature `bpmn.io` floating Context Pad + `Replace Menu` around selection
    bool showMinimap{true};             ///< Interactive bottom-right world minimap with viewport indicator
    bool showZoomControls{true};        ///< Floating bottom-left zoom / fit / reset HUD
    bool showGrid{true};                ///< Render dot/line world grid
    bool snapToGrid{true};              ///< Snap dragged/created elements to grid
    bool smartGuides{true};             ///< Render laser alignment guides (center X/Y, edges) while dragging
    float gridStep{20.0f};              ///< World-space grid step in pixels (10 .. 40)
    bool readOnly{false};               ///< Disable graph topology mutations while keeping pan/zoom/selection

    // --- Token Simulation (`bpmn-js-token-simulation`) & Telemetry ---
    bool tokenSimulation{false};        ///< Enable interactive process token simulation mode
    bool autoSpawnTokens{true};         ///< Automatically trigger Start Events periodically in simulation mode
    float simulationSpeed{1.0f};        ///< Token animation speed multiplier (0.25x .. 3.0x)
    bool showSlaHeatmap{false};         ///< Render SLA duration / bottleneck heatmap glow on activities

    // --- Optional Two-Way State Bindings ---
    bool* tokenSimulationPtr{nullptr};          ///< Optional binding for the canvas "Token Simulation" toggle button
    BpmnRoutingMode* routingModePtr{nullptr};   ///< Optional binding for Orthogonal / Bezier routing toggle
    keel::u32* selectedNodeIdPtr{nullptr};       ///< Optional binding to observe/drive primary selected node ID
    keel::u32* selectedEdgeIdPtr{nullptr};       ///< Optional binding to observe/drive primary selected edge ID
    keel::u32* selectedLaneIdPtr{nullptr};       ///< Optional binding to observe/drive primary selected lane ID

    // --- Sizing & Styling ---
    AxisSize width{};                     ///< Horizontal sizing policy (`Auto` -> `Stretch`, `Fit`, or `Fixed(dp)`)
    AxisSize height{SizeMode::Stretch};   ///< Vertical sizing policy (`Stretch` default, `Fit`, or `Fixed(dp)`)
    StateColor background{};
    Border border{};
    UiSize size{UiSize::Md};
    bool disabled{false};
    UiText tooltip{};
};

/// Rich telemetry and selection state returned by `ui.BpmnEditor(...)`.
struct BpmnEditorResponse : public Response {
    Rect canvasRect{};                  ///< Resolved screen-space viewport rectangle
    Vec2 pan{0.0f, 0.0f};               ///< Current world pan offset in screen pixels
    float zoom{1.0f};                   ///< Current canvas zoom factor (0.25 .. 2.50)
    Vec2 mouseWorldPos{0.0f, 0.0f};     ///< Current cursor position in BPMN world coordinates

    BpmnTool activeTool{BpmnTool::Select};
    keel::u32 selectedNodeId{0};         ///< Primary selected node ID (0 = none)
    keel::u32 selectedEdgeId{0};         ///< Primary selected edge ID (0 = none)
    keel::u32 selectedLaneId{0};         ///< Primary selected lane ID (0 = none)
    keel::usize selectedNodeCount{0};        ///< Number of currently selected nodes (including multi-select)
    keel::u32 hoveredNodeId{0};          ///< Node currently under cursor
    keel::u32 hoveredEdgeId{0};          ///< Edge currently under cursor

    bool simulationActive{false};       ///< True when Token Simulation mode is active
    keel::usize activeTokenCount{0};         ///< Number of live tokens currently flowing/executing
    keel::usize undoDepth{0};                ///< Available Undo snapshots
    keel::usize redoDepth{0};                ///< Available Redo snapshots
    bool diagramModified{false};        ///< True on the frame a structural or property edit occurred
};

/// Autonomous `bpmn.io`-grade BPMN 2.0 Diagram Modeler & Token Simulation Widget.
class BpmnEditor {
public:
    static BpmnEditorResponse Run(
        Context& ctx,
        BpmnDiagram& diagram,
        const BpmnEditorOptions& opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    [[nodiscard]] static Vec2 Measure(
        const BpmnEditorOptions& opts,
        UiSize size,
        ICanvas* canvas = nullptr,
        const UiTheme& theme = {},
        float availableWidth = 0.0f
    );

    /// Serialize `diagram` into standard BPMN 2.0 XML (`<bpmn:definitions>` + `<bpmndi:BPMNDiagram>`).
    [[nodiscard]] static keel::String ExportBpmnXml(const BpmnDiagram& diagram);

    /// Parse standard BPMN 2.0 XML (`<bpmn:process>` + `<bpmndi:BPMNDiagram>`) into `outDiagram`.
    [[nodiscard]] static bool ImportBpmnXml(keel::StringRef xml, BpmnDiagram& outDiagram);
};

} // namespace guikit
