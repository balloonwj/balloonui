#pragma once

#include "../DuiNinePatch.h"

namespace balloonwjui {

// Self-contained unit test for DuiNinePatch.
//
// Two flavors of test live here:
//   * Pure math: ClampInsets / ComputeCells, no DC, no bitmap.
//   * Smoke paint: builds a tiny 9x9 32bpp DIBSection in-process, paints
//     it through DuiNinePatch::Draw onto a memory DC, then samples a
//     handful of pixels to confirm the four corners landed in the right
//     destination corners.
//
// Runs in-process with no HWND. Use from DuiGalleryDlg::OnCreate.
namespace DuiNinePatchTests {

CString RunAll();

} // namespace DuiNinePatchTests

} // namespace balloonwjui
