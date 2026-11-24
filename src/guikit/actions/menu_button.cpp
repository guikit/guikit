#include "guikit/actions/menu_button.hpp"
#include "guikit/context.hpp"
#include "keel/math.h"

namespace guikit {

namespace {

struct MenuButtonState {
    bool open{false};
    Id popupId{};
};

struct MenuButtonPrepared {
    Id menuId{};
    Response btnRes{};
    bool effectiveDisabled{false};
    MenuButtonState* state{nullptr};
};

MenuButtonPrepared RunMenuButtonTrigger(
    Context& ctx,
    ButtonOptions opts,
    ContextMenuOptions& menuOpts,
    keel::SourceLocation loc
) {
    const Id rootId = ctx.MakeChildId(loc);
    auto& state = ctx.GetOrCreateState<MenuButtonState>(rootId);
    const bool effectiveDisabled = opts.disabled || ctx.IsDisabled();

    auto scope = ctx.IdScope(rootId.value);
    const Id menuId = ctx.MakeChildId(keel::SourceLocation::current());

    // Automatically append a trailing chevron unless the button is square or already defines a trailing icon.
    const bool hasExplicitTrailing =
        opts.trailingIcon.IsValid() ||
        (opts.icon.IsValid() && opts.iconPlacement == Placement::Trailing);
    if (!opts.square && !hasExplicitTrailing) {
        opts.trailingIcon = Icon::ChevronDown();
    }

    const bool visualOpen = state.open && state.popupId.IsValid() && ctx.IsPopupOpen(state.popupId);
    opts.active = opts.active || visualOpen;

    const Response btnRes = detail::Button::Run(ctx, opts, keel::SourceLocation::current());

    if (effectiveDisabled) {
        state.open = false;
    } else if (btnRes.clicked) {
        state.open = !state.open;
        ctx.InvalidateAll();
    } else if (ctx.IsFocused(btnRes.id) && ctx.ConsumeKey(Key::Down)) {
        if (!state.open) {
            state.open = true;
            menuOpts.autoFocusFirst = true;
            ctx.InvalidateAll();
        }
    }

    menuOpts.anchorRect = btnRes.rect;
    menuOpts.disabled = effectiveDisabled;
    if (menuOpts.size == UiSize::Md && opts.size != UiSize::Md) {
        menuOpts.size = opts.size;
    }
    if (!menuOpts.minWidth.has_value()) {
        menuOpts.minWidth = keel::max(160.0f, btnRes.rect.w);
    }

    return MenuButtonPrepared{
        .menuId = menuId,
        .btnRes = btnRes,
        .effectiveDisabled = effectiveDisabled,
        .state = &state
    };
}

} // namespace

MenuScope detail::MenuButton::Open(
    Context& ctx,
    ButtonOptions opts,
    ContextMenuOptions menuOpts,
    keel::SourceLocation loc
) {
    const MenuButtonPrepared prep = RunMenuButtonTrigger(ctx, keel::move(opts), menuOpts, loc);
    prep.state->popupId = prep.menuId;
    return detail::ContextMenu::OpenWithId(ctx, prep.menuId, prep.state->open, menuOpts);
}

ContextMenuResponse detail::MenuButton::Run(
    Context& ctx,
    keel::ArrayRef<ContextMenuItem> items,
    ButtonOptions opts,
    ContextMenuOptions menuOpts,
    keel::SourceLocation loc
) {
    const MenuButtonPrepared prep = RunMenuButtonTrigger(ctx, keel::move(opts), menuOpts, loc);
    auto scope = ctx.IdScope(prep.menuId.value);
    ContextMenuResponse res = detail::ContextMenu::Run(ctx, prep.state->open, items, menuOpts, keel::SourceLocation::current());
    prep.state->popupId = res.id;
    res.rect = prep.btnRes.rect;
    res.hovered = prep.btnRes.hovered;
    return res;
}

} // namespace guikit
