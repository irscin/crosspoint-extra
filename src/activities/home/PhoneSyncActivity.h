#pragma once

#include "activities/Activity.h"
#include "util/PhoneTaskSync.h"

// Phone sync screen, opened from the "Phone sync" row of the Home To-do tab. The radio is on only
// while this screen is: it starts on enter and stops on exit (Back, or the sleep path's teardown).
// PhoneTaskSync still pauses it after a while with no phone; Select starts it again.
// Finishes cancelled unless the phone edited the list, so Home rebuilds its rows only when needed.
class PhoneSyncActivity final : public Activity {
 public:
  PhoneSyncActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("PhoneSync", renderer, mappedInput) {}
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
  // A paired phone mid-sync keeps the device awake, but only while it is actually talking: a phone
  // that stays connected and silent must not hold the device out of sleep.
  bool preventAutoSleep() override;

 private:
  static constexpr unsigned long AWAKE_AFTER_REQUEST_MS = 2UL * 60UL * 1000UL;

  PhoneTaskSync sync;
  bool listChanged = false;
  unsigned long lastPhoneActivityMs = 0;

  void start();
  void leave();
};
