#include "stdafx.h"
#include "DuiResMgr.h"
#include "ImageEx.h"
#include "DuiDpi.h"

namespace balloonwjui {

DuiResMgr& DuiResMgr::Inst()
{
    static DuiResMgr s_inst;
    return s_inst;
}

CImageEx* DuiResMgr::LoadImage(LPCTSTR lpszFileName)
{
    CSkinManager* m = CSkinManager::GetInstance();
    if (!m || !lpszFileName)
    {
        return nullptr;
    }
    if (!m->LoadImage(lpszFileName))
    {
        return nullptr;
    }
    return m->GetImage(lpszFileName);
}

CImageEx* DuiResMgr::GetImage(LPCTSTR lpszFileName)
{
    CSkinManager* m = CSkinManager::GetInstance();
    if (!m || !lpszFileName)
    {
        return nullptr;
    }
    return m->GetImage(lpszFileName);
}

void DuiResMgr::ReleaseImage(CImageEx*& lpImg)
{
    CSkinManager* m = CSkinManager::GetInstance();
    if (m)
    {
        m->ReleaseImage(lpImg);
    }
    else
    {
        lpImg = nullptr;
    }
}

CImageEx* DuiResMgr::AcquireImage(LPCTSTR lpszFileName)
{
    if (CImageEx* img = GetImage(lpszFileName))
    {
        return img;
    }
    return LoadImage(lpszFileName);
}

HFONT DuiResMgr::GetDefaultFont()
{
    if (m_hDefaultFont)
    {
        return m_hDefaultFont;
    }
    if (m_dpi <= 0)
    {
        m_dpi = DuiDpi::GetSystemDpi();
    }
    LOGFONT lf = { 0 };
    // 9pt -> negative height in device units. -MulDiv(9, dpi, 72).
    // Matches the standard Windows UI body size; auto-applied to every
    // DUI control + menu + tooltip because they all go through this.
    lf.lfHeight = -::MulDiv(9, m_dpi, 72);
    lf.lfWeight = FW_NORMAL;
    lf.lfCharSet = GB2312_CHARSET;
    lf.lfQuality = CLEARTYPE_QUALITY;
    lf.lfPitchAndFamily = DEFAULT_PITCH | FF_SWISS;
    _tcsncpy_s(lf.lfFaceName, _T("Microsoft YaHei"), _TRUNCATE);
    m_hDefaultFont = ::CreateFontIndirect(&lf);
    if (!m_hDefaultFont)
    {
        // Fallback if YaHei isn't installed - shouldn't happen on Win7+.
        _tcsncpy_s(lf.lfFaceName, _T("SimSun"), _TRUNCATE);
        m_hDefaultFont = ::CreateFontIndirect(&lf);
    }
    return m_hDefaultFont;
}

void DuiResMgr::SetDpi(int dpi)
{
    if (dpi <= 0)
    {
        dpi = DuiDpi::kDefaultDpi;
    }
    if (m_dpi == dpi)
    {
        return;
    }
    m_dpi = dpi;
    if (m_hDefaultFont)
    {
        ::DeleteObject(m_hDefaultFont);
        m_hDefaultFont = nullptr;
    }
    // Lazy rebuild on next GetDefaultFont() so we don't pay for the
    // CreateFontIndirect on a DPI change that no caller cares about.
}

DuiResMgr::~DuiResMgr()
{
    if (m_hDefaultFont)
    {
        ::DeleteObject(m_hDefaultFont);
        m_hDefaultFont = nullptr;
    }
}

} // namespace balloonwjui
