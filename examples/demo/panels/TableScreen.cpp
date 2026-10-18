#include "../DemoContext.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <sstream>
#include <string>
#include <vector>

namespace guikit::demo {

namespace {

struct SignalTargetRow {
    int id{0};
    std::string callsign{};
    std::string modulation{};
    float freqMhz{0.0f};
    float bearingDeg{0.0f};
    float snrDb{0.0f};
    int status{0}; // 0=LOCKED, 1=TRACKING, 2=SCANNING, 3=ALARM
    bool tracked{true};
};

struct TableDemoState {
    size_t datasetMode{0}; // 0 = Rich Signal DataGrid (Widgets + Sort), 1 = 100,000 Rows Virtualized
    UiSize tableSize{UiSize::Md};
    bool striped{true};
    bool gridLines{true};
    bool stickyHeader{true};
    bool resizableColumns{true};
    bool showRowNumbers{true};
    bool showFooter{true};
    bool liveTelemetry{true};

    std::string filterQuery{};
    int selectedRow{0};
    int sortCol{2}; // Default sorted by Frequency
    TableSortDirection sortDir{TableSortDirection::Ascending};

    int virtualSelectedRow{0};
    int virtualSortCol{0};
    TableSortDirection virtualSortDir{TableSortDirection::Ascending};
    int pendingJumpRow{-1};
    size_t lastVisibleCount{0};

    std::vector<SignalTargetRow> rows{};
    bool initialized{false};

    void EnsureInitialized() {
        if (initialized) return;
        initialized = true;

        static constexpr std::array<const char*, 8> kCallsigns = {
            "VHF-BEACON-01", "UHF-LINK-ALPHA", "SAT-DOWNLINK-4", "RADAR-NAV-TX",
            "AIS-TRANSPONDER", "DMR-REPEATER-7", "OFDM-TELEMETRY", "SONAR-BUOY-12"
        };
        static constexpr std::array<const char*, 6> kMods = {
            "QPSK", "OFDM-64", "FM-Narrow", "GMSK", "BPSK-CW", "16-QAM"
        };

        rows.clear();
        rows.reserve(48);
        for (int i = 0; i < 48; ++i) {
            SignalTargetRow r;
            r.id = 1001 + i;
            r.callsign = std::string(kCallsigns[static_cast<size_t>(i) % kCallsigns.size()]) +
                         "-" + std::to_string((i / 8) + 1);
            r.modulation = kMods[static_cast<size_t>(i * 3) % kMods.size()];
            r.freqMhz = 118.500f + static_cast<float>(i) * 14.375f;
            r.bearingDeg = -165.0f + std::fmod(static_cast<float>(i) * 29.5f, 330.0f);
            r.snrDb = 8.0f + std::fmod(static_cast<float>(i) * 7.3f, 31.0f);
            r.status = (r.snrDb > 33.0f) ? 3 : ((r.snrDb > 22.0f) ? 0 : ((r.snrDb > 14.0f) ? 1 : 2));
            r.tracked = (i % 3 != 2);
            rows.push_back(std::move(r));
        }
        SortRows();
    }

