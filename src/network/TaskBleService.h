#pragma once

#include <cstdint>
#include <string>

// BLE GATT peripheral the phone app uses to edit the priorities list while the
// device shows "Phone sync". Only compiled into builds with a NimBLE that has
// the peripheral role (FREEINK_CAP_BLE_TASKS); other builds link the inline
// stubs below, so callers need no #if.
//
// No OS-level pairing: the link is plain GATT and the app authenticates with the shared code in
// its first message (util/TaskSync.h, Session). Messages are tasksync frames (util/TaskSync.h) written to RX and notified on
// TX. The BLE host task only queues bytes; requests run on the main loop.
namespace taskble {

#if defined(FREEINK_CAP_BLE_TASKS) && FREEINK_CAP_BLE_TASKS

constexpr const char* SERVICE_UUID = "6f1a0001-5c7e-4b8a-9d3e-2a4c8f1b7e01";
constexpr const char* RX_UUID = "6f1a0002-5c7e-4b8a-9d3e-2a4c8f1b7e01";  // phone -> device, write
constexpr const char* TX_UUID = "6f1a0003-5c7e-4b8a-9d3e-2a4c8f1b7e01";  // device -> phone, notify

// Heap the radio needs, same floor as the page-turner start (BleHeapRestart.h).
bool hasHeapForStart();
// Bring the stack up and advertise. False when the radio or heap refuses.
bool start(const char* deviceName);
void stop();
bool running();
bool connected();
// Bumped on every new connection, so the caller can reset its per-connection session.
uint32_t connectionId();
// Address of the connected (or last connected) peer, "" if none. Shown on screen.
std::string peerAddress();
// Hangs up the current peer; advertising restarts so another phone can connect.
void dropPeer();
// NimBLE/HCI reason of the last disconnect, 0 if none yet. Shown on screen to debug pairing.
int lastDisconnectReason();
// Next complete request from the phone, if any. Main loop only.
bool popRequest(std::string& out);
// Frames a response onto the TX characteristic. False when nobody is connected.
bool sendResponse(const std::string& message);
// Bumped on connect/disconnect and each request, so the screen knows to redraw.
uint32_t activityCounter();

#else

inline bool hasHeapForStart() { return false; }
inline bool start(const char*) { return false; }
inline void stop() {}
inline bool running() { return false; }
inline bool connected() { return false; }
inline uint32_t connectionId() { return 0; }
inline std::string peerAddress() { return {}; }
inline void dropPeer() {}
inline int lastDisconnectReason() { return 0; }
inline bool popRequest(std::string&) { return false; }
inline bool sendResponse(const std::string&) { return false; }
inline uint32_t activityCounter() { return 0; }

#endif

}  // namespace taskble
