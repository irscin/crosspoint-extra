#include "PhoneSyncActivity.h"

#include <Arduino.h>
#include <GfxRenderer.h>
#include <I18n.h>

#include <cstdio>

#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"

void PhoneSyncActivity::onEnter() {
  Activity::onEnter();
  start();
}

void PhoneSyncActivity::onExit() {
  sync.end();
  Activity::onExit();
}

void PhoneSyncActivity::start() {
  // One attempt per press: a refusal (low heap) is shown, not retried in a loop.
  sync.begin(renderer);
  lastPhoneActivityMs = millis();
  requestUpdate();
}

void PhoneSyncActivity::leave() {
  sync.end();
  ActivityResult result;
  result.isCancelled = !listChanged;
  setResult(std::move(result));
  finish();
}

bool PhoneSyncActivity::preventAutoSleep() {
  return sync.active() && sync.authenticated() && millis() - lastPhoneActivityMs < AWAKE_AFTER_REQUEST_MS;
}

void PhoneSyncActivity::loop() {
  using Button = MappedInputManager::Button;
  if (mappedInput.wasReleased(Button::Back)) {
    leave();
    return;
  }
  if (!sync.active()) {
    if (mappedInput.wasReleased(Button::Confirm)) start();
    return;
  }
  const auto poll = sync.poll();
  if (poll.listChanged) listChanged = true;
  if (poll.redraw || poll.listChanged) {
    lastPhoneActivityMs = millis();
    requestUpdate();
  }
}

void PhoneSyncActivity::render(RenderLock&&) {
  renderer.clearScreen();
  drawNavigationHeader(tr(STR_PHONE_SYNC));

  const char* state = sync.paused()          ? tr(STR_PHONE_SYNC_PAUSED)
                      : sync.failed()        ? tr(STR_PHONE_SYNC_UNAVAILABLE)
                      : sync.authenticated() ? tr(STR_PHONE_SYNC_PAIRED)
                      : sync.connected()     ? tr(STR_PHONE_SYNC_CONNECTED)
                                             : tr(STR_PHONE_SYNC_WAITING);
  const int lineHeight = renderer.getLineHeight(UI_12_FONT_ID);
  int y = renderer.getScreenHeight() / 3;
  renderer.drawCenteredText(UI_12_FONT_ID, y, state, true, EpdFontFamily::BOLD);
  y += 2 * lineHeight;
  if (sync.active() && !sync.code().empty()) {
    char code[96];
    snprintf(code, sizeof(code), "%s: %s", tr(STR_PHONE_SYNC_CODE), sync.code().c_str());
    renderer.drawCenteredText(UI_12_FONT_ID, y, code, true, EpdFontFamily::REGULAR);
  } else if (!sync.active()) {
    renderer.drawCenteredText(UI_12_FONT_ID, y, tr(STR_PHONE_SYNC_RESUME_HINT), true, EpdFontFamily::REGULAR);
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), sync.active() ? "" : tr(STR_SELECT), "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
