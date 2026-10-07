#include "PhoneTaskSync.h"

#include <Arduino.h>
#include <FontCacheManager.h>
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <Logging.h>
#include <esp_random.h>

#include "DeviceName.h"
#include "PrioritiesStore.h"
#include "network/TaskBleService.h"

bool PhoneTaskSync::connected() const { return taskble::connected(); }

void PhoneTaskSync::loadCode() {
  std::string raw;
  pairingCode.clear();
  if (Storage.readFileToString("TASKSYNC", tasksync::CODE_FILE, 128, raw)) pairingCode = tasksync::normalizeCode(raw);
  if (pairingCode.empty()) {
    // No usable file yet: make one the user can read on the ribbon and edit on the SD card.
    pairingCode = tasksync::codeFromRandom(esp_random());
    Storage.mkdir("/.crosspoint");
    if (!Storage.writeFile(tasksync::CODE_FILE, String((pairingCode + "\n").c_str())))
      LOG_ERR("TASKSYNC", "Could not save %s", tasksync::CODE_FILE);
  }
  session = tasksync::Session{pairingCode};
}

bool PhoneTaskSync::begin(GfxRenderer& renderer) {
  if (started) return true;
  didFail = false;
  didPause = false;
  // The stack needs one big block; rebuildable SD-font caches are the largest thing to give back.
  if (auto* cache = renderer.getFontCacheManager()) cache->releaseSdFontCaches();
  char name[40];
  deviceNetworkName(name, sizeof(name), "CrossPoint");
  loadCode();
  if (!taskble::hasHeapForStart() || !taskble::start(name)) {
    LOG_ERR("TASKSYNC", "BLE start refused");
    didFail = true;
    return false;
  }
  started = true;
  sessionConnection = taskble::connectionId();
  lastCounter = taskble::activityCounter();
  lastStoreRevision = PRIORITIES_STORE.revision();
  unauthenticatedSinceMs = 0;
  idleSinceMs = millis();
  return true;
}

void PhoneTaskSync::end() {
  didPause = false;
  if (!started && !didFail) return;
  taskble::stop();
  started = false;
  session = tasksync::Session{pairingCode};
}

PhoneTaskSync::Poll PhoneTaskSync::poll() {
  Poll result;
  if (!started) return result;

  // A new connection starts unauthenticated, whatever the previous one did.
  if (taskble::connectionId() != sessionConnection) {
    sessionConnection = taskble::connectionId();
    session = tasksync::Session{pairingCode};
  }
  std::string request;
  while (taskble::popRequest(request)) {
    const bool wasAuthenticated = session.authenticated;
    taskble::sendResponse(tasksync::handleRequest(PRIORITIES_STORE, session, request));
    if (session.authenticated != wasAuthenticated) result.redraw = true;
    if (session.shouldDrop()) {
      LOG_INF("TASKSYNC", "Dropping peer after wrong codes");
      taskble::dropPeer();
    }
  }
  // A peer that connected but never authenticated holds the only connection slot, and a connected
  // device stops advertising, so the real phone could not find it. Hang up after a grace period.
  if (taskble::connected() && !session.authenticated) {
    if (unauthenticatedSinceMs == 0) unauthenticatedSinceMs = millis();
    if (millis() - unauthenticatedSinceMs >= UNAUTHENTICATED_GRACE_MS) {
      LOG_INF("TASKSYNC", "Dropping unauthenticated peer after %u ms", static_cast<unsigned>(UNAUTHENTICATED_GRACE_MS));
      taskble::dropPeer();
      unauthenticatedSinceMs = 0;
    }
  } else {
    unauthenticatedSinceMs = 0;
  }

  // Nobody connected for a while: stop advertising so a forgotten tab does not drain the battery.
  if (taskble::connected()) idleSinceMs = millis();
  if (tasksync::shouldPauseIdle(taskble::connected(), static_cast<uint32_t>(millis() - idleSinceMs))) {
    LOG_INF("TASKSYNC", "Idle for %u ms with no phone; pausing", static_cast<unsigned>(tasksync::IDLE_PAUSE_MS));
    taskble::stop();
    started = false;
    didPause = true;
    result.redraw = true;
    return result;
  }

  const uint32_t counter = taskble::activityCounter();
  if (counter != lastCounter) {
    lastCounter = counter;
    result.redraw = true;
  }
  if (PRIORITIES_STORE.revision() != lastStoreRevision) {
    lastStoreRevision = PRIORITIES_STORE.revision();
    result.listChanged = true;
  }
  return result;
}
