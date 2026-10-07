#pragma once

#include <cstddef>
#include <string>
#include <vector>

// Today's priorities: a short checklist read from /.crosspoint/priorities.json.
//
// Item fields and limits mirror Flowe's priorities snapshot (id, title, note,
// done; up to 10 items) so a phone sync can later fill the same list. Until
// then the file is the only source: items are edited on the SD card, and the
// device only flips `done`, writing the file back.
//
//   {"version":1,"items":[{"id":"a1","title":"Write report","note":"","done":false}]}
class PrioritiesStore {
 public:
  static constexpr size_t CAPACITY = 10;
  static constexpr size_t MAX_ID = 64;
  static constexpr size_t MAX_TITLE = 96;
  static constexpr size_t MAX_NOTE = 120;

  struct Item {
    std::string id, title, note;
    bool done = false;
  };

  // Re-read the file; a missing or unreadable file leaves an empty list.
  void load();
  const std::vector<Item>& items() const { return list; }
  // Bumped on every successful load, parse or edit, so a screen can tell the list changed.
  uint32_t revision() const { return revisionCounter; }
  void tally(int& active, int& done) const;
  // Flip one item and persist; reverts the flip and returns false if the write fails.
  bool setDone(size_t index, bool done);

  // Edits made by the phone (BLE task sync). Each persists and rolls back on a failed write.
  enum class EditResult { Ok, Full, NotFound, Invalid, SaveFailed };
  // Appends a task with a fresh id ("t<n>"). Title is required; both fields are clipped.
  EditResult add(const std::string& title, const std::string& note);
  EditResult setDoneById(const std::string& id, bool done);
  EditResult removeById(const std::string& id);
  // Replaces the whole list, in the given order (the app's "Send to Device"). Items keep their
  // id when it is non-empty and unique, otherwise get a fresh "t<n>". Nothing changes unless the
  // whole list is valid: at most CAPACITY items, every title non-blank.
  EditResult replaceAll(const std::vector<Item>& items);

  // JSON round trip, split from file access so it can be tested on the host.
  bool parse(const std::string& json);
  std::string serialize() const;

 private:
  uint32_t revisionCounter = 0;
  bool save();
  int indexOfId(const std::string& id) const;
  std::vector<Item> list;
};

extern PrioritiesStore PRIORITIES_STORE;
