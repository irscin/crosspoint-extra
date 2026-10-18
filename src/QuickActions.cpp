#include "QuickActions.h"

#include <HalFrontlight.h>
#include <HalTiltSensor.h>

#include <algorithm>

#include "CrossPointSettings.h"

namespace quickactions {

uint8_t pending = NONE;

namespace {
void slots(uint8_t (&out)[SLOT_COUNT]) {
  out[0] = SETTINGS.quickAction1;
  out[1] = SETTINGS.quickAction2;
  out[2] = SETTINGS.quickAction3;
  out[3] = SETTINGS.quickAction4;
  out[4] = SETTINGS.quickAction5;
}
}  // namespace

bool anyConfigured() {
  uint8_t s[SLOT_COUNT];
  slots(s);
  return std::any_of(std::begin(s), std::end(s), [](const uint8_t id) { return id != NONE && id < COUNT; });
}

std::vector<uint8_t> configured(const bool inReader) {
  uint8_t s[SLOT_COUNT];
  slots(s);
  std::vector<uint8_t> out;
  for (const uint8_t id : s) {
    if (id == NONE || id >= COUNT) continue;
    if (readerOnly(id) && !inReader) continue;
    if (id == FRONTLIGHT && !Frontlight.present()) continue;
    if (id == TILT_PAGE_TURN && !halTiltSensor.isAvailable()) continue;
    if (std::find(out.begin(), out.end(), id) != out.end()) continue;
    out.push_back(id);
  }
  return out;
}

}  // namespace quickactions
