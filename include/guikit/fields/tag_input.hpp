#pragma once

#include "guikit/context.hpp"
#include "guikit/core/control.hpp"
#include "guikit/core/response.hpp"
#include "guikit/style/theme.hpp"
#include "guikit/core/types.hpp"
#include "guikit/graphics/icon.hpp"
#include "keel/core.h"
#include "keel/optional.h"
#include "keel/ref.h"
#include "keel/string.h"

namespace guikit {

class Context;
class ICanvas;

struct TagItem {
    keel::FixedString<64> text{};
    UiVariant variant{UiVariant::Default};
    Icon icon{};

    constexpr bool operator==(const TagItem& o) const noexcept {
        return text.as_ref() == o.text.as_ref() && variant == o.variant && icon == o.icon;
    }
};

class StringTagListSink;
class TagItemListSink;

/// Concept matching any mutable list of writable UTF-8 strings (`keel::Array<keel::String>`,
/// `keel::SmallArray<keel::FixedString<32>, 8>`, `keel::FixedArray<keel::FixedString<32>, 8>`,
/// or external `std::vector<std::string>`) without including STL headers.
template <typename C>
concept WritableTagStringList =
    !keel::IsConst<C> &&
    !keel::IsSame<keel::Decay<C>, StringTagListSink> &&
    requires(C& c, const C& cc) {
        requires keel::IsSame<decltype(cc.size()), keel::usize>;
        c[keel::usize{0}];
        cc[keel::usize{0}];
        c.pop_back();
        c.clear();
    } &&
    keel::WritableString<keel::RemoveCvRef<decltype((*static_cast<C*>(nullptr))[keel::usize{0}])>> &&
    (requires(C& c, keel::StringRef ref) {
        c.allocator();
        keel::RemoveCvRef<decltype(c[keel::usize{0}])>(ref, c.allocator());
    } || requires(C& c, keel::StringRef ref) {
        keel::RemoveCvRef<decltype(c[keel::usize{0}])>(ref);
    } || requires(C& c, const char* p, keel::usize n) {
        keel::RemoveCvRef<decltype(c[keel::usize{0}])>(p, n);
    }) &&
    (requires(C& c) { c.try_push_back(keel::move(c[keel::usize{0}])); } ||
     requires(C& c) { c.push_back(keel::move(c[keel::usize{0}])); });

/// Non-owning type-erased mutable adapter over a list of writable UTF-8 tag strings.
class StringTagListSink {
public:
    struct Ops {
        keel::usize (*size_fn)(const void* obj) noexcept;
        keel::StringRef (*get_fn)(const void* obj, keel::usize idx) noexcept;
        void (*clear_fn)(void* obj);
        void (*push_fn)(void* obj, keel::StringRef ref);
        void (*erase_at_fn)(void* obj, keel::usize idx);
    };

    constexpr StringTagListSink() noexcept = default;
    constexpr StringTagListSink(decltype(nullptr)) noexcept {}

    template <WritableTagStringList C>
    explicit constexpr StringTagListSink(C& list) noexcept
        : m_obj(&list), m_ops(&ops_for<C>()) {}

    [[nodiscard]] constexpr bool has_target() const noexcept { return m_obj != nullptr && m_ops != nullptr; }
    [[nodiscard]] explicit constexpr operator bool() const noexcept { return has_target(); }

    [[nodiscard]] keel::usize size() const noexcept { return has_target() ? m_ops->size_fn(m_obj) : 0; }
    [[nodiscard]] bool empty() const noexcept { return size() == 0; }

    [[nodiscard]] keel::StringRef get(keel::usize i) const noexcept {
        return has_target() ? m_ops->get_fn(m_obj, i) : keel::StringRef{};
    }

    void clear() const {
        if (has_target()) m_ops->clear_fn(m_obj);
    }

    void push_back(keel::StringRef ref) const {
        if (has_target()) m_ops->push_fn(m_obj, ref);
    }

