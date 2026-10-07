#pragma once
// Host stand-in for the SD API PrioritiesStore.cpp touches; the parse/serialize
// logic under test never reaches it.
#include <cstddef>
#include <string>

struct String {
  String(const char* text = "") : value(text) {}
  std::string value;
};

struct HalStorageStub {
  bool failWrites = false;
  std::string file;
  bool readFileToString(const char*, const char*, size_t cap, std::string& out) {
    out = file;
    return !file.empty() && file.size() <= cap;
  }
  bool mkdir(const char*) { return true; }
  bool writeFile(const char*, const String& content) {
    if (failWrites) return false;
    file = content.value;
    return true;
  }
};
inline HalStorageStub Storage;
