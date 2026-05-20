#pragma once

// .cpp must include stdafx.h first.
#include "DuiControl.h"
#include "BalloonUiApi.h"
#include "BalloonUiFeatures.h"
#include <string>
#include <map>
#include <vector>
#include <memory>
#include <functional>

#if BUI_FEATURE_XMLBUILDER

namespace balloonwjui {

#if BUI_FEATURE_FRAMEWINDOW
struct DuiFrameWindowConfig;
#endif

// Declarative DUI tree builder. Replaces hand-written C++ AddChild
// chains for forms, settings pages, and other static layouts.
//
// Subset of XML accepted (no DOCTYPE, no namespaces, no comments, no
// CDATA, no text content — children only):
//
//   <vbox padding="20" gap="6">
//     <label text="Username" textColor="40,40,40" fixedHeight="22"/>
//     <hbox gap="8" fixedHeight="32">
//       <edit  id="100" placeholder="enter name" weight="2"/>
//       <button id="101" text="OK" fixedWidth="80"/>
//     </hbox>
//   </vbox>
//
// Recognized element tags (V1):
//   vbox / hbox / grid          — DuiVBox / DuiHBox / DuiGrid
//   label                       — DuiLabel
//   button                      — DuiButton
//   edit                        — DuiEditHost
//
// Common attributes:
//   id          — UINT, sets ctrlId
//   fixedWidth  — px, layout hint .Fixed(<this>) on the main axis when host is HBox
//   fixedHeight — px, same for VBox
//   weight      — int, layout hint .Weight(...)
//   margin      — int, all four
//
// Container-only attributes:
//   padding     — int (all four sides) or "l,t,r,b"
//   gap         — int between children
//   rows / cols — DuiGrid
//
// Per-control attributes:
//   text        — DuiLabel::SetText, DuiButton::SetText
//   textColor   — "r,g,b" → COLORREF
//   placeholder — DuiEditHost::SetPlaceholder
//   password    — "true"/"false" → DuiEditHost::SetPassword
//   multiline   — "true"/"false"
//   buttonType  — "push"/"icon"/"checkbox"/"radio"
//
// Anything unrecognized is silently ignored — UNLESS the caller passes a
// custom-tag factory (see CustomFactory below). The factory is invoked
// for any tag not recognized as a built-in (vbox / hbox / grid / label /
// button / edit). Typical use case: business-layer self-drawn composite
// controls (chat bubbles, list rows, custom rails) that don't belong in
// the kernel library but want to participate in declarative layout.
//
// A custom-built control may itself contain XML children. The kernel
// will recurse into them and append them via AddChild(); if the custom
// control overrides Layout() it can place those children itself —
// effectively a "self-drawn layout" container. If it does NOT override
// Layout, default behavior is whatever the base DuiControl does (no
// auto layout). Custom controls are therefore both leaf nodes (paint-
// only) and internal nodes (paint-and-layout) at the caller's choice.
//
// Usage:
//   const char* kXml =
//       "<vbox padding=\"20\" gap=\"6\">"
//       "  <label text=\"Username\" fixedHeight=\"22\"/>"
//       "  <edit id=\"100\" placeholder=\"enter name\" fixedHeight=\"32\"/>"
//       "  <button id=\"101\" text=\"OK\" fixedHeight=\"32\"/>"
//       "</vbox>";
//   auto root = balloonwjui::DuiXmlBuilder::FromString(kXml);
//   host->SetRoot(std::move(root));
class BUI_API DuiXmlBuilder
{
public:
    // Pure-data XML node (intermediate AST). Exposed for tests + custom
    // factories.
    struct Node
    {
        std::string                         tag;
        std::map<std::string, std::string>  attrs;
        std::vector<Node>                   children;
    };

    // Custom-tag factory. Receives the parsed Node for an unknown tag
    // and returns the constructed control (or nullptr to fall through to
    // the default "skip unknown" behavior). The builder will then recurse
    // into the Node's children and AddChild() each onto the returned
    // control, so custom containers naturally compose with built-in
    // <vbox>/<hbox>/<grid> children if that's what the author writes.
    using CustomFactory = std::function<std::unique_ptr<DuiControl>(const Node&)>;