    void erase_at(keel::usize idx) const {
        if (has_target()) m_ops->erase_at_fn(m_obj, idx);
    }

private:
    template <WritableTagStringList C>
    static const Ops& ops_for() noexcept {
        using ElemT = keel::RemoveCvRef<decltype((*static_cast<C*>(nullptr))[keel::usize{0}])>;
        static constexpr Ops kOps{
            +[](const void* obj) noexcept -> keel::usize {
                return static_cast<const C*>(obj)->size();
            },
            +[](const void* obj, keel::usize idx) noexcept -> keel::StringRef {
                const auto& c = *static_cast<const C*>(obj);
                if (idx >= c.size()) return {};
                const auto& elem = c[idx];
                return keel::StringRef(elem.data(), keel::as<keel::usize>(elem.size()));
            },
            +[](void* obj) {
                static_cast<C*>(obj)->clear();
            },
            +[](void* obj, keel::StringRef ref) {
                auto& c = *static_cast<C*>(obj);
                auto makeElem = [&]() -> ElemT {
                    if constexpr (requires { c.allocator(); ElemT(ref, c.allocator()); }) {
                        return ElemT(ref, c.allocator());
                    } else if constexpr (requires { ElemT(ref); }) {
                        return ElemT(ref);
                    } else {
                        const char* ptr = ref.data() ? ref.data() : "";
                        return ElemT(ptr, ref.size());
                    }
                };
                if constexpr (requires { c.try_push_back(makeElem()); }) {
                    (void)c.try_push_back(makeElem());
                } else {
                    if constexpr (requires(const C& cc) { cc.full(); }) {
                        if (c.full()) return;
                    }
                    c.push_back(makeElem());
                }
            },
            +[](void* obj, keel::usize idx) {
                auto& c = *static_cast<C*>(obj);
                const keel::usize n = c.size();
                if (idx >= n) return;
                for (keel::usize i = idx + 1; i < n; ++i) {
                    c[i - 1] = keel::move(c[i]);
                }
                c.pop_back();
            },
        };
        return kOps;
    }

    void* m_obj{nullptr};
    const Ops* m_ops{nullptr};
};

/// Concept matching any mutable contiguous list of `TagItem` (`keel::Array<TagItem>`,
/// `keel::SmallArray<TagItem, N>`, `keel::FixedArray<TagItem, N>`, or `std::vector<TagItem>`).
template <typename C>
concept WritableTagItemList =
    !keel::IsConst<C> &&
    !keel::IsSame<keel::Decay<C>, TagItemListSink> &&
    requires(C& c, const C& cc, const TagItem& item) {
        requires keel::IsSame<decltype(c.data()), TagItem*>;
        requires keel::IsSame<decltype(cc.data()), const TagItem*>;
        requires keel::IsSame<decltype(cc.size()), keel::usize>;
        c.push_back(item);
        c.pop_back();
        c.clear();
    };

/// Non-owning type-erased mutable adapter over a list of `TagItem` elements.
class TagItemListSink {
public:
    struct Ops {
        keel::MutArrayRef<TagItem> (*view_fn)(void* obj) noexcept;
        void (*clear_fn)(void* obj);
        void (*push_fn)(void* obj, const TagItem& item);
        void (*erase_at_fn)(void* obj, keel::usize idx);
    };

    constexpr TagItemListSink() noexcept = default;
    constexpr TagItemListSink(decltype(nullptr)) noexcept {}

    template <WritableTagItemList C>
    explicit constexpr TagItemListSink(C& list) noexcept
        : m_obj(&list), m_ops(&ops_for<C>()) {}

    [[nodiscard]] constexpr bool has_target() const noexcept { return m_obj != nullptr && m_ops != nullptr; }
    [[nodiscard]] explicit constexpr operator bool() const noexcept { return has_target(); }

    [[nodiscard]] keel::ArrayRef<TagItem> as_ref() const noexcept {
        if (!has_target()) return {};
        const keel::MutArrayRef<TagItem> m = m_ops->view_fn(m_obj);
        return keel::ArrayRef<TagItem>(m.data(), m.size());
    }

    [[nodiscard]] keel::MutArrayRef<TagItem> as_mut_ref() const noexcept {
        return has_target() ? m_ops->view_fn(m_obj) : keel::MutArrayRef<TagItem>{};
    }

    [[nodiscard]] keel::usize size() const noexcept { return as_ref().size(); }
    [[nodiscard]] bool empty() const noexcept { return as_ref().empty(); }
    [[nodiscard]] const TagItem& operator[](keel::usize i) const noexcept { return as_ref()[i]; }

    void clear() const {
        if (has_target()) m_ops->clear_fn(m_obj);
    }

    void push_back(const TagItem& item) const {
        if (has_target()) m_ops->push_fn(m_obj, item);
    }

