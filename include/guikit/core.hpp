#pragma once

// ==============================================================================
// GuiKit Core Module - Engine, Design System, and 8-Class UI Element Taxonomy
// Zero External OS / Windowing / GPU Dependencies (Pure C++20 + ICanvas / IPlatformWindow)
// ==============================================================================

// Engine Core
#include "guikit/context.hpp"
#include "guikit/backend/i_canvas.hpp"
#include "guikit/backend/i_platform_window.hpp"
#include "guikit/backend/null_canvas.hpp"
#include "guikit/backend/system_theme.hpp"
#include "guikit/backend/vector_backend.hpp"
#include "guikit/core/assets.hpp"
#include "guikit/core/container_scope.hpp"
#include "guikit/core/control.hpp"
#include "guikit/core/element.hpp"
#include "guikit/core/frame_metrics.hpp"
#include "guikit/core/id.hpp"
#include "guikit/core/image_cache.hpp"
#include "guikit/core/main_window.hpp"
#include "guikit/core/node.hpp"
#include "guikit/core/response.hpp"
#include "guikit/core/rich_text.hpp"
#include "guikit/core/scrollbar.hpp"
#include "guikit/core/shell_controls.hpp"
#include "guikit/core/solver.hpp"
#include "guikit/core/text_util.hpp"
#include "guikit/core/types.hpp"

// Design System
#include "guikit/style/tokens.hpp"
#include "guikit/style/theme.hpp"
#include "guikit/style/style_painter.hpp"
#include "guikit/style/field_frame.hpp"

// Low-Level Graphics
#include "guikit/graphics/colormap.hpp"
#include "guikit/graphics/draw_list.hpp"
#include "guikit/graphics/icon.hpp"

// 1. Layout (invisible geometry)
#include "guikit/layout/row.hpp"
#include "guikit/layout/column.hpp"
#include "guikit/layout/wrap.hpp"
#include "guikit/layout/grid.hpp"
#include "guikit/layout/spacer.hpp"
#include "guikit/layout/scroll_area.hpp"
#include "guikit/layout/split_view.hpp"

// 2. Surfaces (cards, panels & application chrome)
#include "guikit/surfaces/box.hpp"
#include "guikit/surfaces/card.hpp"
#include "guikit/surfaces/sidebar.hpp"
#include "guikit/surfaces/activity_bar.hpp"
#include "guikit/surfaces/toolbar.hpp"
#include "guikit/surfaces/bottom_panel.hpp"
#include "guikit/surfaces/status_bar.hpp"
#include "guikit/surfaces/menu_bar.hpp"
#include "guikit/surfaces/accordion.hpp"
#include "guikit/surfaces/carousel.hpp"
#include "guikit/surfaces/dock_area.hpp"

// 3. Overlays (out-of-flow portals & notifications)
#include "guikit/overlays/modal.hpp"
#include "guikit/overlays/popover.hpp"
#include "guikit/overlays/context_menu.hpp"
#include "guikit/overlays/command_palette.hpp"
#include "guikit/overlays/toast.hpp"

// 4. Actions (buttons & navigation triggers)
#include "guikit/actions/button.hpp"
#include "guikit/actions/menu_button.hpp"
#include "guikit/actions/split_button.hpp"
#include "guikit/actions/segmented.hpp"
#include "guikit/actions/tab_bar.hpp"
#include "guikit/actions/breadcrumb.hpp"
#include "guikit/actions/pagination.hpp"
#include "guikit/actions/sidebar_menu.hpp"

// 5. Fields (text & selection inputs)
#include "guikit/fields/input_field.hpp"
#include "guikit/fields/text_area.hpp"
#include "guikit/fields/select.hpp"
#include "guikit/fields/spin_box.hpp"
#include "guikit/fields/date_picker.hpp"
#include "guikit/fields/tag_input.hpp"
#include "guikit/fields/color_picker.hpp"

// 6. Controls (toggles & range controls)
#include "guikit/controls/checkbox.hpp"
#include "guikit/controls/radio.hpp"
#include "guikit/controls/switch.hpp"
#include "guikit/controls/slider.hpp"
#include "guikit/controls/knob.hpp"

// 7. Display (atomic indicators, typography & media)
#include "guikit/display/text.hpp"
#include "guikit/display/heading.hpp"
#include "guikit/display/tag.hpp"
#include "guikit/display/readout.hpp"
#include "guikit/display/separator.hpp"
#include "guikit/display/progress_bar.hpp"
#include "guikit/display/level_meter.hpp"
#include "guikit/display/histogram.hpp"
#include "guikit/display/image.hpp"

// 8. Views (complex data views & canvases)
#include "guikit/views/table.hpp"
#include "guikit/views/tree_view.hpp"
#include "guikit/views/markdown.hpp"
#include "guikit/views/image_view.hpp"
#include "guikit/views/scene.hpp"
