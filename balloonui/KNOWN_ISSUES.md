# balloonui — 已知未修问题

已经诊断出来但还没修的问题。每条记录症状、当前对根因的理解、试过哪些方案、
下一步被什么卡住。

---

## ISSUE-001：HwndHost 在 DuiScrollView 滚动期间出现拖影

**用户可见现象**：在 `DuiGallery` 的 Edit / RichEdit 页，滚动期间（鼠标滚轮、
拖动滚动条、点击 track 翻页），EDIT / RichEdit 文本框出现一帧左右的"拖影" ——
EDIT 移到新位置之后，**旧位置上的 EDIT 像素还残留约一帧**才被 host 的
back-buffer + BitBlt 盖成页面背景色。

用户确认（2026-05-10）："EDIT 在旧位置残留了一番才被清掉（拖影）"。
**这是已存在的老 bug**，不是这次 SetWindowRgn（A 修）或 UpdateWindow
（本次尝试）引入的。

### 已经试过什么

`DuiScrollView::OnSbScrolled` 在 `Invalidate()` 之后加了
`::UpdateWindow(host->m_hWnd)`，本意是把入队的 WM_PAINT 同步派发掉、让
back buffer 在 `SetScrollPos` 返回前就把 EDIT 旧位置盖掉。新加的测试
（`Tests/DuiTier3Tests.cpp` 里）验证了这一行为：

```
ScrollSync_FlushesPendingPaint        — SetScrollPos 后 GetUpdateRect == FALSE
ScrollSync_RepeatedScrollsAllFlushed  — 连续 6 次滚动后同样
ScrollSync_ActuallyMovedEditHwnd      — sanity 检查：EDIT 真的被滚走了
```

三个全绿。**但用户反馈视觉上的拖影还在**，说明单元测试断言"update region 为空"
是必要条件但不充分 —— 测试没覆盖到的某个 paint 时序细节里还有问题。

### 怀疑的根因（未验证）

`DuiHost` 用 `WS_CLIPCHILDREN` + 抑制 `WM_ERASEBKGND` + 单 back-buffer
+ 全 client 区 BitBlt 的 paint 模型。当子 HWND（EDIT）在滚动 cascade 中通过
`SetWindowPos` 移动时：

1. OS 把父窗口"新暴露区域"（EDIT 之前所在位置）标记为需要重画。
2. `WS_CLIPCHILDREN` 让 BitBlt 的 clip region 用**当前**子位置 —— 老 EDIT
   区域不再被排除，所以新一轮 BitBlt 会把 back buffer 像素写到那块。
3. `Invalidate()` + `UpdateWindow()` 把这次 BitBlt 同步触发。测试确认
   `SetScrollPos` 返回时 host 的 update region 已经为空。
4. **但拖影仍然可见。** 推测是：OS 对 BitBlt clip 的计算、或者子窗口自己
   的 WM_PAINT（在父 WM_PAINT 之后被 OS 派发）还有一帧 stale 像素被
   合成出来。可能机制：

   - **OS 延迟擦除**：`WS_CLIPCHILDREN` + 父抑制 `WM_ERASEBKGND` 可能让
     OS 在合成层面 double-buffer 这次移动，短暂展示子窗口旧位置。
   - **EDIT 自己的 WM_PAINT**：`SetWindowPos` 之后，EDIT 收到针对新 client
     区域的 `WM_PAINT`。在那个 paint 完成之前，OS 在新位置展示 stale 子
     窗口像素。如果 OS 在移动期间把子窗口的旧像素合成到新屏幕位置，看起来
     就像拖影。
   - **DWM 合成器**：Win10+ DWM 对子窗口移动可能加了短暂淡出动画。

### 调研被什么卡住

从 PowerShell 程序化复现尝试遇到了几堵墙：

- **跨进程发 `WM_MOUSEWHEEL`**（`SendMessage` / `mouse_event` /
  `SendInput` 都试了）：滚轮能到达 `DuiHost::OnMouseWheel`，但
  `HitTopMost` 返回的是鼠标下面最深的子控件 —— 一般是 Label、Gap 或 EDIT
  —— 这些都不重写 `OnMouseWheel`，而 `DuiHost` **不会**沿父链向上 bubble
  滚轮事件到 scrollable 祖先。所以滚轮被静默吞掉，除非鼠标正好在滚动条上
  或者后续给 `DuiScrollView::OnMouseWheel` 加一个祖先查找逻辑。
- **点击滚动条 track 用 `WM_LBUTTONDOWN`**：位置算对了，但 DuiGallery 在
  几次实验之间反复被最小化（具体原因未明 —— 可能是某个 fixture popup 跟
  `SetForegroundWindow` 互动出现问题）。
- 捕获拖影需要"亚帧"级别的截图时序，PowerShell 基于消息队列的截图循环
  很难精确踩到那个 1 帧窗口。

### 下次接着搞时建议的下一步

1. **顺手值得修的副作用**：扩展 `DuiHost::OnMouseWheel`，从
   `HitTopMost(pt)` 沿父链向上找 `OnMouseWheel` 返回 true 的控件。这跟
   Win32 / Cocoa / web 的标准行为一致（"滚轮滚的是鼠标所在的滚动容器"），
   同时也能解锁拖影 bug 的自动化复现。

2. **试试 `RedrawWindow` 的更强 flag 组合**：
   ```cpp
   ::RedrawWindow(host->m_hWnd, &svRect, NULL,
                  RDW_INVALIDATE | RDW_UPDATENOW | RDW_ERASE | RDW_ALLCHILDREN);
   ```
   `RDW_ALLCHILDREN` 强制子窗口（EDIT）也作为同步 pass 的一部分被重画 ——
   也许能消除 `UpdateWindow` 单独搞不定的"EDIT 端 WM_PAINT 延迟"。

3. **在 `HwndHostControl::Layout` 里 `SetWindowPos` 之前手动擦掉旧 EDIT
   矩形**：在调 `SetWindowPos` 前，把 host 的 back buffer 在 EDIT *旧*
   DUI 矩形那块直接 BitBlt 到屏幕 DC。这样 EDIT 还没移动旧位置就已经
   正确了。

4. **去掉 host 的 WS_CLIPCHILDREN，OnPaint 里自己做 clip**：架构改动大，
   但能消除整个"子窗口移动后父窗口没重画暴露区"的问题大类。

### 涉及文件

- `third_party/balloonui/Controls/Window/DuiScrollBar.cpp` —— 当前
  `OnSbScrolled` 已经加了 `UpdateWindow`（正确但不充分）。
- `third_party/balloonui/HwndHostControl.cpp` —— `Layout` 在滚动 cascade
  里调 `SetWindowPos` 移动子 HWND。
- `third_party/balloonui/DuiHost.cpp` —— `OnPaint`（back buffer + BitBlt）、
  `OnEraseBkgnd`（返回 TRUE）、`OnMouseWheel`（不 bubble）。
- `third_party/balloonui/Tests/DuiTier3Tests.cpp` —— 已有的
  `ScrollSync_*` 和 `HwndHostInSV_*` 测试；需要至少加一个新测试断言"旧
  位置的像素被清掉"，而不是只断言"update region 为空"。