    void erase_at(keel::usize idx) const {
        if (has_target()) m_ops->erase_at_fn(m_obj, idx);
    }

private:
    template <WritableTagItemList C>
    static const Ops& ops_for() noexcept {
        static constexpr Ops kOps{
            +[](void* obj) noexcept -> keel::MutArrayRef<TagItem> {
                auto& c = *static_cast<C*>(obj);
                return keel::MutArrayRef<TagItem>(c.data(), c.size());
            },
            +[](void* obj) {
                static_cast<C*>(obj)->clear();
            },
            +[](void* obj, const TagItem& item) {
                auto& c = *static_cast<C*>(obj);
                if constexpr (requires { c.try_push_back(item); }) {
                    (void)c.try_push_back(item);
                } else {
                    if constexpr (requires(const C& cc) { cc.full(); }) {
                        if (c.full()) return;
                    }
                    c.push_back(item);
                }
            },
            +[](void* obj, keel::usize idx) {
                auto& c = *static_cast<C*>(obj);
                const keel::usize n = c.size();
                if (idx >= n) return;
                TagItem* p = c.data();
                for (keel::usize i = idx + 1; i < n; ++i) {
                    p[i - 1] = keel::move(p[i]);
                }
                c.pop_back();
            },
        };
        return kOps;
    }

    void* m_obj{nullptr};
    const Ops* m_ops{nullptr};
};

struct TagInputOptions {
    UiText placeholder{"Add tag..."};         ///< Placeholder text shown when empty or in trailing inline input
    UiAffix prefix{};                         ///< Optional leading icon/text inside field on left
    UiVariant tagVariant{UiVariant::Primary}; ///< Default visual variant for tag pills
    keel::u32 maxTags{0};                     ///< Maximum allowed tags (0 = unlimited)
    bool allowDuplicates{false};              ///< Whether duplicate tag strings are permitted
    bool splitOnComma{true};                  ///< Commit tag(s) when typing ',' or ';'
    bool splitOnSpace{false};                 ///< Commit tag when typing space ' '
    bool clearable{true};                     ///< Show trailing '×' button to clear all tags
    bool showCounter{false};                  ///< Show tag count badge on the right (e.g. "3" or "3/8")
    bool readOnly{false};                     ///< Non-editable display mode (hides inline input and '×' buttons)
    UiText tooltip{};                         ///< Optional floating tooltip text on hover
    bool disabled{false};                            ///< Interaction suppression
    UiSize size{UiSize::Md};                         ///< Control size token (`Xs`..`Xl`, `Dp(...)`, `Factor(...)`)
    AxisSize width{};                                ///< Horizontal sizing policy (`Auto` -> `Stretch`, `Fit`, or explicit `dp`)
    AxisSize height{};                               ///< Vertical sizing policy (`Auto` -> `Fit`, `Stretch`, or explicit `dp`)
    keel::Optional<keel::f32> minWidth{keel::kNullOpt};  ///< Optional minimum width in logical dp
    keel::Optional<keel::f32> maxWidth{keel::kNullOpt};  ///< Optional maximum width in logical dp
    keel::Optional<keel::f32> minHeight{keel::kNullOpt}; ///< Optional minimum height in logical dp
    keel::Optional<keel::f32> maxHeight{keel::kNullOpt}; ///< Optional maximum height in logical dp
    FieldStyle style{FieldStyle::Default};           ///< Visual field presentation (Default, Filled, Borderless)
    UiVariant status{UiVariant::Default};            ///< Validation / semantic accent status (Default, Success, Warning, Danger, Info)
    StateColor background{};                         ///< Optional background fill override (`{}` = theme default, `StateColor::None()` = transparent)
    Border border{};                                 ///< Optional border width, color, and radius override
};

namespace detail {

class TagInput {
public:
    [[nodiscard]] static Response Run(
        Context& ctx,
        StringTagListSink tags,
        const TagInputOptions& opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    [[nodiscard]] static Response Run(
        Context& ctx,
        TagItemListSink tags,
        const TagInputOptions& opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    [[nodiscard]] static Vec2 Measure(
        keel::ArrayRef<keel::StringRef> tags,
        const TagInputOptions& opts,
        UiSize size,
        ICanvas* canvas,
        const UiTheme& theme,
        keel::f32 availableWidth = 0.0f
    );

    static void Render(const Node* node, ICanvas& canvas, const UiTheme& theme);
};

} // namespace detail

template <WritableTagStringList Arr>
Response TagInput(
    Context& ui,
    Arr& tags,
    TagInputOptions opts = {},
    keel::SourceLocation loc = keel::SourceLocation::current()
) {
    WidgetExecGuard guard(ui);
    return detail::TagInput::Run(ui, StringTagListSink(tags), opts, loc);
}

template <WritableTagItemList Arr>
Response TagInput(
    Context& ui,
    Arr& tags,
    TagInputOptions opts = {},
    keel::SourceLocation loc = keel::SourceLocation::current()
) {
    WidgetExecGuard guard(ui);
    return detail::TagInput::Run(ui, TagItemListSink(tags), opts, loc);
}

} // namespace guikit
