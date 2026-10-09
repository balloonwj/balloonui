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
//   · 位图回调拿到的 HBITMAP <u>只在回调执行期间有效</u>：它就是拖放源交来的那张
//     位图，回调一返回本类就会 ReleaseStgMedium 把它释放掉。要在回调之后继续用
//    （存起来、稍后绘制），必须在回调里自己复制一份（如 CopyImage 加
//     LR_CREATEDIBSECTION，强制真的复制，不会退化成返回原句柄），副本归调用方释放。
//     不要 DeleteObject 回调里拿到的那个句柄。
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
//         [](HBITMAP hbm)                       { /* 粘位图：hbm 只在本回调内有效，要留就当场复制 */ });
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
//       [](HBITMAP hbm)                       { /* paste bitmap; hbm is valid only inside this callback, copy it to keep it */ });
//   helper.Register(m_hWnd);
//   // ... in ~Dialog: helper.Unregister();
class DuiDropTargetHelper
{
public:
    typedef std::function<void(const std::vector<CString>&)> FilesCallback;
    // 位图回调。参数是拖放源交来的位图，<u>只在回调执行期间有效</u>，所有权不归回调方：
    // 回调返回后本类即释放它。要保留就在回调里自己复制一份，副本由调用方释放；
    // 不要对参数本身 DeleteObject。见文件头"工作机制"一节。
    typedef std::function<void(HBITMAP)>                     BitmapCallback;

    // 拖动过程中的三个可选回调（见下面 SetDragCallbacks）。一个都不设时，本类
    // 的行为与只有 Drop 回调的旧版本完全一致。
    //
    //   DragEnterCallback：拖动进入本窗口时回调一次。
    //     ptClient  落点在<u>本窗口客户区</u>的坐标（本类已把 OLE 给的屏幕坐标换算过）。
    //     hasFiles  本次拖的数据里是否含文件列表（CF_HDROP）；false 表示只有位图（CF_BITMAP）。
    //     返回值    false 表示本次拒收 —— 光标显示禁止符，且随后的 Drop 不再回调任何东西。
    //   DragOverCallback：光标在本窗口内移动时持续回调，参数含义同上。返回 false 表示
    //     <u>光标当前所在的位置</u>不能放（光标显示禁止符）—— 窗口内只有一块区域收
    //     拖放时靠它逐帧改判，因为光标在同一个窗口内移动是<u>不会</u>再触发 DragEnter 的。
    //   DragLeaveCallback：拖动离开本窗口时回调。<u>一次 Drop 完成之后也会回调它一次</u>，
    //                      这样调用方收起悬停提示只需写在一个地方。
    typedef std::function<bool(const POINT& ptClient, bool hasFiles)> DragEnterCallback;
    typedef std::function<bool(const POINT& ptClient)>                DragOverCallback;
    typedef std::function<void()>                                     DragLeaveCallback;

    // 带落点坐标的文件回调（见下面 SetFilesAtPointCallback）。
    typedef std::function<void(const std::vector<CString>&, const POINT& ptClient)>
        FilesAtPointCallback;

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

    /**
     *  装拖动过程回调。三个参数都可为空（空的那个就不回调），全空等于没装。
     *  Register 之前或之后调用皆可，但 <u>Unregister 会连同内部实现对象一起丢弃
     *  已装的回调</u>，重新 Register 之前要重新装一遍。
     *    onEnter：拖入时回调，返回 false 表示本次拒收；不装则一律按"能收"处理。
     *    onOver： 拖动过程中光标移动时回调，返回 false 表示当前位置不能放；不装则
     *             沿用 onEnter 的判定。
     *    onLeave：拖离时回调；一次 Drop 完成后也会回调一次。
     */
    void  SetDragCallbacks(DragEnterCallback onEnter, DragOverCallback onOver,
                           DragLeaveCallback onLeave);

    /**
     *  装带落点坐标的文件回调。装了它之后，落手时<u>只</u>回调它、不再回调
     *  SetCallbacks 装的那个 FilesCallback（两者只触发其一，避免同一次拖放被处理
     *  两遍）。位图回调不受影响。
     *    onFilesAtPoint：回调，参数为文件路径列表与落点的客户区坐标；传空表示撤销。
     */
    void  SetFilesAtPointCallback(FilesAtPointCallback onFilesAtPoint);

    /**
     *  取内部的 IDropTarget 实现（没有就先建一个）。供需要自行驱动拖放流程的
     *  高级用法与单元测试使用 —— 正常接入拖放只需 Register + 装回调，不必碰它。
     *  @return 内部实现指针，<u>所有权仍属本类</u>：调用方不得在未 AddRef 的情况下
     *          Release 它，也不要在本类析构或 Unregister 之后继续使用。
     */
    IDropTarget* GetDropTarget();

    // Pure helper: extract paths from an HDROP via DragQueryFile.
    // Returns the list of file paths. Side-effect free.
    static std::vector<CString> ExtractFilesFromHDrop(HDROP hDrop);

    /**
     *  把 OLE 拖放接口给的屏幕坐标换算成某窗口的客户区坐标。纯函数、无副作用。
     *    hwnd：    目标窗口；为空或换算失败时原样返回屏幕坐标。
     *    ptScreen：屏幕坐标（IDropTarget 各方法的 POINTL 参数）。
     *  @return 客户区坐标。
     */
    static POINT ClientPointFromScreen(HWND hwnd, const POINTL& ptScreen);

private:
    DuiDropTargetImpl* m_impl  = nullptr;
    HWND               m_hwnd  = nullptr;
};

} // namespace balloonwjui
