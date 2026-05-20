#include "stdafx.h"
#include "MainFrame.h"
#include "MockData.h"
#include "MockChat.h"
#include "SessionListItem.h"
#include "ChatMessageList.h"
#include "SettingsFrame.h"

#include "DuiAnimation.h"
#include "DuiPaintAA.h"
#include "DuiResMgr.h"
#include "DuiDpi.h"
#include "Controls/Basic/DuiAvatar.h"
#include "Controls/Basic/DuiLabel.h"
#include "Controls/Input/DuiSwitch.h"
#include "Controls/Input/DuiEditHost.h"
#include "Controls/Window/DuiScrollBar.h"
#include "Controls/Layout/DuiLayout.h"
#include "Controls/List/DuiListBox.h"
#include "Controls/List/DuiMenu.h"

#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")

using namespace balloonwjui;

namespace xchat {

// ---- 颜色 / 尺寸常量 -------------------------------------------------------

// 某信品牌绿（与 LoginFrame 保持一致 —— 选中态绿条 / 主品牌色）。
static const COLORREF kBrandGreenRgb = RGB(7, 193, 96);

// 左 nav 暗色背景。某信 PC 实际偏冷调灰 #2E2E2E；选这个色做对照参考。
static const COLORREF kNavBgRgb = RGB(46, 46, 46);

// 中栏会话列表背景（实截近白）。
static const COLORREF kListBgRgb = RGB(245, 245, 245);

// 右栏聊天区背景（实截浅灰）。
static const COLORREF kChatBgRgb = RGB(238, 238, 238);

// nav icon 中央的占位圆点色（亮灰，dark bg 上可见）。
static const COLORREF kNavDotIdleRgb = RGB(160, 160, 160);

// 左 nav 选中绿条尺寸：3px 宽、24px 高，垂直居中在 cell 内。某信实截
// 是 2~3px 实心条，吃在 cell 最左边贴边。
static const int kNavSelBarW    = 3;
static const int kNavSelBarH    = 24;

// nav icon 中央占位圆点直径。idle / selected 同直径，只换色。
static const int kNavDotDiameter = 12;

// 主窗 60Hz 动画 pulse —— Phase 2 阶段没动画控件，但保留方便后续 Phase
// 5+ spinner / 切换动画用。
static const UINT_PTR kAnimPulseTimerId = 0xA01;
static const UINT     kAnimPulseMs      = 16;

// ---- 资源路径解析（与 LoginFrame 同语义，目前各自一份；多处共用时再
//     抽到 XmlFactory.cpp）。 -------------------------------------------------

namespace {

CString ResolveResPath(LPCTSTR rel)
{
    TCHAR exePath[MAX_PATH] = {};
    ::GetModuleFileName(nullptr, exePath, MAX_PATH);
    CString dir = exePath;
    int slash = dir.ReverseFind(_T('\\'));
    if (slash > 0)
    {
        dir = dir.Left(slash);
    }
    CString a = dir + _T("\\res\\") + rel;
    if (::GetFileAttributes(a) != INVALID_FILE_ATTRIBUTES) { return a; }
    CString b = dir + _T("\\..\\XChat\\res\\") + rel;
    if (::GetFileAttributes(b) != INVALID_FILE_ATTRIBUTES) { return b; }
    CString c = dir + _T("\\..\\..\\XChat\\res\\") + rel;
    if (::GetFileAttributes(c) != INVALID_FILE_ATTRIBUTES) { return c; }
    return CString();
}

CString ResolveXmlPath(LPCTSTR fileName)
{
    TCHAR exePath[MAX_PATH] = {};
    ::GetModuleFileName(nullptr, exePath, MAX_PATH);
    CString dir = exePath;
    int slash = dir.ReverseFind(_T('\\'));
    if (slash > 0)
    {
        dir = dir.Left(slash);
    }
    CString a = dir + _T("\\") + fileName;
    if (::GetFileAttributes(a) != INVALID_FILE_ATTRIBUTES) { return a; }
    CString b = dir + _T("\\..\\XChat\\") + fileName;
    if (::GetFileAttributes(b) != INVALID_FILE_ATTRIBUTES) { return b; }
    CString c = dir + _T("\\..\\..\\XChat\\") + fileName;
    if (::GetFileAttributes(c) != INVALID_FILE_ATTRIBUTES) { return c; }
    return CString();
}

std::string ReadFileUtf8(LPCTSTR path)
{
    HANDLE h = ::CreateFile(path, GENERIC_READ, FILE_SHARE_READ, NULL,
                            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) { return std::string(); }
    DWORD sz = ::GetFileSize(h, nullptr);
    std::string out;
    if (sz > 0 && sz < (1 << 20))
    {
        out.resize(sz);
        DWORD got = 0;
        ::ReadFile(h, &out[0], sz, &got, nullptr);
        out.resize(got);
        if (out.size() >= 3
            && (unsigned char)out[0] == 0xEF
            && (unsigned char)out[1] == 0xBB
            && (unsigned char)out[2] == 0xBF)
        {
            out.erase(0, 3);
        }
    }
    ::CloseHandle(h);
    return out;
}

HBITMAP LoadPngAsHBitmap(LPCTSTR path)
{
    Gdiplus::Bitmap bmp(path);
    if (bmp.GetLastStatus() != Gdiplus::Ok) { return nullptr; }
    HBITMAP hbm = nullptr;
    bmp.GetHBITMAP(Gdiplus::Color(0, 0, 0, 0), &hbm);
    return hbm;
}

COLORREF ParseRgbAttr(const std::string& s, COLORREF def)
{
    int r = 0, g = 0, b = 0;
    if (sscanf_s(s.c_str(), "%d,%d,%d", &r, &g, &b) == 3)
    {
        return RGB(r, g, b);
    }
    return def;
}

// =========================================================================
// ColorPanel —— 给 vbox / hbox 画背景色的薄包装。
//
// 用途：main.xml 里左 nav / 中列表 / 右聊天三栏每栏要不同底色。balloonui
// 的 layout 控件本身不画背景；若直接用 vbox，子控件覆盖之外的区域是
// 透明 -> 渲染出系统 BTNFACE。本控件 OnPaint 先 FillRect 自身 m_rcItem
// 一次再走基类 child 迭代。
//
// 派生 DuiVBox 而不是 DuiHBox，是因为本 demo 三栏内部全是垂直堆叠；要
// 横向用直接套 hbox 子控件即可。
// =========================================================================
class ColorPanel : public DuiVBox
{
public:
    void SetBg(COLORREF c) { m_bg = c; Invalidate(); }

    void OnPaint(HDC hdc, const RECT& rcDirty) override
    {
        if (!m_bVisible) { return; }
        HBRUSH hbr = ::CreateSolidBrush(m_bg);
        ::FillRect(hdc, &m_rcItem, hbr);
        ::DeleteObject(hbr);
        // 走基类 OnPaint 让 child 在底色之上画。
        DuiControl::OnPaint(hdc, rcDirty);
    }

private:
    COLORREF m_bg = RGB(255, 255, 255);
};

// =========================================================================
// WxIcons —— 一组用 GDI+ / DuiAA 画的极简线性图标 glyph，给 NavIcon /
// IconButton 共用。
//
// 设计：每个 glyph 都画在调用方传进来的 RECT 里 ——内部按 24×24 逻辑
// 视口设计，再线性缩放到 RECT 边长（取较小边以保证 1:1）。线宽随
// scale 线性放大，最低 1.0f。所有 stroke 走 Gdiplus::Pen，端点 / 拐角
// 圆角，避免锯齿和硬尖。
//
// 这层 helper 完全自给自足，不依赖任何外部资源（PNG / SVG / 字体），
// 让 demo 不需要搭一套图标资源管线就能跑出 PC 某信视觉密度。
// =========================================================================
enum WxIconKind
{
    Wxk_None = 0,
    // 左 nav
    Wxk_ChatBubble,         // 聊天 (圆角对话框 + 内三个点)
    Wxk_People,             // 联系人 (人头 + 肩 + 名册线)
    Wxk_Box,                // 收藏 (3D 立方体)
    Wxk_Aperture,           // 朋友圈 (相机光圈)
    Wxk_VideoBookmark,      // 视频号 (圆角矩形 + 三角)
    Wxk_EyeLook,            // 看一看 (眼形)
    Wxk_MiniProg,           // 小程序 (4 个圆角小方块)
    Wxk_Phone,              // 手机端 (竖矩形 + 上下条)
    Wxk_Hamburger,          // ≡ 三横

    // chat header / 底部 toolbar
    Wxk_Emoji,              // 笑脸
    Wxk_Folder,             // 文件夹
    Wxk_Scissors,           // 剪刀
    Wxk_Mic,                // 麦克风
    Wxk_Plus,               // +
    Wxk_PhoneCall,          // 电话听筒
    Wxk_Dots,               // ⋯ 三点
};

WxIconKind ParseWxIconKind(const std::string& s)
{
    if (s == "chat-bubble")    { return Wxk_ChatBubble; }
    if (s == "people")         { return Wxk_People; }
    if (s == "box")            { return Wxk_Box; }
    if (s == "aperture")       { return Wxk_Aperture; }
    if (s == "video-bookmark") { return Wxk_VideoBookmark; }
    if (s == "eye-look")       { return Wxk_EyeLook; }
    if (s == "mini-prog")      { return Wxk_MiniProg; }
    if (s == "phone")          { return Wxk_Phone; }
    if (s == "hamburger")      { return Wxk_Hamburger; }
    if (s == "emoji")          { return Wxk_Emoji; }
    if (s == "folder")         { return Wxk_Folder; }
    if (s == "scissors")       { return Wxk_Scissors; }
    if (s == "mic")            { return Wxk_Mic; }
    if (s == "plus")           { return Wxk_Plus; }
    if (s == "phone-call")     { return Wxk_PhoneCall; }
    if (s == "dots")           { return Wxk_Dots; }
    return Wxk_None;
}

namespace WxIcons {

// 把一组 24×24 逻辑坐标点缩放进 RECT 中央。s = scale，offX/offY = origin。
struct XF {
    float s, offX, offY;
    Gdiplus::PointF P(float x, float y) const
    {
        return Gdiplus::PointF(offX + x * s, offY + y * s);
    }
    float L(float v) const { return v * s; }
};

XF MakeXF(const RECT& rc)
{
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    int side = (w < h) ? w : h;
    XF xf;
    xf.s    = side / 24.0f;
    xf.offX = (float)rc.left + (w - side) * 0.5f;
    xf.offY = (float)rc.top  + (h - side) * 0.5f;
    return xf;
}

Gdiplus::Color GpColor(COLORREF c)
{
    return Gdiplus::Color(255, GetRValue(c), GetGValue(c), GetBValue(c));
}

void Stroke(Gdiplus::Graphics& g, Gdiplus::Pen& pen,
            const Gdiplus::PointF& a, const Gdiplus::PointF& b)
{
    g.DrawLine(&pen, a, b);
}

void Paint(HDC hdc, const RECT& rc, WxIconKind kind, COLORREF rgb)
{
    if (kind == Wxk_None) { return; }

    Gdiplus::Graphics g(hdc);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);

    XF xf = MakeXF(rc);
    Gdiplus::Color clr = GpColor(rgb);

    // 主线宽：1.6px（24 viewport 时）；scale > 1 时跟着放大但底限 1.2。
    float strokeW = xf.s * 1.6f;
    if (strokeW < 1.2f) { strokeW = 1.2f; }

    Gdiplus::Pen pen(clr, strokeW);
    pen.SetStartCap(Gdiplus::LineCapRound);
    pen.SetEndCap  (Gdiplus::LineCapRound);
    pen.SetLineJoin(Gdiplus::LineJoinRound);

    Gdiplus::SolidBrush brush(clr);

