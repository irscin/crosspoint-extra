#include "TaskSync.h"

#include <ArduinoJson.h>

#include <algorithm>
#include <cctype>
#include <cstdio>

#include "PrioritiesStore.h"

namespace tasksync {

std::vector<std::string> toFrames(const std::string& message, const size_t maxFrameBytes) {
  std::vector<std::string> frames;
  if (maxFrameBytes < 2) return frames;
  const size_t payload = maxFrameBytes - 1;
  size_t offset = 0;
  do {
    const size_t take = std::min(payload, message.size() - offset);
    uint8_t header = 0;
    if (offset == 0) header |= FRAME_FIRST;
    if (offset + take >= message.size()) header |= FRAME_LAST;
    std::string frame(1, static_cast<char>(header));
    frame.append(message, offset, take);
    frames.push_back(std::move(frame));
    offset += take;
  } while (offset < message.size());
  return frames;
}

void Reassembler::reset() {
  buffer.clear();
  active = false;
}

Reassembler::Result Reassembler::push(const uint8_t* data, const size_t length) {
  if (!data || length == 0) return Result::Error;
  const uint8_t header = data[0];
  if (header & FRAME_FIRST) {
    buffer.clear();
    active = true;
  } else if (!active) {
    return Result::Error;  // continuation with no start: ignore until the next FIRST
  }
  if (buffer.size() + (length - 1) > MAX_MESSAGE) {
    reset();
    return Result::Error;
  }
  buffer.append(reinterpret_cast<const char*>(data + 1), length - 1);
  if (header & FRAME_LAST) {
    active = false;
    return Result::Complete;
  }
  return Result::NeedMore;
}

namespace {

void writeSnapshot(JsonDocument& doc, const PrioritiesStore& store) {
  doc["cap"] = PrioritiesStore::CAPACITY;
  JsonArray items = doc["items"].to<JsonArray>();
  for (const auto& item : store.items()) {
    JsonObject entry = items.add<JsonObject>();
    entry["id"] = item.id;
    entry["title"] = item.title;
    entry["note"] = item.note;
    entry["done"] = item.done;
  }
}

const char* errorName(const PrioritiesStore::EditResult result) {
  switch (result) {
    case PrioritiesStore::EditResult::Full:
      return "full";
    case PrioritiesStore::EditResult::NotFound:
      return "not_found";
    case PrioritiesStore::EditResult::SaveFailed:
      return "save_failed";
    default:
      return "bad_request";
  }
}

}  // namespace

std::string normalizeCode(const std::string& raw) {
  size_t begin = 0, end = raw.size();
  while (begin < end && std::isspace(static_cast<unsigned char>(raw[begin]))) ++begin;
  while (end > begin && std::isspace(static_cast<unsigned char>(raw[end - 1]))) --end;
  const std::string code = raw.substr(begin, end - begin);
  if (code.empty() || code.size() > MAX_CODE_CHARS) return {};
  for (const char c : code)
    if (!std::isalnum(static_cast<unsigned char>(c))) return {};
  return code;
}

std::string codeFromRandom(const uint32_t random) {
  char text[8];
  std::snprintf(text, sizeof(text), "%06u", static_cast<unsigned>(random % 1000000u));
  return text;
}

namespace {

std::string respondDenied(const char* error) {
  JsonDocument doc;
  doc["v"] = PROTOCOL_VERSION;
  doc["ok"] = false;
  doc["err"] = error;
  doc["cap"] = PrioritiesStore::CAPACITY;
  doc["items"].to<JsonArray>();  // never leak the list to an unauthenticated peer
  std::string out;
  serializeJson(doc, out);
  return out;
}

std::string respond(const PrioritiesStore& store, const char* error) {
  JsonDocument doc;
  doc["v"] = PROTOCOL_VERSION;
  doc["ok"] = error == nullptr;
  if (error) doc["err"] = error;
  writeSnapshot(doc, store);
  std::string out;
  serializeJson(doc, out);
  return out;
}

}  // namespace

std::string handleRequest(PrioritiesStore& store, Session& session, const std::string& request) {
  JsonDocument doc;
  if (deserializeJson(doc, request)) {
    return session.authenticated ? respond(store, "bad_request") : respondDenied("unauthorized");
  }
  const std::string cmd = doc["cmd"] | "";
  if (cmd == "auth") {
    if (session.authenticated) return respond(store, nullptr);
    if (session.shouldDrop() || session.code.empty() || std::string(doc["code"] | "") != session.code) {
      if (session.badAttempts < MAX_BAD_ATTEMPTS) ++session.badAttempts;
      return respondDenied("bad_code");
    }
    session.authenticated = true;
    return respond(store, nullptr);
  }
  if (!session.authenticated) return respondDenied("unauthorized");
  PrioritiesStore::EditResult result = PrioritiesStore::EditResult::Invalid;
  if (cmd == "list") {
    return respond(store, nullptr);
  } else if (cmd == "add") {
    result = store.add(doc["title"] | "", doc["note"] | "");
  } else if (cmd == "set") {
    if (!doc["items"].is<JsonArrayConst>()) return respond(store, "bad_request");
    std::vector<PrioritiesStore::Item> items;
    for (JsonObjectConst entry : doc["items"].as<JsonArrayConst>()) {
      PrioritiesStore::Item item;
      item.id = entry["id"] | "";
      item.title = entry["title"] | "";
      item.note = entry["note"] | "";
      item.done = entry["done"] | false;
      items.push_back(std::move(item));
    }
    result = store.replaceAll(items);
  } else if (cmd == "toggle") {
    if (!doc["done"].is<bool>()) return respond(store, "bad_request");
    result = store.setDoneById(doc["id"] | "", doc["done"].as<bool>());
  } else if (cmd == "delete") {
    result = store.removeById(doc["id"] | "");
  }
  return respond(store, result == PrioritiesStore::EditResult::Ok ? nullptr : errorName(result));
}

}  // namespace tasksync
