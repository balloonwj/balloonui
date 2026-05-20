#pragma once

#include "../Controls/Layout/DuiLayout.h"

namespace balloonwjui {

// Self-contained unit test for DuiLayout (HBox/VBox/Grid).
// Runs in-process with no HWND, no Win32 message loop. Uses a tiny stub
// child control that only records the rect it was placed at.
//
// Usage from DuiGalleryDlg:
//   CString report = DuiLayoutTests::RunAll();
//   gallery.AppendLog(report);
//
// Each test returns a one-line "[ok] name" or "[FAIL] name expected=... got=...".
namespace DuiLayoutTests {

// Aggregate runner. Returns multi-line CString report; last line = summary.
CString RunAll();

} // namespace DuiLayoutTests

} // namespace balloonwjui