    switch (kind)
    {
    case Wxk_ChatBubble:
    {
        // 圆角对话框 (3,4)→(21,18)，左下小尾巴
        Gdiplus::RectF body(xf.offX + xf.L(3.0f), xf.offY + xf.L(4.0f),
                            xf.L(18.0f), xf.L(14.0f));
        Gdiplus::GraphicsPath path;
        Gdiplus::REAL r = xf.L(4.0f);
        path.AddArc(body.X, body.Y, r * 2, r * 2, 180, 90);
        path.AddArc(body.GetRight() - r * 2, body.Y, r * 2, r * 2, 270, 90);
        path.AddArc(body.GetRight() - r * 2, body.GetBottom() - r * 2, r * 2, r * 2, 0, 90);
        // 加尾巴：右下→左下三角，简化做法
        path.AddLine(body.GetRight() - r, body.GetBottom(),
                     xf.offX + xf.L(9.0f),  xf.offY + xf.L(18.0f));
        path.AddLine(xf.offX + xf.L(9.0f),  xf.offY + xf.L(18.0f),
                     xf.offX + xf.L(7.0f),  xf.offY + xf.L(21.0f));
        path.AddLine(xf.offX + xf.L(7.0f),  xf.offY + xf.L(21.0f),
                     xf.offX + xf.L(7.5f),  xf.offY + xf.L(18.0f));
        path.AddArc(body.X, body.GetBottom() - r * 2, r * 2, r * 2, 90, 90);
        path.CloseFigure();
        g.DrawPath(&pen, &path);
        break;
    }
    case Wxk_People:
    {
        // 左大人 (头+身)，右后小人 (头+身)
        // 大人头：圆心 (10,8) r=3
        Gdiplus::RectF head1(xf.offX + xf.L(7.0f), xf.offY + xf.L(5.0f),
                             xf.L(6.0f), xf.L(6.0f));
        g.DrawEllipse(&pen, head1);
        // 大人身：上凸弧 (3,20)→(17,20) 经 (10,13)
        Gdiplus::GraphicsPath body1;
        body1.AddArc(xf.offX + xf.L(3.0f), xf.offY + xf.L(13.0f),
                     xf.L(14.0f), xf.L(14.0f), 180, 180);
        g.DrawPath(&pen, &body1);
        // 小人头：圆心 (17,7) r=2
        Gdiplus::RectF head2(xf.offX + xf.L(15.0f), xf.offY + xf.L(5.0f),
                             xf.L(4.0f), xf.L(4.0f));
        g.DrawEllipse(&pen, head2);
        // 小人肩：右上小弧
        Gdiplus::GraphicsPath body2;
        body2.AddArc(xf.offX + xf.L(14.0f), xf.offY + xf.L(11.0f),
                     xf.L(10.0f), xf.L(8.0f), 270, 110);
        g.DrawPath(&pen, &body2);
        break;
    }
    case Wxk_Box:
    {
        // 立方体 isometric 投影
        Gdiplus::PointF top   = xf.P(12.0f, 3.0f);
        Gdiplus::PointF tr    = xf.P(21.0f, 7.5f);
        Gdiplus::PointF br    = xf.P(21.0f, 16.5f);
        Gdiplus::PointF bot   = xf.P(12.0f, 21.0f);
        Gdiplus::PointF bl    = xf.P( 3.0f, 16.5f);
        Gdiplus::PointF tl    = xf.P( 3.0f, 7.5f);
        Gdiplus::PointF mid   = xf.P(12.0f, 12.0f);
        Stroke(g, pen, top, tr);
        Stroke(g, pen, tr,  br);
        Stroke(g, pen, br,  bot);
        Stroke(g, pen, bot, bl);
        Stroke(g, pen, bl,  tl);
        Stroke(g, pen, tl,  top);
        Stroke(g, pen, top, mid);
        Stroke(g, pen, tl,  mid);
        Stroke(g, pen, tr,  mid);
        Stroke(g, pen, mid, bot);
        break;
    }
    case Wxk_Aperture:
    {
        // 相机光圈：外圆 + 6 道叶片线（简化为对称 3 条直径，转 30°）
        Gdiplus::RectF rcCirc(xf.offX + xf.L(3.0f), xf.offY + xf.L(3.0f),
                              xf.L(18.0f), xf.L(18.0f));
        g.DrawEllipse(&pen, rcCirc);
        Gdiplus::PointF c = xf.P(12.0f, 12.0f);
        for (int i = 0; i < 3; ++i)
        {
            float ang = (3.14159265f / 3.0f) * i;
            float dx = cosf(ang) * xf.L(7.0f);
            float dy = sinf(ang) * xf.L(7.0f);
            Stroke(g, pen,
                   Gdiplus::PointF(c.X - dx, c.Y - dy),
                   Gdiplus::PointF(c.X + dx, c.Y + dy));
        }
        break;
    }
    case Wxk_VideoBookmark:
    {
        // 圆角矩形 (3,5)–(21,19) + 中央播放三角
        Gdiplus::RectF rcBox(xf.offX + xf.L(3.0f), xf.offY + xf.L(5.0f),
                             xf.L(18.0f), xf.L(14.0f));
        Gdiplus::GraphicsPath rrPath;
        float rr = xf.L(3.0f);
        rrPath.AddArc(rcBox.X, rcBox.Y, rr * 2, rr * 2, 180, 90);
        rrPath.AddArc(rcBox.GetRight() - rr * 2, rcBox.Y, rr * 2, rr * 2, 270, 90);
        rrPath.AddArc(rcBox.GetRight() - rr * 2, rcBox.GetBottom() - rr * 2, rr * 2, rr * 2, 0, 90);
        rrPath.AddArc(rcBox.X, rcBox.GetBottom() - rr * 2, rr * 2, rr * 2, 90, 90);
        rrPath.CloseFigure();
        g.DrawPath(&pen, &rrPath);
        // 三角填充
        Gdiplus::PointF tri[3] = {
            xf.P(10.0f, 9.0f), xf.P(15.5f, 12.0f), xf.P(10.0f, 15.0f)
        };
        g.FillPolygon(&brush, tri, 3);
        break;
    }
    case Wxk_EyeLook:
    {
        // 眼睛 (3,12)→(21,12)，上下两条对称弧；中间 r=2.5 黑圈
        Gdiplus::GraphicsPath upper, lower;
        upper.AddBezier(xf.P(3.0f, 12.0f),  xf.P(8.0f, 6.5f),
                        xf.P(16.0f, 6.5f), xf.P(21.0f, 12.0f));
        lower.AddBezier(xf.P(3.0f, 12.0f),  xf.P(8.0f, 17.5f),
                        xf.P(16.0f, 17.5f), xf.P(21.0f, 12.0f));
        g.DrawPath(&pen, &upper);
        g.DrawPath(&pen, &lower);
        Gdiplus::RectF rcPupil(xf.offX + xf.L(9.5f), xf.offY + xf.L(9.5f),
                               xf.L(5.0f), xf.L(5.0f));
        g.DrawEllipse(&pen, rcPupil);
        break;
    }
    case Wxk_MiniProg:
    {
        // 4 个圆角小方块 (5×5) 排井字
        float sz = xf.L(7.5f);
        float gap = xf.L(2.0f);
        float originX = xf.offX + xf.L(3.0f);
        float originY = xf.offY + xf.L(3.0f);
        for (int row = 0; row < 2; ++row)
        {
            for (int col = 0; col < 2; ++col)
            {
                Gdiplus::RectF rcBlk(originX + col * (sz + gap),
                                     originY + row * (sz + gap),
                                     sz, sz);
                Gdiplus::GraphicsPath p;
                float br2 = xf.L(1.5f);
                p.AddArc(rcBlk.X, rcBlk.Y, br2 * 2, br2 * 2, 180, 90);
                p.AddArc(rcBlk.GetRight() - br2 * 2, rcBlk.Y, br2 * 2, br2 * 2, 270, 90);
                p.AddArc(rcBlk.GetRight() - br2 * 2, rcBlk.GetBottom() - br2 * 2,
                         br2 * 2, br2 * 2, 0, 90);
                p.AddArc(rcBlk.X, rcBlk.GetBottom() - br2 * 2, br2 * 2, br2 * 2, 90, 90);
                p.CloseFigure();
                g.DrawPath(&pen, &p);
            }
        }
        break;
    }
    case Wxk_Phone:
    {
        // 手机：竖圆角矩形 + 顶部听筒线 + 底部 home 圆
        Gdiplus::RectF rcPh(xf.offX + xf.L(7.0f), xf.offY + xf.L(3.0f),
                            xf.L(10.0f), xf.L(18.0f));
        Gdiplus::GraphicsPath p;
        float pr = xf.L(2.0f);
        p.AddArc(rcPh.X, rcPh.Y, pr * 2, pr * 2, 180, 90);
        p.AddArc(rcPh.GetRight() - pr * 2, rcPh.Y, pr * 2, pr * 2, 270, 90);
        p.AddArc(rcPh.GetRight() - pr * 2, rcPh.GetBottom() - pr * 2, pr * 2, pr * 2, 0, 90);
        p.AddArc(rcPh.X, rcPh.GetBottom() - pr * 2, pr * 2, pr * 2, 90, 90);
        p.CloseFigure();
        g.DrawPath(&pen, &p);
        // 顶部听筒线
        Stroke(g, pen, xf.P(10.5f, 5.5f), xf.P(13.5f, 5.5f));
        // 底部 home 圆
        Gdiplus::RectF rcHome(xf.offX + xf.L(11.0f), xf.offY + xf.L(17.5f),
                              xf.L(2.0f), xf.L(2.0f));
        g.FillEllipse(&brush, rcHome);
        break;
    }
    case Wxk_Hamburger:
    {
        // 3 横
        Stroke(g, pen, xf.P(5.0f, 8.0f),  xf.P(19.0f, 8.0f));
        Stroke(g, pen, xf.P(5.0f, 12.0f), xf.P(19.0f, 12.0f));
        Stroke(g, pen, xf.P(5.0f, 16.0f), xf.P(19.0f, 16.0f));
        break;
    }
    case Wxk_Emoji:
    {
        // 笑脸：圆 + 两眼 + 嘴弧
        Gdiplus::RectF rcFace(xf.offX + xf.L(3.0f), xf.offY + xf.L(3.0f),
                              xf.L(18.0f), xf.L(18.0f));
        g.DrawEllipse(&pen, rcFace);
        Gdiplus::RectF rcEye1(xf.offX + xf.L(8.0f), xf.offY + xf.L(9.0f),
                              xf.L(2.0f), xf.L(2.0f));
        Gdiplus::RectF rcEye2(xf.offX + xf.L(14.0f), xf.offY + xf.L(9.0f),
                              xf.L(2.0f), xf.L(2.0f));
        g.FillEllipse(&brush, rcEye1);
        g.FillEllipse(&brush, rcEye2);
        Gdiplus::GraphicsPath mouth;
        mouth.AddArc(xf.offX + xf.L(8.0f), xf.offY + xf.L(11.5f),
                     xf.L(8.0f), xf.L(6.0f), 0, 180);
        g.DrawPath(&pen, &mouth);
        break;
    }
    case Wxk_Folder:
    {
        // 文件夹：上方 tab + 下方主体
        Gdiplus::GraphicsPath p;
        p.AddLine(xf.P(3.0f, 7.0f), xf.P(10.0f, 7.0f));
        p.AddLine(xf.P(10.0f, 7.0f), xf.P(12.0f, 9.0f));
        p.AddLine(xf.P(12.0f, 9.0f), xf.P(21.0f, 9.0f));
        p.AddLine(xf.P(21.0f, 9.0f), xf.P(21.0f, 19.0f));
        p.AddLine(xf.P(21.0f, 19.0f), xf.P(3.0f, 19.0f));
        p.AddLine(xf.P(3.0f, 19.0f), xf.P(3.0f, 7.0f));
        p.CloseFigure();
        g.DrawPath(&pen, &p);
        break;
    }
    case Wxk_Scissors:
    {
        // 剪刀：两小圆 + 两条交叉线
        Gdiplus::RectF rcL(xf.offX + xf.L(3.5f),  xf.offY + xf.L(15.0f),
                           xf.L(5.0f), xf.L(5.0f));
        Gdiplus::RectF rcR(xf.offX + xf.L(15.5f), xf.offY + xf.L(15.0f),
                           xf.L(5.0f), xf.L(5.0f));
        g.DrawEllipse(&pen, rcL);
        g.DrawEllipse(&pen, rcR);
        Stroke(g, pen, xf.P(8.5f, 15.0f),  xf.P(20.0f, 4.0f));
        Stroke(g, pen, xf.P(15.5f, 15.0f), xf.P(4.0f, 4.0f));
        break;
    }
    case Wxk_Mic:
    {
        // 麦：圆角矩形话头 + 下方 U + 短竖线
        Gdiplus::RectF rcCap(xf.offX + xf.L(9.0f), xf.offY + xf.L(3.0f),
                             xf.L(6.0f), xf.L(11.0f));
        Gdiplus::GraphicsPath p;
        float cr = xf.L(3.0f);
        p.AddArc(rcCap.X, rcCap.Y, cr * 2, cr * 2, 180, 180);
        p.AddArc(rcCap.X, rcCap.GetBottom() - cr * 2, cr * 2, cr * 2, 0, 180);
        p.CloseFigure();
        g.DrawPath(&pen, &p);
        // U 弧
        Gdiplus::GraphicsPath base;
        base.AddArc(xf.offX + xf.L(6.0f), xf.offY + xf.L(11.0f),
                    xf.L(12.0f), xf.L(8.0f), 0, 180);
        g.DrawPath(&pen, &base);
        // 短竖线 + 横线
        Stroke(g, pen, xf.P(12.0f, 19.0f), xf.P(12.0f, 21.5f));
        Stroke(g, pen, xf.P(9.0f, 21.5f),  xf.P(15.0f, 21.5f));
        break;
    }
    case Wxk_Plus:
    {
        // + 字（不带圆框，搜索栏旁那种）
        Stroke(g, pen, xf.P(12.0f, 5.0f), xf.P(12.0f, 19.0f));
        Stroke(g, pen, xf.P(5.0f, 12.0f), xf.P(19.0f, 12.0f));
        break;
    }
    case Wxk_PhoneCall:
    {
        // 听筒：左上→右下倾斜的胶囊
        Gdiplus::GraphicsPath p;
        // 简化：画一段斜的圆角矩形轮廓
        Gdiplus::Matrix m;
        m.RotateAt(-30.0f, Gdiplus::PointF(xf.offX + xf.L(12.0f),
                                           xf.offY + xf.L(12.0f)));
        Gdiplus::RectF rcPh(xf.offX + xf.L(8.0f), xf.offY + xf.L(2.0f),
                            xf.L(8.0f), xf.L(20.0f));
        float pr = xf.L(3.5f);
        p.AddArc(rcPh.X, rcPh.Y, pr * 2, pr * 2, 180, 90);
        p.AddArc(rcPh.GetRight() - pr * 2, rcPh.Y, pr * 2, pr * 2, 270, 90);
        p.AddArc(rcPh.GetRight() - pr * 2, rcPh.GetBottom() - pr * 2, pr * 2, pr * 2, 0, 90);
        p.AddArc(rcPh.X, rcPh.GetBottom() - pr * 2, pr * 2, pr * 2, 90, 90);
        p.CloseFigure();
        // 内部：上下两个椭圆"听 / 麦"占位
        Gdiplus::RectF rcEar(xf.offX + xf.L(10.0f), xf.offY + xf.L(3.5f),
                             xf.L(4.0f), xf.L(2.5f));
        Gdiplus::RectF rcMouth(xf.offX + xf.L(10.0f), xf.offY + xf.L(18.0f),
                               xf.L(4.0f), xf.L(2.5f));
        p.AddEllipse(rcEar);
        p.AddEllipse(rcMouth);
        p.Transform(&m);
        g.DrawPath(&pen, &p);
        break;
    }
    case Wxk_Dots:
    {
        // ⋯ 三横向小圆
        float r = xf.L(1.6f);
        for (int i = 0; i < 3; ++i)
        {
            float cx = xf.offX + xf.L(7.0f + 5.0f * i);
            float cy = xf.offY + xf.L(12.0f);
            Gdiplus::RectF rcDot(cx - r, cy - r, r * 2, r * 2);
            g.FillEllipse(&brush, rcDot);
        }
        break;
    }
    default:
        break;
    }
}

} // namespace WxIcons

// =========================================================================
// NavIcon —— 左 nav 单元（75×52，深色背景上画一个 stroke glyph）。
//
// 视觉：cell 中央 24×24 logical glyph 上下左右各 14px padding；选中态
// 左侧 3px×24px 品牌绿条 + glyph 颜色变品牌绿；非选中 glyph 灰
// (180,180,180)。点击 glyph 通过 DUIN_CLICK 通知父（仅汉堡 cell 用，
// 通过 SetCtrlId 启用）。
// =========================================================================
class NavIcon : public DuiControl
{
public:
    void SetSelected(bool b)       { m_selected = b; Invalidate(); }
    bool IsSelected() const        { return m_selected; }
    void SetGlyph(WxIconKind k)    { m_glyph = k; Invalidate(); }