    // Parse an XML string into a DUI tree. Returns the root control or
    // null on parse error / empty input. The caller takes ownership.
    // If `factory` is set, it is called for every tag not recognized as
    // a built-in.
    static std::unique_ptr<DuiControl> FromString(LPCSTR xmlUtf8,
                                                  const CustomFactory& factory = {});

    // Convenience: parse a UTF-16 string. (Internally re-encodes to
    // UTF-8 for the parser.)
    static std::unique_ptr<DuiControl> FromString(LPCWSTR xmlW,
                                                  const CustomFactory& factory = {});

    // Lower-level parse step. Returns true on success.
    static bool Parse(LPCSTR xmlUtf8, Node& outRoot);

    // Build a DuiControl tree from a pre-parsed Node. Returns null on
    // unrecognized root tag (when factory is empty or returns null).
    static std::unique_ptr<DuiControl> Build(const Node& root,
                                             const CustomFactory& factory = {});

    // ──────────────────────────────────────────────────────────────────
    // Frame-window XML (top-level <frame-window> 元素)
    // ──────────────────────────────────────────────────────────────────
    //
    // 顶层 XML 形如：
    //
    //   <frame-window
    //       title="好友资料"
    //       title-bar-height="40"
    //       title-bar-transparent="true"
    //       title-text-color="255,255,255"
    //       caption-glyph-color="255,255,255"
    //       has-min-button="true" has-max-button="true" has-close-button="true"
    //       min-w="320" min-h="240"
    //       resizable="true"
    //       border-px="8"
    //       client-border-color="200,200,200"          <!-- 或 "none" -->
    //       bg-image="BuddyInfoDlgBg.png"              <!-- 相对 exe 目录 -->
    //       bg-src-insets="10,69,10,10"
    //       bg-dst-insets="10,40,10,10">
    //     <vbox padding="24,16,24,16" gap="10">
    //       ...                                          <!-- 客户区控件树 -->
    //     </vbox>
    //   </frame-window>
    //
    // 属性约定：
    //   · COLORREF 写 "r,g,b" 三段；client-border-color 还接受 "none"
    //     表示 CLR_INVALID（关闭边框）。
    //   · RECT (insets / padding) 写 "l,t,r,b" 或单值（4 边相同）。
    //   · 布尔 "true"/"false"（其它值视为 false）。
    //   · 任何属性缺省 = "未设" — DuiFrameWindow::ApplyConfig 不会调
    //     对应 setter，frame 保留默认行为。
    //   · bg-dst-insets 缺省 → 退化为 src == dst（经典 9-grid，4 角不缩）。
    //   · bg-image 路径：绝对路径直接用；相对路径由 ResolveAssetPath
    //     转成"绝对（相对 exe 目录）"再写回 cfg.bgImagePath。
    //
    // 接口：解析时同时填两份输出 —— frame 配置 + 客户区控件树。
    //   · outConfig 拿到所有 frame 属性（含已解析 bg-image 绝对路径）
    //   · 返回值是 <frame-window> 内部的<u>第一个</u>子元素构造的控件树
    //     （= 客户区根）。子元素必须 0 个或 1 个；多个时只取第一个、
    //     忽略其余（与 SetClientContent 单一 root 的语义匹配）。
    //
    // factory 与 FromString 同义，作用于客户区控件树里出现的未知 tag。
#if BUI_FEATURE_FRAMEWINDOW
    static std::unique_ptr<DuiControl> FromFrameXml(LPCSTR xmlUtf8,
                                                    DuiFrameWindowConfig& outConfig,
                                                    const CustomFactory& factory = {});
    static std::unique_ptr<DuiControl> FromFrameXml(LPCWSTR xmlW,
                                                    DuiFrameWindowConfig& outConfig,
                                                    const CustomFactory& factory = {});
#endif

    // 把 user 传入的资源相对路径转成绝对路径。规则：
    //   · 绝对路径（以盘符 / "\\" 开头）→ 直接返回。
    //   · 相对路径 → 拼到 GetModuleFileName(NULL) 所在目录后返回。
    //   · 空字符串 → 返回空。
    //
    // 这是 XML 解析器内部用来处理 <frame-window bg-image="..."> 的工具，
    // 也对外暴露给业务自己用（如自定义 factory 里加载 icon 资源）。
    static CString ResolveAssetPath(LPCTSTR userPath);
};

} // namespace balloonwjui

#endif // BUI_FEATURE_XMLBUILDER
