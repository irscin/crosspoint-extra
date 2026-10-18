#pragma once

#include <GfxRenderer.h>
#include <I18n.h>

#include <vector>

#include "QuickActions.h"
#include "activities/UiListActivity.h"

class MappedInputManager;

// The menu a long press of Back opens: the user's configured quick actions. Picking one closes the
// menu and hands the action to main (quickactions::pending), which runs it on the screen underneath.
class QuickActionsActivity final : public UiListActivity {
 public:
  QuickActionsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, bool inReader);

  void onEnter() override;

 private:
  int listCount() const override { return static_cast<int>(actions.size()); }
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  const char* headerTitle() const override;

  const bool inReader;
  std::vector<uint8_t> actions;
  std::vector<freeink::ui::ListItem> rowItems;
};
