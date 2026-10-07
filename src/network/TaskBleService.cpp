#include "TaskBleService.h"

#if defined(FREEINK_CAP_BLE_TASKS) && FREEINK_CAP_BLE_TASKS

#include <Arduino.h>
#include <Logging.h>
#include <NimBLEDevice.h>

#include <atomic>
#include <deque>
#include <mutex>

#include "BleHeapRestart.h"
#include "util/TaskSync.h"

namespace taskble {
namespace {

constexpr uint16_t PREFERRED_MTU = 185;
constexpr size_t MAX_QUEUED_REQUESTS = 4;
constexpr uint16_t DEFAULT_ATT_PAYLOAD = 20;  // MTU 23 minus the 3-byte ATT header

NimBLEServer* server = nullptr;
NimBLECharacteristic* txChar = nullptr;
bool isRunning = false;
std::atomic<bool> isConnected{false};
std::atomic<uint32_t> connectionCounter{0};
std::atomic<int> disconnectReason{0};
std::atomic<uint16_t> framePayload{DEFAULT_ATT_PAYLOAD};
std::atomic<uint32_t> counter{0};

std::mutex queueMutex;
std::string peerAddr;  // guarded by queueMutex
tasksync::Reassembler reassembler;
std::deque<std::string> requests;

class ServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer*, NimBLEConnInfo& info) override {
    isConnected.store(true);
    connectionCounter.fetch_add(1);
    framePayload.store(DEFAULT_ATT_PAYLOAD);
    {
      std::lock_guard<std::mutex> lock(queueMutex);
      reassembler.reset();
      requests.clear();
      peerAddr = info.getAddress().toString();
    }
    counter.fetch_add(1);
  }
  void onDisconnect(NimBLEServer*, NimBLEConnInfo&, int reason) override {
    isConnected.store(false);
    disconnectReason.store(reason);
    counter.fetch_add(1);
    // Keep advertising so the phone can reconnect without reopening the screen.
    NimBLEDevice::startAdvertising();
  }
  void onMTUChange(uint16_t mtu, NimBLEConnInfo&) override {
    framePayload.store(mtu > 3 ? static_cast<uint16_t>(mtu - 3) : DEFAULT_ATT_PAYLOAD);
  }
};

class RxCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* characteristic, NimBLEConnInfo&) override {
    const NimBLEAttValue value = characteristic->getValue();
    std::lock_guard<std::mutex> lock(queueMutex);
    const auto result = reassembler.push(value.data(), value.size());
    if (result == tasksync::Reassembler::Result::Complete && requests.size() < MAX_QUEUED_REQUESTS) {
      requests.push_back(reassembler.message());
      counter.fetch_add(1);
    }
  }
};

ServerCallbacks serverCallbacks;
RxCallbacks rxCallbacks;

}  // namespace

bool hasHeapForStart() {
  return ESP.getFreeHeap() >= bleheap::kMinimumFreeBytes && ESP.getMaxAllocHeap() >= bleheap::kMinimumLargestBlockBytes;
}

bool start(const char* deviceName) {
  if (isRunning) return true;
  if (NimBLEDevice::isInitialized()) NimBLEDevice::deinit(true);
  if (!NimBLEDevice::init(deviceName ? deviceName : "CrossPoint")) {
    LOG_ERR("TASKBLE", "NimBLEDevice::init failed");
    return false;
  }
  NimBLEDevice::setMTU(PREFERRED_MTU);

  server = NimBLEDevice::createServer();
  server->setCallbacks(&serverCallbacks, false);
  NimBLEService* service = server->createService(SERVICE_UUID);
  NimBLECharacteristic* rx = service->createCharacteristic(
      RX_UUID, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR,
      tasksync::MAX_MESSAGE / 8);
  rx->setCallbacks(&rxCallbacks);
  txChar = service->createCharacteristic(TX_UUID, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY, 512);

  NimBLEAdvertising* advertising = NimBLEDevice::getAdvertising();
  advertising->addServiceUUID(SERVICE_UUID);
  advertising->enableScanResponse(true);
  advertising->setName(deviceName ? deviceName : "CrossPoint");
  if (!advertising->start()) {
    LOG_ERR("TASKBLE", "advertising failed");
    NimBLEDevice::deinit(true);
    server = nullptr;
    txChar = nullptr;
    return false;
  }
  isRunning = true;
  isConnected.store(false);
  counter.fetch_add(1);
  return true;
}

void stop() {
  if (!isRunning && !NimBLEDevice::isInitialized()) return;
  NimBLEDevice::deinit(true);
  server = nullptr;
  txChar = nullptr;
  isRunning = false;
  isConnected.store(false);
  std::lock_guard<std::mutex> lock(queueMutex);
  reassembler.reset();
  requests.clear();
}

bool running() { return isRunning; }
bool connected() { return isConnected.load(); }
uint32_t connectionId() { return connectionCounter.load(); }
int lastDisconnectReason() { return disconnectReason.load(); }

std::string peerAddress() {
  std::lock_guard<std::mutex> lock(queueMutex);
  return peerAddr;
}

void dropPeer() {
  if (!server) return;
  for (const uint16_t handle : server->getPeerDevices()) server->disconnect(handle);
}
uint32_t activityCounter() { return counter.load(); }

bool popRequest(std::string& out) {
  std::lock_guard<std::mutex> lock(queueMutex);
  if (requests.empty()) return false;
  out = std::move(requests.front());
  requests.pop_front();
  return true;
}

bool sendResponse(const std::string& message) {
  if (!isRunning || !txChar || !isConnected.load()) return false;
  const auto frames = tasksync::toFrames(message, framePayload.load());
  for (const auto& frame : frames) {
    // A busy controller refuses a notify; give it a few ticks rather than dropping the frame.
    bool sent = false;
    for (int attempt = 0; attempt < 50 && !sent; ++attempt) {
      sent = txChar->notify(reinterpret_cast<const uint8_t*>(frame.data()), frame.size());
      if (!sent) delay(10);
    }
    if (!sent) return false;
  }
  return true;
}

}  // namespace taskble

#endif  // FREEINK_CAP_BLE_TASKS
