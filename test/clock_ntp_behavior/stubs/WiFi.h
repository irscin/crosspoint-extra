#pragma once
#include <cstdint>
#include <string>
constexpr int WL_CONNECTED = 3;
// Enough of Arduino's IPAddress for HalClock's NTP server lookup.
struct IPAddress {
  uint32_t value = 0;
  IPAddress() = default;
  explicit IPAddress(uint32_t v) : value(v) {}
  bool operator!=(const IPAddress& other) const { return value != other.value; }
  std::string toString() const {
    return std::to_string(value >> 24) + "." + std::to_string((value >> 16) & 255) + "." +
           std::to_string((value >> 8) & 255) + "." + std::to_string(value & 255);
  }
};
struct WiFiClass {
  int status() const;
  int hostByName(const char* host, IPAddress& out) const;
};
extern WiFiClass WiFi;
