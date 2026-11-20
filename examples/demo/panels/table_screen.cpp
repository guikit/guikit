#include "../demo_context.hpp"
#include "keel/math.h"

namespace guikit::demo {

namespace {

struct SignalTargetRow {
    keel::i32 id{0};
    keel::FixedString<64> callsign{};
    keel::FixedString<32> modulation{};
    float freqMhz{0.0f};
    float bearingDeg{0.0f};
    float snrDb{0.0f};
    keel::i32 status{0}; // 0=LOCKED, 1=TRACKING, 2=SCANNING, 3=ALARM
    bool tracked{true};
};

struct TableDemoState {
    keel::usize datasetMode{0}; // 0 = Rich Signal DataGrid (Widgets + Sort), 1 = 100,000 Rows Virtualized
    UiSize tableSize{UiSize::Md};
    bool striped{true};
    bool gridLines{true};
    bool stickyHeader{true};
    bool resizableColumns{true};
    bool showRowNumbers{true};
    bool showFooter{true};
    bool liveTelemetry{true};

    keel::SmallString<128> filterQuery{};
    keel::i32 selectedRow{0};
    keel::i32 sortCol{2}; // Default sorted by Frequency
    TableSortDirection sortDir{TableSortDirection::Ascending};

    keel::i32 virtualSelectedRow{0};
    keel::i32 virtualSortCol{0};
    TableSortDirection virtualSortDir{TableSortDirection::Ascending};
    keel::i32 pendingJumpRow{-1};
    keel::usize lastVisibleCount{0};

    keel::Array<SignalTargetRow> rows{};
    bool initialized{false};

    void EnsureInitialized() {
        if (initialized) return;
        initialized = true;

        static constexpr keel::StaticArray<const char*, 8> kCallsigns = {
            "VHF-BEACON-01", "UHF-LINK-ALPHA", "SAT-DOWNLINK-4", "RADAR-NAV-TX",
            "AIS-TRANSPONDER", "DMR-REPEATER-7", "OFDM-TELEMETRY", "SONAR-BUOY-12"
        };
        static constexpr keel::StaticArray<const char*, 6> kMods = {
            "QPSK", "OFDM-64", "FM-Narrow", "GMSK", "BPSK-CW", "16-QAM"
        };

        rows.clear();
        rows.reserve(48);
        for (keel::i32 i = 0; i < 48; ++i) {
            SignalTargetRow r;
            r.id = 1001 + i;
            r.callsign = keel::format_fixed<64>(
                "{}-{}",
                kCallsigns[static_cast<keel::usize>(i) % kCallsigns.size()],
                (i / 8) + 1
            );
            r.modulation = kMods[static_cast<keel::usize>(i * 3) % kMods.size()];
            r.freqMhz = 118.500f + static_cast<float>(i) * 14.375f;
            r.bearingDeg = -165.0f + keel::fmod(static_cast<float>(i) * 29.5f, 330.0f);
            r.snrDb = 8.0f + keel::fmod(static_cast<float>(i) * 7.3f, 31.0f);
            r.status = (r.snrDb > 33.0f) ? 3 : ((r.snrDb > 22.0f) ? 0 : ((r.snrDb > 14.0f) ? 1 : 2));
            r.tracked = (i % 3 != 2);
            rows.push_back(r);
        }
        SortRows();
    }

