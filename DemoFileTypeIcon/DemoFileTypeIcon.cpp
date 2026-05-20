#include "stdafx.h"
#include "DemoFileTypeIcon.h"
#include "DuiNotify.h"

using namespace balloonwjui;

namespace {

// 单一来源色板：每个常见后缀的 (bg, fg, fold) 三元组。
// fg = 文字色，bg = 大背景，fold = 折角阴影色（比 bg 略深）。
// 目录写一遍，全部应用。
struct PaletteEntry { LPCTSTR ext; COLORREF bg; COLORREF fg; COLORREF fold; };
const PaletteEntry kPalettes[] = {
    // 文档
    { _T("pdf"),  RGB(232,  90,  90), RGB(255, 255, 255), RGB(180,  60,  60) },
    { _T("doc"),  RGB( 60, 130, 220), RGB(255, 255, 255), RGB( 30, 100, 180) },
    { _T("docx"), RGB( 60, 130, 220), RGB(255, 255, 255), RGB( 30, 100, 180) },
    { _T("xls"),  RGB( 50, 160, 110), RGB(255, 255, 255), RGB( 30, 130,  90) },
    { _T("xlsx"), RGB( 50, 160, 110), RGB(255, 255, 255), RGB( 30, 130,  90) },
    { _T("ppt"),  RGB(220, 130,  60), RGB(255, 255, 255), RGB(180, 100,  40) },
    { _T("pptx"), RGB(220, 130,  60), RGB(255, 255, 255), RGB(180, 100,  40) },
    // 媒体
    { _T("mp4"),  RGB(120,  80, 200), RGB(255, 255, 255), RGB( 90,  60, 170) },
    { _T("avi"),  RGB(120,  80, 200), RGB(255, 255, 255), RGB( 90,  60, 170) },
    { _T("mov"),  RGB(120,  80, 200), RGB(255, 255, 255), RGB( 90,  60, 170) },
    { _T("mp3"),  RGB(180,  90, 180), RGB(255, 255, 255), RGB(140,  70, 140) },
    { _T("png"),  RGB( 60, 170, 200), RGB(255, 255, 255), RGB( 30, 140, 180) },
    { _T("jpg"),  RGB( 60, 170, 200), RGB(255, 255, 255), RGB( 30, 140, 180) },
    { _T("jpeg"), RGB( 60, 170, 200), RGB(255, 255, 255), RGB( 30, 140, 180) },
    { _T("gif"),  RGB( 60, 170, 200), RGB(255, 255, 255), RGB( 30, 140, 180) },
    // 压缩
    { _T("zip"),  RGB(140, 130, 110), RGB(255, 255, 255), RGB(110, 100,  80) },
    { _T("rar"),  RGB(140, 130, 110), RGB(255, 255, 255), RGB(110, 100,  80) },
    { _T("7z"),   RGB(140, 130, 110), RGB(255, 255, 255), RGB(110, 100,  80) },
    // 设计源文件
    { _T("fig"),  RGB( 50, 160, 220), RGB(255, 255, 255), RGB( 30, 130, 190) },
    { _T("psd"),  RGB( 30,  80, 180), RGB(255, 255, 255), RGB( 20,  60, 140) },
    { _T("sketch"), RGB(255, 180,  60), RGB(120,  80,  20), RGB(220, 150,  40) },
};

const DemoFileTypeIcon::Palette kDefault = {
    RGB(150, 150, 160), RGB(255, 255, 255), RGB(110, 110, 120)
};

} // anonymous

DemoFileTypeIcon::Palette DemoFileTypeIcon::LookupPalette(LPCTSTR ext)
{
    if (!ext || !*ext) return kDefault;
    CString needle = ext;
    needle.MakeLower();
    for (auto& e : kPalettes)
    {
        if (needle == e.ext)
        {
            return { e.bg, e.fg, e.fold };
        }
    }
    return kDefault;
}

void DemoFileTypeIcon::SetExt(LPCTSTR ext)
{
    CString s = ext ? ext : _T("");
    s.MakeLower();
    if (m_ext == s) return;
    m_ext = s;
    // 自动同步 label 为大写
    m_label = s;
    m_label.MakeUpper();
    Invalidate();
}

void DemoFileTypeIcon::SetLabel(LPCTSTR t)
{
    CString s = t ? t : _T("");
    if (m_label == s) return;
    m_label = s;
    Invalidate();
}

void DemoFileTypeIcon::SetCornerRadius(int r)
{
    if (r < 0) r = 0;
    if (m_radius == r) return;
    m_radius = r;
    Invalidate();
}

// -----------------------------------------------------------------------------
// OnPaint:
//   1) 圆角背景（按 ext 配色）
//   2) 右上角折角三角（fold 色，模拟"翻页"质感）
//   3) hover 时整体加亮（HSV 提升等价：浅一层）
//   4) 居中扩展名文字
// -----------------------------------------------------------------------------
void DemoFileTypeIcon::OnPaint(HDC hdc, const RECT&)
{
    Palette p = LookupPalette(m_ext);

    // hover 时把所有色拉浅一档
    if (IsHover())
    {
        auto lift = [](COLORREF c) {
            int r = GetRValue(c) + 20;
            int g = GetGValue(c) + 20;
            int b = GetBValue(c) + 20;
            if (r > 255) r = 255;
            if (g > 255) g = 255;
            if (b > 255) b = 255;
            return RGB(r, g, b);
        };
        p.bg   = lift(p.bg);
        p.fold = lift(p.fold);
    }

    // ---- 1) 圆角背景 ----
    HBRUSH brBg  = ::CreateSolidBrush(p.bg);
    HGDIOBJ oldBr = ::SelectObject(hdc, brBg);
    HGDIOBJ oldPn = ::SelectObject(hdc, ::GetStockObject(NULL_PEN));
    ::RoundRect(hdc,
                m_rcItem.left, m_rcItem.top,
                m_rcItem.right, m_rcItem.bottom,
                m_radius * 2, m_radius * 2);
    ::SelectObject(hdc, oldBr);
    ::SelectObject(hdc, oldPn);
    ::DeleteObject(brBg);

    // ---- 2) 右上角折角 ----
    int sz = (m_rcItem.right - m_rcItem.left) / 4;
    if (sz > 14) sz = 14;
    if (sz >= 6)
    {
        POINT tri[3] = {
            { m_rcItem.right - sz, m_rcItem.top },
            { m_rcItem.right,      m_rcItem.top + sz },
            { m_rcItem.right,      m_rcItem.top },
        };
        HBRUSH brFold = ::CreateSolidBrush(p.fold);
        HGDIOBJ ob = ::SelectObject(hdc, brFold);
        HGDIOBJ op = ::SelectObject(hdc, ::GetStockObject(NULL_PEN));
        ::Polygon(hdc, tri, 3);
        ::SelectObject(hdc, ob);
        ::SelectObject(hdc, op);
        ::DeleteObject(brFold);
    }

    // ---- 3) 居中文字 ----
    if (!m_label.IsEmpty())
    {
        HFONT f = DuiResMgr::Inst().GetDefaultFont();
        HGDIOBJ oldF = ::SelectObject(hdc, f);
        ::SetBkMode(hdc, TRANSPARENT);
        ::SetTextColor(hdc, p.fg);
        RECT rc = m_rcItem;
        ::DrawText(hdc, m_label, -1, &rc,
                   DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        ::SelectObject(hdc, oldF);
    }
}

bool DemoFileTypeIcon::OnLButtonUp(POINT, UINT)
{
    NotifyParent(DUIN_CLICK);
    return true;
}
