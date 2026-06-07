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

HFONT DuiResMgr::GetFontByPointSize(int pt, bool bold)
{
    // 退化:磅值非法时回退到默认字体(避免 caller 检查负值)。
    if (pt <= 0)
    {
        return GetDefaultFont();
    }
    if (m_dpi <= 0)
    {
        m_dpi = DuiDpi::GetSystemDpi();
    }

    // 缓存 key:磅值左移 1 位 + 粗细标志, 与 admin ui::UiFont 同方案。
    const int key = (pt << 1) | (bold ? 1 : 0);

    auto it = m_fontCache.find(key);
    if (it != m_fontCache.end())
    {
        return it->second;
    }

    LOGFONT lf = { 0 };
    // pt -> 设备单位的负 height:-MulDiv(pt, dpi, 72)。
    lf.lfHeight = -::MulDiv(pt, m_dpi, 72);
    lf.lfWeight = bold ? FW_BOLD : FW_NORMAL;
    lf.lfCharSet = GB2312_CHARSET;
    lf.lfQuality = CLEARTYPE_QUALITY;
    lf.lfPitchAndFamily = DEFAULT_PITCH | FF_SWISS;
    _tcsncpy_s(lf.lfFaceName, _T("Microsoft YaHei"), _TRUNCATE);
    HFONT hf = ::CreateFontIndirect(&lf);
    if (!hf)
    {
        // YaHei 未装的兜底:用 SimSun 再试一次。
        _tcsncpy_s(lf.lfFaceName, _T("SimSun"), _TRUNCATE);
        hf = ::CreateFontIndirect(&lf);
    }
    if (hf)
    {
        m_fontCache[key] = hf;
    }
    return hf;
}

HFONT DuiResMgr::GetAntiAliasedFontByPointSize(int pt, bool bold)
{
    if (pt <= 0)
    {
        return GetDefaultFont();    // 默认字体已是 CLEARTYPE_QUALITY, 但
                                    // pt<=0 是"用默认"语义, 不必返 AA。
    }
    if (m_dpi <= 0)
    {
        m_dpi = DuiDpi::GetSystemDpi();
    }

    const int key = (pt << 1) | (bold ? 1 : 0);
    auto it = m_aaFontCache.find(key);
    if (it != m_aaFontCache.end())
    {
        return it->second;
    }

    LOGFONT lf = { 0 };
    lf.lfHeight = -::MulDiv(pt, m_dpi, 72);
    lf.lfWeight = bold ? FW_BOLD : FW_NORMAL;
    lf.lfCharSet = GB2312_CHARSET;
    lf.lfQuality = ANTIALIASED_QUALITY;      // 与 GetFontByPointSize 唯一差别
    lf.lfPitchAndFamily = DEFAULT_PITCH | FF_SWISS;
    _tcsncpy_s(lf.lfFaceName, _T("Microsoft YaHei"), _TRUNCATE);
    HFONT hf = ::CreateFontIndirect(&lf);
    if (!hf)
    {
        _tcsncpy_s(lf.lfFaceName, _T("SimSun"), _TRUNCATE);
        hf = ::CreateFontIndirect(&lf);
    }
    if (hf)
    {
        m_aaFontCache[key] = hf;
    }
    return hf;
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
    // (pt, bold) 缓存也整张清空,惰性重建。
    for (auto& kv : m_fontCache)
    {
        if (kv.second)
        {
            ::DeleteObject(kv.second);
        }
    }
    m_fontCache.clear();
    for (auto& kv : m_aaFontCache)
    {
        if (kv.second)
        {
            ::DeleteObject(kv.second);
        }
    }
    m_aaFontCache.clear();
    // Lazy rebuild on next GetDefaultFont() / GetFontByPointSize() so we
    // don't pay for the CreateFontIndirect on a DPI change that no caller
    // cares about.
}

DuiResMgr::~DuiResMgr()
{
    if (m_hDefaultFont)
    {
        ::DeleteObject(m_hDefaultFont);
        m_hDefaultFont = nullptr;
    }
    for (auto& kv : m_fontCache)
    {
        if (kv.second)
        {
            ::DeleteObject(kv.second);
        }
    }
    m_fontCache.clear();
    for (auto& kv : m_aaFontCache)
    {
        if (kv.second)
        {
            ::DeleteObject(kv.second);
        }
    }
    m_aaFontCache.clear();
}

} // namespace balloonwjui
