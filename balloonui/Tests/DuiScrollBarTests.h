#pragma once

#include "../BalloonUiFeatures.h"
#if BUI_FEATURE_SCROLLBAR

#include "../Controls/Window/DuiScrollBar.h"

namespace balloonwjui {

// Self-contained unit tests for DuiScrollBar.
//
// Returns multi-line CString report; last line = summary.
namespace DuiScrollBarTests {
    CString RunAll();
}

} // namespace balloonwjui

#endif // BUI_FEATURE_SCROLLBAR
