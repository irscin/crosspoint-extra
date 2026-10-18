#pragma once

#include <I18nKeys.h>

#include <cstdint>
#include <iterator>
#include <vector>

// Quick Actions: up to five user-picked actions, listed in a menu that a long press of Back opens
// (Home and the EPUB reader). A slot stores the action's index in LABELS, so a new action goes last.
namespace quickactions {

enum Id : uint8_t {
  NONE = 0,
  SLEEP,
  REFRESH,
  NEXT_PAGE,
  PREV_PAGE,
  FOOTNOTES,
  BOOKMARK,
  SYNC_PROGRESS,
  FILE_TRANSFER,
  BROWSE_FILES,
  CALIBRE,
  JOIN_NETWORK,
  HOTSPOT,
  LOOKUP,
  CLIPPING,
  FRONTLIGHT,
  TILT_PAGE_TURN,
  COUNT
};

inline constexpr uint8_t SLOT_COUNT = 5;

inline constexpr StrId LABELS[] = {
    StrId::STR_NONE_OPT,        StrId::STR_SLEEP,         StrId::STR_FORCE_REFRESH,  StrId::STR_NEXT_PAGE,
    StrId::STR_PREV_PAGE,       StrId::STR_FOOTNOTES,     StrId::STR_TOGGLE_BOOKMARK, StrId::STR_SYNC_PROGRESS,
    StrId::STR_FILE_TRANSFER,   StrId::STR_BROWSE_FILES,  StrId::STR_CALIBRE_WIRELESS, StrId::STR_JOIN_NETWORK,
    StrId::STR_CREATE_HOTSPOT,  StrId::STR_LOOKUP,        StrId::STR_QUOTES_SAVE_ACTION,
    StrId::STR_TOGGLE_FRONTLIGHT, StrId::STR_TILT_PAGE_TURN};
static_assert(std::size(LABELS) == COUNT, "one label per quick action");

// These act on the open book, so the menu offers them only inside an EPUB.
constexpr bool readerOnly(const uint8_t id) {
  switch (id) {
    case NEXT_PAGE:
    case PREV_PAGE:
    case FOOTNOTES:
    case BOOKMARK:
    case SYNC_PROGRESS:
    case LOOKUP:
    case CLIPPING:
    case TILT_PAGE_TURN:
      return true;
    default:
      return false;
  }
}

// True when at least one slot holds an action.
bool anyConfigured();

// The configured actions in slot order, without repeats, minus those this screen or board cannot run.
std::vector<uint8_t> configured(bool inReader);

// Set by the menu when the user picks an action; main takes it once the menu has closed.
extern uint8_t pending;

// Runs one action on the screen in front. Defined in main.cpp.
void run(uint8_t id);

}  // namespace quickactions
