#pragma once

// ==============================================================================
// GuiKit Core Module - Context, Layout Engine, Base Widgets & Panels
// Zero External OS / Windowing / GPU Dependencies (Pure C++20 + ICanvas)
// ==============================================================================

#include "guikit/context.hpp"
#include "guikit/backend/i_canvas.hpp"
#include "guikit/backend/vector_backend.hpp"
#include "guikit/core/allocator.hpp"
#include "guikit/core/assets.hpp"
#include "guikit/core/container_scope.hpp"
#include "guikit/core/control.hpp"
#include "guikit/core/element.hpp"
#include "guikit/core/frame_metrics.hpp"
#include "guikit/core/id.hpp"
#include "guikit/core/main_window.hpp"
#include "guikit/core/node.hpp"
#include "guikit/core/response.hpp"
#include "guikit/core/scrollbar.hpp"
#include "guikit/core/solver.hpp"
#include "guikit/core/text_util.hpp"
#include "guikit/core/theme.hpp"
#include "guikit/core/tokens.hpp"
#include "guikit/core/types.hpp"
#include "guikit/core/ui_string.hpp"
#include "guikit/core/ui_vector.hpp"
#include "guikit/graphics/icon.hpp"
