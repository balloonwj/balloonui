#pragma once

#include <atlstr.h>

namespace CaptureMode {

// Runs DuiGallery in headless screenshot mode. Iterates every page from
// Pages::GetPages(), forces layout + paint into the host back buffer,
// then for each registered Pages::CaptureMark BitBlt's the marked
// control's rect into a 32bpp DIB and saves as
// <outDir>\ctl-<mark-name>.png via GDI+.
//
// Window is created with WS_DISABLED, off-screen at (-32000, -32000),
// so capture does not steal focus or flash the screen. Caller must have
// already called OleInitialize / _Module.Init / DuiDpi::OptInPerMonitorV2.
//
// Returns the number of PNGs successfully written, or -1 on fatal error
// (couldn't create window / output dir not writable).
int RunCaptureAll(LPCTSTR outDir);

} // namespace CaptureMode
