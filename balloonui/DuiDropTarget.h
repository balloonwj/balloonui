#pragma once

// .cpp 必须先 include stdafx.h（项目 PCH 约定）。

// =================================================================
// DuiDropTargetHelper —— OLE 拖放接收器（文件 + bitmap）
// =================================================================
//
// 用途：把一个 HWND 注册成 OLE Drop Target，接收从 Explorer / 桌面
// 拖来的文件（CF_HDROP），或从其它图像 app 拖来的位图（CF_BITMAP）。
// 聊天输入框的"拖入文件 / 拖入图片"功能就是用它实现的。
//
// 工作机制：
//   · caller 通过 SetCallbacks 装俩 handler。drop 成功时对应 callback
//     在 UI 线程上调（IDropTarget::Drop 在目标窗口线程跑）。
//   · 生命期：caller 持有 helper 实例。host HWND 就绪时 Register(hwnd)，
//     dtor 里 Unregister()。包装的 IDropTarget 由 OLE 引用计数。
//   · 需要 ::OleInitialize 已经调过（典型 WinMain 早期一次）。
//
// 代码用法（聊天输入对话框）：
//
//     ::OleInitialize(nullptr);   // 启动时一次
//     balloonwjui::DuiDropTargetHelper helper;
//     helper.SetCallbacks(
//         [](const std::vector<CString>& paths) { /* 发文件 */ },
//         [](HBITMAP hbm)                       { /* 粘位图 */ });
//     helper.Register(m_hWnd);
//     // ~Dialog 里：helper.Unregister();
//
// XML 用法：N/A（横切式 helper，不是控件）。

#include <windows.h>
#include <ole2.h>
#include <shlobj.h>
#include <functional>
#include <vector>

namespace balloonwjui {

class DuiDropTargetImpl;     // forward decl, lives in cpp

// Usage (typically attached to a chat input dialog's HWND):
//   ::OleInitialize(nullptr);   // once at startup
//   balloonwjui::DuiDropTargetHelper helper;
//   helper.SetCallbacks(
//       [](const std::vector<CString>& paths) { /* send files */ },
//       [](HBITMAP hbm)                       { /* paste bitmap */ });
//   helper.Register(m_hWnd);
//   // ... in ~Dialog: helper.Unregister();
class DuiDropTargetHelper
{
public:
    typedef std::function<void(const std::vector<CString>&)> FilesCallback;
    typedef std::function<void(HBITMAP)>                     BitmapCallback;

    DuiDropTargetHelper();
    ~DuiDropTargetHelper();

    // Wire up an HWND. Returns true on success. Calls
    // RegisterDragDrop internally; balanced by Unregister or the dtor.
    bool  Register(HWND hwnd);
    void  Unregister();
    bool  IsRegistered() const { return m_hwnd != nullptr; }

    // Install handlers. Either may be null. If both are null, drops
    // are accepted but nothing happens (useful for "we know how to
    // accept, but the data isn't interesting yet" testing).
    void  SetCallbacks(FilesCallback onFiles, BitmapCallback onBitmap);

    // Pure helper: extract paths from an HDROP via DragQueryFile.
    // Returns the list of file paths. Side-effect free.
    static std::vector<CString> ExtractFilesFromHDrop(HDROP hDrop);

private:
    DuiDropTargetImpl* m_impl  = nullptr;
    HWND               m_hwnd  = nullptr;
};

} // namespace balloonwjui
