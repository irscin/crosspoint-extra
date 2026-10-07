#include "PrioritiesStore.h"

#include <ArduinoJson.h>
#include <HalStorage.h>
#include <Logging.h>

#include <algorithm>

PrioritiesStore PRIORITIES_STORE;

namespace {
constexpr const char* PATH = "/.crosspoint/priorities.json";
constexpr size_t MAX_FILE_BYTES = 16 * 1024;

std::string clipped(const char* text, const size_t maxBytes) {
  const std::string full = text ? text : "";
  if (full.size() <= maxBytes) return full;
  // Cut before the character that straddles the limit, never inside it.
  size_t cut = maxBytes;
  while (cut > 0 && (static_cast<unsigned char>(full[cut]) & 0xC0) == 0x80) --cut;
  return full.substr(0, cut);
}
}  // namespace

bool PrioritiesStore::parse(const std::string& json) {
  JsonDocument doc;
  if (deserializeJson(doc, json) || !doc["items"].is<JsonArrayConst>()) return false;
  std::vector<Item> parsed;
  for (JsonObjectConst entry : doc["items"].as<JsonArrayConst>()) {
    if (parsed.size() >= CAPACITY) break;
    Item item;
    item.id = clipped(entry["id"] | "", MAX_ID);
    item.title = clipped(entry["title"] | "", MAX_TITLE);
    item.note = clipped(entry["note"] | "", MAX_NOTE);
    item.done = entry["done"] | false;
    if (item.title.empty()) continue;
    parsed.push_back(std::move(item));
  }
  list = std::move(parsed);
  ++revisionCounter;
  return true;
}

std::string PrioritiesStore::serialize() const {
  JsonDocument doc;
  doc["version"] = 1;
  JsonArray items = doc["items"].to<JsonArray>();
  for (const auto& item : list) {
    JsonObject entry = items.add<JsonObject>();
    entry["id"] = item.id;
    entry["title"] = item.title;
    entry["note"] = item.note;
    entry["done"] = item.done;
  }
  std::string out;
  serializeJson(doc, out);
  return out;
}

void PrioritiesStore::load() {
  list.clear();
  ++revisionCounter;
  std::string json;
  if (!Storage.readFileToString("PRIO", PATH, MAX_FILE_BYTES, json)) return;
  if (!parse(json)) LOG_ERR("PRIO", "priorities.json is not valid");
}

void PrioritiesStore::tally(int& active, int& done) const {
  active = done = 0;
  for (const auto& item : list) (item.done ? done : active)++;
}

bool PrioritiesStore::save() {
  const std::string json = serialize();
  Storage.mkdir("/.crosspoint");
  if (Storage.writeFile(PATH, String(json.c_str()))) {
    ++revisionCounter;
    return true;
  }
  LOG_ERR("PRIO", "Could not save priorities.json");
  return false;
}

int PrioritiesStore::indexOfId(const std::string& id) const {
  if (id.empty()) return -1;
  for (size_t i = 0; i < list.size(); ++i)
    if (list[i].id == id) return static_cast<int>(i);
  return -1;
}

bool PrioritiesStore::setDone(const size_t index, const bool done) {
  if (index >= list.size()) return false;
  const bool before = list[index].done;
  list[index].done = done;
  if (save()) return true;
  list[index].done = before;
  return false;
}

PrioritiesStore::EditResult PrioritiesStore::add(const std::string& title, const std::string& note) {
  const std::string cleanTitle = clipped(title.c_str(), MAX_TITLE);
  if (cleanTitle.empty()) return EditResult::Invalid;
  if (list.size() >= CAPACITY) return EditResult::Full;
  // "t<n>": one past the highest numeric id in use, so ids are never reused while a task lives.
  unsigned next = 1;
  for (const auto& item : list) {
    if (item.id.size() < 2 || item.id[0] != 't') continue;
    unsigned value = 0;
    bool numeric = true;
    for (size_t i = 1; i < item.id.size(); ++i) {
      if (item.id[i] < '0' || item.id[i] > '9' || value > 100000) {
        numeric = false;
        break;
      }
      value = value * 10 + static_cast<unsigned>(item.id[i] - '0');
    }
    if (numeric && value >= next) next = value + 1;
  }
  Item item;
  item.id = "t" + std::to_string(next);
  item.title = cleanTitle;
  item.note = clipped(note.c_str(), MAX_NOTE);
  list.push_back(std::move(item));
  if (save()) return EditResult::Ok;
  list.pop_back();
  return EditResult::SaveFailed;
}

PrioritiesStore::EditResult PrioritiesStore::setDoneById(const std::string& id, const bool done) {
  const int index = indexOfId(id);
  if (index < 0) return EditResult::NotFound;
  return setDone(static_cast<size_t>(index), done) ? EditResult::Ok : EditResult::SaveFailed;
}

PrioritiesStore::EditResult PrioritiesStore::removeById(const std::string& id) {
  const int index = indexOfId(id);
  if (index < 0) return EditResult::NotFound;
  const Item removed = list[static_cast<size_t>(index)];
  list.erase(list.begin() + index);
  if (save()) return EditResult::Ok;
  list.insert(list.begin() + index, removed);
  return EditResult::SaveFailed;
}

PrioritiesStore::EditResult PrioritiesStore::replaceAll(const std::vector<Item>& items) {
  if (items.size() > CAPACITY) return EditResult::Full;
  std::vector<Item> next;
  next.reserve(items.size());
  for (const auto& source : items) {
    Item item;
    item.title = clipped(source.title.c_str(), MAX_TITLE);
    if (item.title.empty()) return EditResult::Invalid;
    item.note = clipped(source.note.c_str(), MAX_NOTE);
    item.id = clipped(source.id.c_str(), MAX_ID);
    item.done = source.done;
    next.push_back(std::move(item));
  }
  // Ids that are blank or repeated get "t<n>", one past the highest "t<n>" now in the list.
  unsigned highest = 0;
  const auto numeric = [](const std::string& id) -> unsigned {
    if (id.size() < 2 || id[0] != 't') return 0;
    unsigned value = 0;
    for (size_t i = 1; i < id.size(); ++i) {
      if (id[i] < '0' || id[i] > '9' || value > 100000) return 0;
      value = value * 10 + static_cast<unsigned>(id[i] - '0');
    }
    return value;
  };
  for (const auto& item : next) highest = std::max(highest, numeric(item.id));
  for (size_t i = 0; i < next.size(); ++i) {
    bool repeated = next[i].id.empty();
    for (size_t j = 0; j < i && !repeated; ++j) repeated = next[j].id == next[i].id;
    if (repeated) next[i].id = "t" + std::to_string(++highest);
  }
  std::swap(list, next);
  if (save()) return EditResult::Ok;
  std::swap(list, next);
  return EditResult::SaveFailed;
}
