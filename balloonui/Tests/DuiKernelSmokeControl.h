#pragma once

#include "../DuiControl.h"

namespace balloonwjui {

// Smoke-test control for Phase 1 kernel validation. Not a production widget.
//
// Behavior:
//   - Paints a colored rectangle. Color cycles per state (normal/hover/down/focus).
//   - On left click: bubbles DUIN_CLICK to parent (default DuiControl behavior).
//   - On left double click: bubbles DUIN_DBLCLK.
//   - Captures mouse on LButtonDown, releases on LButtonUp - used to verify
//     the host's DUI capture wiring forwards mouse events outside the rect.
//   - Tab-stop = true so we can verify focus traversal.
//
// DuiGalleryDlg places three of these side-by-side and listens for
// WM_DUI_NOTIFY to log click/focus/hover events.
class DuiKernelSmokeControl : public DuiControl
{
public:
    DuiKernelSmokeControl();

    void SetLabel(LPCTSTR sz) { m_label = sz; Invalidate(); }
    int  GetClickCount() const { return m_clickCount; }

    // DuiControl
    void OnPaint(HDC hdc, const RECT& rcDirty) override;
    bool OnLButtonDown (POINT pt, UINT mkFlags) override;
    bool OnLButtonUp   (POINT pt, UINT mkFlags) override;
    bool OnSetCursor   (POINT pt) override;

private:
    CString m_label;
    int     m_clickCount = 0;
};

} // namespace balloonwjui
