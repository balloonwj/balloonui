#pragma once

#include "../../BalloonUiFeatures.h"
#if BUI_FEATURE_EMOJIPANEL

// .cpp 必须先 include stdafx.h（项目 PCH 约定）。
#include "../../DuiControl.h"
#include <vector>

namespace balloonwjui {

// =================================================================
// DuiEmojiPanel —— Emoji 选择网格（无 HWND）
// =================================================================
//
// 用途：聊天输入框右侧的"😀"按钮弹出的 emoji 选择面板。N 行 × M 列固定
// emoji 集合，鼠标 hover + 点击挑一个，挑中的 UTF-16 序列被通知出去
// （由 host 把它插入 RichEdit）。
//
// 工作机制：
//   · 每个格子有"插入文本"（RichEdit 应该插的 UTF-16 序列，可以是单
//     BMP 字符 / surrogate pair / ZWJ 拼接 cluster）和可选 HBITMAP icon
//     （显示用 glyph）。有 icon 时画 icon，没 icon 时用 "Segoe UI Emoji"
//     字体画文字 —— 后者只适合单色 emoji，因为 GDI 的 DrawText 即使带
//     彩色字体也只画灰阶；要 Win10+ 彩色 emoji 必须用 icon 路径。
//   · HBITMAP 是 caller-owned，必须比面板存活久。
//   · hover / press 按格跟踪；mouse-up 在同一格内 = 一次成功 pick，
//     先发 WM_DUI_NOTIFY (DUIN_VALUECHANGED) 再调 PickCallback（如果设了）。
//   · 布局是 intrinsic：GetDesiredSize 返回 columns * cellSize × rows *
//     cellSize，让 popup host 据此设置 popup 窗口大小。
//   · "Segoe UI Emoji" 字体懒构造、面板持有所有权，析构时 DeleteObject。
//
// 代码用法：
//
//     auto panel = std::unique_ptr<DuiEmojiPanel>(new DuiEmojiPanel());
//     panel->SetColumns(8);
//     panel->SetCellSize(32);
//     panel->AddEmojiBitmap(_T("\xD83D\xDE0A"), hSmileBmp, _T("微笑"));
//     panel->AddEmojiBitmap(_T("\xD83D\xDC4D"), hThumbBmp, _T("点赞"));
//     panel->SetPickCallback(
//         [](void* ud, LPCTSTR seq, int idx) {
//             static_cast<MyComposer*>(ud)->InsertEmoji(seq);
//         },
//         this);
//     popup->SetContent(std::move(panel));
//     // 或者：不放 popup，直接挂到父 host 里，然后在父 OnDuiNotify
//     // 处理 DUIN_VALUECHANGED 事件。
//
// XML 用法：<u>暂未原生支持</u>。原因：emoji set 是有 model 的（每项
// 有自己的"插入序列 + tooltip + icon HBITMAP"），加之 HBITMAP 不能在
// XML 里表达。要 XML 化的话业务侧 CustomFactory 注册 <emoji-panel> +
// 解析 <emoji seq="..." tip="..." /> 子节点（icon 还是 C++ 侧挂，详见
// guides.html §3.6）。
//
// 事件：
//   · DUIN_VALUECHANGED — 用户挑了一个 emoji；extra = 选中项 index。
//                          通知<u>先于</u> PickCallback 回调发出。
class BUI_API DuiEmojiPanel : public DuiControl
{
public:
    // 用户 pick 时回调。
    //   userdata：SetPickCallback 时传入的指针。
    //   sequence：被选中的 UTF-16 插入序列（用于 RichEdit 插入）。
    //   index：选中项在 m_emoji 里的索引。
    typedef void (*PickCallback)(void* userdata, LPCTSTR sequence, int index);

    DuiEmojiPanel();

    // ---- emoji 集合管理 ----

    // 追加一个文本-only emoji 项。
    //   utf16Sequence：要插入的 UTF-16 序列。
    //   tooltip：可选 tooltip 文字（hover 时显示）。
    void    AddEmoji   (LPCTSTR utf16Sequence, LPCTSTR tooltip = nullptr);

    // 批量追加 emoji 序列数组（无 tooltip / 无 icon）。
    //   sequences：UTF-16 序列指针数组。
    //   count：数组长度。
    void    AddEmojiSet(LPCTSTR const* sequences, int count);

    // 追加一个带彩色 icon 的 emoji 项（推荐路径）。
    //   utf16Sequence：插入到 RichEdit 的实际 UTF-16 序列。
    //   icon：caller-owned HBITMAP，显示用 glyph；必须比面板存活久。
    //   tooltip：可选。
    void    AddEmojiBitmap(LPCTSTR utf16Sequence, HBITMAP icon, LPCTSTR tooltip = nullptr);

    // 清空所有项。
    void    Clear();

    // 当前项数。
    int     GetCount   () const;

    // 第 idx 项的插入序列 / tooltip / icon。
    CString GetEmojiAt (int idx) const;
    CString GetTooltipAt(int idx) const;
    HBITMAP GetIconAt   (int idx) const;

    // ---- 布局参数 ----

    // 单格边长（px）。clamp >= 16；默认 32。
    void    SetCellSize(int px);
    int     GetCellSize() const { return m_cellSize; }

    // 列数。clamp >= 1；默认 8。行数自动 = ceil(count / columns)。
    void    SetColumns (int n);
    int     GetColumns () const { return m_columns; }

    // intrinsic 期望大小 = columns * cellSize × rows * cellSize。
    // 让 popup host 用这个尺寸来设 popup 窗口大小。
    SIZE    GetDesiredSize() const override;

    // 当前行数 = ceil(count / columns)。
    int     GetRowCount() const;

    // ---- pick 回调 ----

    // 设置 pick 回调函数。可在 host 弹 popup 时用，避免走 WM_DUI_NOTIFY
    // 一圈延迟（popup 想立即 Hide 的场景）。
    //   cb：回调函数；nullptr 取消注册。
    //   userdata：回调时透传给第一个参数。
    void    SetPickCallback(PickCallback cb, void* userdata);

    // ---- hit-test ----

    // 给定 host-客户区坐标，返回所在格的索引；不在任何格上返回 -1。
    int     HitTestIndex(POINT pt) const;

    // ---- DuiControl 覆写 ----
    void    OnPaint     (HDC hdc, const RECT& rcDirty) override;
    bool    OnLButtonDown(POINT pt, UINT mkFlags) override;
    bool    OnLButtonUp  (POINT pt, UINT mkFlags) override;
    bool    OnMouseMove  (POINT pt, UINT mkFlags) override;
    bool    OnMouseLeave () override;

    // 测试用：当前 hover 项索引。
    int     Test_HoverIdx() const { return m_hoverIdx; }

private:
    struct E { CString seq; CString tip; HBITMAP icon; };
    HFONT  EnsureEmojiFont() const;     // 懒构造、自有
    RECT   CellRect(int idx) const;

    std::vector<E>  m_emoji;
    int             m_cellSize  = 32;
    int             m_columns   = 8;
    int             m_hoverIdx  = -1;
    int             m_pressIdx  = -1;

    PickCallback    m_pickCb    = nullptr;
    void*           m_pickUd    = nullptr;

    mutable HFONT   m_hEmojiFont = nullptr;
    mutable int     m_emojiFontPx = 0;          // 上次请求的 cellSize-6
};

} // namespace balloonwjui

#endif // BUI_FEATURE_EMOJIPANEL