    bool OnLButtonUp(POINT, UINT) override
    {
        // 只有显式 SetCtrlId 的 cell（如汉堡 id=91）才发通知；其它 nav
        // cell id 为 0，父侧 OnDuiNotify 直接忽略。
        if (GetCtrlId() != 0)
        {
            NotifyParent(DUIN_CLICK, 0);
        }
        return true;
    }

    void OnPaint(HDC hdc, const RECT& /*rcDirty*/) override
    {
        if (!m_bVisible) { return; }

        // 1) 选中绿条：3px 宽、24px 高，竖向居中贴左边。
        if (m_selected)
        {
            int cellH = m_rcItem.bottom - m_rcItem.top;
            int barTop = m_rcItem.top + (cellH - kNavSelBarH) / 2;
            RECT bar = { m_rcItem.left,
                         barTop,
                         m_rcItem.left + kNavSelBarW,
                         barTop + kNavSelBarH };
            HBRUSH hbr = ::CreateSolidBrush(kBrandGreenRgb);
            ::FillRect(hdc, &bar, hbr);
            ::DeleteObject(hbr);
        }

        // 2) 中央 glyph
        int cx = (m_rcItem.left + m_rcItem.right) / 2;
        int cy = (m_rcItem.top  + m_rcItem.bottom) / 2;
        const int kGlyphSide = 22;     // 22×22 glyph，给 cell 高 52 留余量
        RECT rcGlyph = { cx - kGlyphSide / 2,
                         cy - kGlyphSide / 2,
                         cx + kGlyphSide / 2,
                         cy + kGlyphSide / 2 };
        COLORREF glyphColor = m_selected ? kBrandGreenRgb : kNavDotIdleRgb;
        WxIcons::Paint(hdc, rcGlyph, m_glyph, glyphColor);
    }

private:
    bool       m_selected = false;
    WxIconKind m_glyph    = Wxk_None;
};

// =========================================================================
// IconButton —— 32×32 通用工具图标按钮。chat header 右上 3 个、底部
// toolbar 5 个、会话搜索栏旁 "+" 全部用它。
//
// 视觉：透明底（caller 给 host 背景），中央 22×22 stroke glyph；非
// hover 灰 (110,110,110)，hover 加浅灰圆角底 (240,240,240) + glyph
// 变深灰。Phase P0 不接 click。
// =========================================================================
class IconButton : public DuiControl
{
public:
    void SetGlyph(WxIconKind k)         { m_glyph = k; Invalidate(); }
    void SetIdleColor(COLORREF c)       { m_idle = c; Invalidate(); }
    void SetHoverBg(COLORREF c)         { m_hoverBg = c; Invalidate(); }

    bool OnMouseEnter() override        { m_hover = true;  Invalidate(); return true; }
    bool OnMouseLeave() override        { m_hover = false; Invalidate(); return true; }

    void OnPaint(HDC hdc, const RECT&) override
    {
        if (!m_bVisible) { return; }
        if (m_hover)
        {
            // 浅灰圆角底：给 cell 内缩 2px，避免和邻居挤
            HBRUSH hbr = ::CreateSolidBrush(m_hoverBg);
            HRGN hrgn = ::CreateRoundRectRgn(m_rcItem.left + 2, m_rcItem.top + 2,
                                             m_rcItem.right - 1, m_rcItem.bottom - 1,
                                             6, 6);
            ::FillRgn(hdc, hrgn, hbr);
            ::DeleteObject(hrgn);
            ::DeleteObject(hbr);
        }
        // 中央 glyph
        int cx = (m_rcItem.left + m_rcItem.right) / 2;
        int cy = (m_rcItem.top  + m_rcItem.bottom) / 2;
        const int kGlyphSide = 20;
        RECT rcG = { cx - kGlyphSide / 2, cy - kGlyphSide / 2,
                     cx + kGlyphSide / 2, cy + kGlyphSide / 2 };
        WxIcons::Paint(hdc, rcG, m_glyph, m_idle);
    }

private:
    WxIconKind m_glyph   = Wxk_None;
    COLORREF   m_idle    = RGB(110, 110, 110);
    COLORREF   m_hoverBg = RGB(232, 232, 232);
    bool       m_hover   = false;
};

// =========================================================================
// SendButton —— 聊天底部右下"发送"按钮。
//
// 视觉：浅灰圆角底 (235,235,235)、深灰文字 (90,90,90)；hover 微深底色，
// 文字色不变。disabled 状态用同 idle。Phase P0 不接 click（demo 没真
// 发送逻辑）。
// =========================================================================
class SendButton : public DuiControl
{
public:
    void SetText(LPCTSTR sz)        { m_text = sz ? sz : _T(""); Invalidate(); }

    bool OnMouseEnter() override { m_hover = true;  Invalidate(); return true; }
    bool OnMouseLeave() override { m_hover = false; Invalidate(); return true; }

    void OnPaint(HDC hdc, const RECT&) override
    {
        if (!m_bVisible) { return; }
        COLORREF bg = m_hover ? RGB(220, 220, 220) : RGB(238, 238, 238);
        HBRUSH hbr = ::CreateSolidBrush(bg);
        HRGN hrgn = ::CreateRoundRectRgn(m_rcItem.left, m_rcItem.top,
                                         m_rcItem.right + 1, m_rcItem.bottom + 1,
                                         4, 4);
        ::FillRgn(hdc, hrgn, hbr);
        ::DeleteObject(hrgn);
        ::DeleteObject(hbr);

        HFONT hOldFont = (HFONT)::SelectObject(hdc, DuiResMgr::Inst().GetDefaultFont());
        int oldBk = ::SetBkMode(hdc, TRANSPARENT);
        COLORREF oldClr = ::SetTextColor(hdc, RGB(90, 90, 90));
        ::DrawText(hdc, m_text.IsEmpty() ? _T("发送") : (LPCTSTR)m_text, -1, &m_rcItem,
                   DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        ::SetTextColor(hdc, oldClr);
        ::SetBkMode(hdc, oldBk);
        ::SelectObject(hdc, hOldFont);
    }

private:
    CString m_text  = _T("发送");
    bool    m_hover = false;
};

// =========================================================================
// WatermarkPanel —— 右栏空 chat 状态的居中"某信"双气泡水印 logo。
//
// 只在 --empty CLI arg 启动 / 用户切到一个不存在的 session 时显示。视觉
// 参考 主面板.png：浅灰色 (220,220,220)，两个互叠的圆角对话气泡，每个
// 内有 3 个小点。整体居中绘制；尺寸取 cell 较小边的 14% 做基准（窗口
// 1100×720 时约 100×100）。
// =========================================================================
class WatermarkPanel : public DuiControl
{
public:
    void OnPaint(HDC hdc, const RECT&) override
    {
        if (!m_bVisible) { return; }

        // 先填充浅灰底色，让 watermark 区域和别的 chat 区一致。
        HBRUSH hbgBr = ::CreateSolidBrush(RGB(255, 255, 255));
        ::FillRect(hdc, &m_rcItem, hbgBr);
        ::DeleteObject(hbgBr);

        Gdiplus::Graphics g(hdc);
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);

        int w = m_rcItem.right - m_rcItem.left;
        int h = m_rcItem.bottom - m_rcItem.top;
        int side = (w < h) ? w : h;
        // 水印整体边长：cell 较小边 / 6（窗口高 660 时 ≈ 110）
        float wmSide = side / 6.0f;
        if (wmSide < 80.0f) { wmSide = 80.0f; }

        float cx = (float)m_rcItem.left + w * 0.5f;
        float cy = (float)m_rcItem.top  + h * 0.5f;
        Gdiplus::Color clrFill(255, 220, 220, 220);
        Gdiplus::SolidBrush brush(clrFill);

        // 后面的小气泡：右下，圆角矩形 + 尾巴
        float bigW  = wmSide * 0.78f;
        float bigH  = wmSide * 0.62f;
        float smW   = wmSide * 0.55f;
        float smH   = wmSide * 0.42f;
        // big 气泡：左上偏移
        Gdiplus::RectF bigRc(cx - wmSide * 0.42f, cy - wmSide * 0.32f, bigW, bigH);
        // small 气泡：右下偏移
        Gdiplus::RectF smRc(cx - wmSide * 0.05f, cy + wmSide * 0.02f, smW, smH);

        // 绘 small 气泡（在 big 后面，先画在底层）
        {
            Gdiplus::GraphicsPath p;
            float r = smH * 0.45f;
            p.AddArc(smRc.X, smRc.Y, r * 2, r * 2, 180, 90);
            p.AddArc(smRc.GetRight() - r * 2, smRc.Y, r * 2, r * 2, 270, 90);
            p.AddArc(smRc.GetRight() - r * 2, smRc.GetBottom() - r * 2, r * 2, r * 2, 0, 90);
            // 尾巴：右下
            p.AddLine(smRc.GetRight() - r * 0.6f, smRc.GetBottom(),
                      smRc.GetRight() + smRc.Width * 0.18f, smRc.GetBottom() + smRc.Height * 0.28f);
            p.AddLine(smRc.GetRight() + smRc.Width * 0.18f, smRc.GetBottom() + smRc.Height * 0.28f,
                      smRc.GetRight() - r * 1.6f, smRc.GetBottom());
            p.AddArc(smRc.X, smRc.GetBottom() - r * 2, r * 2, r * 2, 90, 90);
            p.CloseFigure();
            g.FillPath(&brush, &p);
        }
        // 绘 big 气泡（覆盖部分 small）
        {
            Gdiplus::GraphicsPath p;
            float r = bigH * 0.42f;
            p.AddArc(bigRc.X, bigRc.Y, r * 2, r * 2, 180, 90);
            p.AddArc(bigRc.GetRight() - r * 2, bigRc.Y, r * 2, r * 2, 270, 90);
            p.AddArc(bigRc.GetRight() - r * 2, bigRc.GetBottom() - r * 2, r * 2, r * 2, 0, 90);
            // 尾巴：左下
            p.AddLine(bigRc.X + r * 1.6f, bigRc.GetBottom(),
                      bigRc.X - bigRc.Width * 0.16f, bigRc.GetBottom() + bigRc.Height * 0.28f);
            p.AddLine(bigRc.X - bigRc.Width * 0.16f, bigRc.GetBottom() + bigRc.Height * 0.28f,
                      bigRc.X + r * 0.6f, bigRc.GetBottom());
            p.AddArc(bigRc.X, bigRc.GetBottom() - r * 2, r * 2, r * 2, 90, 90);
            p.CloseFigure();
            g.FillPath(&brush, &p);
        }
        // 两个气泡内的 3 个小点（用更深一档的灰，让它在白底气泡上能看见）
        Gdiplus::Color clrDot(255, 240, 240, 240);
        Gdiplus::SolidBrush dotBrush(clrDot);
        float dotR = wmSide * 0.025f;
        // big 气泡内 3 点
        for (int i = 0; i < 3; ++i)
        {
            float dx = bigRc.X + bigRc.Width  * (0.30f + 0.20f * i);
            float dy = bigRc.Y + bigRc.Height * 0.50f;
            g.FillEllipse(&dotBrush, dx - dotR, dy - dotR, dotR * 2, dotR * 2);
        }
        // small 气泡内 3 点
        for (int i = 0; i < 3; ++i)
        {
            float dx = smRc.X + smRc.Width  * (0.30f + 0.20f * i);
            float dy = smRc.Y + smRc.Height * 0.50f;
            g.FillEllipse(&dotBrush, dx - dotR, dy - dotR, dotR * 2, dotR * 2);
        }
    }
};

// =========================================================================
// LogoImage —— 右栏空状态的居中 PNG 占位（同 LoginFrame::QrImage 思路，
// 但额外做"image 维持原始比例 + 居中绘制"，因为 logo 不能被拉伸）。
// =========================================================================
class LogoImage : public DuiControl
{
public:
    explicit LogoImage(HBITMAP hbm) : m_hbm(hbm) {}
    ~LogoImage() override
    {
        if (m_hbm) { ::DeleteObject(m_hbm); }
    }

