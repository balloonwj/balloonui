#pragma once

#include "../../BalloonUiFeatures.h"
#if BUI_FEATURE_GIF

// .cpp 必须先 include stdafx.h（项目 PCH 约定）。
#include "../../DuiControl.h"
#include <vector>

namespace balloonwjui {

// =================================================================
// DuiGif —— 动图解码器（数据持有，不画图）
// =================================================================
//
// 用途：解码并缓存一个多帧 GIF 文件。本身不显示，只提供"第 N 帧的
// HBITMAP"和"按时间算应该显示哪帧"。配合 DuiGifControl 才能真正画到
// 屏上。
//
// 设计约束：
//   · Load 时<u>急切</u>把所有帧解到 m_frames 里。内存代价 = N × 4 ×
//     w × h，对单 emoji 大小的 GIF（典型 32×32 × 8 帧 = 32 KB）完全
//     可接受；不适合超大 GIF。
//   · 每帧延迟从 GIF 的 PropertyTagFrameDelay 拿（10ms 单位）。0 延迟
//     被 clamp 到 100ms（与浏览器行为一致）。
//   · 动画时间是单调毫秒（caller 驱动 Tick）；DuiGifControl 把它包进
//     DuiAnimMgr，让 DuiGallery / 主对话框靠 host 的 shared pulse 不需要
//     per-control WM_TIMER。
//
// 工作机制：
//   · 纯数据持有：LoadFromFile 用 GDI+ 急切解码，每帧缓存一个 HBITMAP。
//   · 帧 HBITMAP 由 DuiGif 持有所有权；caller <u>不要</u>对返回的
//     HBITMAP 调 DeleteObject。
//   · FrameAt(elapsedMs, delays, count) 是纯函数，不依赖 GDI+，便于单测。
//
// 代码用法：见 DuiGifControl 的用法段。
//
// XML 用法：<u>暂未原生支持</u>。GIF 资源加载需要文件路径解析（同
// DuiAvatar 的 image 属性问题），且业务侧通常想自管 GIF 缓存（重复
// emoji 不要重复解）。要 XML 化业务自己 CustomFactory 注册 <gif
// path="..."/> 标签 + 解析路径自加载。
//
// 替代关系：CSkinGif（老 emoji panel + buddy chat history 用过）。
class BUI_API DuiGif
{
public:
    DuiGif();
    ~DuiGif();

    // 从文件加载。替换之前的内容。
    //   path：GIF 文件磁盘路径。
    //   返回：true 表示加载成功且 GetFrameCount() > 0；false 表示文件
    //         不存在 / 解码失败。
    bool    LoadFromFile(LPCTSTR path);

    // 帧总数 / 整图宽 / 高（像素）。
    int     GetFrameCount() const { return (int)m_frames.size(); }
    int     GetWidth()  const { return m_w; }
    int     GetHeight() const { return m_h; }

    // 第 idx 帧的延迟（毫秒）。索引越界返回 0。
    int     GetFrameDelayMs(int idx) const;

    // 一个完整循环的总时长（毫秒）。
    int     GetTotalDurationMs() const;

    // 第 idx 帧的 HBITMAP。<u>所有权在 DuiGif</u>，caller 不要 DeleteObject。
    HBITMAP GetFrameHbitmap(int idx) const;

    // 静态纯 helper：给定从开始计的 elapsedMs + 每帧延迟列表，算出当前
    // 应显示的帧索引。动画自动循环（elapsedMs 模 total）。
    //   elapsedMs：从动画开始计算的毫秒数。
    //   delaysMs：每帧延迟数组（毫秒）。
    //   count：数组长度。
    //   返回：[0, count) 帧索引。
    static int FrameAt(unsigned long elapsedMs,
                       const int* delaysMs, int count);

    // 用本 GIF 的延迟列表算当前帧。等价 FrameAt(elapsedMs, m_delaysMs, count)。
    int     FrameAt(unsigned long elapsedMs) const;

private:
    void    Clear();

    int                  m_w = 0;
    int                  m_h = 0;
    std::vector<HBITMAP> m_frames;
    std::vector<int>     m_delaysMs;
};

// =================================================================
// DuiGifControl —— 把 DuiGif 画到 DUI 树上的控件（无 HWND）
// =================================================================
//
// 用途：给一个已加载的 DuiGif 装上"画到屏上"和"自动播放"的能力。聊天
// 列表里的 emoji、消息历史里的动图、对话框的 loading 动画都用它。
//
// 工作机制：
//   · 纯绘制 DuiControl；通过 SetGif 拿走 DuiGif 的所有权。
//   · Start() 注册到 DuiAnimMgr，让播放靠 host shared animation pulse
//     驱动（不需要 per-control WM_TIMER）。Stop() 暂停但保留当前帧不变。
//   · Stretch（默认）= 拉伸适配 m_rcItem；非 stretch = 1:1 居中。
//
// 代码用法：
//
//     auto gif = std::unique_ptr<DuiGif>(new DuiGif());
//     if (gif->LoadFromFile(_T("emoji\\smile.gif")))
//     {
//         auto ctrl = std::unique_ptr<DuiGifControl>(new DuiGifControl());
//         ctrl->SetGif(std::move(gif));
//         ctrl->SetStretch(false);   // 1:1 居中
//         ctrl->Start();
//         parent->AddChild(std::move(ctrl));
//     }
//
// XML 用法：暂未原生支持，见 DuiGif 的 XML 段说明。
//
// 事件：无（纯绘制）。
class BUI_API DuiGifControl : public DuiControl
{
public:
    DuiGifControl();
    ~DuiGifControl();

    // 安装 / 替换 GIF。控件拿走所有权；之前的 GIF 被销毁。
    //   gif：DuiGif unique_ptr；nullptr 表示清空。
    void    SetGif(std::unique_ptr<DuiGif> gif);
    DuiGif* GetGif() const { return m_gif.get(); }

    // 开始播放（注册到 DuiAnimMgr）。已 running 则无效。
    void    Start();

    // 暂停。当前帧保留显示。
    void    Stop();
    bool    IsRunning() const { return m_running; }

    // 强制设当前帧（测试 / 程序定位用）。
    //   idx：[0, GetFrameCount())。
    void    SetFrameIndex(int idx);
    int     GetFrameIndex() const { return m_frameIdx; }

    // 设置 / 读取拉伸模式。
    //   b：true（默认）= 拉伸到 m_rcItem；false = 1:1 居中。
    void    SetStretch(bool b) { m_stretch = b; Invalidate(); }
    bool    GetStretch() const { return m_stretch; }

    // ---- DuiControl 覆写 ----
    void    OnPaint(HDC hdc, const RECT& rcDirty) override;
    SIZE    GetDesiredSize() const override;

private:
    std::unique_ptr<DuiGif> m_gif;
    int                     m_frameIdx = 0;
    bool                    m_running  = false;
    bool                    m_stretch  = true;
    unsigned long           m_startMs  = 0;
};

} // namespace balloonwjui

#endif // BUI_FEATURE_GIF
