#pragma once

#include "guikit/core/node.hpp"
#include "guikit/core/theme.hpp"
#include "guikit/core/types.hpp"

namespace guikit {

class ICanvas;

/// Resolves 2D flex layout for an arbitrary tree of GuiKit nodes.
class Solver {
public:
    /// Resolves shell-container spacing and border defaults prior to geometric measurement.
    static void ResolveShellStyles(Node* root, const UiTheme& theme);

    /// Measures intrinsic content sizes bottom-up with the given theme.
    static void Measure(Node* root, ICanvas* canvas, const UiTheme& theme);

    /// Measures intrinsic content sizes bottom-up with default dark theme.
    static void Measure(Node* root, ICanvas* canvas = nullptr);

    /// Resolves constrained height for a node given an explicit available width.
    static keel::f32 ResolveHeight(Node* node, keel::f32 availableWidth, ICanvas* canvas, const UiTheme& theme);

    /// Computes final coordinates and dimensions top-down based on parent constraints.
    static void Arrange(Node* root, Rect availableRect, const UiTheme& theme, ICanvas* canvas = nullptr);

    /// Computes final coordinates and dimensions top-down with default dark theme.
    static void Arrange(Node* root, Rect availableRect);
};

} // namespace guikit
