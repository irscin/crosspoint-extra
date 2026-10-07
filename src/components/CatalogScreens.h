#pragma once
#include <initializer_list>

#include "UiAppHost.h"

class GfxRenderer;

// Shared FreeInkUI screen builders for the network catalog activities (the
// OPDS book browser and the SD-plugin catalogs): centered status/message
// blocks and a download-progress screen.

// One line of a centered message block; bold marks a heading line.
struct CatalogLine {
  const char* text;
  bool bold = false;
};

// Vertically centered stack of short lines in the remaining content band
// (error screens, completion notices, sign-in hints).
void catalogCenteredBlock(UiAppHost::UiScreen& screen, std::initializer_list<CatalogLine> lines);

// Centered download screen: heading, item title, download progress and, when
// cancelAction is a real action, a Cancel button. A progress bar is shown when
// the total is known (total > 0), otherwise a running byte count.
void catalogDownloadScreen(UiAppHost::UiScreen& screen, const char* status, size_t progress, size_t total,
                           freeink::ui::ActionId cancelAction = freeink::ui::NO_ACTION);
