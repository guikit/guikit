#include "guikit/core/focus_manager.hpp"

namespace guikit {

namespace {

[[nodiscard]] bool ContainsId(keel::ArrayRef<Id> list, Id target) noexcept {
    for (const Id& id : list) {
        if (id == target) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] keel::usize FindIdIndex(keel::ArrayRef<Id> list, Id target) noexcept {
    for (keel::usize i = 0; i < list.size(); ++i) {
        if (list[i] == target) {
            return i;
        }
    }
    return keel::kInvalidIndex;
}

template <typename T, typename Pred>
void EraseIf(keel::Array<T>& arr, Pred&& pred) noexcept {
    keel::usize writeIdx = 0;
    for (keel::usize i = 0; i < arr.size(); ++i) {
        if (!pred(arr[i])) {
            if (writeIdx != i) {
                arr[writeIdx] = keel::move(arr[i]);
            }
            ++writeIdx;
        }
    }
    arr.resize(writeIdx);
}

} // namespace

void FocusManager::BeginFrame() noexcept {
    m_currFocusableIds.clear();
    m_currModalFocusables.clear();
}

void FocusManager::CommitFrame() {
    m_prevFocusableIds = keel::move(m_currFocusableIds);
    m_currFocusableIds.clear();
    m_prevModalFocusables = keel::move(m_currModalFocusables);
    m_currModalFocusables.clear();
}

void FocusManager::RegisterFocusable(Id id, bool disabled, Id activeModalId) {
    if (!id.IsValid() || disabled) return;
    if (!ContainsId(m_currFocusableIds, id)) {
        m_currFocusableIds.push_back(id);
    }
    if (activeModalId.IsValid()) {
        for (const auto& entry : m_currModalFocusables) {
            if (entry.modalId == activeModalId && entry.focusableId == id) {
                return;
            }
        }
        m_currModalFocusables.push_back(ModalFocusableEntry{activeModalId, id});
    }
}

void FocusManager::ClearModalFocusables(Id modalId) noexcept {
    if (!modalId.IsValid()) {
        m_currModalFocusables.clear();
        m_prevModalFocusables.clear();
        return;
    }
    EraseIf(m_currModalFocusables, [modalId](const ModalFocusableEntry& e) { return e.modalId == modalId; });
    EraseIf(m_prevModalFocusables, [modalId](const ModalFocusableEntry& e) { return e.modalId == modalId; });
}

Id FocusManager::ComputeNextFocusTarget(Id topModalId, bool reverse) const noexcept {
    keel::SmallArray<Id, 32> topModalFocusables;
    if (topModalId.IsValid()) {
        for (const auto& entry : m_prevModalFocusables) {
            if (entry.modalId == topModalId) {
                topModalFocusables.push_back(entry.focusableId);
            }
        }
    }
    const keel::ArrayRef<Id> activeFocusables = !topModalFocusables.empty()
        ? keel::ArrayRef<Id>(topModalFocusables.data(), topModalFocusables.size())
        : keel::ArrayRef<Id>(m_prevFocusableIds.data(), m_prevFocusableIds.size());
    if (activeFocusables.empty()) {
        return Id::Null();
    }

    const keel::usize curIdx = FindIdIndex(activeFocusables, m_focusedId);
    if (curIdx == keel::kInvalidIndex) {
        return reverse ? activeFocusables[activeFocusables.size() - 1] : activeFocusables[0];
    }
    const keel::usize n = activeFocusables.size();
    const keel::usize nextIdx = reverse ? ((curIdx + n - 1) % n) : ((curIdx + 1) % n);
    return activeFocusables[nextIdx];
}

bool FocusManager::IsInsideModal(Id modalId, Id id) const noexcept {
    if (!modalId.IsValid() || !id.IsValid()) return false;
    for (const auto& entry : m_prevModalFocusables) {
        if (entry.modalId == modalId && entry.focusableId == id) {
            return true;
        }
    }
    return false;
}

bool FocusManager::IsInCurrentFocusables(Id id) const noexcept {
    return ContainsId(m_currFocusableIds, id);
}

bool FocusManager::IsInPrevFocusables(Id id) const noexcept {
    return ContainsId(m_prevFocusableIds, id);
}

} // namespace guikit
