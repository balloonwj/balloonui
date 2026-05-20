#pragma once

// .cpp 必须先 include stdafx.h（项目 PCH 约定）。

// =================================================================
// DuiAsyncImage —— 异步图片加载（避免阻塞 UI）
// =================================================================
//
// 用途：IM 客户端的头像 + 聊天历史里的内嵌图轻松上百张；UI 线程同步
// 解码 PNG → HBITMAP 一次几毫秒，几百张串起来就是秒级 jank。本模块
// 把解码挪到 worker thread，完成后通过隐藏 message-only 窗口 PostMessage
// 回 UI 线程触发回调，让控件 SetBitmap / Invalidate。
//
// V1 范围：仅文件路径（HTTP/HTTPS 留后面）。单 worker thread 对每会话
// 50–200 张图够用 —— 瓶颈在 GDI+ Bitmap::FromFile（大概 ~100KB PNG
// 单位数 ms），不在线程数。
//
// 生命期：callback 是 std::function 可捕获 caller 状态（如 DuiAvatar*
// + 一个 token 用于丢掉过期 result）。caller 自己跟踪 per-控件
// "in-flight request id" 并跳过不匹配的回调。
//
// 代码用法：
//
//     // DuiAvatar 控件里发起异步加载：
//     DWORD_PTR token = ++m_inFlightToken;
//     balloonwjui::DuiAsyncImage::Submit(_T("C:/cache/avatar_42.png"),
//         [this, token](const balloonwjui::DuiAsyncImage::Result& r) {
//             if (token != m_inFlightToken) {
//                 if (r.ok) { ::DeleteObject(r.hbm); }
//                 return;
//             }
//             if (r.ok) { SetBitmap(r.hbm); /* 拿走所有权 */ }
//         }, token);
//
//     // 取消（已经开始解的可能仍 fire 一次回调）：
//     balloonwjui::DuiAsyncImage::Cancel(reqId);
//
// XML 用法：N/A（运行时 API）。

#include <windows.h>
#include <functional>
#include <memory>

namespace balloonwjui {

// Usage:
//   // From a DuiAvatar control, request a fresh load:
//   DWORD_PTR token = ++m_inFlightToken;
//   balloonwjui::DuiAsyncImage::Submit(_T("C:/cache/avatar_42.png"),
//       [this, token](const balloonwjui::DuiAsyncImage::Result& r) {
//           if (token != m_inFlightToken) {
//               if (r.ok) { ::DeleteObject(r.hbm); }
//               return;
//           }
//           if (r.ok) { SetBitmap(r.hbm); /* takes ownership */ }
//       }, token);
//
//   // To cancel a still-pending request:
//   balloonwjui::DuiAsyncImage::Cancel(reqId);

namespace DuiAsyncImage
{
    // Result delivered to the callback. ok=true when hbm is valid;
    // hbm ownership transfers to the callback — caller is responsible
    // for ::DeleteObject when done with it.
    struct Result
    {
        bool      ok;
        HBITMAP   hbm;
        int       width;
        int       height;
        DWORD_PTR userToken;        // mirrors the token passed to Submit
    };

    typedef std::function<void(const Result&)> Callback;

    // Submit a load request. Returns a request id (monotonic, > 0).
    // userToken is opaque, copied into Result.userToken for the
    // callback's bookkeeping.
    DWORD_PTR Submit(LPCTSTR path, const Callback& cb, DWORD_PTR userToken = 0);

    // Cancel a pending request. If the worker has already started
    // decoding, the callback may still fire with an "ok" result; the
    // cancellation guarantees only that the callback is not invoked
    // after Cancel returns. Most controls track a per-control
    // "current id" instead and ignore stale results.
    void  Cancel(DWORD_PTR requestId);

    // Drain pending callbacks queued on the calling thread. The mgr's
    // hidden message-only window ordinarily delivers them via the
    // standard message loop; tests that don't pump messages can call
    // this directly.
    void  PumpForTests();

    // For tests: process one request synchronously on the calling
    // thread, bypassing the worker. Useful for verifying the load +
    // dispatch path without thread-timing flake.
    void  RunSyncForTests(LPCTSTR path, DWORD_PTR userToken, const Callback& cb);
}

} // namespace balloonwjui
