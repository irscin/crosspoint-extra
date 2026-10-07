#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

class PrioritiesStore;

// Wire protocol the phone app uses to edit the priorities list over BLE.
// See docs/ble-tasks.md. Transport-independent: the BLE service only moves
// framed bytes in and out of these functions.
namespace tasksync {

constexpr int PROTOCOL_VERSION = 1;
// Largest request or response, reassembled. Ten tasks with full-length ids, titles and notes fit.
constexpr size_t MAX_MESSAGE = 4096;

// One BLE write or notification carries one frame: a header byte, then payload.
// A message that fits one frame has both FIRST and LAST set.
constexpr uint8_t FRAME_FIRST = 0x01;
constexpr uint8_t FRAME_LAST = 0x02;

// Splits a message into frames of at most maxFrameBytes (header included).
std::vector<std::string> toFrames(const std::string& message, size_t maxFrameBytes);

// Rebuilds a message from frames. A frame out of sequence, or a message over
// MAX_MESSAGE, drops the partial message and reports Error; the next FIRST
// frame starts clean.
class Reassembler {
 public:
  enum class Result { NeedMore, Complete, Error };
  Result push(const uint8_t* data, size_t length);
  // Valid after Complete, until the next push.
  const std::string& message() const { return buffer; }
  void reset();

 private:
  std::string buffer;
  bool active = false;
};

// Shared secret between the device and the app. There is no OS-level pairing: the app proves it
// knows the code with its first message, and the device refuses everything until it does. The code is
// read from /.crosspoint/phone-sync-code.txt (created with a random one if missing) and shown on the
// Phone sync screen; the user types it into the app once. The link itself is not encrypted, so this
// keeps stray apps out but does not stop a determined attacker within Bluetooth range.
constexpr const char* CODE_FILE = "/.crosspoint/phone-sync-code.txt";
constexpr size_t MAX_CODE_CHARS = 32;
constexpr uint8_t MAX_BAD_ATTEMPTS = 3;

// The trimmed file content when it is a usable code (1..32 letters or digits), otherwise "".
std::string normalizeCode(const std::string& raw);
// A fresh six-digit code from a random number.
std::string codeFromRandom(uint32_t random);

// With nobody connected for this long the radio is switched off (the To-do tab shows "paused").
constexpr uint32_t IDLE_PAUSE_MS = 5u * 60u * 1000u;
constexpr bool shouldPauseIdle(const bool connected, const uint32_t idleMs, const uint32_t limitMs = IDLE_PAUSE_MS) {
  return !connected && idleMs >= limitMs;
}

// Per-connection state; build a fresh one whenever a new phone connects.
struct Session {
  std::string code;  // what the app must send; empty refuses every attempt
  bool authenticated = false;
  uint8_t badAttempts = 0;
  // True once the phone has sent too many wrong codes: the caller should hang up.
  bool shouldDrop() const { return badAttempts >= MAX_BAD_ATTEMPTS; }
};

// Runs one JSON request against the store and returns the JSON response.
// First {"cmd":"auth","code":"<Session::code>"}; until it succeeds every other command is answered
// {"ok":false,"err":"unauthorized","items":[]} and a wrong code {"err":"bad_code","items":[]}.
// Requests: {"cmd":"list"}, {"cmd":"add","title":"..","note":".."},
// {"cmd":"toggle","id":"..","done":true}, {"cmd":"delete","id":".."},
// {"cmd":"set","items":[{"id":"..","title":"..","note":"..","done":false}]} (replaces the list).
// Every response carries the current list, so the app resyncs from any reply:
//   {"v":1,"ok":true,"cap":10,"items":[{"id":..,"title":..,"note":..,"done":..}]}
//   {"v":1,"ok":false,"err":"full|not_found|bad_request|save_failed|unauthorized|bad_code", ...}
std::string handleRequest(PrioritiesStore& store, Session& session, const std::string& request);

}  // namespace tasksync
