#include "stdafx.h"
#include "CloudMelodyFonts.h"
#include "CloudMelodyTheme.h"

namespace cloudmelody {

namespace Fonts {

namespace {

struct FontSpec
{
    int     sizePx;
    int     weight;
    LPCTSTR face;
};

// 与 DESIGN.md typography 对齐
static const FontSpec kSpecs[KindCount] = {
    /* DisplayLg   */ { 30, kFontWeightBold,     kFontFaceCJK },
    /* HeadlineMd  */ { 22, kFontWeightSemibold, kFontFaceCJK },
    /* TitleSm     */ { 16, kFontWeightSemibold, kFontFaceCJK },
    /* BodyMd      */ { 14, kFontWeightRegular,  kFontFaceCJK },
    /* BodySm      */ { 13, kFontWeightRegular,  kFontFaceCJK },
    /* LabelXs     */ { 11, kFontWeightMedium,   kFontFaceCJK },
};

// 进程级单例 HFONT 缓存。OS 在进程退出时回收，不需要显式 DeleteObject。
static HFONT g_cache[KindCount] = { nullptr };

HFONT BuildFont(const FontSpec& s)
{
    return ::CreateFont(
        -s.sizePx,                  // 负值 = 像素高度
        0,                          // 宽度自动
        0, 0,                       // 旋转角
        s.weight,
        FALSE, FALSE, FALSE,        // italic / underline / strikeout
        GB2312_CHARSET,             // 中文渲染靠这个；DEFAULT_CHARSET 偶尔
                                    // 会 fallback 到 ANSI 把汉字变方块
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,          // ClearType 子像素抗锯齿
        VARIABLE_PITCH | FF_SWISS,
        s.face);
}

} // anonymous

HFONT Get(Kind kind)
{
    if (kind < 0 || kind >= KindCount)
    {
        return nullptr;
    }
    if (!g_cache[kind])
    {
        g_cache[kind] = BuildFont(kSpecs[kind]);
    }
    return g_cache[kind];
}

} // namespace Fonts

} // namespace cloudmelody
