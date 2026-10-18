#include "../DemoContext.hpp"
#include "guikit/widgets/BpmnEditor.hpp"
#include <format>

namespace guikit::demo {

namespace {

void BuildOrderFulfillmentDiagram(guikit::BpmnDiagram& d) {
    d.Clear();
    d.name = "E-Commerce Order & Payment Orchestration";

    const uint32_t laneCust = d.AddLane("Order Fulfillment Pool", "Customer & Checkout", guikit::Rect(60.0f, 60.0f, 1340.0f, 190.0f));
    const uint32_t lanePay  = d.AddLane("Order Fulfillment Pool", "Payment & Fraud Engine", guikit::Rect(60.0f, 250.0f, 1340.0f, 230.0f));
    const uint32_t laneWh   = d.AddLane("Order Fulfillment Pool", "Warehouse & Logistics", guikit::Rect(60.0f, 480.0f, 1340.0f, 220.0f));

    // Row 1 (Customer & Checkout): center Y = 150.0f
    const uint32_t nStart = d.AddNode({
        .type = guikit::BpmnNodeType::StartEvent,
        .label = "Order Placed",
        .pos = {130.0f, 132.0f},
        .eventDef = guikit::BpmnEventDefinition::Message,
        .laneId = laneCust
    });

    const uint32_t nCheckout = d.AddNode({
        .type = guikit::BpmnNodeType::Task,
        .label = "Verify Cart & KYC",
        .documentation = "SLA: 180ms",
        .pos = {220.0f, 110.0f},
        .taskType = guikit::BpmnTaskType::User,
        .laneId = laneCust,
        .slaDurationMs = 320.0f
    });

    // Row 2 (Payment & Fraud Engine): center Y = 340.0f
    const uint32_t nRiskRule = d.AddNode({
        .type = guikit::BpmnNodeType::Task,
        .label = "Evaluate Fraud DMN",
        .documentation = "DMN Table v4.2",
        .pos = {220.0f, 300.0f},
        .taskType = guikit::BpmnTaskType::BusinessRule,
        .laneId = lanePay,
        .slaDurationMs = 280.0f
    });

    const uint32_t nGwRisk = d.AddNode({
        .type = guikit::BpmnNodeType::Gateway,
        .label = "Risk Approved?",
        .pos = {400.0f, 315.0f},
        .gatewayType = guikit::BpmnGatewayType::Exclusive,
        .laneId = lanePay
    });

    const uint32_t nCharge = d.AddNode({
        .type = guikit::BpmnNodeType::Task,
        .label = "Charge Payment API",
        .documentation = "Stripe / Adyen RPC",
        .pos = {530.0f, 300.0f},
        .taskType = guikit::BpmnTaskType::Service,
        .laneId = lanePay,
        .slaDurationMs = 640.0f
    });

    const uint32_t nBoundErr = d.AddNode({
        .type = guikit::BpmnNodeType::BoundaryEvent,
        .label = "Card Declined",
        .eventDef = guikit::BpmnEventDefinition::Error,
        .attachedToNodeId = nCharge,
        .boundaryRatioX = 0.72f,
        .laneId = lanePay
    });

    const uint32_t nNotifyFail = d.AddNode({
        .type = guikit::BpmnNodeType::Task,
        .label = "Send Decline Alert",
        .documentation = "Email + Push",
        .pos = {680.0f, 110.0f},
        .taskType = guikit::BpmnTaskType::Send,
        .laneId = laneCust,
        .slaDurationMs = 240.0f
    });

    const uint32_t nEndCancel = d.AddNode({
        .type = guikit::BpmnNodeType::EndEvent,
        .label = "Order Aborted",
        .pos = {860.0f, 132.0f},
        .eventDef = guikit::BpmnEventDefinition::Error,
        .laneId = laneCust
    });

    const uint32_t nGwFork = d.AddNode({
        .type = guikit::BpmnNodeType::Gateway,
        .label = "Parallel Fork",
        .pos = {760.0f, 315.0f},
        .gatewayType = guikit::BpmnGatewayType::Parallel,
        .laneId = lanePay
    });

    const uint32_t nInvoice = d.AddNode({
        .type = guikit::BpmnNodeType::Task,
        .label = "Emit Fiscal Invoice",
        .documentation = "PDF + Ledger",
        .pos = {870.0f, 300.0f},
        .taskType = guikit::BpmnTaskType::Script,
        .laneId = lanePay,
        .slaDurationMs = 360.0f
    });

    // Row 3 (Warehouse & Logistics): center Y = 580.0f
    const uint32_t nPickPack = d.AddNode({
        .type = guikit::BpmnNodeType::Task,
        .label = "Reserve & Pick Items",
        .documentation = "WMS Batch |||",
        .pos = {870.0f, 540.0f},
        .taskType = guikit::BpmnTaskType::Service,
        .laneId = laneWh,
        .slaDurationMs = 880.0f,
        .parallelMultiInstance = true
    });

    const uint32_t nDataStore = d.AddNode({
        .type = guikit::BpmnNodeType::DataStore,
        .label = "Inventory DB",
        .pos = {690.0f, 600.0f},
        .laneId = laneWh
    });

    const uint32_t nGwJoin = d.AddNode({
        .type = guikit::BpmnNodeType::Gateway,
        .label = "Sync Join",
        .pos = {1040.0f, 555.0f},
        .gatewayType = guikit::BpmnGatewayType::Parallel,
        .laneId = laneWh
    });

    const uint32_t nSubShip = d.AddNode({
        .type = guikit::BpmnNodeType::SubProcess,
        .label = "Carrier Dispatch & Track",
        .pos = {1140.0f, 538.0f},
        .laneId = laneWh,
        .slaDurationMs = 750.0f
    });

    const uint32_t nEndOk = d.AddNode({
        .type = guikit::BpmnNodeType::EndEvent,
        .label = "Order Fulfilled",
        .pos = {1330.0f, 562.0f},
        .eventDef = guikit::BpmnEventDefinition::None,
        .laneId = laneWh
    });

    const uint32_t nNote = d.AddNode({
        .type = guikit::BpmnNodeType::TextAnnotation,
        .label = "PCI-DSS 4.0 Tokenized\nIdempotent Retry = 3x",
        .pos = {440.0f, 412.0f},
        .laneId = lanePay
    });

    d.AddEdge(nStart, nCheckout, guikit::BpmnFlowType::SequenceFlow);
    d.AddEdge(nCheckout, nRiskRule, guikit::BpmnFlowType::SequenceFlow);
    d.AddEdge(nRiskRule, nGwRisk, guikit::BpmnFlowType::SequenceFlow);
    const uint32_t eApproved = d.AddEdge(nGwRisk, nCharge, guikit::BpmnFlowType::DefaultFlow, "Approved");
    d.AddEdge(nGwRisk, nNotifyFail, guikit::BpmnFlowType::ConditionalFlow, "High Risk", guikit::BpmnPortSide::Top, guikit::BpmnPortSide::Left);
    d.AddEdge(nBoundErr, nNotifyFail, guikit::BpmnFlowType::SequenceFlow, "Declined", guikit::BpmnPortSide::Bottom, guikit::BpmnPortSide::Bottom);
    d.AddEdge(nNotifyFail, nEndCancel, guikit::BpmnFlowType::SequenceFlow);

    d.AddEdge(nCharge, nGwFork, guikit::BpmnFlowType::SequenceFlow);
    d.AddEdge(nGwFork, nInvoice, guikit::BpmnFlowType::SequenceFlow);
    d.AddEdge(nGwFork, nPickPack, guikit::BpmnFlowType::SequenceFlow, {}, guikit::BpmnPortSide::Bottom, guikit::BpmnPortSide::Left);
    d.AddEdge(nInvoice, nGwJoin, guikit::BpmnFlowType::SequenceFlow, {}, guikit::BpmnPortSide::Right, guikit::BpmnPortSide::Top);
    d.AddEdge(nPickPack, nGwJoin, guikit::BpmnFlowType::SequenceFlow);
    d.AddEdge(nGwJoin, nSubShip, guikit::BpmnFlowType::SequenceFlow);
    d.AddEdge(nSubShip, nEndOk, guikit::BpmnFlowType::SequenceFlow);

    d.AddEdge(nDataStore, nPickPack, guikit::BpmnFlowType::Association);
    d.AddEdge(nCharge, nNote, guikit::BpmnFlowType::Association);

    if (auto* gw = d.FindNode(nGwRisk)) gw->activeOutgoingEdgeId = eApproved;
}

void BuildCiCdCanaryDiagram(guikit::BpmnDiagram& d) {
    d.Clear();
    d.name = "Cloud CI/CD Canary Deployment & Auto-Rollback";

    const uint32_t laneBuild = d.AddLane("GitOps Delivery Pool", "Build & SAST Security", guikit::Rect(60.0f, 70.0f, 1240.0f, 210.0f));
    const uint32_t laneProd  = d.AddLane("GitOps Delivery Pool", "Kubernetes Production SRE", guikit::Rect(60.0f, 280.0f, 1240.0f, 240.0f));

    // Row 1 (Build & SAST Security): center Y = 170.0f
    const uint32_t nStart = d.AddNode({
        .type = guikit::BpmnNodeType::StartEvent,
        .label = "Main Commit",
        .pos = {130.0f, 152.0f},
        .eventDef = guikit::BpmnEventDefinition::Signal,
        .laneId = laneBuild
    });
    const uint32_t nBuild = d.AddNode({
        .type = guikit::BpmnNodeType::Task,
        .label = "Compile & Containerize",
        .documentation = "Docker Buildx",
        .pos = {220.0f, 130.0f},
        .taskType = guikit::BpmnTaskType::Script,
        .laneId = laneBuild,
        .slaDurationMs = 520.0f
    });
    const uint32_t nScan = d.AddNode({
        .type = guikit::BpmnNodeType::Task,
        .label = "SBOM & CVE Policy",
        .documentation = "OPA Gatekeeper",
        .pos = {400.0f, 130.0f},
        .taskType = guikit::BpmnTaskType::BusinessRule,
        .laneId = laneBuild,
        .slaDurationMs = 380.0f
    });

    // Row 2 (Kubernetes Production SRE): center Y = 390.0f
    const uint32_t nCanary = d.AddNode({
        .type = guikit::BpmnNodeType::Task,
        .label = "Deploy Canary (10%)",
        .documentation = "Argo Rollouts",
        .pos = {560.0f, 350.0f},
        .taskType = guikit::BpmnTaskType::Service,
        .laneId = laneProd,
        .slaDurationMs = 740.0f
    });
    const uint32_t nBoundSla = d.AddNode({
        .type = guikit::BpmnNodeType::BoundaryEvent,
        .label = "p99 > 250ms",
        .eventDef = guikit::BpmnEventDefinition::Error,
        .attachedToNodeId = nCanary,
        .boundaryRatioX = 0.70f,
        .laneId = laneProd
    });
    const uint32_t nWait = d.AddNode({
        .type = guikit::BpmnNodeType::IntermediateEvent,
        .label = "Soak 15m",
        .pos = {760.0f, 372.0f},
        .eventDef = guikit::BpmnEventDefinition::Timer,
        .laneId = laneProd,
        .slaDurationMs = 450.0f
    });
    const uint32_t nPromote = d.AddNode({
        .type = guikit::BpmnNodeType::Task,
        .label = "Promote 100% Traffic",
        .documentation = "Istio VirtualService",
        .pos = {870.0f, 350.0f},
        .taskType = guikit::BpmnTaskType::Service,
        .laneId = laneProd,
        .slaDurationMs = 420.0f
    });
    const uint32_t nRollback = d.AddNode({
        .type = guikit::BpmnNodeType::Task,
        .label = "Auto-Rollback Helm",
        .documentation = "PagerDuty Incident",
        .pos = {870.0f, 130.0f},
        .taskType = guikit::BpmnTaskType::Send,
        .laneId = laneBuild,
        .slaDurationMs = 340.0f
    });
    const uint32_t nEndLive = d.AddNode({
        .type = guikit::BpmnNodeType::EndEvent,
        .label = "Release Live",
        .pos = {1060.0f, 372.0f},
        .eventDef = guikit::BpmnEventDefinition::Signal,
        .laneId = laneProd
    });
    const uint32_t nEndAbort = d.AddNode({
        .type = guikit::BpmnNodeType::EndEvent,
        .label = "Rolled Back",
        .pos = {1060.0f, 152.0f},
        .eventDef = guikit::BpmnEventDefinition::Terminate,
        .laneId = laneBuild
    });

    d.AddEdge(nStart, nBuild);
    d.AddEdge(nBuild, nScan);
    d.AddEdge(nScan, nCanary, guikit::BpmnFlowType::SequenceFlow, {}, guikit::BpmnPortSide::Bottom, guikit::BpmnPortSide::Left);
    d.AddEdge(nCanary, nWait);
    d.AddEdge(nWait, nPromote);
    d.AddEdge(nPromote, nEndLive);
    d.AddEdge(nBoundSla, nRollback, guikit::BpmnFlowType::SequenceFlow, "SLO Breach", guikit::BpmnPortSide::Bottom, guikit::BpmnPortSide::Left);
    d.AddEdge(nRollback, nEndAbort);
}

void BuildAiUnderwritingDiagram(guikit::BpmnDiagram& d) {
    d.Clear();
    d.name = "AI Credit Underwriting & Escalation Workflow";

    const uint32_t laneApi  = d.AddLane("FinTech Lending Pool", "API Gateway & ML Scoring", guikit::Rect(60.0f, 70.0f, 1220.0f, 210.0f));
    const uint32_t laneRisk = d.AddLane("FinTech Lending Pool", "Credit Committee & Legal", guikit::Rect(60.0f, 280.0f, 1220.0f, 220.0f));

    // Row 1 (API Gateway & ML Scoring): center Y = 170.0f
    const uint32_t nStart = d.AddNode({
        .type = guikit::BpmnNodeType::StartEvent,
        .label = "Loan Request",
        .pos = {130.0f, 152.0f},
        .eventDef = guikit::BpmnEventDefinition::Message,
        .laneId = laneApi
    });
    const uint32_t nMl = d.AddNode({
        .type = guikit::BpmnNodeType::Task,
        .label = "Run XGBoost Credit Model",
        .documentation = "Feature Store RPC",
        .pos = {220.0f, 130.0f},
        .taskType = guikit::BpmnTaskType::Service,
        .laneId = laneApi,
        .slaDurationMs = 360.0f
    });
    const uint32_t nGw = d.AddNode({
        .type = guikit::BpmnNodeType::Gateway,
        .label = "Score Tier?",
        .pos = {410.0f, 145.0f},
        .gatewayType = guikit::BpmnGatewayType::Exclusive,
        .laneId = laneApi
    });

    // Row 2 (Credit Committee & Legal): center Y = 380.0f
    const uint32_t nManual = d.AddNode({
        .type = guikit::BpmnNodeType::Task,
        .label = "Senior Underwriter Audit",
        .documentation = "Manual KYC Review",
        .pos = {540.0f, 340.0f},
        .taskType = guikit::BpmnTaskType::User,
        .laneId = laneRisk,
        .slaDurationMs = 920.0f
    });
    const uint32_t nBoundEsc = d.AddNode({
        .type = guikit::BpmnNodeType::BoundaryEvent,
        .label = "SLA > 4h",
        .eventDef = guikit::BpmnEventDefinition::Escalation,
        .nonInterrupting = true,
        .attachedToNodeId = nManual,
        .boundaryRatioX = 0.68f,
        .laneId = laneRisk
    });
    const uint32_t nContract = d.AddNode({
        .type = guikit::BpmnNodeType::Task,
        .label = "Disburse & Sign Contract",
        .documentation = "Call Activity",
        .pos = {820.0f, 130.0f},
        .taskType = guikit::BpmnTaskType::CallActivity,
        .laneId = laneApi,
        .slaDurationMs = 480.0f
    });
    const uint32_t nEnd = d.AddNode({
        .type = guikit::BpmnNodeType::EndEvent,
        .label = "Loan Funded",
        .pos = {1020.0f, 152.0f},
        .eventDef = guikit::BpmnEventDefinition::None,
        .laneId = laneApi
    });

    d.AddEdge(nStart, nMl);
    d.AddEdge(nMl, nGw);
    const uint32_t eAuto = d.AddEdge(nGw, nContract, guikit::BpmnFlowType::DefaultFlow, "Auto-Approve (>740)");
    d.AddEdge(nGw, nManual, guikit::BpmnFlowType::ConditionalFlow, "Manual Review", guikit::BpmnPortSide::Bottom, guikit::BpmnPortSide::Left);
    d.AddEdge(nManual, nContract, guikit::BpmnFlowType::SequenceFlow, "Cleared", guikit::BpmnPortSide::Right, guikit::BpmnPortSide::Bottom);
    d.AddEdge(nBoundEsc, nContract, guikit::BpmnFlowType::MessageFlow, "Escalated", guikit::BpmnPortSide::Bottom, guikit::BpmnPortSide::Bottom);
    d.AddEdge(nContract, nEnd);

    if (auto* gw = d.FindNode(nGw)) gw->activeOutgoingEdgeId = eAuto;
}

struct BpmnDemoState {
    bool initialized{false};
    size_t presetIdx{0};
    guikit::BpmnDiagram diagram{};

