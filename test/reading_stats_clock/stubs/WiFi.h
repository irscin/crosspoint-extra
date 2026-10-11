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
  std::string toString() const { return "192.0.2.1"; }
};

struct WiFiClass {
  int status() const;
  int hostByName(const char*, IPAddress& out) const {
    out = IPAddress(0xC0000201u);
    return 1;
  }
};

extern WiFiClass WiFi;
