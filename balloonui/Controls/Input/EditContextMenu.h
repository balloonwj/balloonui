/**
 *  文本框右键菜单的"菜单模型"纯逻辑。
 *
 *  根据文本框当前状态（是否只读 / 有无选区 / 剪贴板有无文本 / 有无文本 /
 *  是否密码框）算出右键菜单该有哪些项、各项是否可用（灰显）。抽成无副作用
 *  的纯函数，是为了能脱离真 HWND 单元测试 —— DuiEditHost / DuiRichEditHost
 *  各自的 WM_CONTEXTMENU 子类过程在运行期读取自身状态后调用本函数，再据
 *  返回的模型构建 DuiMenu 弹出。
 *
 *  本文件刻意不依赖 DuiMenu、也不碰任何 Win32 资源，只产出"模型 + 文案"，
 *  让"算什么菜单"与"怎么弹菜单"彻底分开。
 *
 *  典型用法（见 DuiEditHost.cpp / DuiRichEditHost.cpp 的 ShowContextMenu）：
 *      EditContextState st;
 *      st.m_readOnly = ...;            // 从控件自身状态填
 *      st.m_hasSelection = ...;
 *      st.m_clipboardHasText = ...;
 *      st.m_hasText = ...;
 *      st.m_isPassword = ...;
 *      std::vector<EditContextMenuItem> model = BuildEditContextMenu(st);
 *      // 遍历 model：分隔条 → AppendSeparator；其余 → AppendItem / AppendDisabled，
 *      // 文案取 EditContextCommandLabel(cmd)。
 *
 *  balloonwj@qq.com   2026-06-02
 */
#pragma once

#include "../../BalloonUiFeatures.h"
#if defined(BUI_FEATURE_EDIT) || defined(BUI_FEATURE_RICHEDIT)

#include <vector>

namespace balloonwjui {

// 文本框右键菜单的命令标识。数值直接当作 DuiMenu 项的 id 使用：TrackPopup
// 返回被选项的该值后，控件的 WM_CONTEXTMENU 处理据此调用对应编辑命令。
enum EditContextCommand
{
    EditCtxCmd_None      = 0,    // 占位 / 无命令（正常构建不会作为菜单项出现）
    EditCtxCmd_Cut       = 1,    // 剪切：把选区文本移入剪贴板并删除原文（仅读写、有选区时可用）
    EditCtxCmd_Copy      = 2,    // 复制：把选区文本拷入剪贴板、不改原文（有选区且非密码框时可用）
    EditCtxCmd_Paste     = 3,    // 粘贴：把剪贴板文本插入光标处 / 替换选区（仅读写、剪贴板有文本时可用）
    EditCtxCmd_SelectAll = 4,    // 全选：选中文本框内全部文本（框内有文本时可用）
    EditCtxCmd_Separator = 5,    // 分隔条：视觉分组用，不可点击、无 enabled 含义
};

// 文本框当前状态 —— 构建菜单模型的全部输入。无 HWND 依赖，便于单测。
struct EditContextState
{
    bool m_readOnly         = false;   // 文本框是否只读：只读时菜单无"剪切""粘贴"两项
    bool m_hasSelection     = false;   // 当前是否有非空选区：决定"剪切""复制"是否可用
    bool m_clipboardHasText = false;   // 剪贴板里是否有文本：决定"粘贴"是否可用
    bool m_hasText          = false;   // 文本框内是否有任何文本：决定"全选"是否可用
    bool m_isPassword       = false;   // 是否密码框：密码框下 OS 禁止复制 / 剪切，故两项灰显
};

// 菜单模型里的一项。
struct EditContextMenuItem
{
    EditContextCommand m_cmd     = EditCtxCmd_None;  // 命令（含分隔条）
    bool               m_enabled = false;           // 是否可用；false = 灰显（分隔条忽略此值）
};

// 根据 state 构建右键菜单模型（纯函数，无副作用）。
//   读写模式产出：剪切 / 复制 / 粘贴 /（分隔条）/ 全选
//   只读模式产出：复制 /（分隔条）/ 全选
//   各项 enabled 规则：
//     · 剪切 / 复制：有选区 且 非密码框（密码框 OS 本身禁止把内容拷出，给出
//       可点的项反而误导，故灰显）；
//     · 粘贴：剪贴板有文本；
//     · 全选：框内有文本。
//   state：文本框当前状态（见 EditContextState 各字段注释）。
//   返回：有序的菜单项列表（含分隔条项）；调用方按序填进 DuiMenu。
inline std::vector<EditContextMenuItem> BuildEditContextMenu(const EditContextState& state)
{
    std::vector<EditContextMenuItem> items;

    // 剪切 / 复制能否用：要有选区，且非密码框。
    const bool copyEnabled = state.m_hasSelection && !state.m_isPassword;

    if (!state.m_readOnly)
    {
        // ---- 读写：剪切 / 复制 / 粘贴 ----
        EditContextMenuItem cut;
        cut.m_cmd     = EditCtxCmd_Cut;
        cut.m_enabled = copyEnabled;        // 剪切 = 复制 + 删除，可用条件同复制
        items.push_back(cut);

        EditContextMenuItem copy;
        copy.m_cmd     = EditCtxCmd_Copy;
        copy.m_enabled = copyEnabled;
        items.push_back(copy);

        EditContextMenuItem paste;
        paste.m_cmd     = EditCtxCmd_Paste;
        paste.m_enabled = state.m_clipboardHasText;
        items.push_back(paste);
    }
    else
    {
        // ---- 只读：仅复制（无剪切、无粘贴）----
        EditContextMenuItem copy;
        copy.m_cmd     = EditCtxCmd_Copy;
        copy.m_enabled = copyEnabled;
        items.push_back(copy);
    }

    // 分隔条：把编辑类命令与"全选"分开。
    EditContextMenuItem sep;
    sep.m_cmd = EditCtxCmd_Separator;
    items.push_back(sep);

    // 全选：框内有文本才可用。
    EditContextMenuItem selAll;
    selAll.m_cmd     = EditCtxCmd_SelectAll;
    selAll.m_enabled = state.m_hasText;
    items.push_back(selAll);

    return items;
}

// 命令对应的菜单显示文案（带 & 助记符，与 Windows 标准文本框右键菜单一致：
// 剪切(T) / 复制(C) / 粘贴(P) / 全选(A)）。
//   cmd：命令；分隔条 / None 返回空串。
//   返回：只读的字符串字面量（调用方不持有、不释放）。
inline LPCTSTR EditContextCommandLabel(EditContextCommand cmd)
{
    switch (cmd)
    {
    case EditCtxCmd_Cut:
        return _T("剪切(&T)");
    case EditCtxCmd_Copy:
        return _T("复制(&C)");
    case EditCtxCmd_Paste:
        return _T("粘贴(&P)");
    case EditCtxCmd_SelectAll:
        return _T("全选(&A)");
    default:
        return _T("");
    }
}

} // namespace balloonwjui

#endif // BUI_FEATURE_EDIT || BUI_FEATURE_RICHEDIT