    void SortRows() {
        if (sortCol < 0 || sortDir == TableSortDirection::None) return;
        const bool asc = (sortDir == TableSortDirection::Ascending);
        std::stable_sort(rows.begin(), rows.end(), [&](const SignalTargetRow& a, const SignalTargetRow& b) {
            switch (sortCol) {
                case 0: return asc ? (a.tracked > b.tracked) : (a.tracked < b.tracked);
                case 1: return asc ? (a.id < b.id) : (a.id > b.id);
                case 2: return asc ? (a.callsign < b.callsign) : (a.callsign > b.callsign);
                case 3: return asc ? (a.modulation < b.modulation) : (a.modulation > b.modulation);
                case 4: return asc ? (a.freqMhz < b.freqMhz) : (a.freqMhz > b.freqMhz);
                case 5: return asc ? (a.bearingDeg < b.bearingDeg) : (a.bearingDeg > b.bearingDeg);
                case 6: return asc ? (a.snrDb < b.snrDb) : (a.snrDb > b.snrDb);
                case 7: return asc ? (a.status < b.status) : (a.status > b.status);
                default: return false;
            }
        });
    }
};

TableDemoState g_tableDemo;

} // namespace

void RenderTableInspector(guikit::Context& ui, DemoContext& /*ctx*/) {
    g_tableDemo.EnsureInitialized();

    static constexpr std::array<std::string_view, 2> kModeLabels = {"Rich DataGrid", "100k Virtual"};
    if (auto el = ui.Element({
        .label = "Table Mode",
        .description = "Switch between Rich Widget DataGrid (48 targets) and 100,000-row O(1) virtualized log"
    })) {
        if (ui.StepSlider(g_tableDemo.datasetMode, kModeLabels, {
            .showTicks = true,
            .showLabels = true
        }).changed) {
            ui.InvalidateAll();
        }
    }

    if (auto el = ui.Element({.label = "Row & Header Size"})) {
        if (ui.StepSlider(g_tableDemo.tableSize, {
            .showTicks = true,
            .showLabels = true
        }).changed) {
            ui.InvalidateAll();
        }
    }

    if (auto el = ui.Element({.label = "Zebra Striped Rows"})) {
        if (ui.Switch(g_tableDemo.striped).changed) ui.InvalidateAll();
    }
    if (auto el = ui.Element({.label = "Cell Grid Lines"})) {
        if (ui.Switch(g_tableDemo.gridLines).changed) ui.InvalidateAll();
    }
    if (auto el = ui.Element({.label = "Sticky Header"})) {
        if (ui.Switch(g_tableDemo.stickyHeader).changed) ui.InvalidateAll();
    }
    if (auto el = ui.Element({.label = "Resizable Columns"})) {
        if (ui.Switch(g_tableDemo.resizableColumns).changed) ui.InvalidateAll();
    }
    if (auto el = ui.Element({.label = "Show Row Numbers (#)"})) {
        if (ui.Switch(g_tableDemo.showRowNumbers).changed) ui.InvalidateAll();
    }
    if (auto el = ui.Element({.label = "Show Summary Footer"})) {
        if (ui.Switch(g_tableDemo.showFooter).changed) ui.InvalidateAll();
    }

    if (g_tableDemo.datasetMode == 0) {
        if (auto el = ui.Element({
            .label = "Live Signal Telemetry",
            .description = "Animate SNR level meters and bearings in real time"
        })) {
            if (ui.Switch(g_tableDemo.liveTelemetry).changed) {
                ui.InvalidateAll();
            }
        }
    } else {
        ui.Heading({.label = "VIRTUAL JUMP (100,000 ROWS)", .size = UiSize::Xs, .divider = true});
        if (auto row = ui.Row({.gap = 6.0f})) {
            if (ui.Button({.text = "Top (#1)", .color = StateColor::Secondary(), .stretch = StretchMode::Fill, .size = UiSize::Sm})) {
                g_tableDemo.pendingJumpRow = 0;
            }
            if (ui.Button({.text = "Mid (#50k)", .color = StateColor::Secondary(), .stretch = StretchMode::Fill, .size = UiSize::Sm})) {
                g_tableDemo.pendingJumpRow = 49999;
            }
            if (ui.Button({.text = "End (#100k)", .color = StateColor::Primary(), .stretch = StretchMode::Fill, .size = UiSize::Sm})) {
                g_tableDemo.pendingJumpRow = 99999;
            }
        }
    }
}

void RenderTableStage(guikit::Context& ui, DemoContext& /*ctx*/) {
    g_tableDemo.EnsureInitialized();

    if (auto col = ui.Column({
        .gap = 10.0f,
        .padding = Padding::All(8.0f),
        .widthMode = SizeMode::Stretch,
        .heightMode = SizeMode::Stretch
    })) {
        if (g_tableDemo.datasetMode == 0) {
            // Animate live telemetry if enabled
            if (g_tableDemo.liveTelemetry) {
                const float t = static_cast<float>((ui.GetCurrentTime)());
                for (size_t i = 0; i < g_tableDemo.rows.size(); ++i) {
                    auto& r = g_tableDemo.rows[i];
                    if (!r.tracked) continue;
                    const float wave = std::sin(t * 2.2f + static_cast<float>(i) * 0.7f);
                    r.snrDb = std::clamp(20.0f + wave * 14.0f + static_cast<float>(i % 5) * 1.5f, 3.0f, 39.5f);
                    r.bearingDeg += std::cos(t * 1.1f + static_cast<float>(i)) * 0.08f;
                    r.status = (r.snrDb > 34.0f) ? 3 : ((r.snrDb > 22.0f) ? 0 : ((r.snrDb > 12.0f) ? 1 : 2));
                }
                ui.RequestRepaint(2);
            }

            // Top Filter & Action Bar
            if (auto topBar = ui.Row({.gap = 10.0f, .align = Alignment::Center})) {
                if (ui.InputField(g_tableDemo.filterQuery, {
                    .placeholder = "Filter by callsign or modulation (e.g. OFDM, SAT, QPSK)...",
                    .prefix = Icon::Search(),
                    .size = UiSize::Sm,
                    .stretch = StretchMode::Fill,
                    .clearable = true
                }).changed) {
                    g_tableDemo.selectedRow = 0;
                }

                if (ui.Button({
                    .text = "Sort by SNR",
                    .icon = Icon::Play(),
                    .color = StateColor::Secondary(),
                    .stretch = StretchMode::Compact,
                    .size = UiSize::Sm
                })) {
                    g_tableDemo.sortCol = 6;
                    g_tableDemo.sortDir = TableSortDirection::Descending;
                    g_tableDemo.SortRows();
                }
            }

            // Build filtered indices
            std::vector<size_t> filteredIndices;
            filteredIndices.reserve(g_tableDemo.rows.size());
            for (size_t i = 0; i < g_tableDemo.rows.size(); ++i) {
                if (g_tableDemo.filterQuery.empty()) {
                    filteredIndices.push_back(i);
                } else {
                    const auto& r = g_tableDemo.rows[i];
                    if (r.callsign.find(g_tableDemo.filterQuery) != std::string::npos ||
                        r.modulation.find(g_tableDemo.filterQuery) != std::string::npos) {
                        filteredIndices.push_back(i);
                    }
                }
            }

            static const std::array<TableColumn, 8> kSignalCols = {{
                {.title = "Track",      .width = 62.0f,  .minWidth = 52.0f, .align = Align::Center, .sortable = true},
                {.title = "Target ID",  .width = 86.0f,  .minWidth = 68.0f, .align = Align::Start,  .sortable = true},
                {.title = "Callsign",   .flex = 1.5f,    .minWidth = 130.0f,.align = Align::Start,  .sortable = true},
                {.title = "Modulation", .width = 102.0f, .minWidth = 80.0f, .align = Align::Center, .sortable = true},
                {.title = "Frequency",  .width = 116.0f, .minWidth = 90.0f, .align = Align::End,    .sortable = true},
                {.title = "Azimuth",    .width = 96.0f,  .minWidth = 80.0f, .align = Align::End,    .sortable = true},
                {.title = "SNR Meter",  .flex = 1.4f,    .minWidth = 150.0f,.align = Align::Start,  .sortable = true},
                {.title = "State",      .width = 104.0f, .minWidth = 84.0f, .align = Align::Center, .sortable = true}
            }};

            char footerBuf[128];
            std::snprintf(
                footerBuf, sizeof(footerBuf),
                "Selected: %s | Double-click column divider to reset width",
                (g_tableDemo.selectedRow >= 0 && static_cast<size_t>(g_tableDemo.selectedRow) < filteredIndices.size())
                    ? g_tableDemo.rows[filteredIndices[static_cast<size_t>(g_tableDemo.selectedRow)]].callsign.c_str()
                    : "None"
            );

            if (auto table = ui.Table(kSignalCols, {
                .label = "RF & Acoustic Target Acquisition Table",
                .description = "Click column headers to sort, drag header dividers to resize, use Up/Down/PgUp/PgDn keys",
                .icon = Icon::Search(),
                .badge = {"LIVE DSP", UiVariant::Success},
                .totalRows = filteredIndices.size(),
                .selectedRow = &g_tableDemo.selectedRow,
                .sortColumn = &g_tableDemo.sortCol,
                .sortDirection = &g_tableDemo.sortDir,
                .stickyHeader = g_tableDemo.stickyHeader,
                .striped = g_tableDemo.striped,
                .gridLines = g_tableDemo.gridLines,
                .resizableColumns = g_tableDemo.resizableColumns,
                .showRowNumbers = g_tableDemo.showRowNumbers,
                .showFooter = g_tableDemo.showFooter,
                .footerText = footerBuf,
                .size = g_tableDemo.tableSize
            })) {
                if (table.SortChanged()) {
                    g_tableDemo.SortRows();
                }

                for (size_t visRow = table.FirstVisibleRow(); visRow < table.EndVisibleRow(); ++visRow) {
                    if (visRow >= filteredIndices.size()) break;
                    SignalTargetRow& item = g_tableDemo.rows[filteredIndices[visRow]];

                    TableRowOptions rowOpts{};
                    if (item.status == 3) {
                        rowOpts.tint = UiVariant::Danger;
                    }

                    if (auto row = table.Row(visRow, rowOpts)) {
                        // Col 0: Embedded Checkbox widget
                        if (auto cell = table.Cell({.align = Align::Center})) {
                            ui.Checkbox(item.tracked, {.color = StateColor::Primary(), .size = UiSize::Sm});
                        }

                        // Col 1: Target ID
                        table.CellText(ui.Format("T-{:04d}", item.id), {.variant = UiVariant::Secondary});

                        // Col 2: Callsign with leading icon
                        table.CellText(item.callsign, {.icon = Icon::Play()});

                        // Col 3: Modulation badge
                        table.CellBadge(item.modulation, UiVariant::Info);

                        // Col 4: Frequency
                        table.CellText(ui.Format("{:.3f} MHz", item.freqMhz));

                        // Col 5: Azimuth bearing
                        table.CellText(
                            ui.Format("{:+05.1f} deg", item.bearingDeg),
                            {.variant = (std::abs(item.bearingDeg) < 25.0f) ? UiVariant::Success : UiVariant::Default}
                        );

                        // Col 6: Embedded Segmented VU ProgressBar widget
                        if (auto cell = table.Cell()) {
                            ui.ProgressBar(item.snrDb, 0.0f, 40.0f, {
                                .valueFormat = "%.1f dB",
                                .segments = 12,
                                .size = UiSize::Sm,
                                .multiZoneColor = true
                            });
                        }

                        // Col 7: Status pill badge
                        static constexpr std::array<const char*, 4> kStatusLabels = {"LOCKED", "TRACKING", "SCANNING", "OVERLOAD"};
                        static constexpr std::array<UiVariant, 4> kStatusVars = {
                            UiVariant::Success, UiVariant::Primary, UiVariant::Warning, UiVariant::Danger
                        };
                        const size_t stIdx = static_cast<size_t>(std::clamp(item.status, 0, 3));
                        table.CellBadge(kStatusLabels[stIdx], kStatusVars[stIdx]);
                    }
                }
            }
        } else {
            // Mode 1: 100,000 Rows Virtualized Stress-Test
            constexpr size_t kTotalVirtualRows = 100000;

            static const std::array<TableColumn, 6> kVirtualCols = {{
                {.title = "Packet ID",   .width = 110.0f, .minWidth = 80.0f,  .align = Align::Start,  .sortable = true},
                {.title = "Timestamp",   .width = 125.0f, .minWidth = 95.0f,  .align = Align::Start,  .sortable = true},
                {.title = "Source Node", .flex = 1.3f,    .minWidth = 120.0f, .align = Align::Start,  .sortable = true},
                {.title = "Channel Freq",.width = 125.0f, .minWidth = 95.0f,  .align = Align::End,    .sortable = true},
                {.title = "Payload RSSI",.flex = 1.2f,    .minWidth = 140.0f, .align = Align::Start,  .sortable = true},
                {.title = "Integrity",   .width = 105.0f, .minWidth = 85.0f,  .align = Align::Center, .sortable = true}
            }};

            char ftrBuf[128];
            std::snprintf(
                ftrBuf, sizeof(ftrBuf),
                "Virtualized O(1) Clipper: Materialized %zu rows in DOM out of 100,000",
                g_tableDemo.lastVisibleCount
            );

            if (auto table = ui.Table(kVirtualCols, {
                .label = "100,000-Row Virtualized Telemetry Stream",
                .description = "Only ~20 visible rows are evaluated and rasterized per frame (O(1) memory & CPU cost)",
                .icon = Icon::Copy(),
                .badge = {"100,000 ROWS", UiVariant::Primary},
                .totalRows = kTotalVirtualRows,
                .selectedRow = &g_tableDemo.virtualSelectedRow,
                .sortColumn = &g_tableDemo.virtualSortCol,
                .sortDirection = &g_tableDemo.virtualSortDir,
                .stickyHeader = g_tableDemo.stickyHeader,
                .striped = g_tableDemo.striped,
                .gridLines = g_tableDemo.gridLines,
                .resizableColumns = g_tableDemo.resizableColumns,
                .showRowNumbers = g_tableDemo.showRowNumbers,
                .showFooter = g_tableDemo.showFooter,
                .footerText = ftrBuf,
                .size = g_tableDemo.tableSize
            })) {
                if (g_tableDemo.pendingJumpRow >= 0) {
                    g_tableDemo.virtualSelectedRow = g_tableDemo.pendingJumpRow;
                    table.ScrollToRow(static_cast<size_t>(g_tableDemo.pendingJumpRow));
                    g_tableDemo.pendingJumpRow = -1;
                }

                const size_t firstRow = table.FirstVisibleRow();
                const size_t endRow = table.EndVisibleRow();
                g_tableDemo.lastVisibleCount = (endRow >= firstRow) ? (endRow - firstRow) : 0;

                const bool desc = (g_tableDemo.virtualSortDir == TableSortDirection::Descending);

                for (size_t r = firstRow; r < endRow; ++r) {
                    const size_t logicalIdx = desc ? (kTotalVirtualRows - 1 - r) : r;
                    if (auto row = table.Row(r)) {
                        table.CellText(ui.Format("PKT-{:06}", logicalIdx + 1), {.variant = UiVariant::Secondary});

                        const uint32_t mins = static_cast<uint32_t>((logicalIdx / 600) % 60);
                        const uint32_t secs = static_cast<uint32_t>((logicalIdx / 10) % 60);
                        const uint32_t tenths = static_cast<uint32_t>(logicalIdx % 10);
                        table.CellText(ui.Format("12:{:02}:{:02}.{}00", mins, secs, tenths));

                        static constexpr std::array<const char*, 6> kNodes = {
                            "ARRAY-NORTH-A1", "ARRAY-EAST-B4", "HYDROPHONE-C2",
                            "SDR-RECEIVER-09", "BEAMFORMER-DSP", "SATCOM-GATEWAY"
                        };
                        table.CellText(kNodes[logicalIdx % kNodes.size()]);

                        const float freq = 430.000f + static_cast<float>(logicalIdx % 250) * 0.025f;
                        table.CellText(ui.Format("{:.3f} MHz", freq));

                        const float rssi = -95.0f + static_cast<float>((logicalIdx * 17) % 65);
                        if (auto cell = table.Cell()) {
                            ui.ProgressBar(rssi, -100.0f, -30.0f, {
                                .valueFormat = "%.0f dBm",
                                .segments = 10,
                                .size = UiSize::Xs,
                                .multiZoneColor = true
                            });
                        }

                        const bool crcOk = (logicalIdx % 19 != 0);
                        table.CellBadge(crcOk ? "CRC OK" : "RETRANSMIT", crcOk ? UiVariant::Success : UiVariant::Warning);
                    }
                }
            }
        }
    }
}

std::string GenerateTableCode(const DemoContext& /*ctx*/) {
    std::ostringstream out;
    out << "static const std::array<guikit::TableColumn, 5> cols = {{\n"
        << "    {.title = \"ID\",        .width = 80.0f,  .sortable = true},\n"
        << "    {.title = \"Callsign\",  .flex = 1.5f,    .sortable = true},\n"
        << "    {.title = \"Frequency\", .width = 120.0f, .align = guikit::Align::End, .sortable = true},\n"
        << "    {.title = \"SNR Level\", .flex = 1.4f,    .sortable = true},\n"
        << "    {.title = \"Status\",    .width = 100.0f, .align = guikit::Align::Center, .sortable = true}\n"
        << "}};\n\n"
        << "if (auto table = ui.Table(cols, {\n"
        << "    .label = \"RF & Acoustic Target Table\",\n"
        << "    .totalRows = rows.size(),\n"
        << "    .selectedRow = &selectedRow,\n"
        << "    .sortColumn = &sortCol,\n"
        << "    .sortDirection = &sortDir,\n"
        << "    .stickyHeader = " << (g_tableDemo.stickyHeader ? "true" : "false") << ",\n"
        << "    .striped = " << (g_tableDemo.striped ? "true" : "false") << ",\n"
        << "    .gridLines = " << (g_tableDemo.gridLines ? "true" : "false") << ",\n"
        << "    .resizableColumns = " << (g_tableDemo.resizableColumns ? "true" : "false") << ",\n"
        << "    .showRowNumbers = " << (g_tableDemo.showRowNumbers ? "true" : "false") << ",\n"
        << "    .showFooter = " << (g_tableDemo.showFooter ? "true" : "false") << ",\n"
        << "    .size = guikit::UiSize::" << SizeName(g_tableDemo.tableSize) << "\n"
        << "})) {\n"
        << "    for (size_t r = table.FirstVisibleRow(); r < table.EndVisibleRow(); ++r) {\n"
        << "        if (auto row = table.Row(r)) {\n"
        << "            table.CellText(ui.Format(\"T-{:04d}\", rows[r].id));\n"
        << "            table.CellText(rows[r].callsign);\n"
        << "            table.CellText(ui.Format(\"{:.3f} MHz\", rows[r].freqMhz));\n"
        << "            if (auto cell = table.Cell()) {\n"
        << "                ui.ProgressBar(rows[r].snrDb, 0.0f, 40.0f, {.segments = 12, .multiZoneColor = true});\n"
        << "            }\n"
        << "            table.CellBadge(\"LOCKED\", guikit::UiVariant::Success);\n"
        << "        }\n"
        << "    }\n"
        << "}";
    return out.str();
}

} // namespace guikit::demo
