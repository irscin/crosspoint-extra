#pragma once

#include <algorithm>
#include <string>
#include <vector>

// Where a wake from sleep lands when "Wake into the book" wants the book (wantsBook): the book last opened, if it
// is still in the Recent list and still on the card. A book taken off Recent, by hand or when
// finished, is left alone by the next wake. Empty means Home.
namespace wakebook {

// Whether the wake wants the book at all: always ("On"), or for "Where I left off" only when sleep
// began in the reader (bookToOpen still checks it is in Recent and on the card).
inline bool wantsBook(const bool always, const bool whereLeftOff, const bool sleptInReader) {
  return always || (whereLeftOff && sleptInReader);
}

template <typename Book, typename Exists>
std::string bookToOpen(const bool sleepWake, const bool settingOn, const std::string& lastBook,
                       const std::vector<Book>& recents, Exists&& onCard) {
  if (!sleepWake || !settingOn || lastBook.empty()) return {};
  const bool inRecents =
      std::any_of(recents.begin(), recents.end(), [&](const Book& b) { return b.path == lastBook; });
  if (!inRecents || !onCard(lastBook)) return {};
  return lastBook;
}

}  // namespace wakebook
