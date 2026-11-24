#pragma once

#include "guikit/core/id.hpp"
#include "guikit/core/types.hpp"
#include "keel/array.h"
#include "keel/core.h"
#include "keel/ref.h"
#include "keel/small_array.h"

namespace guikit {

/// Records a focusable widget owned by a specific Modal dialog for modal focus trapping.
struct ModalFocusableEntry {
    Id modalId{Id::Null()};
    Id focusableId{Id::Null()};
};

/// Manages hover, active, keyboard focus, input modality, focus-visible semantics,
/// Tab/Shift-Tab focus traversal, and modal focus trapping/restoration.
class FocusManager {
public:
    FocusManager() = default;
    ~FocusManager() = default;

    void BeginFrame() noexcept;
    void CommitFrame();

    void RegisterFocusable(Id id, bool disabled, Id activeModalId);

    void ClearModalFocusables(Id modalId) noexcept;

    /// Computes the next (or previous, when `reverse == true`) focusable ID within the active scope
    /// (trapped to `topModalId` when a modal has registered focusables).
    [[nodiscard]] Id ComputeNextFocusTarget(Id topModalId, bool reverse) const noexcept;

    /// Checks whether `id` was registered as focusable inside `topModalId` in the current/committed frame.
    [[nodiscard]] bool IsInsideModal(Id modalId, Id id) const noexcept;

    /// Returns true if `id` is present in the current-frame focusable list.
    [[nodiscard]] bool IsInCurrentFocusables(Id id) const noexcept;

    /// Returns true if `id` is present in the previous/committed focusable list.
    [[nodiscard]] bool IsInPrevFocusables(Id id) const noexcept;

    [[nodiscard]] Id GetHoveredId() const noexcept { return m_hoveredId; }
    void SetHoveredId(Id id) noexcept { m_hoveredId = id; }

    [[nodiscard]] Id GetActiveId() const noexcept { return m_activeId; }
    void SetActiveId(Id id) noexcept { m_activeId = id; }

    [[nodiscard]] Id GetFocusedId() const noexcept { return m_focusedId; }
    void SetFocusedId(Id id) noexcept { m_focusedId = id; }

    [[nodiscard]] InputModality GetInputModality() const noexcept { return m_inputModality; }
    void SetInputModality(InputModality modality) noexcept { m_inputModality = modality; }

    [[nodiscard]] bool IsFocusVisible() const noexcept { return m_focusVisible; }
    void SetFocusVisible(bool visible) noexcept { m_focusVisible = visible; }

    [[nodiscard]] Id GetSavedFocusBeforeModal() const noexcept { return m_savedFocusBeforeModal; }
    void SetSavedFocusBeforeModal(Id id) noexcept { m_savedFocusBeforeModal = id; }

    [[nodiscard]] keel::ArrayRef<Id> GetPrevFocusableIds() const noexcept { return m_prevFocusableIds; }
    [[nodiscard]] keel::ArrayRef<ModalFocusableEntry> GetPrevModalFocusables() const noexcept { return m_prevModalFocusables; }

private:
    Id m_hoveredId{Id::Null()};
    Id m_activeId{Id::Null()};
    Id m_focusedId{Id::Null()};
    Id m_savedFocusBeforeModal{Id::Null()};
    InputModality m_inputModality{InputModality::Pointer};
    bool m_focusVisible{false};
    keel::Array<Id> m_prevFocusableIds;
    keel::Array<Id> m_currFocusableIds;
    keel::Array<ModalFocusableEntry> m_prevModalFocusables;
    keel::Array<ModalFocusableEntry> m_currModalFocusables;
};

} // namespace guikit