    void OnPaint(HDC hdc, const RECT&) override
    {
        if (!m_hbm || !m_bVisible) { return; }
        BITMAP bm = {};
        ::GetObject(m_hbm, sizeof(bm), &bm);
        if (bm.bmWidth <= 0 || bm.bmHeight <= 0) { return; }

        // 原尺寸居中。占满 cell 没意义 —— logo 一般小于 cell。
        int cellW = m_rcItem.right - m_rcItem.left;
        int cellH = m_rcItem.bottom - m_rcItem.top;
        int x = m_rcItem.left + (cellW - bm.bmWidth)  / 2;
        int y = m_rcItem.top  + (cellH - bm.bmHeight) / 2;

        HDC mem = ::CreateCompatibleDC(hdc);
        HGDIOBJ old = ::SelectObject(mem, m_hbm);
        // PNG with alpha channel — 用 AlphaBlend 而非 BitBlt，否则透明
        // 区被画成黑。
        BLENDFUNCTION bf = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
        ::AlphaBlend(hdc, x, y, bm.bmWidth, bm.bmHeight,
                     mem, 0, 0, bm.bmWidth, bm.bmHeight, bf);
        ::SelectObject(mem, old);
        ::DeleteDC(mem);
    }

private:
    HBITMAP m_hbm = nullptr;
};

// =========================================================================
// FrequentContactsRow —— 公众号页"常看的号"行（Phase 7）。
//
// 视觉：6 个 36×36 圆头像横排 + 行右侧 "更多 >" 文字。每个头像下面没有
// 名字（参考截图里也是这样 —— 名字小到看不见，做 demo 时直接省掉）。
// 静态 6 个写死头像（letter + 配色），点击 / hover 不接。
// =========================================================================
class FrequentContactsRow : public DuiControl
{
public:
    void OnPaint(HDC hdc, const RECT&) override
    {
        if (!m_bVisible) { return; }

        struct Av { TCHAR letter; COLORREF bg; LPCTSTR name; };
        static const Av kAvs[] =
        {
            { _T('云'), RGB(245, 158, 10),  _T("云头条") },
            { _T('叶'), RGB( 22, 163, 74),  _T("叶小校") },
            { _T('P'),  RGB( 51, 121, 200), _T("Python...")},
            { _T('f'),  RGB(217,  70, 239), _T("findyi") },
            { _T('程'), RGB(  6, 182, 212), _T("程序员鱼皮") },
            { _T('托'), RGB(168,  85, 247), _T("托腾阿秀") },
        };

        int avSz   = 38;
        int gap    = 14;
        int x      = m_rcItem.left + 16;       // 行左 padding
        int avY    = m_rcItem.top  + 22;       // 给上方 "常看的号" / "更多" 标题留 18px
        int nameY  = avY + avSz + 4;

        HFONT hOldFont = (HFONT)::SelectObject(hdc, balloonwjui::DuiResMgr::Inst().GetDefaultFont());
        int oldBk = ::SetBkMode(hdc, TRANSPARENT);

        // 顶部 "常看的号" 标题
        {
            COLORREF oldClr = ::SetTextColor(hdc, RGB(120, 120, 120));
            RECT rcCap = { m_rcItem.left + 16, m_rcItem.top + 4,
                           m_rcItem.left + 200, m_rcItem.top + 22 };
            ::DrawText(hdc, _T("常看的号"), -1, &rcCap,
                       DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);
            ::SetTextColor(hdc, oldClr);
        }

        for (size_t i = 0; i < sizeof(kAvs) / sizeof(kAvs[0]); ++i)
        {
            // 圆头像（FillEllipse 抗锯齿）
            RECT rcAv = { x, avY, x + avSz, avY + avSz };
            balloonwjui::DuiAA::FillEllipse(hdc, rcAv, kAvs[i].bg);
            // 单字
            COLORREF oldClr = ::SetTextColor(hdc, RGB(255, 255, 255));
            TCHAR ch[2] = { kAvs[i].letter, 0 };
            ::DrawText(hdc, ch, 1, &rcAv,
                       DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            // 名字小字
            ::SetTextColor(hdc, RGB(120, 120, 120));
            RECT rcName = { x - 6, nameY, x + avSz + 6, nameY + 16 };
            ::DrawText(hdc, kAvs[i].name, -1, &rcName,
                       DT_CENTER | DT_TOP | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
            ::SetTextColor(hdc, oldClr);

            x += avSz + gap;
        }

        // 行右上 "更多 >"
        COLORREF oldClr = ::SetTextColor(hdc, RGB(140, 140, 140));
        RECT rcMore = { m_rcItem.right - 80, m_rcItem.top + 4,
                        m_rcItem.right - 14, m_rcItem.top + 22 };
        ::DrawText(hdc, _T("更多 >"), -1, &rcMore,
                   DT_RIGHT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);
        ::SetTextColor(hdc, oldClr);

        ::SetBkMode(hdc, oldBk);
        ::SelectObject(hdc, hOldFont);
    }
};

// =========================================================================
// ArticleList —— 公众号页文章卡列表（Phase 7）。
//
// Phase 7 只画 3 张写死的卡片，每张高度 ~150px，纵向堆叠。没有 scroll ——
// 卡数量少，视口够装；后续要加卡时换 DuiVirtualList + 变高 paint。
//
// 单卡布局：
//   ┌───────────────────────────────────────────────┐
//   │ [Av] 公众号名                       3 分钟前  │
//   │ 文章标题（粗，单行截断）                      │
//   │ 文章预览第一行（小灰，多行换行）  [thumb]     │
//   │ 余 N 朋友在看                                 │
//   └───────────────────────────────────────────────┘
//
// 卡片用 1px 浅灰底分隔（不画卡边框 —— 参考截图里是 list 风格，不是 card
// 风格）。
// =========================================================================
class ArticleList : public DuiControl
{
public:
    void OnPaint(HDC hdc, const RECT&) override
    {
        if (!m_bVisible) { return; }

        struct Article {
            TCHAR    avLetter;
            COLORREF avBg;
            LPCTSTR  pubName;
            LPCTSTR  time;
            LPCTSTR  title;
            LPCTSTR  preview;
            LPCTSTR  footer;
        };
        static const Article kArts[] =
        {
            { _T('J'), RGB(7, 193, 96),
              _T("JavaGuide"),         _T("3 分钟前"),
              _T("收到工资 11824.15 元，爱你 DeepSeek！"),
              _T("深度求索发福利，这次是 8000 元的 DeepSeek 大礼包。如果你也想体验请戳 →"),
              _T("46 朋友在看") },
            { _T('程'), RGB(245, 158, 10),
              _T("程序员大彪"),         _T("55 分钟前"),
              _T("如何热议：为什么大多数程序员都不做个人独立开发？"),
              _T("最近有个程序员朋友问我，为什么大家都讨论独立开发，但真去做的人很少？我想了几个原因..."),
              _T("余 7 朋友在看") },
            { _T('阿'), RGB(51, 121, 200),
              _T("阿Q技术站"),         _T("1 个小时前"),
              _T("【春招面经】滴滴 C++ 一面通过，问的很奇，但又不完全脱离项目的八股拷打..."),
              _T("一面 60 分钟，面试官人挺好，主要围绕项目深入问，技术栈 C++/Linux/Redis..."),
              _T("1 个朋友在看") },
        };

        int x         = m_rcItem.left;
        int y         = m_rcItem.top + 4;
        int cardW     = m_rcItem.right - m_rcItem.left;
        int cardPadX  = 16;
        int cardH     = 150;
        int avSz      = 32;

        HFONT hOldFont = (HFONT)::SelectObject(hdc, balloonwjui::DuiResMgr::Inst().GetDefaultFont());
        int oldBk = ::SetBkMode(hdc, TRANSPARENT);

        for (size_t i = 0; i < sizeof(kArts) / sizeof(kArts[0]); ++i)
        {
            const Article& a = kArts[i];

            // 卡顶部分隔线（除第一张以外）
            if (i > 0)
            {
                HPEN hSep = ::CreatePen(PS_SOLID, 1, RGB(232, 232, 232));
                HPEN hOldPen = (HPEN)::SelectObject(hdc, hSep);
                ::MoveToEx(hdc, x + cardPadX,        y, nullptr);
                ::LineTo  (hdc, x + cardW - cardPadX, y);
                ::SelectObject(hdc, hOldPen);
                ::DeleteObject(hSep);
            }

            int innerY = y + 12;

            // 行 1：avatar + 公众号名 + 右上时间
            RECT rcAv = { x + cardPadX, innerY, x + cardPadX + avSz, innerY + avSz };
            HBRUSH hbr = ::CreateSolidBrush(a.avBg);
            HRGN hrgn = ::CreateRoundRectRgn(rcAv.left, rcAv.top, rcAv.right + 1, rcAv.bottom + 1, 8, 8);
            ::FillRgn(hdc, hrgn, hbr);
            ::DeleteObject(hrgn);
            ::DeleteObject(hbr);
            COLORREF oldClr = ::SetTextColor(hdc, RGB(255, 255, 255));
            TCHAR ch[2] = { a.avLetter, 0 };
            ::DrawText(hdc, ch, 1, &rcAv,
                       DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

            // 公众号名
            ::SetTextColor(hdc, RGB(40, 40, 40));
            RECT rcPub = { x + cardPadX + avSz + 8, innerY,
                           x + cardW - cardPadX - 80, innerY + avSz / 2 + 4 };
            ::DrawText(hdc, a.pubName, -1, &rcPub,
                       DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);
            // 时间
            ::SetTextColor(hdc, RGB(150, 150, 150));
            RECT rcTime = { x + cardW - cardPadX - 80, innerY,
                            x + cardW - cardPadX,     innerY + 18 };
            ::DrawText(hdc, a.time, -1, &rcTime,
                       DT_RIGHT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);

            // 行 2：标题（左边占 2/3，右边给 thumb 留位置）
            int titleY = innerY + avSz + 8;
            ::SetTextColor(hdc, RGB(20, 20, 20));
            RECT rcTitle = { x + cardPadX, titleY,
                             x + cardW - cardPadX - 92, titleY + 24 };
            ::DrawText(hdc, a.title, -1, &rcTitle,
                       DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);

            // 行 3：preview 多行 + 右侧 thumb
            int previewY = titleY + 26;
            ::SetTextColor(hdc, RGB(120, 120, 120));
            RECT rcPreview = { x + cardPadX, previewY,
                               x + cardW - cardPadX - 92, previewY + 36 };
            ::DrawText(hdc, a.preview, -1, &rcPreview,
                       DT_LEFT | DT_TOP | DT_NOPREFIX | DT_WORDBREAK | DT_END_ELLIPSIS);

            // thumb 占位（80×60 浅灰圆角）
            RECT rcThumb = { x + cardW - cardPadX - 80, titleY + 4,
                             x + cardW - cardPadX,     titleY + 64 };
            HBRUSH hbrT = ::CreateSolidBrush(RGB(220, 220, 220));
            HRGN hrgnT = ::CreateRoundRectRgn(rcThumb.left, rcThumb.top,
                                              rcThumb.right + 1, rcThumb.bottom + 1, 6, 6);
            ::FillRgn(hdc, hrgnT, hbrT);
            ::DeleteObject(hrgnT);
            ::DeleteObject(hbrT);

            // 行 4：底部"余 N 朋友在看"
            int footerY = previewY + 38;
            ::SetTextColor(hdc, RGB(150, 150, 150));
            RECT rcFooter = { x + cardPadX, footerY,
                              x + cardW - cardPadX, footerY + 18 };
            ::DrawText(hdc, a.footer, -1, &rcFooter,
                       DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);

            ::SetTextColor(hdc, oldClr);
            y += cardH;
        }

        ::SetBkMode(hdc, oldBk);
        ::SelectObject(hdc, hOldFont);
    }
};

// =========================================================================
// ContactMgmtButton —— "通讯录管理" 大按钮（Phase 6）。
//
// 按参考 联系人.png：白底 + 浅灰 1px 边框 + 圆角 + 中央 "👥 通讯录管理"
// 文字（人形 icon 占位用一个简化记号 + 文本）。Phase 6 静态，不接 hover /
// 点击。
// =========================================================================
class ContactMgmtButton : public DuiControl
{
public:
    void OnPaint(HDC hdc, const RECT&) override
    {
        if (!m_bVisible) { return; }

        // 1) 白底 + 圆角矩形 + 浅灰边框
        HBRUSH hbr = ::CreateSolidBrush(RGB(255, 255, 255));
        HRGN hrgn = ::CreateRoundRectRgn(m_rcItem.left, m_rcItem.top,
                                         m_rcItem.right + 1, m_rcItem.bottom + 1,
                                         8, 8);
        ::FillRgn(hdc, hrgn, hbr);
        ::DeleteObject(hrgn);
        ::DeleteObject(hbr);

        HPEN hPen = ::CreatePen(PS_SOLID, 1, RGB(220, 220, 220));
        HPEN hOldPen = (HPEN)::SelectObject(hdc, hPen);
        HBRUSH hOldBr = (HBRUSH)::SelectObject(hdc, ::GetStockObject(NULL_BRUSH));
        ::RoundRect(hdc, m_rcItem.left, m_rcItem.top,
                    m_rcItem.right, m_rcItem.bottom, 8, 8);
        ::SelectObject(hdc, hOldBr);
        ::SelectObject(hdc, hOldPen);
        ::DeleteObject(hPen);

        // 2) 中央 "👥 通讯录管理" —— icon 占位用两个堆叠的小圆，文字跟在右边
        int cx = (m_rcItem.left + m_rcItem.right) / 2;
        int cy = (m_rcItem.top  + m_rcItem.bottom) / 2;

        // 测算文字宽度，把 icon + 文字组合居中
        HFONT hOldFont = (HFONT)::SelectObject(hdc, DuiResMgr::Inst().GetDefaultFont());
        SIZE sz = {};
        LPCTSTR text = _T("通讯录管理");
        ::GetTextExtentPoint32(hdc, text, lstrlen(text), &sz);
        int iconW = 22;     // 占位 icon 宽
        int gap   = 8;
        int total = iconW + gap + sz.cx;
        int baseX = cx - total / 2;

        // icon：两个圆（用 DuiAA 抗锯齿），代表"两个人"
        int iconCx1 = baseX + 6;
        int iconCx2 = baseX + 16;
        int iconCy  = cy;
        RECT rcDot1 = { iconCx1 - 5, iconCy - 5, iconCx1 + 5, iconCy + 5 };
        RECT rcDot2 = { iconCx2 - 5, iconCy - 5, iconCx2 + 5, iconCy + 5 };
        balloonwjui::DuiAA::FillEllipse(hdc, rcDot1, RGB(60, 60, 60));
        balloonwjui::DuiAA::FillEllipse(hdc, rcDot2, RGB(60, 60, 60));

        // 文字
        int oldBk = ::SetBkMode(hdc, TRANSPARENT);
        COLORREF oldClr = ::SetTextColor(hdc, RGB(40, 40, 40));
        RECT rcText = { baseX + iconW + gap, m_rcItem.top,
                        m_rcItem.right, m_rcItem.bottom };
        ::DrawText(hdc, text, -1, &rcText,
                   DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        ::SetTextColor(hdc, oldClr);
        ::SetBkMode(hdc, oldBk);
        ::SelectObject(hdc, hOldFont);
    }
};

// =========================================================================
// ContactCategoryList —— 联系人页中栏的分类列表，可点击展开/折叠 + 滚动。
//
// 7 个分类，每个有一组虚构 mock children（"新的朋友" 为空）。
//   · 主分类行：44 px 高，chevron（▶折叠 / ▼展开）+ name + 右侧 count
//   · 子项行：36 px 高，左缩进 + 24×24 圆头像 + name
//
// 联系人 cat 用 100 姓 × 50 名组合程序生成 3071 个虚构联系人，"展开后
// 能下拉到底"——内部维护 m_scrollY，OnMouseWheel 上下滚动；超出视口的
// 子项仍走 paint 路径（用 viewport 矩形快速 cull），不进 paint 的行没
// GDI 开销。
//
// hover：主分类行底色变浅灰 + 光标变手型。子项行不接 hover/click（demo
// 没真"打开会话"逻辑）。
// =========================================================================
class ContactCategoryList : public DuiControl
{
public:
    ContactCategoryList()
    {
        for (int i = 0; i < kCatCount; ++i) { m_expanded[i] = false; }
        m_expanded[6] = true;     // 联系人 默认展开

        // 内嵌纵向 DuiScrollBar：作为 child 控件，让 host dispatch 路径
        // 自动处理 scrollbar 的鼠标事件（thumb 拖动 / 轨道点击 / line/page）。
        // 自己 OnPaint 用 m_sb->GetPos() 作为内容区滚动偏移。SetOnScroll
        // 回调让 thumb 拖动时立即 Invalidate 重画内容（不靠 DUI_NOTIFY
        // 转一圈）。
        auto sb = std::unique_ptr<DuiScrollBar>(new DuiScrollBar(/*horizontal=*/false));
        sb->SetLineSize(36);     // 一档滚轮 = 一行子项高
        sb->SetAutoHide(true);   // 默认隐藏，hover/scroll 渐入
        sb->SetOnScroll(&OnScrollThunk, this);
        m_sb = sb.get();
        AddChild(std::move(sb));
    }

    void Layout(const RECT& rcAvail) override
    {
        m_rcItem = rcAvail;
        // scrollbar 占右侧 kSbWidth；内容区在左侧。
        // 注意：DuiControl::Layout 默认实现<u>不</u>设 m_rcItem，只是把
        // rcAvail forward 给 children。要让 sb 真的拿到 rect 必须调
        // SetRect（它会设 m_rcItem 并触发自家 Layout）。
        if (m_sb)
        {
            RECT rcSb = { rcAvail.right - kSbWidth, rcAvail.top,
                          rcAvail.right,            rcAvail.bottom };
            m_sb->SetRect(rcSb);
            UpdateScrollRange_();
        }
    }

    bool OnLButtonUp(POINT pt, UINT) override
    {
        int catIdx = HitCategoryRow(pt.y);
        if (catIdx >= 0)
        {
            m_expanded[catIdx] = !m_expanded[catIdx];
            UpdateScrollRange_();
            Invalidate();
            return true;
        }
        return false;
    }

    bool OnMouseMove(POINT pt, UINT) override
    {
        if (m_sb) { m_sb->TriggerShow(); }
        int catIdx = HitCategoryRow(pt.y);
        if (catIdx != m_hoverCat)
        {
            m_hoverCat = catIdx;
            Invalidate();
        }
        return true;
    }

    bool OnMouseLeave() override
    {
        if (m_hoverCat != -1)
        {
            m_hoverCat = -1;
            Invalidate();
        }
        if (m_sb) { m_sb->StartFadeOut(); }
        return true;
    }

    bool OnSetCursor(POINT pt) override
    {
        if (HitCategoryRow(pt.y) >= 0)
        {
            ::SetCursor(::LoadCursor(nullptr, IDC_HAND));
            return true;
        }
        return false;
    }

    bool OnMouseWheel(POINT, short zDelta, UINT) override
    {
        if (!m_sb) { return false; }
        m_sb->TriggerShow();
        int delta = (int)((double)zDelta * 60.0 / (double)WHEEL_DELTA);
        m_sb->SetPos(m_sb->GetPos() - delta, /*notify*/true);
        return true;
    }

    void OnPaint(HDC hdc, const RECT& rcDirty) override
    {
        if (!m_bVisible) { return; }

        int scrollY = m_sb ? m_sb->GetPos() : 0;
        int x = m_rcItem.left;
        int y = m_rcItem.top - scrollY;       // 应用滚动偏移
        int w = (m_rcItem.right - kSbWidth) - m_rcItem.left;   // 内容区减去 sb 宽

        HFONT hOldFont = (HFONT)::SelectObject(hdc, DuiResMgr::Inst().GetDefaultFont());
        int oldBk = ::SetBkMode(hdc, TRANSPARENT);

        const auto& cats = Cats();
        for (int i = 0; i < kCatCount; ++i)
        {
            const Cat& cat = cats[i];

            // 主分类行 —— 在视口内才画
            if (y + kCatRowH > m_rcItem.top && y < m_rcItem.bottom)
            {
                if (i == m_hoverCat)
                {
                    RECT rcRow = { x, y, x + w, y + kCatRowH };
                    HBRUSH hbr = ::CreateSolidBrush(RGB(238, 238, 238));
                    ::FillRect(hdc, &rcRow, hbr);
                    ::DeleteObject(hbr);
                }
                DrawChevron(hdc, x + 16, y + kCatRowH / 2, m_expanded[i]);

                COLORREF oldClr = ::SetTextColor(hdc, RGB(40, 40, 40));
                RECT rcName = { x + 32, y, x + w - 60, y + kCatRowH };
                ::DrawText(hdc, cat.name, -1, &rcName,
                           DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
                ::SetTextColor(hdc, RGB(150, 150, 150));
                RECT rcCount = { x + w - 60, y, x + w - 14, y + kCatRowH };
                ::DrawText(hdc, cat.count, -1, &rcCount,
                           DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
                ::SetTextColor(hdc, oldClr);
            }
            y += kCatRowH;

            if (m_expanded[i])
            {
                int n = (int)cat.children.size();
                // 跳过完全在视口上方的子项行（不调 paint）
                if (y + n * kChildRowH < m_rcItem.top)
                {
                    y += n * kChildRowH;
                    continue;
                }
                for (int j = 0; j < n; ++j)
                {
                    if (y >= m_rcItem.bottom) { break; }    // 视口下方剩余跳出
                    if (y + kChildRowH > m_rcItem.top)      // 在视口内才画
                    {
                        PaintChildRow(hdc, x, y, w, cat.children[j]);
                    }
                    y += kChildRowH;
                }
            }
        }

        ::SetBkMode(hdc, oldBk);
        ::SelectObject(hdc, hOldFont);

        // 让基类 OnPaint 把 scrollbar child 画出来。
        DuiControl::OnPaint(hdc, rcDirty);
    }

private:
    struct Child { TCHAR letter; COLORREF bg; CString name; };
    struct Cat   { CString name; CString count; std::vector<Child> children; };

    // ---- mock 数据：lazy build，第一次调用初始化全部 7 个分类 -----------
    //
    // 联系人 cat 用 100 姓 × 50 名 + 8 色循环组合生成 3071 个虚构联系人。
    // 没用任何"主面板会话列表里出现过的真人名"（张小黑/张小方/范鑫/胡鑫
    // /思无邪/张玲/666/金灿/刘康/马昆/方海舟 zh/洪成刚/张伟），全部独立
    // 命名空间。
    static const std::vector<Cat>& Cats()
    {
        static std::vector<Cat> cats;
        if (!cats.empty()) { return cats; }

        cats.reserve(kCatCount);

        // [0] 新的朋友 — 空
        cats.push_back({ _T("新的朋友"), _T(""), {} });

        // [1] 群聊 — 5 个虚构群
        cats.push_back({ _T("群聊"), _T("93"), std::vector<Child>{
            {_T('开'), RGB(245, 158,  10), _T("开发者交流群")        },
            {_T('运'), RGB(168,  85, 247), _T("运维分享小组")        },
            {_T('技'), RGB(  6, 182, 212), _T("技术干货精选")        },
            {_T('周'), RGB( 51, 121, 200), _T("周末户外活动群")      },
            {_T('美'), RGB( 22, 163,  74), _T("美食探店群")          },
        }});

        // [2] 公众号 — 6 个虚构号
        cats.push_back({ _T("公众号"), _T("243"), std::vector<Child>{
            {_T('科'), RGB(  7, 193,  96), _T("科技日报")            },
            {_T('架'), RGB(  7, 193,  96), _T("架构师之路")          },
            {_T('深'), RGB(245, 158,  10), _T("深度学习实战")        },
            {_T('云'), RGB( 51, 121, 200), _T("云原生周刊")          },
            {_T('安'), RGB(245, 158,  10), _T("安全圈早报")          },
            {_T('开'), RGB( 22, 163,  74), _T("开源观察")            },
        }});

        // [3] 服务号 — 3 个
        cats.push_back({ _T("服务号"), _T("48"), std::vector<Child>{
            {_T('银'), RGB(  7, 193,  96), _T("某行信用卡服务")      },
            {_T('快'), RGB( 51, 121, 200), _T("快递查询助手")        },
            {_T('医'), RGB(217,  70, 239), _T("医院预约挂号")        },
        }});

        // [4] 企业某信联系人 — 3 个
        cats.push_back({ _T("企业某信联系人"), _T("16"), std::vector<Child>{
            {_T('客'), RGB(245, 158,  10), _T("客户经理-小田")       },
            {_T('销'), RGB(  6, 182, 212), _T("销售顾问-阿凯")       },
            {_T('技'), RGB( 51, 121, 200), _T("技术支持-老于")       },
        }});

        // [5] 我的企业 — 1 个
        cats.push_back({ _T("我的企业"), _T("1"), std::vector<Child>{
            {_T('神'), RGB(  7, 193,  96), _T("神行太保科技")        },
        }});

        // [6] 联系人 — 用水浒主题数据生成 3071 个独特联系人。
        //   · 前 108 = 水浒 108 将真名（"宋江""卢俊义""吴用"...）—— 历史
        //     人物，无重复，最有梁山味。
        //   · 后 2963 = 30 绰号 × 100 姓 拼接（"豹子头陈""及时雨李"...），
        //     30 × 99 = 2970 unique 槽，截 2963 个用，刚够。
        //
        // letter 取真名 / 绰号的首字。bg 走 8 色循环。
        std::vector<Child> friends_;
        friends_.reserve(3071);

        // ---- 108 将真名 ---------------------------------------------------
        static const LPCTSTR k108[] = {
            _T("宋江"),  _T("卢俊义"),_T("吴用"),  _T("公孙胜"),_T("关胜"),  _T("林冲"),  _T("秦明"),  _T("呼延灼"),
            _T("花荣"),  _T("柴进"),  _T("李应"),  _T("朱仝"),  _T("鲁智深"),_T("武松"),  _T("董平"),  _T("张清"),
            _T("杨志"),  _T("徐宁"),  _T("索超"),  _T("戴宗"),  _T("刘唐"),  _T("李逵"),  _T("史进"),  _T("穆弘"),
            _T("雷横"),  _T("李俊"),  _T("阮小二"),_T("张横"),  _T("阮小五"),_T("张顺"),  _T("阮小七"),_T("杨雄"),
            _T("石秀"),  _T("解珍"),  _T("解宝"),  _T("燕青"),  _T("朱武"),  _T("黄信"),  _T("孙立"),  _T("宣赞"),
            _T("郝思文"),_T("韩滔"),  _T("彭玘"),  _T("单廷珪"),_T("魏定国"),_T("萧让"),  _T("裴宣"),  _T("欧鹏"),
            _T("邓飞"),  _T("燕顺"),  _T("杨林"),  _T("凌振"),  _T("蒋敬"),  _T("吕方"),  _T("郭盛"),  _T("安道全"),
            _T("皇甫端"),_T("王英"),  _T("扈三娘"),_T("鲍旭"),  _T("樊瑞"),  _T("孔明"),  _T("孔亮"),  _T("项充"),
            _T("李衮"),  _T("金大坚"),_T("马麟"),  _T("童威"),  _T("童猛"),  _T("孟康"),  _T("侯健"),  _T("陈达"),
            _T("杨春"),  _T("郑天寿"),_T("陶宗旺"),_T("宋清"),  _T("乐和"),  _T("龚旺"),  _T("丁得孙"),_T("穆春"),
            _T("曹正"),  _T("宋万"),  _T("杜迁"),  _T("薛永"),  _T("施恩"),  _T("李忠"),  _T("周通"),  _T("汤隆"),
            _T("杜兴"),  _T("邹渊"),  _T("邹润"),  _T("朱贵"),  _T("朱富"),  _T("蔡福"),  _T("蔡庆"),  _T("李立"),
            _T("李云"),  _T("焦挺"),  _T("石勇"),  _T("孙新"),  _T("顾大嫂"),_T("张青"),  _T("孙二娘"),_T("王定六"),
            _T("郁保四"),_T("白胜"),  _T("时迁"),  _T("段景住"),
        };

        // ---- 30 个梁山经典绰号 -------------------------------------------
        static const LPCTSTR kNick[] = {
            _T("及时雨"),  _T("玉麒麟"),_T("智多星"),_T("入云龙"),_T("大刀"),    _T("豹子头"),
            _T("霹雳火"),  _T("双鞭"),  _T("小李广"),_T("小旋风"),_T("扑天雕"),  _T("美髯公"),
            _T("花和尚"),  _T("行者"),  _T("双枪将"),_T("没羽箭"),_T("青面兽"),  _T("金枪手"),
            _T("急先锋"),  _T("神行太保"),_T("赤发鬼"),_T("黑旋风"),_T("九纹龙"),_T("没遮拦"),
            _T("插翅虎"),  _T("混江龙"),_T("立地太岁"),_T("船火儿"),_T("浪里白条"),_T("活阎罗"),
        };

        // ---- 100 姓（与原版一致；用作绰号后缀） ---------------------------
        static const LPCTSTR kSur[] = {
            _T("安"), _T("白"), _T("蔡"), _T("曹"), _T("陈"), _T("成"), _T("程"), _T("崔"),
            _T("戴"), _T("邓"), _T("董"), _T("窦"), _T("杜"), _T("段"), _T("樊"), _T("范"),
            _T("方"), _T("冯"), _T("付"), _T("傅"), _T("高"), _T("龚"), _T("谷"), _T("顾"),
            _T("管"), _T("郭"), _T("韩"), _T("郝"), _T("何"), _T("贺"), _T("洪"), _T("侯"),
            _T("胡"), _T("华"), _T("黄"), _T("霍"), _T("姬"), _T("贾"), _T("江"), _T("姜"),
            _T("蒋"), _T("焦"), _T("金"), _T("孔"), _T("赖"), _T("蓝"), _T("郎"), _T("黎"),
            _T("李"), _T("廉"), _T("梁"), _T("廖"), _T("林"), _T("凌"), _T("刘"), _T("龙"),
            _T("卢"), _T("陆"), _T("吕"), _T("罗"), _T("马"), _T("毛"), _T("孟"), _T("莫"),
            _T("欧"), _T("潘"), _T("彭"), _T("齐"), _T("钱"), _T("乔"), _T("秦"), _T("邱"),
            _T("曲"), _T("任"), _T("沈"), _T("施"), _T("石"), _T("史"), _T("舒"), _T("宋"),
            _T("苏"), _T("孙"), _T("谭"), _T("汤"), _T("唐"), _T("陶"), _T("田"), _T("童"),
            _T("万"), _T("汪"), _T("王"), _T("韦"), _T("卫"), _T("魏"), _T("吴"), _T("武"),
            _T("夏"), _T("肖"), _T("谢"), _T("徐"),
        };
        static const COLORREF kBg[] = {
            RGB( 51, 121, 200), RGB(  7, 193,  96), RGB(245, 158,  10),
            RGB(168,  85, 247), RGB(  6, 182, 212), RGB(217,  70, 239),
            RGB( 22, 163,  74), RGB(101, 113, 130),
        };
        const int n108 = sizeof(k108) / sizeof(k108[0]);   // 108
        const int nNick = sizeof(kNick)/ sizeof(kNick[0]); // 30
        const int nSur  = sizeof(kSur) / sizeof(kSur[0]);  // 100
        const int nBg   = sizeof(kBg)  / sizeof(kBg[0]);   // 8

        // 前 108：真名
        for (int i = 0; i < n108; ++i)
        {
            Child c;
            c.letter = k108[i][0];
            c.bg     = kBg[i % nBg];
            c.name   = k108[i];
            friends_.push_back(std::move(c));
        }
        // 后 2963：绰号 + 姓
        const int kRem = 3071 - n108;     // 2963
        for (int i = 0; i < kRem; ++i)
        {
            const TCHAR* nick = kNick[i % nNick];
            const TCHAR* sur  = kSur [(i / nNick) % nSur];
            Child c;
            c.letter = nick[0];
            c.bg     = kBg[(i + n108) % nBg];   // +n108 防 idx 0 重复颜色与第 1 个真名
            c.name   = CString(nick) + CString(sur);
            friends_.push_back(std::move(c));
        }
        cats.push_back({ _T("联系人"), _T("3071"), std::move(friends_) });

        return cats;
    }

    // 计算总内容像素高度（所有 cat header + 已展开 cat 的 children 行）。
    int ContentHeight_() const
    {
        const auto& cats = Cats();
        int h = 0;
        for (int i = 0; i < kCatCount; ++i)
        {
            h += kCatRowH;
            if (m_expanded[i])
            {
                h += (int)cats[i].children.size() * kChildRowH;
            }
        }
        return h;
    }

    // 同步 scrollbar 的 range / page。expand 状态变化或控件大小变化时调。
    // DuiScrollBar 的 max 语义是"最大有效 pos" = contentH - viewH（不是
    // contentH 本身）。viewH ≤ contentH 时正常；> contentH 时 max=0 隐 thumb。
    void UpdateScrollRange_()
    {
        if (!m_sb) { return; }
        int viewH    = m_rcItem.bottom - m_rcItem.top;
        int contentH = ContentHeight_();
        int maxPos   = contentH - viewH;
        if (maxPos < 0) { maxPos = 0; }
        m_sb->SetRange(0, maxPos);
        m_sb->SetPage(viewH);
        // 老 pos 可能超过新 max，让 SetPos 自动 clamp。
        m_sb->SetPos(m_sb->GetPos(), /*notify*/false);
    }

    static void OnScrollThunk(void* user, int /*newPos*/)
    {
        if (auto* self = static_cast<ContactCategoryList*>(user))
        {
            self->Invalidate();
        }
    }

    // 命中测试：给一个 host-coord y，返回是哪个分类的 header 行（0..6），
    // 不是 header 行（在子项区或视口外）返回 -1。处理 scrollbar 偏移。
    int HitCategoryRow(int hitY) const
    {
        const auto& cats = Cats();
        int scrollY = m_sb ? m_sb->GetPos() : 0;
        int y = m_rcItem.top - scrollY;
        for (int i = 0; i < kCatCount; ++i)
        {
            if (hitY >= y && hitY < y + kCatRowH)
            {
                return i;
            }
            y += kCatRowH;
            if (m_expanded[i])
            {
                y += (int)cats[i].children.size() * kChildRowH;
            }
        }
        return -1;
    }

    static void DrawChevron(HDC hdc, int cx, int cy, bool expanded)
    {
        const COLORREF clr = RGB(140, 140, 140);
        if (expanded)
        {
            POINT pts[3] = { { cx - 4, cy - 2 }, { cx + 4, cy - 2 }, { cx, cy + 3 } };
            balloonwjui::DuiAA::FillPolygon(hdc, pts, 3, clr);
        }
        else
        {
            POINT pts[3] = { { cx - 2, cy - 4 }, { cx - 2, cy + 4 }, { cx + 3, cy } };
            balloonwjui::DuiAA::FillPolygon(hdc, pts, 3, clr);
        }
    }

    static void PaintChildRow(HDC hdc, int x, int y, int w, const Child& c)
    {
        int avSz = 24;
        int avX  = x + 36;
        int avY  = y + (kChildRowH - avSz) / 2;
        RECT rcAv = { avX, avY, avX + avSz, avY + avSz };
        balloonwjui::DuiAA::FillEllipse(hdc, rcAv, c.bg);

        COLORREF oldClr = ::SetTextColor(hdc, RGB(255, 255, 255));
        TCHAR ch[2] = { c.letter, 0 };
        ::DrawText(hdc, ch, 1, &rcAv,
                   DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        ::SetTextColor(hdc, RGB(60, 60, 60));
        RECT rcName = { avX + avSz + 10, y, x + w - 14, y + kChildRowH };
        ::DrawText(hdc, c.name, -1, &rcName,
                   DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
        ::SetTextColor(hdc, oldClr);
    }

private:
    static const int kCatCount  = 7;
    static const int kCatRowH   = 44;
    static const int kChildRowH = 36;
    static const int kSbWidth   = 10;     // 滚动条宽度

    bool                            m_expanded[kCatCount];
    int                             m_hoverCat = -1;
    balloonwjui::DuiScrollBar*      m_sb       = nullptr;   // unique_ptr 的所有权移交了 child 树；这里只缓存裸指针快速访问
};

// =========================================================================
// MemberCell —— 群信息侧栏的成员小卡（Phase 5）。
//
// 视觉：32×32 圆角矩形头像（带 fallback 单字） + 下面一行小字昵称。整个
// cell 占 60×60 左右，4 列网格摆 4 个真成员 + 2 个占位（"添加" / "移除"
// 用空虚线框 + "+" / "−" 字符占位 —— 简化做法）。
//
// 静态：不接 hover / 点击 —— Phase 5 demo 不需要交互。
// =========================================================================
class MemberCell : public DuiControl
{
public:
    void SetLetter(TCHAR c)        { m_letter = c; }
    void SetAvatarBg(COLORREF c)   { m_avBg = c; }
    void SetName(LPCTSTR n)        { m_name = n ? n : _T(""); }
    void SetPlaceholderGlyph(TCHAR c) { m_placeholder = c; m_isPlaceholder = true; }

    void OnPaint(HDC hdc, const RECT&) override
    {
        if (!m_bVisible) { return; }

        int cellW = m_rcItem.right  - m_rcItem.left;
        int cellH = m_rcItem.bottom - m_rcItem.top;
        int avSz  = 36;

        int avX = m_rcItem.left + (cellW - avSz) / 2;
        int avY = m_rcItem.top  + 2;

        if (m_isPlaceholder)
        {
            // 虚线框 + 居中字符（"+" 或 "−"）
            HPEN hPen = ::CreatePen(PS_DASH, 1, RGB(180, 180, 180));
            HPEN hOldPen = (HPEN)::SelectObject(hdc, hPen);
            HBRUSH hOldBr = (HBRUSH)::SelectObject(hdc, ::GetStockObject(NULL_BRUSH));
            ::Rectangle(hdc, avX, avY, avX + avSz, avY + avSz);
            ::SelectObject(hdc, hOldBr);
            ::SelectObject(hdc, hOldPen);
            ::DeleteObject(hPen);

            HFONT hOldFont = (HFONT)::SelectObject(hdc, DuiResMgr::Inst().GetDefaultFont());
            int oldBk = ::SetBkMode(hdc, TRANSPARENT);
            COLORREF oldClr = ::SetTextColor(hdc, RGB(150, 150, 150));
            TCHAR ch[2] = { m_placeholder, 0 };
            RECT rcAv = { avX, avY, avX + avSz, avY + avSz };
            ::DrawText(hdc, ch, 1, &rcAv,
                       DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            ::SetTextColor(hdc, oldClr);
            ::SetBkMode(hdc, oldBk);
            ::SelectObject(hdc, hOldFont);
        }
        else
        {
            // 圆角矩形头像 + 单字
            HBRUSH hbr = ::CreateSolidBrush(m_avBg);
            HRGN hrgn = ::CreateRoundRectRgn(avX, avY, avX + avSz + 1, avY + avSz + 1, 10, 10);
            ::FillRgn(hdc, hrgn, hbr);
            ::DeleteObject(hrgn);
            ::DeleteObject(hbr);

            HFONT hOldFont = (HFONT)::SelectObject(hdc, DuiResMgr::Inst().GetDefaultFont());
            int oldBk = ::SetBkMode(hdc, TRANSPARENT);
            COLORREF oldClr = ::SetTextColor(hdc, RGB(255, 255, 255));
            TCHAR ch[2] = { m_letter, 0 };
            RECT rcAv = { avX, avY, avX + avSz, avY + avSz };
            ::DrawText(hdc, ch, 1, &rcAv,
                       DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            ::SetTextColor(hdc, oldClr);
            ::SetBkMode(hdc, oldBk);
            ::SelectObject(hdc, hOldFont);
        }

        // 名字行（cell 底部）
        HFONT hOldFont = (HFONT)::SelectObject(hdc, DuiResMgr::Inst().GetDefaultFont());
        int oldBk = ::SetBkMode(hdc, TRANSPARENT);
        COLORREF oldClr = ::SetTextColor(hdc, RGB(80, 80, 80));
        RECT rcName = { m_rcItem.left, avY + avSz + 2,
                        m_rcItem.right, m_rcItem.bottom };
        ::DrawText(hdc, m_isPlaceholder ? CString(_T("")) : m_name,
                   -1, &rcName,
                   DT_CENTER | DT_TOP | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
        // placeholder 的"添加" / "移除" 字样画在头像下方
        if (m_isPlaceholder)
        {
            CString placeholderName = (m_placeholder == _T('+')) ? _T("添加") : _T("移除");
            ::DrawText(hdc, placeholderName, -1, &rcName,
                       DT_CENTER | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);
        }
        ::SetTextColor(hdc, oldClr);
        ::SetBkMode(hdc, oldBk);
        ::SelectObject(hdc, hOldFont);
    }

private:
    TCHAR    m_letter        = _T(' ');
    COLORREF m_avBg          = RGB(120, 120, 120);
    CString  m_name;
    bool     m_isPlaceholder = false;
    TCHAR    m_placeholder   = _T('+');
};

// =========================================================================
// GroupMembersGrid —— 群信息侧栏的成员 grid（按 sessionIdx 动态取数据）。
//
// 之前 main.xml 里写死 4 个 member-cell + 添加/移除占位 = 不管点哪个群
// 都显示同一组人。改为单个 group-members 控件 + SetSessionIdx，由
// ApplySessionView 在切群时把 idx 灌进来；本控件按 Sessions()[idx].members
// 动态画 2×4 grid（前 8 名成员 + 末尾 +/- 占位）。
// =========================================================================
class GroupMembersGrid : public DuiControl
{
public:
    void SetSessionIdx(int idx)
    {
        if (m_sessionIdx == idx) { return; }
        m_sessionIdx = idx;
        Invalidate();
    }

    void OnPaint(HDC hdc, const RECT&) override
    {
        if (!m_bVisible) { return; }
        if (m_sessionIdx < 0) { return; }
        const auto& sessions = Sessions();
        if (m_sessionIdx >= (int)sessions.size()) { return; }
        const auto& members = sessions[m_sessionIdx].members;

        // 2 行 × 4 列 = 8 cell；最后两 cell 是 + / - 占位。每 cell 60×72，
        // gap 8。比之前的 60×60 加高 12px：原来 name 区只有 16 高，9pt
        // 中文字符（高 14-16px）的底部笔画刚好被 cell 下边界裁掉看着像
        // "字底盖住"。72 高 → name 区 28 高，字完整显示 + 余量。
        // group-members fixedHeight 也跟着提到 152（2×72 + gap 8）。
        const int kRows = 2;
        const int kCols = 4;
        const int kCellW = 60;
        const int kCellH = 72;
        const int kGap   = 8;
        const int totalW = kCols * kCellW + (kCols - 1) * kGap;
        int originX = m_rcItem.left + ((m_rcItem.right - m_rcItem.left) - totalW) / 2;
        int originY = m_rcItem.top;

        HFONT hOldFont = (HFONT)::SelectObject(hdc, DuiResMgr::Inst().GetDefaultFont());
        int oldBk = ::SetBkMode(hdc, TRANSPARENT);

        // 末两个 cell 固定 + / -。前 6 cell 展示 members[0..5]
        const int kRealMax = kRows * kCols - 2;        // 6
        for (int row = 0; row < kRows; ++row)
        {
            for (int col = 0; col < kCols; ++col)
            {
                int idx = row * kCols + col;
                int x = originX + col * (kCellW + kGap);
                int y = originY + row * (kCellH + kGap);

                if (idx < kRealMax && idx < (int)members.size())
                {
                    PaintMember_(hdc, x, y, kCellW, kCellH, members[idx]);
                }
                else if (idx == kRealMax)
                {
                    PaintPlaceholder_(hdc, x, y, kCellW, kCellH, _T('+'), _T("添加"));
                }
                else if (idx == kRealMax + 1)
                {
                    PaintPlaceholder_(hdc, x, y, kCellW, kCellH, _T('-'), _T("移除"));
                }
            }
        }

        ::SetBkMode(hdc, oldBk);
        ::SelectObject(hdc, hOldFont);
    }

private:
    static void PaintMember_(HDC hdc, int x, int y, int cellW, int /*cellH*/,
                             const GroupMember& m)
    {
        const int avSz = 36;
        int avX = x + (cellW - avSz) / 2;
        int avY = y + 2;

        // 圆角矩形头像（GDI+ AA path）
        Gdiplus::Graphics g(hdc);
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
        Gdiplus::REAL r = 8.0f;
        Gdiplus::REAL fX = (Gdiplus::REAL)avX;
        Gdiplus::REAL fY = (Gdiplus::REAL)avY;
        Gdiplus::REAL fW = (Gdiplus::REAL)avSz;
        Gdiplus::GraphicsPath path;
        path.AddArc(fX,            fY,            r * 2, r * 2, 180, 90);
        path.AddArc(fX + fW - r*2, fY,            r * 2, r * 2, 270, 90);
        path.AddArc(fX + fW - r*2, fY + fW - r*2, r * 2, r * 2,   0, 90);
        path.AddArc(fX,            fY + fW - r*2, r * 2, r * 2,  90, 90);
        path.CloseFigure();
        Gdiplus::SolidBrush br(Gdiplus::Color(255,
                                              GetRValue(m.avatarBg),
                                              GetGValue(m.avatarBg),
                                              GetBValue(m.avatarBg)));
        g.FillPath(&br, &path);

        // 头像内单字
        COLORREF oldClr = ::SetTextColor(hdc, RGB(255, 255, 255));
        TCHAR ch[2] = { m.avatarLetter, 0 };
        RECT rcAv = { avX, avY, avX + avSz, avY + avSz };
        ::DrawText(hdc, ch, 1, &rcAv,
                   DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        // 名字（cell 底部，给充裕的字底空间避免汉字被截）
        ::SetTextColor(hdc, RGB(80, 80, 80));
        RECT rcName = { x, avY + avSz + 4, x + cellW, avY + avSz + 28 };
        ::DrawText(hdc, m.name, -1, &rcName,
                   DT_CENTER | DT_TOP | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
        ::SetTextColor(hdc, oldClr);
    }

    static void PaintPlaceholder_(HDC hdc, int x, int y, int cellW, int /*cellH*/,
                                  TCHAR glyph, LPCTSTR caption)
    {
        const int avSz = 36;
        int avX = x + (cellW - avSz) / 2;
        int avY = y + 2;

        // 虚线框 + 居中字符
        HPEN hPen = ::CreatePen(PS_DASH, 1, RGB(180, 180, 180));
        HPEN hOldPen = (HPEN)::SelectObject(hdc, hPen);
        HBRUSH hOldBr = (HBRUSH)::SelectObject(hdc, ::GetStockObject(NULL_BRUSH));
        ::Rectangle(hdc, avX, avY, avX + avSz, avY + avSz);
        ::SelectObject(hdc, hOldBr);
        ::SelectObject(hdc, hOldPen);
        ::DeleteObject(hPen);

        COLORREF oldClr = ::SetTextColor(hdc, RGB(150, 150, 150));
        TCHAR ch[2] = { glyph, 0 };
        RECT rcAv = { avX, avY, avX + avSz, avY + avSz };
        ::DrawText(hdc, ch, 1, &rcAv,
                   DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        RECT rcCap = { x, avY + avSz + 4, x + cellW, avY + avSz + 28 };
        ::DrawText(hdc, caption, -1, &rcCap,
                   DT_CENTER | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);
        ::SetTextColor(hdc, oldClr);
    }

private:
    int m_sessionIdx = -1;
};

// =========================================================================
// SessionListPainter —— DuiVirtualList paint 回调用的桥（C 函数指针）。
//
// VirtualList 把 (rowIndex, rect, selected, hover) 喂过来，桥从 MockData
// 拉数据并调 PaintSessionRow。user 不需要 —— 数据源是进程级 static，所以
// 传 nullptr 即可。点击 / 选中变化的通知后续再加（Phase 4 会用到）。
// =========================================================================
static void SessionListPainter(void* /*user*/, HDC hdc, int rowIndex,
                               const RECT& rowRect, bool selected, bool hover)
{
    const auto& v = Sessions();
    if (rowIndex < 0 || rowIndex >= (int)v.size())
    {
        return;
    }
    PaintSessionRow(hdc, rowRect, v[rowIndex], selected, hover);
}

// =========================================================================
// CustomFactory：把 main.xml 里几个自定义 tag 翻译成对应控件实例。
// =========================================================================
DuiXmlBuilder::CustomFactory MakeMainXmlFactoryImpl()
{
    return [](const DuiXmlBuilder::Node& n) -> std::unique_ptr<DuiControl>
    {
        // <control> spacer（同 LoginFrame，但 main.xml 也用得上）
        if (n.tag == "control")
        {
            return std::unique_ptr<DuiControl>(new DuiControl());
        }
        if (n.tag == "color-panel")
        {
            auto p = std::unique_ptr<ColorPanel>(new ColorPanel());
            auto it = n.attrs.find("color");
            if (it != n.attrs.end())
            {
                p->SetBg(ParseRgbAttr(it->second, RGB(255, 255, 255)));
            }
            return p;
        }
        if (n.tag == "nav-icon")
        {
            auto ni = std::unique_ptr<NavIcon>(new NavIcon());
            auto it = n.attrs.find("selected");
            if (it != n.attrs.end()
                && (it->second == "true" || it->second == "1"))
            {
                ni->SetSelected(true);
            }
            it = n.attrs.find("glyph");
            if (it != n.attrs.end())
            {
                ni->SetGlyph(ParseWxIconKind(it->second));
            }
            return ni;
        }
        if (n.tag == "icon-button")
        {
            auto ib = std::unique_ptr<IconButton>(new IconButton());
            auto it = n.attrs.find("glyph");
            if (it != n.attrs.end())
            {
                ib->SetGlyph(ParseWxIconKind(it->second));
            }
            it = n.attrs.find("color");
            if (it != n.attrs.end())
            {
                ib->SetIdleColor(ParseRgbAttr(it->second, RGB(110, 110, 110)));
            }
            it = n.attrs.find("hoverBg");
            if (it != n.attrs.end())
            {
                ib->SetHoverBg(ParseRgbAttr(it->second, RGB(232, 232, 232)));
            }
            return ib;
        }
        if (n.tag == "watermark")
        {
            return std::unique_ptr<DuiControl>(new WatermarkPanel());
        }
        if (n.tag == "send-button")
        {
            auto sb = std::unique_ptr<SendButton>(new SendButton());
            auto it = n.attrs.find("text");
            if (it != n.attrs.end() && !it->second.empty())
            {
                int cw = ::MultiByteToWideChar(CP_UTF8, 0, it->second.c_str(),
                                               -1, nullptr, 0);
                if (cw > 0)
                {
                    std::wstring w(cw, 0);
                    ::MultiByteToWideChar(CP_UTF8, 0, it->second.c_str(),
                                          -1, &w[0], cw);
                    while (!w.empty() && w.back() == 0) { w.pop_back(); }
                    sb->SetText(CString(w.c_str()));
                }
            }
            return sb;
        }
        if (n.tag == "logo-image")
        {
            std::string src = "wechat_logo_gray.png";
            auto it = n.attrs.find("src");
            if (it != n.attrs.end() && !it->second.empty())
            {
                src = it->second;
            }
            CString relW(src.c_str());
            CString abs = ResolveResPath(relW);
            HBITMAP hbm = abs.IsEmpty() ? nullptr : LoadPngAsHBitmap(abs);
            return std::unique_ptr<DuiControl>(new LogoImage(hbm));
        }
        if (n.tag == "user-avatar")
        {
            auto av = std::unique_ptr<DuiAvatar>(new DuiAvatar());
            av->SetShape(DuiAvatar::ShapeRoundRect);
            av->SetCornerRadius(8);
            av->SetName(_T("Balloonwj"));
            av->SetFallbackBgColor(kBrandGreenRgb);
            return av;
        }
        if (n.tag == "session-list")
        {
            // 中栏会话列表：DuiVirtualList + paint 回调。Phase 3 阶段
            // 默认选中第一条（"文件传输助手"）跟参考截图对齐"主面板"
            // 状态。
            auto vl = std::unique_ptr<DuiVirtualList>(new DuiVirtualList());
            vl->SetRowHeight(GetSessionRowHeight());
            vl->SetRowCount((int)Sessions().size());
            vl->SetPaintRowCallback(&SessionListPainter, nullptr);
            vl->SetCurSel(0, /*notify*/false);
            return vl;
        }
        if (n.tag == "chat-message-list")
        {
            // 单聊消息流：自管 layout + scroll，paint 5 种 kind。
            // 初始用 session 0 (文件传输助手) 的 mock；ApplySessionView
            // 切会话时调 SetMessages 替换为目标 session 的 mock。
            auto cl = std::unique_ptr<ChatMessageList>(new ChatMessageList());
            cl->SetMessages(&MessagesForSession(0));
            cl->SetCtrlId(310);     // 给个 id，让 ApplySessionView 能 FindCtrlById
            return cl;
        }
        if (n.tag == "chat-title")
        {
            // 单聊 header 左侧的会话名标签。默认显示 CurrentChatTitle()，
            // 但允许 XML 用 `text="..."` 覆盖（如 publics 视图直接写
            // "公众号"，不依赖 mock 状态）。
            auto lb = std::unique_ptr<DuiLabel>(new DuiLabel());
            auto it = n.attrs.find("text");
            if (it != n.attrs.end() && !it->second.empty())
            {
                int cw = ::MultiByteToWideChar(CP_UTF8, 0, it->second.c_str(),
                                               -1, nullptr, 0);
                if (cw > 0)
                {
                    std::wstring w(cw, 0);
                    ::MultiByteToWideChar(CP_UTF8, 0, it->second.c_str(),
                                          -1, &w[0], cw);
                    while (!w.empty() && w.back() == 0) { w.pop_back(); }
                    lb->SetText(CString(w.c_str()));
                }
            }
            else
            {
                lb->SetText(CurrentChatTitle());
            }
            lb->SetTextColor(RGB(26, 26, 26));
            return lb;
        }
        // ── Phase 5：群信息侧栏的 3 个自定义 tag ────────────────────────
        if (n.tag == "member-cell")
        {
            auto mc = std::unique_ptr<MemberCell>(new MemberCell());
            auto it = n.attrs.find("placeholder");
            if (it != n.attrs.end() && !it->second.empty())
            {
                mc->SetPlaceholderGlyph((TCHAR)it->second[0]);
            }
            else
            {
                it = n.attrs.find("letter");
                if (it != n.attrs.end() && !it->second.empty())
                {
                    // attr 是 UTF-8 std::string；取首字时只支持 ASCII。中文
                    // letter 走单独属性 `letterW="中"` 走 wide path（demo 没用上）。
                    mc->SetLetter((TCHAR)it->second[0]);
                }
                it = n.attrs.find("bg");
                if (it != n.attrs.end())
                {
                    mc->SetAvatarBg(ParseRgbAttr(it->second, RGB(120, 120, 120)));
                }
                it = n.attrs.find("name");
                if (it != n.attrs.end())
                {
                    // UTF-8 → CString 转换
                    int cw = ::MultiByteToWideChar(CP_UTF8, 0, it->second.c_str(),
                                                   -1, nullptr, 0);
                    if (cw > 0)
                    {
                        std::wstring w(cw, 0);
                        ::MultiByteToWideChar(CP_UTF8, 0, it->second.c_str(),
                                              -1, &w[0], cw);
                        while (!w.empty() && w.back() == 0) { w.pop_back(); }
                        mc->SetName(CString(w.c_str()));
                    }
                }
            }
            return mc;
        }
        if (n.tag == "info-field")
        {
            // vbox = 小灰 label + 大黑 label，两行。caller 通过 attr 喂进去。
            auto vb = std::unique_ptr<DuiVBox>(new DuiVBox());
            // label 行（小、灰）
            auto labLab = std::unique_ptr<DuiLabel>(new DuiLabel());
            labLab->SetTextColor(RGB(140, 140, 140));
            auto it = n.attrs.find("label");
            if (it != n.attrs.end())
            {
                int cw = ::MultiByteToWideChar(CP_UTF8, 0, it->second.c_str(),
                                               -1, nullptr, 0);
                if (cw > 0)
                {
                    std::wstring w(cw, 0);
                    ::MultiByteToWideChar(CP_UTF8, 0, it->second.c_str(),
                                          -1, &w[0], cw);
                    while (!w.empty() && w.back() == 0) { w.pop_back(); }
                    labLab->SetText(CString(w.c_str()));
                }
            }
            DuiLayout::Hint hLab; hLab.fixedMain = 18;
            vb->AddChild(std::move(labLab), hLab);

            // value 行（大、深）
            auto valLab = std::unique_ptr<DuiLabel>(new DuiLabel());
            valLab->SetTextColor(RGB(40, 40, 40));
            it = n.attrs.find("value");
            if (it != n.attrs.end())
            {
                int cw = ::MultiByteToWideChar(CP_UTF8, 0, it->second.c_str(),
                                               -1, nullptr, 0);
                if (cw > 0)
                {
                    std::wstring w(cw, 0);
                    ::MultiByteToWideChar(CP_UTF8, 0, it->second.c_str(),
                                          -1, &w[0], cw);
                    while (!w.empty() && w.back() == 0) { w.pop_back(); }
                    valLab->SetText(CString(w.c_str()));
                }
            }
            DuiLayout::Hint hVal; hVal.fixedMain = 22;
            vb->AddChild(std::move(valLab), hVal);
            return vb;
        }
        if (n.tag == "group-members")
        {
            // 群信息栏的成员 grid，按当前 session 动态显示。ApplySessionView
            // 通过 FindCtrlById 找它（调用方在 main.xml 给它显式 id="220"）。
            return std::unique_ptr<DuiControl>(new GroupMembersGrid());
        }
        // ── Phase 6：联系人页 ────────────────────────────────────────
        if (n.tag == "contact-mgmt-button")
        {
            return std::unique_ptr<DuiControl>(new ContactMgmtButton());
        }
        if (n.tag == "contact-tree")
        {
            return std::unique_ptr<DuiControl>(new ContactCategoryList());
        }
        // ── Phase 7：公众号页 ────────────────────────────────────────
        if (n.tag == "freq-contacts")
        {
            return std::unique_ptr<DuiControl>(new FrequentContactsRow());
        }
        if (n.tag == "article-list")
        {
            return std::unique_ptr<DuiControl>(new ArticleList());
        }
        if (n.tag == "info-mute-row")
        {
            // hbox = label + spacer + DuiSwitch。Phase 5 静态 off。
            auto hb = std::unique_ptr<DuiHBox>(new DuiHBox());
            auto lb = std::unique_ptr<DuiLabel>(new DuiLabel());
            lb->SetTextColor(RGB(40, 40, 40));
            auto it = n.attrs.find("label");
            if (it != n.attrs.end())
            {
                int cw = ::MultiByteToWideChar(CP_UTF8, 0, it->second.c_str(),
                                               -1, nullptr, 0);
                if (cw > 0)
                {
                    std::wstring w(cw, 0);
                    ::MultiByteToWideChar(CP_UTF8, 0, it->second.c_str(),
                                          -1, &w[0], cw);
                    while (!w.empty() && w.back() == 0) { w.pop_back(); }
                    lb->SetText(CString(w.c_str()));
                }
            }
            DuiLayout::Hint hLb;     // weight=1（默认）—— 占满主轴左侧
            hb->AddChild(std::move(lb), hLb);

            auto sw = std::unique_ptr<DuiSwitch>(new DuiSwitch());
            sw->SetChecked(false, /*animated*/false, /*notify*/false);
            DuiLayout::Hint hSw; hSw.fixedMain = 46;   // DuiSwitch 默认 46×24
            hb->AddChild(std::move(sw), hSw);
            return hb;
        }
        return nullptr;
    };
}

// XML 兜底（找不到 main.xml 时用，保底窗口能起来 —— 至少能显示三栏底色）。
const char* kFallbackMainXmlUtf8 =
    "<frame-window title=\"XChat\""
    " min-w=\"800\" min-h=\"540\" resizable=\"true\""
    " has-min-button=\"true\" has-max-button=\"true\" has-close-button=\"true\">"
    "  <hbox padding=\"0\" gap=\"0\">"
    "    <color-panel color=\"46,46,46\" fixedWidth=\"75\"/>"
    "    <color-panel color=\"245,245,245\" fixedWidth=\"290\"/>"
    "    <color-panel color=\"238,238,238\" weight=\"1\"/>"
    "  </hbox>"
    "</frame-window>";

} // anonymous

DuiXmlBuilder::CustomFactory MakeMainXmlFactory()
{
    return MakeMainXmlFactoryImpl();
}

// =============================================================================
// XChatMainFrame
// =============================================================================
LRESULT XChatMainFrame::OnCreate_(UINT, WPARAM, LPARAM, BOOL& bHandled)
{
    SetTimer(kAnimPulseTimerId, kAnimPulseMs);
    bHandled = FALSE;
    return 0;
}

LRESULT XChatMainFrame::OnDestroy_(UINT, WPARAM, LPARAM, BOOL& bHandled)
{
    KillTimer(kAnimPulseTimerId);
    DuiAnimMgr::Inst().Clear();
    ::PostQuitMessage(0);
    bHandled = FALSE;
    return 0;
}

LRESULT XChatMainFrame::OnTimer_(UINT, WPARAM wp, LPARAM, BOOL& bHandled)
{
    if (wp == kAnimPulseTimerId)
    {
        DuiAnimMgr::Inst().TickAll(::GetTickCount());
    }
    bHandled = FALSE;
    return 0;
}

void XChatMainFrame::LoadMainXml(LPCTSTR xmlName)
{
    LPCTSTR name = xmlName ? xmlName : _T("main.xml");
    std::string xml = ReadFileUtf8(ResolveXmlPath(name));
    if (xml.empty())
    {
        // 找不到指定 xml 时回落到 main.xml 内嵌 fallback；contacts 等
        // 变体没 fallback —— 文件丢失就退回 chat 视图，至少能起来。
        xml = kFallbackMainXmlUtf8;
    }
    DuiFrameWindowConfig cfg;
    auto root = DuiXmlBuilder::FromFrameXml(xml.c_str(), cfg, MakeMainXmlFactory());
    ApplyConfig(cfg);
    if (root)
    {
        SetClientContent(std::move(root));
    }
    // 初始 view = session 0 的 pageType（main.xml 默认 session-list CurSel=0）。
    // contacts / publics 变体里没有 session-list / id 200/210/202，
    // ApplySessionView 内部 FindCtrlById 拿 nullptr 就直接返回，无副作用。
    ApplySessionView(0);

    // 真 EDIT HWND-hosted 控件需 caller 在 host HWND 就绪后调 EnsureCreated。
    // builder 只构控件树，不会自动 create HWND。Placeholder 由 XML
    // `placeholder="..."` 属性指定，DuiEditHost 内部走 EM_SETCUEBANNER。
    //   id=60  ：左侧顶部搜索框
    //   id=400 ：底部聊天输入框
    if (auto* root = GetClientContent())
    {
        if (auto* search = dynamic_cast<DuiEditHost*>(root->FindCtrlById(60)))
        {
            search->EnsureCreated(m_hWnd);
        }
        if (auto* input = dynamic_cast<DuiEditHost*>(root->FindCtrlById(400)))
        {
            input->EnsureCreated(m_hWnd);
        }
    }
}

void XChatMainFrame::ApplySessionView(int sessionIdx)
{
    DuiControl* root = GetClientContent();
    if (!root)
    {
        return;
    }
    const auto& v = Sessions();

    DuiControl* paneChat      = root->FindCtrlById(200);
    DuiControl* paneInfo      = root->FindCtrlById(210);
    DuiControl* panePublics   = root->FindCtrlById(202);
    DuiControl* paneWatermark = root->FindCtrlById(204);
    DuiLabel*   chatTitle     = dynamic_cast<DuiLabel*>(root->FindCtrlById(300));

    // sessionIdx == -1 → 空 chat 状态（仅显 watermark）；越界也走同分支。
    bool isEmpty = (sessionIdx < 0 || sessionIdx >= (int)v.size());

    bool showChat      = false;
    bool showInfo      = false;
    bool showPublics   = false;
    bool showWatermark = false;

    if (isEmpty)
    {
        showWatermark = true;
    }
    else
    {
        SessionPageType pt = v[sessionIdx].pageType;
        showChat    = (pt == Page_Single) || (pt == Page_Group);
        showInfo    = (pt == Page_Group);
        showPublics = (pt == Page_Publics);
    }

    if (paneChat)      { paneChat->SetVisible(showChat); }
    if (paneInfo)      { paneInfo->SetVisible(showInfo); }
    if (panePublics)   { panePublics->SetVisible(showPublics); }
    if (paneWatermark) { paneWatermark->SetVisible(showWatermark); }

    if (chatTitle && showChat)
    {
        // 群聊带 (4) 后缀的话原 mock 已经写在 name 里；不带的就直接用。
        chatTitle->SetText(v[sessionIdx].name);
    }

    // 切换 chat-message-list 的数据源到当前 session 的 mock 对话。
    if (showChat)
    {
        if (auto* cl = dynamic_cast<ChatMessageList*>(root->FindCtrlById(310)))
        {
            cl->SetMessages(&MessagesForSession(sessionIdx));
        }
    }
    // 群信息栏的成员 grid 按 sessionIdx 取对应群的 members。
    if (showInfo)
    {
        if (auto* gm = dynamic_cast<GroupMembersGrid*>(root->FindCtrlById(220)))
        {
            gm->SetSessionIdx(sessionIdx);
        }
    }

    // SetVisible 只 Invalidate 不触发父 layout；ForceLayout 又只让<u>当前</u>
    // 节点 Layout 一次，子节点的 SetRect 在 EqualRect 时早 return 不会再
    // 进它们的 Layout 路径。所以手动找到两个层级的"切换发生地"分别 force：
    //   ① paneChat / panePublics / paneWatermark 的父 = 右栏 vbox
    //   ② paneChat（控制 210 子项 Single vs Group 切换）
    DuiControl* rightCol = paneChat ? paneChat->GetParent()
                         : (paneWatermark ? paneWatermark->GetParent() : nullptr);
    if (rightCol)
    {
        rightCol->ForceLayout(rightCol->GetRect());
    }
    if (paneChat && showChat)
    {
        paneChat->ForceLayout(paneChat->GetRect());
    }
}

void XChatMainFrame::ShowEmptyView()
{
    DuiControl* root = GetClientContent();
    if (!root) { return; }
    // 清 session-list 选中（避免某行高亮但右侧空白的歧义）
    if (auto* vl = dynamic_cast<DuiVirtualList*>(root->FindCtrlById(50)))
    {
        vl->SetCurSel(-1, /*notify*/false);
    }
    ApplySessionView(-1);
}

LRESULT XChatMainFrame::OnDuiNotify_(UINT, WPARAM wp, LPARAM lp, BOOL& bHandled)
{
    auto* n = reinterpret_cast<balloonwjui::DuiNotify*>(lp);
    if (n && n->code == balloonwjui::DUIN_VALUECHANGED && (UINT)wp == 50u)
    {
        // session-list 选中行变化 → 切右栏 view。extra 是新选中 idx。
        ApplySessionView((int)n->extra);
    }
    else if (n && n->code == balloonwjui::DUIN_CLICK && (UINT)wp == 91u)
    {
        // 汉堡 nav-icon 点击 → 弹 6 项菜单。pCtrl 是 void*，强转回 DuiControl*。
        ShowHamburgerMenu_(static_cast<balloonwjui::DuiControl*>(n->pCtrl));
    }
    else if (n && n->code == balloonwjui::DUIN_CLICK && (UINT)wp == 90u)
    {
        // 聊天 nav-icon → 切回主面板（main.xml）。
        // 注意：LoadMainXml 会 SetClientContent 替换整棵客户区树 ——
        // 当前帧下任何 in-flight 的事件 dispatch 到 ShowHamburgerMenu_
        // 这种"借用 pCtrl 的"路径不在此分支命中，安全。
        LoadMainXml(_T("main.xml"));
    }
    else if (n && n->code == balloonwjui::DUIN_CLICK && (UINT)wp == 92u)
    {
        // 联系人 nav-icon → 加载 main_contacts.xml。
        LoadMainXml(_T("main_contacts.xml"));
    }
    bHandled = FALSE;
    return 0;
}

void XChatMainFrame::ShowHamburgerMenu_(balloonwjui::DuiControl* navIcon)
{
    using balloonwjui::DuiMenu;

    // 菜单 6 项，id 用 1001..1006；"设置" = 1006，选中后弹设置 frame。
    DuiMenu menu;
    menu.AppendItem(1002, _T("聊天文件"));
    menu.AppendItem(1003, _T("聊天记录管理"));
    menu.AppendItem(1004, _T("锁定"));
    menu.AppendItem(1005, _T("意见反馈"));
    menu.AppendItem(1006, _T("设置"));

    // 屏幕坐标 = 汉堡 cell 右上角（让菜单从按钮右侧弹出，跟参考截图位置一致）。
    POINT pt = { 0, 0 };
    if (navIcon)
    {
        RECT rcNav = navIcon->GetRect();    // host 客户区坐标
        pt.x = rcNav.right + 4;
        pt.y = rcNav.top;
        ::ClientToScreen(m_hWnd, &pt);
    }
    else
    {
        // fallback：屏幕中央
        ::GetCursorPos(&pt);
    }

    UINT chosen = menu.TrackPopup(pt.x, pt.y, m_hWnd);
    if (chosen == 1006)
    {
        // 弹设置弹窗。模态：禁掉 main，等设置关再恢复。
        SettingsFrame settings;
        if (settings.Create(m_hWnd, CWindow::rcDefault,
                            _T(""),
                            WS_OVERLAPPEDWINDOW, 0))
        {
            settings.LoadSettingsXml();
            settings.ResizeClient(800, 680);
            settings.CenterWindow(m_hWnd);
            settings.ShowWindow(SW_SHOWNORMAL);
            settings.UpdateWindow();
            settings.RunModal(m_hWnd);
        }
    }
}

} // namespace xchat
