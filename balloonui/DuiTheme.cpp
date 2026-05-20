#include "stdafx.h"
#include "DuiTheme.h"

namespace balloonwjui {

DuiTheme& DuiTheme::Inst()
{
    static DuiTheme s_inst;
    return s_inst;
}

DuiTheme::DuiTheme()
    : m_fontFace(_T("Microsoft YaHei"))
{
    FillLight();
}

COLORREF DuiTheme::Get(Slot s) const
{
    if (s < 0 || s >= SlotCount)
    {
        return RGB(0, 0, 0);
    }
    return m_colors[s];
}

void DuiTheme::Set(Slot s, COLORREF c)
{
    if (s < 0 || s >= SlotCount)
    {
        return;
    }
    if (m_colors[s] == c)
    {
        return;
    }
    m_colors[s] = c;
    Bump();
}

void DuiTheme::ApplyPreset(Preset p)
{
    if (p == Light)
    {
        FillLight();
    }
    else if (p == Dark)
    {
        FillDark();
    }
    else if (p == HighContrast)
    {
        FillHighContrast();
    }
    else
    {
        FillLight();
    }
    Bump();
}

void DuiTheme::SetDefaultFontFace(LPCTSTR face)
{
    CString f = face ? face : _T("");
    if (m_fontFace == f)
    {
        return;
    }
    m_fontFace = f;
    Bump();
}

void DuiTheme::SetDefaultFontPt(int pt)
{
    if (pt < 6)
    {
        pt = 6;
    }
    if (pt > 96)
    {
        pt = 96;
    }
    if (m_fontPt == pt)
    {
        return;
    }
    m_fontPt = pt;
    Bump();
}

int DuiTheme::SubscribeChange(ChangeCallback cb, void* userdata)
{
    if (!cb)
    {
        return 0;
    }
    Sub s;
    s.token = m_nextToken++;
    s.cb = cb;
    s.ud = userdata;
    m_subs.push_back(s);
    return s.token;
}

void DuiTheme::Unsubscribe(int token)
{
    for (auto it = m_subs.begin(); it != m_subs.end(); ++it)
    {
        if (it->token == token)
        {
            m_subs.erase(it);
            return;
        }
    }
}

void DuiTheme::Bump()
{
    ++m_version;
    // Snapshot subscriber list before firing — listeners may mutate
    // (Subscribe/Unsubscribe) during their own callback.
    auto snap = m_subs;
    for (auto& s : snap)
    {
        if (s.cb)
        {
            s.cb(s.ud);
        }
    }
}

void DuiTheme::FillLight()
{
    m_colors[BrandPrimary]   = RGB( 45, 108, 223);   // #2D6CDF
    m_colors[BrandHover]     = RGB( 37,  89, 184);   // #2559B8
    m_colors[BrandPressed]   = RGB( 30,  74, 153);   // #1E4A99
    m_colors[BrandBorder]    = RGB( 30,  74, 153);
    m_colors[TextOnPrimary]  = RGB(255, 255, 255);
    m_colors[TextDefault]    = RGB( 30,  30,  30);
    m_colors[TextSubtle]     = RGB(110, 110, 110);
    m_colors[TextDisabled]   = RGB(170, 170, 170);
    m_colors[TextLink]       = RGB( 45, 108, 223);
    m_colors[SurfaceBg]      = RGB(255, 255, 255);
    m_colors[SurfaceAltBg]   = RGB(245, 245, 248);
    m_colors[BorderLight]    = RGB(220, 220, 224);
    m_colors[BorderHeavy]    = RGB(180, 180, 188);
    m_colors[RowHover]       = RGB(232, 240, 252);
    m_colors[RowSel]         = RGB( 45, 108, 223);
    m_colors[TextOnRowSel]   = RGB(255, 255, 255);
    m_colors[StatusOnline]   = RGB( 60, 175,  80);
    m_colors[StatusAway]     = RGB(240, 175,  40);
    m_colors[StatusBusy]     = RGB(220,  60,  60);
    m_colors[StatusOffline]  = RGB(150, 150, 150);
}

// HighContrast: black background, pure-yellow accents, max-contrast
// borders. Matches Windows "High Contrast Black" feel without depending
// on the OS theme so DUI surfaces look correct regardless of system
// settings — paired with the Tier 4 accessibility goals.
void DuiTheme::FillHighContrast()
{
    m_colors[BrandPrimary]   = RGB(255, 255,   0);   // pure yellow
    m_colors[BrandHover]     = RGB(255, 255, 100);
    m_colors[BrandPressed]   = RGB(200, 200,   0);
    m_colors[BrandBorder]    = RGB(255, 255, 255);
    m_colors[TextOnPrimary]  = RGB(  0,   0,   0);
    m_colors[TextDefault]    = RGB(255, 255, 255);
    m_colors[TextSubtle]     = RGB(200, 200, 200);
    m_colors[TextDisabled]   = RGB(120, 120, 120);
    m_colors[TextLink]       = RGB(  0, 255, 255);   // cyan
    m_colors[SurfaceBg]      = RGB(  0,   0,   0);
    m_colors[SurfaceAltBg]   = RGB( 20,  20,  20);
    m_colors[BorderLight]    = RGB(255, 255, 255);
    m_colors[BorderHeavy]    = RGB(255, 255, 255);
    m_colors[RowHover]       = RGB( 80,  80,   0);
    m_colors[RowSel]         = RGB(255, 255,   0);
    m_colors[TextOnRowSel]   = RGB(  0,   0,   0);
    m_colors[StatusOnline]   = RGB(  0, 255,   0);
    m_colors[StatusAway]     = RGB(255, 255,   0);
    m_colors[StatusBusy]     = RGB(255,   0,   0);
    m_colors[StatusOffline]  = RGB(180, 180, 180);
}

void DuiTheme::FillDark()
{
    m_colors[BrandPrimary]   = RGB( 80, 140, 240);
    m_colors[BrandHover]     = RGB(100, 160, 255);
    m_colors[BrandPressed]   = RGB( 60, 120, 220);
    m_colors[BrandBorder]    = RGB(100, 160, 255);
    m_colors[TextOnPrimary]  = RGB(255, 255, 255);
    m_colors[TextDefault]    = RGB(230, 230, 235);
    m_colors[TextSubtle]     = RGB(160, 160, 170);
    m_colors[TextDisabled]   = RGB(100, 100, 110);
    m_colors[TextLink]       = RGB(120, 180, 255);
    m_colors[SurfaceBg]      = RGB( 30,  32,  38);
    m_colors[SurfaceAltBg]   = RGB( 40,  44,  52);
    m_colors[BorderLight]    = RGB( 60,  64,  72);
    m_colors[BorderHeavy]    = RGB( 80,  84,  92);
    m_colors[RowHover]       = RGB( 50,  60,  80);
    m_colors[RowSel]         = RGB( 80, 140, 240);
    m_colors[TextOnRowSel]   = RGB(255, 255, 255);
    m_colors[StatusOnline]   = RGB( 90, 200, 110);
    m_colors[StatusAway]     = RGB(255, 200,  60);
    m_colors[StatusBusy]     = RGB(255,  90,  90);
    m_colors[StatusOffline]  = RGB(140, 140, 150);
}

} // namespace balloonwjui
