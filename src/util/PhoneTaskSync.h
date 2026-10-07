#pragma once

#include <cstdint>
#include <string>

#include "util/TaskSync.h"

class GfxRenderer;

// Phone sync as a component: advertises the BLE task service while the Home To-do tab is open, serves
// the companion app's requests against PRIORITIES_STORE, and exposes the state the tab's ribbon shows.
// Builds without FREEINK_CAP_BLE_TASKS get inert behaviour (begin() fails), so callers need no #if.
class PhoneTaskSync {
 public:
  struct Poll {
    bool redraw = false;       // state or connection changed
    bool listChanged = false;  // the phone edited the to-do list
  };

  // Starts advertising and loads the code. False if the radio or heap refused (see failed()).
  bool begin(GfxRenderer& renderer);
  void end();
  bool active() const { return started; }
  bool failed() const { return didFail; }
  // The radio was switched off after sitting idle with no phone connected; leave and re-enter the tab to resume.
  bool paused() const { return didPause; }
  // Serves queued requests and drops stale or wrong-code peers. Call from the main loop.
  Poll poll();

  const std::string& code() const { return pairingCode; }
  bool authenticated() const { return session.authenticated; }
  bool connected() const;

 private:
  void loadCode();

  bool started = false;
  bool didFail = false;
  bool didPause = false;
  unsigned long idleSinceMs = 0;
  std::string pairingCode;
  tasksync::Session session;
  uint32_t sessionConnection = 0;
  uint32_t lastCounter = 0;
  uint32_t lastStoreRevision = 0;
  unsigned long unauthenticatedSinceMs = 0;
  static constexpr unsigned long UNAUTHENTICATED_GRACE_MS = 20000;
};