    guikit::BpmnThemePreset themePreset{guikit::BpmnThemePreset::Auto};
    guikit::BpmnRoutingMode routingMode{guikit::BpmnRoutingMode::Orthogonal};

    bool showLeftPalette{true};
    bool showTopBar{true};
    bool showContextPad{true};
    bool showMinimap{true};
    bool showZoomControls{true};
    bool showGrid{true};
    bool snapToGrid{true};
    bool smartGuides{true};
    float gridStep{20.0f};
    bool readOnly{false};

    bool tokenSimulation{false};
    bool autoSpawnTokens{true};
    float simulationSpeed{1.0f};
    bool showSlaHeatmap{false};

    uint32_t selectedNodeId{0};
    uint32_t selectedEdgeId{0};
    uint32_t selectedLaneId{0};
    guikit::BpmnEditorResponse lastResp{};
    std::string exportedXmlBuffer{};
    std::string xmlStatusText{};

    void EnsureInitialized() {
        if (!initialized) {
            BuildOrderFulfillmentDiagram(diagram);
            initialized = true;
        }
    }

    void LoadPreset(size_t idx) {
        presetIdx = idx;
        selectedNodeId = 0;
        selectedEdgeId = 0;
        selectedLaneId = 0;
        xmlStatusText.clear();
        if (idx == 0) BuildOrderFulfillmentDiagram(diagram);
        else if (idx == 1) BuildCiCdCanaryDiagram(diagram);
        else BuildAiUnderwritingDiagram(diagram);
    }
};

BpmnDemoState s_bpmn;

} // namespace

void RenderBpmnInspector(guikit::Context& ui, DemoContext&) {
    s_bpmn.EnsureInitialized();

    static constexpr std::array<std::string_view, 3> kPresetNames = {
        "1. E-Commerce Order & Payment Orchestration",
        "2. Cloud CI/CD Canary & Auto-Rollback",
        "3. AI Credit Underwriting & Escalation"
    };
    if (auto el = ui.Element({.label = "Enterprise Process Preset"})) {
        if (ui.Select(s_bpmn.presetIdx, kPresetNames).changed) {
            s_bpmn.LoadPreset(s_bpmn.presetIdx);
            ui.InvalidateAll();
        }
    }

    if (auto row = ui.Row({.gap = 6.0f})) {
        if (ui.Button({.text = "Reset Preset", .icon = guikit::Icon::Refresh(), .color = guikit::StateColor::Secondary(), .stretch = guikit::StretchMode::Fill})) {
            s_bpmn.LoadPreset(s_bpmn.presetIdx);
            ui.InvalidateAll();
        }
        if (ui.Button({.text = "Clear Canvas", .icon = guikit::Icon::Trash(), .color = guikit::StateColor::Ghost(), .stretch = guikit::StretchMode::Fill})) {
            s_bpmn.diagram.Clear();
            s_bpmn.selectedNodeId = 0;
            s_bpmn.selectedEdgeId = 0;
            s_bpmn.selectedLaneId = 0;
            ui.InvalidateAll();
        }
    }

    // --- BPMN 2.0 XML Serialization & Round-Trip Import ---
    ui.Size(guikit::UiSize::Xs).Heading({.label = "BPMN 2.0 XML SERIALIZATION", .divider = true});
    if (auto row = ui.Row({.gap = 6.0f})) {
        if (ui.Button({.text = "Export BPMN XML", .icon = guikit::Icon::Save(), .color = guikit::StateColor::Secondary(), .stretch = guikit::StretchMode::Fill})) {
            s_bpmn.exportedXmlBuffer = guikit::BpmnEditor::ExportBpmnXml(s_bpmn.diagram);
            ui.SetClipboardText(s_bpmn.exportedXmlBuffer);
            s_bpmn.xmlStatusText = std::format("Exported {} bytes of BPMN 2.0 XML (+DI) to clipboard", s_bpmn.exportedXmlBuffer.size());
            ui.InvalidateAll();
        }
        if (ui.Button({.text = "Round-Trip Import", .icon = guikit::Icon::Folder(), .color = guikit::StateColor::Primary(), .stretch = guikit::StretchMode::Fill})) {
            if (s_bpmn.exportedXmlBuffer.empty()) {
                s_bpmn.exportedXmlBuffer = guikit::BpmnEditor::ExportBpmnXml(s_bpmn.diagram);
            }
            guikit::BpmnDiagram imported{};
            if (guikit::BpmnEditor::ImportBpmnXml(s_bpmn.exportedXmlBuffer, imported)) {
                s_bpmn.diagram = std::move(imported);
                s_bpmn.xmlStatusText = std::format("Imported {} nodes, {} flows, {} lanes from BPMN 2.0 XML",
                    s_bpmn.diagram.nodes.size(), s_bpmn.diagram.edges.size(), s_bpmn.diagram.lanes.size());
            } else {
                s_bpmn.xmlStatusText = "Failed to parse BPMN 2.0 XML buffer";
            }
            ui.InvalidateAll();
        }
    }
    if (!s_bpmn.xmlStatusText.empty()) {
        ui.Text(s_bpmn.xmlStatusText, {.role = guikit::TextRole::BodySmall, .variant = guikit::UiVariant::Secondary});
    }

    // --- Token Simulation Section ---
    ui.Size(guikit::UiSize::Xs).Heading({.label = "TOKEN SIMULATION (BPMN-JS)", .divider = true});
    if (auto el = ui.Element({
        .label = "Live Token Simulation",
        .description = "Animate instances, parallel forks & XOR switching"
    })) {
        if (ui.Switch(s_bpmn.tokenSimulation).changed) {
            if (!s_bpmn.tokenSimulation) s_bpmn.diagram.ResetSimulationStats();
            ui.InvalidateAll();
        }
    }
    if (auto el = ui.Element({.label = "Auto-Spawn Process Instances"})) {
        if (ui.Switch(s_bpmn.autoSpawnTokens).changed) ui.InvalidateAll();
    }
    if (auto el = ui.Element({.label = "Simulation Speed"})) {
        if (ui.Slider(s_bpmn.simulationSpeed, 0.25f, 3.0f, {.valueFormat = "%.2fx", .step = 0.25f}).changed) ui.InvalidateAll();
    }
    if (auto el = ui.Element({.label = "SLA Duration Heatmap Overlay"})) {
        if (ui.Switch(s_bpmn.showSlaHeatmap).changed) ui.InvalidateAll();
    }

    // --- Selected Element Property Inspector ---
    if (guikit::BpmnNode* selNode = s_bpmn.diagram.FindNode(s_bpmn.selectedNodeId)) {
        ui.Size(guikit::UiSize::Xs).Heading({.label = "SELECTED NODE PROPERTIES", .divider = true});
        if (auto el = ui.Element({.label = "Node Label"})) {
            if (ui.InputField(selNode->label).changed) ui.InvalidateAll();
        }
        if (selNode->type == guikit::BpmnNodeType::Task || selNode->type == guikit::BpmnNodeType::SubProcess) {
            if (auto el = ui.Element({.label = "Documentation / Tech Stack"})) {
                if (ui.InputField(selNode->documentation).changed) ui.InvalidateAll();
            }
            if (auto el = ui.Element({.label = "Nominal SLA Duration"})) {
                if (ui.Slider(selNode->slaDurationMs, 100.0f, 1500.0f, {.valueFormat = "%.0f ms", .step = 20.0f}).changed) ui.InvalidateAll();
            }
            if (selNode->type == guikit::BpmnNodeType::SubProcess) {
                if (auto el = ui.Element({.label = "Expanded Sub-Process Container"})) {
                    if (ui.Switch(selNode->isExpanded).changed) {
                        const guikit::Vec2 oldC = selNode->Center();
                        selNode->size = selNode->isExpanded ? guikit::Vec2{320.0f, 180.0f} : guikit::BpmnDiagram::DefaultNodeSize(guikit::BpmnNodeType::SubProcess);
                        selNode->pos = {oldC.x - selNode->size.x * 0.5f, oldC.y - selNode->size.y * 0.5f};
                        s_bpmn.diagram.SyncBoundaryEvents();
                        ui.InvalidateAll();
                    }
                }
            }
            if (selNode->type == guikit::BpmnNodeType::Task) {
                static constexpr std::array<std::string_view, 9> kTaskKindNames = {
                    "None", "User Task", "Service Task", "Send Task", "Receive Task",
                    "Script Task", "Manual Task", "Business Rule (DMN)", "Call Activity"
                };
                size_t tk = static_cast<size_t>(selNode->taskType);
                if (auto el = ui.Element({.label = "Task Subtype"})) {
                    if (ui.Select(tk, kTaskKindNames).changed) {
                        selNode->taskType = static_cast<guikit::BpmnTaskType>(tk);
                        ui.InvalidateAll();
                    }
                }
                if (auto el = ui.Element({.label = "Parallel Multi-Instance (|||)"})) {
                    if (ui.Switch(selNode->parallelMultiInstance).changed) ui.InvalidateAll();
                }
                if (auto el = ui.Element({.label = "Standard Loop Marker"})) {
                    if (ui.Switch(selNode->loopMarker).changed) ui.InvalidateAll();
                }
            }
        } else if (selNode->type == guikit::BpmnNodeType::Gateway) {
            static constexpr std::array<std::string_view, 4> kGwNames = {
                "Exclusive (XOR x)", "Parallel (AND +)", "Inclusive (OR o)", "Event-Based"
            };
            size_t gk = static_cast<size_t>(selNode->gatewayType);
            if (auto el = ui.Element({.label = "Gateway Semantics"})) {
                if (ui.Select(gk, kGwNames).changed) {
                    selNode->gatewayType = static_cast<guikit::BpmnGatewayType>(gk);
                    ui.InvalidateAll();
                }
            }
        } else if (selNode->type == guikit::BpmnNodeType::StartEvent ||
                   selNode->type == guikit::BpmnNodeType::IntermediateEvent ||
                   selNode->type == guikit::BpmnNodeType::BoundaryEvent ||
                   selNode->type == guikit::BpmnNodeType::EndEvent) {
            static constexpr std::array<std::string_view, 7> kEvNames = {
                "None", "Message", "Timer", "Error", "Signal", "Escalation", "Terminate"
            };
            size_t ek = static_cast<size_t>(selNode->eventDef);
            if (auto el = ui.Element({.label = "Event Trigger / Result"})) {
                if (ui.Select(ek, kEvNames).changed) {
                    selNode->eventDef = static_cast<guikit::BpmnEventDefinition>(ek);
                    ui.InvalidateAll();
                }
            }
            if (selNode->type == guikit::BpmnNodeType::IntermediateEvent || selNode->type == guikit::BpmnNodeType::BoundaryEvent) {
                if (auto el = ui.Element({.label = "Non-Interrupting (Dashed)"})) {
                    if (ui.Switch(selNode->nonInterrupting).changed) ui.InvalidateAll();
                }
            }
        }
        if (std::abs(selNode->labelOffset.x) > 0.5f || std::abs(selNode->labelOffset.y) > 0.5f) {
            if (ui.Button({.text = "Reset External Label Position", .color = guikit::StateColor::Ghost(), .stretch = guikit::StretchMode::Fill})) {
                selNode->labelOffset = {0.0f, 0.0f};
                ui.InvalidateAll();
            }
        }
    } else if (guikit::BpmnEdge* selEdge = s_bpmn.diagram.FindEdge(s_bpmn.selectedEdgeId)) {
        ui.Size(guikit::UiSize::Xs).Heading({.label = "SELECTED FLOW PROPERTIES", .divider = true});
        if (auto el = ui.Element({.label = "Flow Condition / Label"})) {
            if (ui.InputField(selEdge->label).changed) ui.InvalidateAll();
        }
        static constexpr std::array<std::string_view, 5> kFlowNames = {
            "Sequence Flow", "Conditional Flow (Diamond)", "Default Flow (Slash \\)", "Message Flow (Dashed)", "Association (Dotted)"
        };
        size_t fk = static_cast<size_t>(selEdge->flowType);
        if (auto el = ui.Element({.label = "Flow Type"})) {
            if (ui.Select(fk, kFlowNames).changed) {
                selEdge->flowType = static_cast<guikit::BpmnFlowType>(fk);
                ui.InvalidateAll();
            }
        }
        if (!selEdge->waypoints.empty() || std::abs(selEdge->labelOffset.x) > 0.5f || std::abs(selEdge->labelOffset.y) > 0.5f) {
            if (ui.Button({.text = "Reset Custom Route & Label to Auto", .icon = guikit::Icon::Refresh(), .color = guikit::StateColor::Secondary(), .stretch = guikit::StretchMode::Fill})) {
                selEdge->waypoints.clear();
                selEdge->labelOffset = {0.0f, 0.0f};
                ui.InvalidateAll();
            }
        }
    } else if (guikit::BpmnLane* selLane = s_bpmn.diagram.FindLane(s_bpmn.selectedLaneId)) {
        ui.Size(guikit::UiSize::Xs).Heading({.label = "SELECTED SWIMLANE", .divider = true});
        if (auto el = ui.Element({.label = "Pool Participant"})) {
            if (ui.InputField(selLane->poolName).changed) ui.InvalidateAll();
        }
        if (auto el = ui.Element({.label = "Lane Role Name"})) {
            if (ui.InputField(selLane->name).changed) ui.InvalidateAll();
        }
    }

    // --- Canvas & Modeler Appearance ---
    ui.Size(guikit::UiSize::Xs).Heading({.label = "MODELER & CANVAS UX", .divider = true});
    static constexpr std::array<std::string_view, 4> kThemeNames = {"Auto", "bpmn.io", "Camunda", "Blueprint"};
    size_t thIdx = static_cast<size_t>(s_bpmn.themePreset);
    if (auto el = ui.Element({.label = "Color Theme"})) {
        if (ui.StepSlider(thIdx, kThemeNames, {.showTicks = true, .showLabels = true}).changed) {
            s_bpmn.themePreset = static_cast<guikit::BpmnThemePreset>(thIdx);
            ui.InvalidateAll();
        }
    }

    static constexpr std::array<std::string_view, 2> kRouteNames = {"Orthogonal (Manhattan)", "Cubic Bezier"};
    size_t rtIdx = static_cast<size_t>(s_bpmn.routingMode);
    if (auto el = ui.Element({.label = "Edge Routing"})) {
        if (ui.StepSlider(rtIdx, kRouteNames, {.showTicks = true, .showLabels = true}).changed) {
            s_bpmn.routingMode = static_cast<guikit::BpmnRoutingMode>(rtIdx);
            ui.InvalidateAll();
        }
    }

    if (auto el = ui.Element({.label = "Left Tool Palette (V/H/L/S/C)"})) {
        if (ui.Switch(s_bpmn.showLeftPalette).changed) ui.InvalidateAll();
    }
    if (auto el = ui.Element({.label = "Floating Context Pad & Replace Menu"})) {
        if (ui.Switch(s_bpmn.showContextPad).changed) ui.InvalidateAll();
    }
    if (auto el = ui.Element({.label = "Top Process & Simulation Bar"})) {
        if (ui.Switch(s_bpmn.showTopBar).changed) ui.InvalidateAll();
    }
    if (auto el = ui.Element({.label = "Interactive World Minimap"})) {
        if (ui.Switch(s_bpmn.showMinimap).changed) ui.InvalidateAll();
    }
    if (auto el = ui.Element({.label = "Snap to Grid"})) {
        if (ui.Switch(s_bpmn.snapToGrid).changed) ui.InvalidateAll();
    }
    if (auto el = ui.Element({.label = "Smart Laser Alignment Guides"})) {
        if (ui.Switch(s_bpmn.smartGuides).changed) ui.InvalidateAll();
    }
}

void RenderBpmnStage(guikit::Context& ui, DemoContext&) {
    s_bpmn.EnsureInitialized();

    s_bpmn.lastResp = ui.BpmnEditor(s_bpmn.diagram, {
        .routingMode = s_bpmn.routingMode,
        .themePreset = s_bpmn.themePreset,
        .showLeftPalette = s_bpmn.showLeftPalette,
        .showTopBar = s_bpmn.showTopBar,
        .showContextPad = s_bpmn.showContextPad,
        .showMinimap = s_bpmn.showMinimap,
        .showZoomControls = s_bpmn.showZoomControls,
        .showGrid = s_bpmn.showGrid,
        .snapToGrid = s_bpmn.snapToGrid,
        .smartGuides = s_bpmn.smartGuides,
        .gridStep = s_bpmn.gridStep,
        .readOnly = s_bpmn.readOnly,
        .tokenSimulation = s_bpmn.tokenSimulation,
        .autoSpawnTokens = s_bpmn.autoSpawnTokens,
        .simulationSpeed = s_bpmn.simulationSpeed,
        .showSlaHeatmap = s_bpmn.showSlaHeatmap,
        .tokenSimulationPtr = &s_bpmn.tokenSimulation,
        .routingModePtr = &s_bpmn.routingMode,
        .selectedNodeIdPtr = &s_bpmn.selectedNodeId,
        .selectedEdgeIdPtr = &s_bpmn.selectedEdgeId,
        .selectedLaneIdPtr = &s_bpmn.selectedLaneId,
        .heightMode = guikit::SizeMode::Stretch
    });
}

std::string GenerateBpmnCode(const DemoContext&) {
    static constexpr std::array<std::string_view, 4> kThemeEnum = {
        "Auto", "BpmnIoClassic", "CamundaDark", "Blueprint"
    };
    return std::format(
        "// 100% bpmn.io-Parity BPMN 2.0 Modeler, Token Simulator & XML Round-Trip\n"
        "guikit::BpmnEditorResponse resp = ui.BpmnEditor(diagram, {{\n"
        "    .routingMode = guikit::BpmnRoutingMode::{},\n"
        "    .themePreset = guikit::BpmnThemePreset::{},\n"
        "    .showLeftPalette = {},\n"
        "    .showTopBar = {},\n"
        "    .showContextPad = {},\n"
        "    .showMinimap = {},\n"
        "    .snapToGrid = {},\n"
        "    .smartGuides = {},\n"
        "    .tokenSimulation = {},\n"
        "    .autoSpawnTokens = {},\n"
        "    .simulationSpeed = {:.2f}f,\n"
        "    .showSlaHeatmap = {},\n"
        "    .heightMode = guikit::SizeMode::Stretch\n"
        "}});\n"
        "\n"
        "// Standard BPMN 2.0 XML + BPMNDI Serialization / Deserialization\n"
        "std::string xml = guikit::BpmnEditor::ExportBpmnXml(diagram);\n"
        "guikit::BpmnEditor::ImportBpmnXml(xml, diagram);",
        (s_bpmn.routingMode == guikit::BpmnRoutingMode::Orthogonal) ? "Orthogonal" : "Bezier",
        kThemeEnum[static_cast<size_t>(s_bpmn.themePreset)],
        s_bpmn.showLeftPalette ? "true" : "false",
        s_bpmn.showTopBar ? "true" : "false",
        s_bpmn.showContextPad ? "true" : "false",
        s_bpmn.showMinimap ? "true" : "false",
        s_bpmn.snapToGrid ? "true" : "false",
        s_bpmn.smartGuides ? "true" : "false",
        s_bpmn.tokenSimulation ? "true" : "false",
        s_bpmn.autoSpawnTokens ? "true" : "false",
        s_bpmn.simulationSpeed,
        s_bpmn.showSlaHeatmap ? "true" : "false"
    );
}

} // namespace guikit::demo