    void SortRows() {
        if (sortCol < 0 || sortDir == TableSortDirection::None) return;
        const bool asc = (sortDir == TableSortDirection::Ascending);
        keel::sort(rows, [&](const SignalTargetRow& a, const SignalTargetRow& b) {
            switch (sortCol) {
                case 0: return asc ? (a.tracked > b.tracked) : (a.tracked < b.tracked);
                case 1: return asc ? (a.id < b.id) : (a.id > b.id);
                case 2: return asc ? (a.callsign.as_ref() < b.callsign.as_ref()) : (a.callsign.as_ref() > b.callsign.as_ref());
                case 3: return asc ? (a.modulation.as_ref() < b.modulation.as_ref()) : (a.modulation.as_ref() > b.modulation.as_ref());
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

    static constexpr keel::StaticArray<keel::StringRef, 2> kModeLabels = {"Rich DataGrid", "100k Virtual"};
    if (auto el = ui.Element({
        .label = "Table Mode",
        .description = "Switch between Rich Widget DataGrid (48 targets) and 100,000-row O(1) virtualized log"
    })) {
        if (ui.Segmented(g_tableDemo.datasetMode, kModeLabels).changed) {
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
            .label = "Filter Rows",
            .layout = ElementLayout::Top
        })) {
            if (ui.InputField(g_tableDemo.filterQuery, {
                .placeholder = "Callsign or modulation...",
                .prefix = Icon::Search(),
                .clearable = true,
                .size = UiSize::Sm
            }).changed) {
                g_tableDemo.selectedRow = 0;
            }
        }
        if (auto el = ui.Element({
            .label = "Live Signal Telemetry",
            .description = "Animate SNR level meters and bearings in real time"
        })) {
            if (ui.Switch(g_tableDemo.liveTelemetry).changed) {
                ui.InvalidateAll();
            }
        }
    } else {
        ui.Heading({.label = "VIRTUAL JUMP (100,000 ROWS)", .level = HeadingLevel::H6, .separator = HeadingSeparator::Bottom});
        if (auto row = ui.Row({.gap = 6.0f})) {
            if (ui.Button({.text = "Top (#1)", .size = UiSize::Sm, .width = SizeMode::Stretch, .color = StateColor::Secondary()})) {
                g_tableDemo.pendingJumpRow = 0;
            }
            if (ui.Button({.text = "Mid (#50k)", .size = UiSize::Sm, .width = SizeMode::Stretch, .color = StateColor::Secondary()})) {
                g_tableDemo.pendingJumpRow = 49999;
            }
            if (ui.Button({.text = "End (#100k)", .size = UiSize::Sm, .width = SizeMode::Stretch, .color = StateColor::Primary()})) {
                g_tableDemo.pendingJumpRow = 99999;
            }
        }
    }
}

void RenderTableStage(guikit::Context& ui, DemoContext& /*ctx*/) {
    g_tableDemo.EnsureInitialized();

    if (g_tableDemo.datasetMode == 0) {
        // Animate live telemetry if enabled
        if (g_tableDemo.liveTelemetry) {
            const float t = static_cast<float>((ui.GetCurrentTime)());
            for (keel::usize i = 0; i < g_tableDemo.rows.size(); ++i) {
                auto& r = g_tableDemo.rows[i];
                if (!r.tracked) continue;
                const float wave = keel::sin(t * 2.2f + static_cast<float>(i) * 0.7f);
                r.snrDb = keel::clamp(20.0f + wave * 14.0f + static_cast<float>(i % 5) * 1.5f, 3.0f, 39.5f);
                r.bearingDeg += keel::cos(t * 1.1f + static_cast<float>(i)) * 0.08f;
                r.status = (r.snrDb > 34.0f) ? 3 : ((r.snrDb > 22.0f) ? 0 : ((r.snrDb > 12.0f) ? 1 : 2));
            }
            ui.RequestRepaint(2);
        }

        // Build filtered indices
        keel::SmallArray<keel::usize, 64> filteredIndices;
        const keel::StringRef query = g_tableDemo.filterQuery.as_ref();
        for (keel::usize i = 0; i < g_tableDemo.rows.size(); ++i) {
            if (query.empty()) {
                filteredIndices.push_back(i);
            } else {
                const auto& r = g_tableDemo.rows[i];
                if (r.callsign.as_ref().contains(query) || r.modulation.as_ref().contains(query)) {
                    filteredIndices.push_back(i);
                }
            }
        }

        static const keel::StaticArray<TableColumn, 8> kSignalCols = {{
            {.title = "Track",      .width = 62.0f,  .minWidth = 52.0f, .align = Align::Center, .sortable = true},
            {.title = "Target ID",  .width = 86.0f,  .minWidth = 68.0f, .align = Align::Start,  .sortable = true},
            {.title = "Callsign",   .flex = 1.5f,    .minWidth = 130.0f,.align = Align::Start,  .sortable = true},
            {.title = "Modulation", .width = 102.0f, .minWidth = 80.0f, .align = Align::Center, .sortable = true},
            {.title = "Frequency",  .width = 116.0f, .minWidth = 90.0f, .align = Align::End,    .sortable = true},
            {.title = "Azimuth",    .width = 96.0f,  .minWidth = 80.0f, .align = Align::End,    .sortable = true},
            {.title = "SNR Meter",  .flex = 1.4f,    .minWidth = 150.0f,.align = Align::Start,  .sortable = true},
            {.title = "State",      .width = 104.0f, .minWidth = 84.0f, .align = Align::Center, .sortable = true}
        }};

        const keel::StringRef selCallsign =
            (g_tableDemo.selectedRow >= 0 && static_cast<keel::usize>(g_tableDemo.selectedRow) < filteredIndices.size())
                ? g_tableDemo.rows[filteredIndices[static_cast<keel::usize>(g_tableDemo.selectedRow)]].callsign.as_ref()
                : keel::StringRef("None");

        char footerBuf[128];
        keel::format_buf(
            footerBuf, sizeof(footerBuf),
            "Selected: {} | Double-click column divider to reset width",
            selCallsign
        );

        if (auto table = ui.Table(kSignalCols, {
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

            for (keel::usize visRow = table.FirstVisibleRow(); visRow < table.EndVisibleRow(); ++visRow) {
                if (visRow >= filteredIndices.size()) break;
                SignalTargetRow& item = g_tableDemo.rows[filteredIndices[visRow]];

                TableRowOptions rowOpts{};
                if (item.status == 3) {
                    rowOpts.tint = UiVariant::Danger;
                }

                if (auto row = table.Row(visRow, rowOpts)) {
                    // Col 0: Embedded Checkbox widget
                    ui.Checkbox(item.tracked, {.size = UiSize::Sm, .color = StateColor::Primary()});

                    // Col 1: Target ID
                    ui.Text(ui.Format("T-{:04d}", item.id), {.variant = UiVariant::Secondary});

                    // Col 2: Callsign with leading icon
                    ui.Element({.label = item.callsign.as_ref(), .icon = Icon::Play(), .width = SizeMode::Fit});

                    // Col 3: Modulation badge
                    ui.Element({.badge = {item.modulation.as_ref(), UiVariant::Info}, .width = SizeMode::Fit});

                    // Col 4: Frequency
                    ui.Text(ui.Format("{:.3f} MHz", item.freqMhz));

                    // Col 5: Azimuth bearing
                    ui.Text(
                        ui.Format("{:+05.1f} deg", item.bearingDeg),
                        {.variant = (keel::abs(item.bearingDeg) < 25.0f) ? UiVariant::Success : UiVariant::Default}
                    );

                    // Col 6: Embedded Segmented VU ProgressBar widget
                    ui.ProgressBar(item.snrDb, 0.0f, 40.0f, {
                        .valueFormat = "%.1f dB",
                        .segments = 12,
                        .multiZoneColor = true,
                        .size = UiSize::Sm
                    });

                    // Col 7: Status pill badge
                    static constexpr keel::StaticArray<const char*, 4> kStatusLabels = {"LOCKED", "TRACKING", "SCANNING", "OVERLOAD"};
                    static constexpr keel::StaticArray<UiVariant, 4> kStatusVars = {
                        UiVariant::Success, UiVariant::Primary, UiVariant::Warning, UiVariant::Danger
                    };
                    const keel::usize stIdx = static_cast<keel::usize>(keel::clamp(item.status, 0, 3));
                    ui.Element({.badge = {kStatusLabels[stIdx], kStatusVars[stIdx]}, .width = SizeMode::Fit});
                }
            }
        }
    } else {
        // Mode 1: 100,000 Rows Virtualized Stress-Test
        constexpr keel::usize kTotalVirtualRows = 100000;

        static const keel::StaticArray<TableColumn, 6> kVirtualCols = {{
            {.title = "Packet ID",   .width = 110.0f, .minWidth = 80.0f,  .align = Align::Start,  .sortable = true},
            {.title = "Timestamp",   .width = 125.0f, .minWidth = 95.0f,  .align = Align::Start,  .sortable = true},
            {.title = "Source Node", .flex = 1.3f,    .minWidth = 120.0f, .align = Align::Start,  .sortable = true},
            {.title = "Channel Freq",.width = 125.0f, .minWidth = 95.0f,  .align = Align::End,    .sortable = true},
            {.title = "Payload RSSI",.flex = 1.2f,    .minWidth = 140.0f, .align = Align::Start,  .sortable = true},
            {.title = "Integrity",   .width = 105.0f, .minWidth = 85.0f,  .align = Align::Center, .sortable = true}
        }};

        char ftrBuf[128];
        keel::format_buf(
            ftrBuf, sizeof(ftrBuf),
            "Virtualized O(1) Clipper: Materialized {} rows in DOM out of 100,000",
            g_tableDemo.lastVisibleCount
        );

        if (auto table = ui.Table(kVirtualCols, {
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
                table.ScrollToRow(static_cast<keel::usize>(g_tableDemo.pendingJumpRow));
                g_tableDemo.pendingJumpRow = -1;
            }

            const keel::usize firstRow = table.FirstVisibleRow();
            const keel::usize endRow = table.EndVisibleRow();
            g_tableDemo.lastVisibleCount = (endRow >= firstRow) ? (endRow - firstRow) : 0;

            const bool desc = (g_tableDemo.virtualSortDir == TableSortDirection::Descending);

            for (keel::usize r = firstRow; r < endRow; ++r) {
                const keel::usize logicalIdx = desc ? (kTotalVirtualRows - 1 - r) : r;
                if (auto row = table.Row(r)) {
                    ui.Text(ui.Format("PKT-{:06}", logicalIdx + 1), {.variant = UiVariant::Secondary});

                    const keel::u32 mins = static_cast<keel::u32>((logicalIdx / 600) % 60);
                    const keel::u32 secs = static_cast<keel::u32>((logicalIdx / 10) % 60);
                    const keel::u32 tenths = static_cast<keel::u32>(logicalIdx % 10);
                    ui.Text(ui.Format("12:{:02}:{:02}.{}00", mins, secs, tenths));

                    static constexpr keel::StaticArray<const char*, 6> kNodes = {
                        "ARRAY-NORTH-A1", "ARRAY-EAST-B4", "HYDROPHONE-C2",
                        "SDR-RECEIVER-09", "BEAMFORMER-DSP", "SATCOM-GATEWAY"
                    };
                    ui.Text(kNodes[logicalIdx % kNodes.size()]);

                    const float freq = 430.000f + static_cast<float>(logicalIdx % 250) * 0.025f;
                    ui.Text(ui.Format("{:.3f} MHz", freq));

                    const float rssi = -95.0f + static_cast<float>((logicalIdx * 17) % 65);
                    ui.ProgressBar(rssi, -100.0f, -30.0f, {
                        .valueFormat = "%.0f dBm",
                        .segments = 10,
                        .multiZoneColor = true,
                        .size = UiSize::Xs
                    });

                    const bool crcOk = (logicalIdx % 19 != 0);
                    ui.Element({
                        .badge = {crcOk ? "CRC OK" : "RETRANSMIT", crcOk ? UiVariant::Success : UiVariant::Warning},
                        .width = SizeMode::Fit
                    });
                }
            }
        }
    }
}

keel::String GenerateTableCode(const DemoContext& /*ctx*/) {
    return keel::format(
        "static const keel::StaticArray<guikit::TableColumn, 5> cols = {{{{\n"
        "    {{.title = \"ID\",        .width = 80.0f,  .sortable = true}},\n"
        "    {{.title = \"Callsign\",  .flex = 1.5f,    .sortable = true}},\n"
        "    {{.title = \"Frequency\", .width = 120.0f, .align = guikit::Align::End, .sortable = true}},\n"
        "    {{.title = \"SNR Level\", .flex = 1.4f,    .sortable = true}},\n"
        "    {{.title = \"Status\",    .width = 100.0f, .align = guikit::Align::Center, .sortable = true}}\n"
        "}}}};\n\n"
        "if (auto table = ui.Table(cols, {{\n"
        "    .totalRows = rows.size(),\n"
        "    .selectedRow = &selectedRow,\n"
        "    .sortColumn = &sortCol,\n"
        "    .sortDirection = &sortDir,\n"
        "    .stickyHeader = {},\n"
        "    .striped = {},\n"
        "    .gridLines = {},\n"
        "    .resizableColumns = {},\n"
        "    .showRowNumbers = {},\n"
        "    .showFooter = {}{}\n"
        "}})) {{\n"
        "    for (size_t r = table.FirstVisibleRow(); r < table.EndVisibleRow(); ++r) {{\n"
        "        if (auto row = table.Row(r)) {{\n"
        "            ui.Text(ui.Format(\"T-{{:04d}}\", rows[r].id));\n"
        "            ui.Text(rows[r].callsign);\n"
        "            ui.Text(ui.Format(\"{{:.3f}} MHz\", rows[r].freqMhz));\n"
        "            ui.ProgressBar(rows[r].snrDb, 0.0f, 40.0f, {{.segments = 12, .multiZoneColor = true}});\n"
        "            ui.Element({{.badge = {{\"LOCKED\", guikit::UiVariant::Success}}, .width = guikit::SizeMode::Fit}});\n"
        "        }}\n"
        "    }}\n"
        "}}",
        g_tableDemo.stickyHeader ? "true" : "false",
        g_tableDemo.striped ? "true" : "false",
        g_tableDemo.gridLines ? "true" : "false",
        g_tableDemo.resizableColumns ? "true" : "false",
        g_tableDemo.showRowNumbers ? "true" : "false",
        g_tableDemo.showFooter ? "true" : "false",
        FormatUiSizePrefix(g_tableDemo.tableSize)
    );
}

} // namespace guikit::demo
