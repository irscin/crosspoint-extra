#include "QuickActionsActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "components/UIThemeTokens.h"

namespace fui = freeink::ui;

QuickActionsActivity::QuickActionsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, const bool inReader)
    : UiListActivity("QuickActions", renderer, mappedInput), inReader(inReader) {}

void QuickActionsActivity::onEnter() {
  UiListActivity::onEnter();
  actions = quickactions::configured(inReader);
  rowItems.clear();
  rowItems.reserve(actions.size());
  for (size_t i = 0; i < actions.size(); ++i) {
    fui::ListItem item;
    item.label = I18N.get(quickactions::LABELS[actions[i]]);
    item.actionValue = static_cast<int16_t>(i);
    rowItems.push_back(item);
  }
  nav.selected = 0;
}

const char* QuickActionsActivity::headerTitle() const { return tr(STR_QUICK_ACTIONS); }

void QuickActionsActivity::activateIndex(const int index) {
  app.clearTapFlash();
  if (index >= 0 && index < static_cast<int>(actions.size())) quickactions::pending = actions[index];
  finish();
}

void QuickActionsActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const Rect safe = UITheme::getInstance().getScreenSafeArea(renderer, true, false);
  screen.setContentMarginFromScreen(fui::Insets{
      static_cast<int16_t>(safe.y + metrics.topPadding + metrics.headerHeight),
      static_cast<int16_t>(renderer.getScreenWidth() - (safe.x + safe.width)),
      static_cast<int16_t>(renderer.getScreenHeight() - (safe.y + safe.height)), static_cast<int16_t>(safe.x)});
  screen.spacer(static_cast<int16_t>(metrics.verticalSpacing));

  fui::ListProps props;
  props.items = rowItems.data();
  props.count = static_cast<uint16_t>(rowItems.size());
  props.action = ACTION_ROW;
  props.inputMask = fui::InputTouch;  // physical buttons stay in loop()
  props.labelText = uiMenuLabelText(screen.theme());
  props.labelText.maxLines = 2;
  syncListViewport(screen, props);
  screen.list(props);
}
