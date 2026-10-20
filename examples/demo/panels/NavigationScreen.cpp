#include "../DemoContext.hpp"
#include "guikit/widgets/Breadcrumb.hpp"
#include "guikit/widgets/Pagination.hpp"
#include <array>
#include <format>

namespace guikit::demo {

namespace {

struct NavigationDemoState {
    // Breadcrumb configuration
    size_t richCrumbIdx{4};
    size_t plainCrumbIdx{2};
    guikit::BreadcrumbSeparator separator{guikit::BreadcrumbSeparator::Chevron};
    int maxVisibleItems{0}; // 0 = show all, 2..5 = collapse middle items into "..."
    bool showCrumbIcons{true};
    bool showCrumbBadges{true};

    // Pagination configuration
    size_t currentPage{2};
    int totalPages{12};
    int siblingCount{1};
    bool showFirstLast{true};
    bool showPrevNext{true};
    bool showPageInfo{true};
};

NavigationDemoState s_nav;

constexpr std::string_view SeparatorEnumName(guikit::BreadcrumbSeparator sep) noexcept {
    switch (sep) {
        case guikit::BreadcrumbSeparator::Chevron: return "Chevron";
        case guikit::BreadcrumbSeparator::Slash:   return "Slash";
        case guikit::BreadcrumbSeparator::Arrow:   return "Arrow";
        case guikit::BreadcrumbSeparator::Dot:     return "Dot";
    }
    return "Chevron";
}

struct TelemetryPacketRow {
    std::string_view id;
    std::string_view subsystem;
    std::string_view endpoint;
    std::string_view latency;
    std::string_view status;
    guikit::UiVariant statusVariant;
};

constexpr std::array<TelemetryPacketRow, 16> kDatasetRows = {{
    {"PKT-1040", "Core Router",     "/clusters/eu-west/node-01", "0.42 ms", "ONLINE",   guikit::UiVariant::Success},
    {"PKT-1041", "Auth Gateway",    "/clusters/eu-west/auth",    "1.18 ms", "VERIFIED", guikit::UiVariant::Info},
    {"PKT-1042", "Radar DSP #1",    "/dsp/beamformer/ch-01",     "0.85 ms", "LOCKED",   guikit::UiVariant::Primary},
    {"PKT-1043", "Storage Shard A", "/storage/nvme-pool-0",      "2.64 ms", "SYNCING",  guikit::UiVariant::Warning},
    {"PKT-1044", "Sonar Array",     "/acoustics/hydrophone-4",   "0.55 ms", "ONLINE",   guikit::UiVariant::Success},
    {"PKT-1045", "Telemetry Bus",   "/bus/can-fd/bridge-2",      "3.90 ms", "DEGRADED", guikit::UiVariant::Danger},
    {"PKT-1046", "Flight Control",  "/avionics/fcu-primary",     "0.31 ms", "LOCKED",   guikit::UiVariant::Primary},
    {"PKT-1047", "Optical Link",    "/comms/laser-transceiver",  "0.19 ms", "ONLINE",   guikit::UiVariant::Success},
    {"PKT-1048", "Codec Engine",    "/media/h265-encoder-0",     "4.12 ms", "ACTIVE",   guikit::UiVariant::Info},
    {"PKT-1049", "Thermal Loop",    "/env/coolant-manifold",     "1.05 ms", "NOMINAL",  guikit::UiVariant::Success},
    {"PKT-1050", "Power Inverter",  "/power/hv-bus-a",           "2.20 ms", "WARN",     guikit::UiVariant::Warning},
    {"PKT-1051", "GNSS Receiver",   "/nav/rtk-base-station",     "0.74 ms", "LOCKED",   guikit::UiVariant::Primary},
    {"PKT-1052", "Packet Broker",   "/net/ingress-filter",       "0.63 ms", "ONLINE",   guikit::UiVariant::Success},
    {"PKT-1053", "Crypto Vault",    "/sec/hsm-enclave-02",       "1.49 ms", "VERIFIED", guikit::UiVariant::Info},
    {"PKT-1054", "Waveform Synth",  "/rf/lo-synthesizer",        "0.91 ms", "LOCKED",   guikit::UiVariant::Primary},
    {"PKT-1055", "Audit Logger",    "/sys/journal-replica",      "1.82 ms", "ONLINE",   guikit::UiVariant::Success}
}};

} // namespace

void RenderNavigationInspector(guikit::Context& ui, DemoContext& ctx) {
    if (auto el = ui.Element({.label = "Control Size"})) {
        if (ui.StepSlider(ctx.controlsSize, {.showTicks = true, .showLabels = true, .includeNone = true}).changed) {
            ui.InvalidateAll();
        }
    }
    ctx.RenderVariantPicker(ui);

    ui.Size(guikit::UiSize::Xs).Heading({
        .label = "Breadcrumb Options",
        .divider = true
    });

    static constexpr std::array<std::string_view, 4> kSepLabels = {"Chevron", "Slash", "Arrow", "Dot"};
    size_t sepIdx = static_cast<size_t>(s_nav.separator);
    if (auto el = ui.Element({.label = "Separator Style", .layout = guikit::ElementLayout::Stacked})) {
        if (ui.Segmented(sepIdx, kSepLabels).changed) {
            s_nav.separator = static_cast<guikit::BreadcrumbSeparator>(sepIdx);
            ui.InvalidateAll();
        }
    }

    if (auto el = ui.Element({
        .label = "Max Visible Items",
        .description = "0 = show all; >= 2 collapses middle items into '...'"
    })) {
        if (ui.Slider(s_nav.maxVisibleItems, 0, 5, {
            .valueFormat = (s_nav.maxVisibleItems == 0)
                ? guikit::UiText{"All", {"All", "%d items"}}
                : guikit::UiText{"%d items", {"All", "%d items"}}
        }).changed) {
            if (s_nav.maxVisibleItems == 1) s_nav.maxVisibleItems = 2;
            ui.InvalidateAll();
        }
    }

    if (auto el = ui.Element({.label = "Show Item Icons"})) {
        if (ui.Switch(s_nav.showCrumbIcons).changed) ui.InvalidateAll();
    }
    if (auto el = ui.Element({.label = "Show Item Badges"})) {
        if (ui.Switch(s_nav.showCrumbBadges).changed) ui.InvalidateAll();
    }

    ui.Size(guikit::UiSize::Xs).Heading({
        .label = "Pagination Options",
        .divider = true
    });

    if (auto el = ui.Element({.label = "Total Pages"})) {
        if (ui.Slider(s_nav.totalPages, 3, 30, {.valueFormat = "%d pages"}).changed) {
            if (s_nav.currentPage >= static_cast<size_t>(s_nav.totalPages)) {
                s_nav.currentPage = static_cast<size_t>(s_nav.totalPages - 1);
            }
            ui.InvalidateAll();
        }
    }

    if (auto el = ui.Element({
        .label = "Sibling Count",
        .description = "Neighbors shown on each side of active page"
    })) {
        if (ui.Slider(s_nav.siblingCount, 0, 3, {.valueFormat = "%d"}).changed) {
            ui.InvalidateAll();
        }
    }

    if (auto el = ui.Element({.label = "First / Last Buttons (« »)"})) {
        if (ui.Switch(s_nav.showFirstLast).changed) ui.InvalidateAll();
    }
    if (auto el = ui.Element({.label = "Prev / Next Buttons (‹ ›)"})) {
        if (ui.Switch(s_nav.showPrevNext).changed) ui.InvalidateAll();
    }
    if (auto el = ui.Element({.label = "Page Info Summary"})) {
        if (ui.Switch(s_nav.showPageInfo).changed) ui.InvalidateAll();
    }
    if (auto el = ui.Element({.label = "Disabled State"})) {
        if (ui.Switch(ctx.elemDisabled).changed) ui.InvalidateAll();
    }
}

void RenderNavigationStage(guikit::Context& ui, DemoContext& ctx) {
    const guikit::StateColor activeCol = guikit::StateColor::FromVariant(ctx.btnVariant);
    const uint32_t maxVis = (s_nav.maxVisibleItems >= 2) ? static_cast<uint32_t>(s_nav.maxVisibleItems) : 0u;

    // 1. Breadcrumb Showcase Card (Rich + Plain String-View)
    if (auto bcCard = ui.Card({
        .gap = 12.0f,
        .padding = guikit::Padding::All(16.0f),
        .widthMode = guikit::SizeMode::Stretch
    })) {
        std::array<guikit::BreadcrumbItem, 6> richCrumbs = {{
            {.label = "Workspace",   .icon = s_nav.showCrumbIcons ? guikit::Icon::Folder()   : guikit::Icon{}},
            {.label = "Clusters",    .icon = s_nav.showCrumbIcons ? guikit::Icon::Settings() : guikit::Icon{}, .badge = s_nav.showCrumbBadges ? "EU" : ""},
            {.label = "Production",  .icon = s_nav.showCrumbIcons ? guikit::Icon::Flash()    : guikit::Icon{}},
            {.label = "Telemetry",   .icon = s_nav.showCrumbIcons ? guikit::Icon::Search()   : guikit::Icon{}},
            {.label = "PacketNodes", .icon = s_nav.showCrumbIcons ? guikit::Icon::Eye()      : guikit::Icon{}, .badge = s_nav.showCrumbBadges ? "v2.4" : ""},
            {.label = "LiveStream",  .icon = s_nav.showCrumbIcons ? guikit::Icon::Play()     : guikit::Icon{}, .badge = s_nav.showCrumbBadges ? "16" : ""}
        }};

        ui.Size(guikit::UiSize::Sm).Heading({
            .label = "Hierarchical Breadcrumb Navigation",
            .description = ui.Format(
                "Active Segment: [{}] {}  |  Separator: {}  |  Max Visible: {}",
                s_nav.richCrumbIdx,
                richCrumbs[std::min(s_nav.richCrumbIdx, richCrumbs.size() - 1)].label.View(),
                SeparatorEnumName(s_nav.separator),
                maxVis == 0 ? std::string("All") : std::to_string(maxVis)
            ),
            .divider = true
        });

        {
            auto sz = ui.Size(ctx.controlsSize);
            guikit::Breadcrumb::Run(ui, s_nav.richCrumbIdx, std::span<const guikit::BreadcrumbItem>(richCrumbs), {
                .separator = s_nav.separator,
                .maxVisibleItems = maxVis,
                .color = activeCol,
                .background = guikit::StateColor::Surface(),
                .border = {.width = 1.0f, .radius = 8.0f},
                .disabled = ctx.elemDisabled
            });
        }

        static constexpr std::array<std::string_view, 5> kPlainCrumbs = {
            "Root", "Devices", "Subsystems", "Sensors", "Calibration"
        };
        if (auto plainRow = ui.Row({.gap = 10.0f, .align = guikit::Alignment::Center})) {
            ui.Text("Plain String Path:", {.role = guikit::TextRole::LabelSmall, .variant = guikit::UiVariant::Muted});
            ui.Size(ctx.controlsSize).Breadcrumb(s_nav.plainCrumbIdx, std::span<const std::string_view>(kPlainCrumbs), {
                .separator = s_nav.separator,
                .maxVisibleItems = maxVis,
                .color = activeCol,
                .disabled = ctx.elemDisabled
            });
        }
    }

    // 2. Paginated Telemetry Explorer Card
    if (auto pageCard = ui.Card({
        .gap = 12.0f,
        .padding = guikit::Padding::All(16.0f),
        .widthMode = guikit::SizeMode::Stretch,
        .heightMode = guikit::SizeMode::Stretch
    })) {
        const size_t totalPages = static_cast<size_t>(std::max(1, s_nav.totalPages));
        if (s_nav.currentPage >= totalPages) {
            s_nav.currentPage = totalPages - 1;
        }

        ui.Size(guikit::UiSize::Sm).Heading({
            .label = "Paginated Telemetry Endpoint Explorer",
            .description = ui.Format(
                "Viewing Page {} of {} (4 records per page, smart ellipsis window with siblingCount = {})",
                s_nav.currentPage + 1,
                totalPages,
                s_nav.siblingCount
            ),
            .badge = {ui.Format("Page {}/{}", s_nav.currentPage + 1, totalPages), guikit::UiVariant::Primary},
            .divider = true
        });

        // Live paginated record cards for the active page
        constexpr size_t kRowsPerPage = 4;
        if (auto listCol = ui.Column({
            .gap = 8.0f,
            .widthMode = guikit::SizeMode::Stretch,
            .heightMode = guikit::SizeMode::Stretch
        })) {
            for (size_t r = 0; r < kRowsPerPage; ++r) {
                const size_t globalIdx = s_nav.currentPage * kRowsPerPage + r;
                const TelemetryPacketRow& row = kDatasetRows[globalIdx % kDatasetRows.size()];

                if (auto itemCard = ui.Card({
                    .padding = guikit::Padding::Symmetric(12.0f, 8.0f),
                    .border = {.width = 1.0f, .radius = 6.0f},
                    .widthMode = guikit::SizeMode::Stretch
                })) {
                    if (auto rowLayout = ui.Row({
                        .gap = 12.0f,
                        .align = guikit::Alignment::Center,
                        .widthMode = guikit::SizeMode::Stretch
                    })) {
                        ui.Text(ui.Format("#{:03d}  {}", globalIdx + 1, row.id), {
                            .role = guikit::TextRole::CodeMedium,
                            .variant = guikit::UiVariant::Primary
                        });
                        ui.Text(row.subsystem, {
                            .role = guikit::TextRole::LabelLarge
                        });
                        ui.Text(row.endpoint, {
                            .role = guikit::TextRole::CodeSmall,
                            .variant = guikit::UiVariant::Secondary
                        });
                        ui.Spacer();
                        ui.Text(row.latency, {
                            .role = guikit::TextRole::CodeSmall,
                            .variant = guikit::UiVariant::Muted
                        });
                        ui.Size(guikit::UiSize::Xs).Button({
                            .text = row.status,
                            .color = guikit::StateColor::FromVariant(row.statusVariant)
                        });
                    }
                }
            }
        }

        // Bottom Pagination Control Bar
        if (auto footerRow = ui.Row({
            .gap = 12.0f,
            .align = guikit::Alignment::Center,
            .justify = guikit::Justify::SpaceBetween,
            .widthMode = guikit::SizeMode::Stretch
        })) {
            ui.Size(ctx.controlsSize).Pagination(s_nav.currentPage, totalPages, {
                .siblingCount = static_cast<uint32_t>(std::max(0, s_nav.siblingCount)),
                .showFirstLast = s_nav.showFirstLast,
                .showPrevNext = s_nav.showPrevNext,
                .showPageInfo = s_nav.showPageInfo,
                .color = activeCol,
                .disabled = ctx.elemDisabled
            });

            if (auto jumpRow = ui.Row({.gap = 6.0f, .align = guikit::Alignment::Center})) {
                if (ui.Size(guikit::UiSize::Xs).Button({
                    .text = "Jump to First",
                    .icon = guikit::Icon::ArrowLeft(),
                    .disabled = (s_nav.currentPage == 0 || ctx.elemDisabled)
                }).clicked) {
                    s_nav.currentPage = 0;
                    ui.InvalidateAll();
                }
                if (ui.Size(guikit::UiSize::Xs).Button({
                    .text = "Jump to Last",
                    .icon = guikit::Icon::ArrowRight(),
                    .iconPlacement = guikit::Placement::Trailing,
                    .disabled = (s_nav.currentPage + 1 >= totalPages || ctx.elemDisabled)
                }).clicked) {
                    s_nav.currentPage = totalPages - 1;
                    ui.InvalidateAll();
                }
            }
        }
    }
}

std::string GenerateNavigationCode(const DemoContext& ctx) {
    const std::string sizePrefix = FormatUiSizePrefix(ctx.controlsSize);
    const uint32_t maxVis = (s_nav.maxVisibleItems >= 2) ? static_cast<uint32_t>(s_nav.maxVisibleItems) : 0u;

    return std::format(
        "// 1. Rich Breadcrumb with icons, badges & middle-item collapse\n"
        "static const std::array<guikit::BreadcrumbItem, 6> crumbs = {{{{\n"
        "    {{.label = \"Workspace\",   .icon = guikit::Icon::Folder()}},\n"
        "    {{.label = \"Clusters\",    .icon = guikit::Icon::Settings(), .badge = \"EU\"}},\n"
        "    {{.label = \"Production\",  .icon = guikit::Icon::Flash()}},\n"
        "    {{.label = \"Telemetry\",   .icon = guikit::Icon::Search()}},\n"
        "    {{.label = \"PacketNodes\", .icon = guikit::Icon::Eye(),      .badge = \"v2.4\"}},\n"
        "    {{.label = \"LiveStream\",  .icon = guikit::Icon::Play(),     .badge = \"16\"}}\n"
        "}}}};\n"
        "ui.{}Breadcrumb(activeCrumb, crumbs, {{\n"
        "    .separator = guikit::BreadcrumbSeparator::{},\n"
        "    .maxVisibleItems = {},\n"
        "    .color = guikit::StateColor::{}(),\n"
        "    .disabled = {}\n"
        "}});\n\n"
        "// 2. Pagination with smart ellipsis & page summary\n"
        "ui.{}Pagination(currentPage, {}, {{\n"
        "    .siblingCount = {},\n"
        "    .showFirstLast = {},\n"
        "    .showPrevNext = {},\n"
        "    .showPageInfo = {},\n"
        "    .color = guikit::StateColor::{}(),\n"
        "    .disabled = {}\n"
        "}});",
        sizePrefix,
        SeparatorEnumName(s_nav.separator),
        maxVis,
        VariantName(ctx.btnVariant),
        ctx.elemDisabled ? "true" : "false",
        sizePrefix,
        s_nav.totalPages,
        s_nav.siblingCount,
        s_nav.showFirstLast ? "true" : "false",
        s_nav.showPrevNext ? "true" : "false",
        s_nav.showPageInfo ? "true" : "false",
        VariantName(ctx.btnVariant),
        ctx.elemDisabled ? "true" : "false"
    );
}

} // namespace guikit::demo
