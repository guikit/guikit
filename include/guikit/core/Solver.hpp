#pragma once

#include "guikit/core/Node.hpp"
#include "guikit/core/Theme.hpp"
#include "guikit/core/Types.hpp"

namespace guikit {

class ICanvas;

/// Resolves 2D flex layout for an arbitrary tree of GuiKit nodes.
class Solver {
public:
    /// Measures intrinsic content sizes bottom-up with the given theme.
    static void Measure(Node* root, ICanvas* canvas, const UiTheme& theme);

    /// Measures intrinsic content sizes bottom-up with default dark theme.
    static void Measure(Node* root, ICanvas* canvas = nullptr);

    /// Computes final coordinates and dimensions top-down based on parent constraints.
    static void Arrange(Node* root, Rect availableRect, const UiTheme& theme);

    /// Computes final coordinates and dimensions top-down with default dark theme.
    static void Arrange(Node* root, Rect availableRect);
};

/// Type alias for backward compatibility.
using LayoutSolver = Solver;

} // namespace guikit
