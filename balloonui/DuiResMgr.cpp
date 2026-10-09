#include "stdafx.h"
#include "DuiResMgr.h"
#include "ImageEx.h"
#include "DuiDpi.h"
#include "DuiTheme.h"   // GetDefaultFontFace：默认字体的字体名

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

namespace {

// 默认字体的磅值：标准 Windows 界面正文字号。
const int kDefaultFontPt = 9;
// 1 英寸 = 72 磅。字高按 lfHeight = -MulDiv(pt, dpi, 72) 由磅值换算为设备像素。
const int kPointsPerInch = 72;
// 缓存键中 DPI 所占的起始位：低 32 位放 (磅值 << 1) | 是否加粗，高 32 位放 DPI。
const int kDpiKeyShift = 32;

// 由 (dpi, pt, bold) 生成字体缓存键。三者任一不同即得到不同的键。
unsigned long long MakeFontKey(int dpi, int pt, bool bold)
{
    const unsigned long long low = ((unsigned long long)(unsigned int)pt << 1)
                                 | (bold ? 1ULL : 0ULL);
    return ((unsigned long long)(unsigned int)dpi << kDpiKeyShift) | low;
}

} // namespace

int DuiResMgr::EnsureDpi()
{
    if (m_dpi <= 0)
    {
        m_dpi = DuiDpi::GetSystemDpi();
    }
    return m_dpi;
}

HFONT DuiResMgr::GetCachedFont(FontCache& cache, int dpi, int pt, bool bold, BYTE quality)
{
    const unsigned long long key = MakeFontKey(dpi, pt, bold);
    FontCache::const_iterator it = cache.find(key);
    if (it != cache.end())
    {
        return it->second;
    }

    LOGFONT lf = { 0 };
    // pt -> 设备单位的负 height:-MulDiv(pt, dpi, 72)。
    lf.lfHeight = -::MulDiv(pt, dpi, kPointsPerInch);
    lf.lfWeight = bold ? FW_BOLD : FW_NORMAL;
    // 字体名取 DuiTheme 的默认字体（缺省为微软雅黑，宿主可按界面语言改换）。字符集：微软雅黑沿用 GB2312，
    // 与改动之前一致；其它字体（如 Segoe UI、Yu Gothic UI）不支持 GB2312，指定它会让系统换用别的字体，改用 DEFAULT
    const CString face = DuiTheme::Inst().GetDefaultFontFace();
    lf.lfCharSet = (face.CompareNoCase(_T("Microsoft YaHei")) == 0) ? GB2312_CHARSET : DEFAULT_CHARSET;
    lf.lfQuality = quality;
    lf.lfPitchAndFamily = DEFAULT_PITCH | FF_SWISS;
    _tcsncpy_s(lf.lfFaceName, face, _TRUNCATE);
    HFONT hf = ::CreateFontIndirect(&lf);
    if (!hf)
    {
        // YaHei 未装的兜底:用 SimSun 再试一次。
        _tcsncpy_s(lf.lfFaceName, _T("SimSun"), _TRUNCATE);
        hf = ::CreateFontIndirect(&lf);
    }
    if (hf)
    {
        cache[key] = hf;
    }
    return hf;
}

void DuiResMgr::ReleaseCache(FontCache& cache)
{
    for (FontCache::iterator it = cache.begin(); it != cache.end(); ++it)
    {
        if (it->second)
        {
            ::DeleteObject(it->second);
        }
    }
    cache.clear();
}

int DuiResMgr::ResolveDpi(int dpi)
{
    return (dpi > 0) ? dpi : EnsureDpi();
}

HFONT DuiResMgr::GetDefaultFont()
{
    return GetDefaultFontForDpi(EnsureDpi());
}

HFONT DuiResMgr::GetFontByPointSize(int pt, bool bold)
{
    return GetFontByPointSizeForDpi(pt, bold, EnsureDpi());
}

HFONT DuiResMgr::GetAntiAliasedFontByPointSize(int pt, bool bold)
{
    return GetAntiAliasedFontByPointSizeForDpi(pt, bold, EnsureDpi());
}

HFONT DuiResMgr::GetDefaultFontForDpi(int dpi)
{
    // 9pt 正文字号，与标准 Windows 界面一致；所有 DUI 控件、菜单、提示条
    // 未单独指定字体时都用它。
    return GetCachedFont(m_defaultFontCache, ResolveDpi(dpi), kDefaultFontPt, false,
                         CLEARTYPE_QUALITY);
}

HFONT DuiResMgr::GetFontByPointSizeForDpi(int pt, bool bold, int dpi)
{
    // 退化:磅值非法时回退到默认字体(避免 caller 检查负值)。
    if (pt <= 0)
    {
        return GetDefaultFontForDpi(dpi);
    }
    return GetCachedFont(m_fontCache, ResolveDpi(dpi), pt, bold, CLEARTYPE_QUALITY);
}

HFONT DuiResMgr::GetAntiAliasedFontByPointSizeForDpi(int pt, bool bold, int dpi)
{
    if (pt <= 0)
    {
        return GetDefaultFontForDpi(dpi);   // 默认字体已是 CLEARTYPE_QUALITY, 但
                                            // pt<=0 是"用默认"语义, 不必返 AA。
    }
    // 与 GetFontByPointSizeForDpi 唯一差别是 ANTIALIASED_QUALITY。
    return GetCachedFont(m_aaFontCache, ResolveDpi(dpi), pt, bold, ANTIALIASED_QUALITY);
}

void DuiResMgr::SetDpi(int dpi)
{
    if (dpi <= 0)
    {
        dpi = DuiDpi::kDefaultDpi;
    }
    // 只切换当前 DPI，已创建的字体一律保留：控件可能仍保存着它们的句柄，在这里
    // DeleteObject 会让这些控件随后用已失效的句柄绘制。各 DPI 的字体分别缓存，
    // 切回用过的 DPI 时直接复用。
    m_dpi = dpi;
}

DuiResMgr::~DuiResMgr()
{
    ReleaseCache(m_defaultFontCache);
    ReleaseCache(m_fontCache);
    ReleaseCache(m_aaFontCache);
}

} // namespace balloonwjui
