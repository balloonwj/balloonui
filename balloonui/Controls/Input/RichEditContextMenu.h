/**
 *  无窗口富文本控件（DuiRichEdit）右键菜单的「菜单模型」纯逻辑。
 *
 *  根据控件当前状态（是否只读 / 有无选区 / 剪贴板有无文本 / 文档是否为空 /
 *  能否撤销重做）算出右键菜单该有哪些项、各项是否可用（灰显）。抽成无副作用
 *  的纯函数，是为了能脱离窗口与消息循环做单元测试 —— 真正弹出菜单的那一步
 *  是同步阻塞的，测试里不能真弹。
 *
 *  本文件刻意不依赖 DuiMenu、也不碰任何 Win32 资源，只产出「模型 + 文案」，
 *  让「算什么菜单」与「怎么弹菜单」彻底分开。
 *
 *  与同目录 EditContextMenu.h 的关系：那一份服务的是早先内嵌真 Win32 输入框
 *  子窗口的纯文本输入框实现，只有剪切、复制、粘贴、全选四条命令。本文件当初
 *  另起一份而不是往那边加命令，是因为往那边的枚举里塞撤销、重做、删除、粘贴
 *  为纯文本，会连带改变那个控件的菜单，属于计划外的行为变化。那份实现现已删除，
 *  如今的普通输入框控件 DuiEdit 继承自 DuiRichEdit，右键菜单走的就是本文件
 *  这一份模型。
 *
 *  典型用法（见 DuiRichEdit::ShowContextMenu）：
 *      DuiRichEditMenuState st;
 *      st.m_readOnly = ...;              // 从控件自身状态填
 *      st.m_hasSelection = ...;
 *      ...
 *      std::vector<DuiRichEditMenuItem> items;
 *      BuildDuiRichEditContextMenu(st, items);   // 默认项
 *      // ... 调用方可在此追加自定义项、或增删改默认项 ...
 *      NormalizeDuiRichEditContextMenu(items);   // 规整分隔条
 *      // 遍历 items：分隔条 → AppendSeparator；其余 → AppendItem / AppendDisabled
 *
 *  balloonwj@qq.com   2026-08-14
 */
#pragma once

#include "../../BalloonUiFeatures.h"
#include "../../DuiTextResolver.h"   // ResolveText：内置文字经宿主登记的回调转换（多语言）
#if BUI_FEATURE_RICHTEXT

#include <vector>
#include <atlstr.h>

namespace balloonwjui {

// 右键菜单的内置命令编号。
//
// 这些数值直接当作 DuiMenu 项的 id 使用：TrackPopup 返回被选项的该值后，
// 控件据此调用对应的编辑命令。
//
// 取值刻意只占 1 到 20 号，20 之后留空是给内置命令以后扩充用的；
// 调用方追加的自定义项必须从 kRichEditMenuCustomBase 起编号。
enum DuiRichEditMenuCommand
{
    //占位值。正常构建出的菜单里不会出现，仅用于表示「没有命令」。
    kRichEditCmdNone       = 0,
    //撤销：回退上一次编辑。只读时不出现，不能撤销时灰显。
    kRichEditCmdUndo       = 1,
    //重做：把刚撤销掉的编辑再做一遍。只读时不出现，不能重做时灰显。
    kRichEditCmdRedo       = 2,
    //剪切：把选区文本移入剪贴板并删除原文。只读时不出现，无选区时灰显。
    kRichEditCmdCut        = 3,
    //复制：把选区文本拷入剪贴板、不改原文。任何模式下都出现，无选区时灰显。
    kRichEditCmdCopy       = 4,
    //粘贴：把剪贴板内容插入光标处 / 替换选区，保留其原有格式。
    //只读时不出现，剪贴板无文本时灰显。
    kRichEditCmdPaste      = 5,
    //粘贴为纯文本：只取剪贴板里的文字，丢掉字体、字号、颜色等格式。
    //从浏览器等外部程序粘贴时多数场景要的是这一项。可用条件同「粘贴」。
    kRichEditCmdPastePlain = 6,
    //删除：删掉选区文本，且**不**写剪贴板（与剪切的唯一区别）。
    //只读时不出现，无选区时灰显。
    kRichEditCmdDelete     = 7,
    //全选：选中文档全部内容。任何模式下都出现，文档为空时灰显。
    kRichEditCmdSelectAll  = 8,
};

// 调用方追加的自定义菜单项的编号下限。
//
// 低于该值的编号会与上面的内置命令撞号，症状是「点了自定义项却执行了粘贴」
// 这类很难查的现象，所以控件在登记自定义项时会挡掉并记一条日志。
const UINT kRichEditMenuCustomBase = 1000;

// 构建菜单模型所需的控件状态。无窗口依赖，便于单元测试。
struct DuiRichEditMenuState
{
    bool m_readOnly         = false;   // 控件是否只读：只读时只保留复制与全选
    bool m_hasSelection     = false;   // 是否有非空选区：决定剪切 / 复制 / 删除是否可用
    bool m_clipboardHasText = false;   // 剪贴板里是否有文本：决定两种粘贴是否可用
    bool m_hasText          = false;   // 文档内是否有任何文本：决定全选是否可用
    bool m_canUndo          = false;   // 引擎报告能否撤销：决定撤销是否可用
    bool m_canRedo          = false;   // 引擎报告能否重做：决定重做是否可用
};

// 菜单模型里的一项。
//
// 内置项与自定义项用的是同一个结构 —— 对「怎么弹菜单」那一步而言两者没有
// 区别，区别只在编号落在哪一段，以及被选中之后由谁来处理。
struct DuiRichEditMenuItem
{
    //命令编号。内置项取 DuiRichEditMenuCommand，自定义项由调用方指定
    //（须不小于 kRichEditMenuCustomBase）。分隔条此值无意义。
    UINT    m_id       = kRichEditCmdNone;
    //显示文案，含 & 助记符。分隔条此值为空。
    CString m_text;
    //是否可用。false 表示灰显、点不动。分隔条忽略此值。
    bool    m_enabled  = false;
    //本项是不是一条分隔条。为 true 时上面三个字段都不参与。
    bool    m_separator = false;
};

// 内置命令对应的菜单文案（带 & 助记符，与 Windows 自带富文本框的右键菜单
// 用词一致）。
//   nCmd：内置命令编号。传入自定义编号或分隔条时返回空串。
//   返回：只读的字符串字面量，调用方不持有、不释放。
inline LPCTSTR DuiRichEditMenuCommandLabel(UINT nCmd)
{
    switch (nCmd)
    {
    //撤销
    case kRichEditCmdUndo:
        return ResolveText(_T("撤销(&U)"));
    //重做
    case kRichEditCmdRedo:
        return ResolveText(_T("重做(&R)"));
    //剪切
    case kRichEditCmdCut:
        return ResolveText(_T("剪切(&T)"));
    //复制
    case kRichEditCmdCopy:
        return ResolveText(_T("复制(&C)"));
    //粘贴（保留格式）
    case kRichEditCmdPaste:
        return ResolveText(_T("粘贴(&P)"));
    //粘贴为纯文本（丢弃格式）
    case kRichEditCmdPastePlain:
        return ResolveText(_T("粘贴为纯文本(&L)"));
    //删除选区，不写剪贴板
    case kRichEditCmdDelete:
        return ResolveText(_T("删除(&D)"));
    //全选
    case kRichEditCmdSelectAll:
        return ResolveText(_T("全选(&A)"));
    //自定义编号、分隔条、以及占位值都没有内置文案
    default:
        return _T("");
    }
}

// 造一个普通菜单项。内部辅助，供本文件与调用方共用。
//   nCmd：命令编号。
//   szText：显示文案；传空指针时取内置文案。
//   bEnabled：是否可用。
inline DuiRichEditMenuItem MakeDuiRichEditMenuItem(UINT nCmd, LPCTSTR szText,
                                                  bool bEnabled)
{
    DuiRichEditMenuItem item;
    item.m_id        = nCmd;
    item.m_text      = (szText != nullptr) ? szText : DuiRichEditMenuCommandLabel(nCmd);
    item.m_enabled   = bEnabled;
    item.m_separator = false;
    return item;
}

// 造一条分隔条。
inline DuiRichEditMenuItem MakeDuiRichEditMenuSeparator()
{
    DuiRichEditMenuItem item;
    item.m_separator = true;
    return item;
}

// 按控件状态构建默认菜单，结果**追加**到 items 末尾（不清空原有内容）。
//
// 产出的内容：
//   读写模式：撤销 / 重做 /（分隔条）/ 剪切 / 复制 / 粘贴 / 粘贴为纯文本 /
//             删除 /（分隔条）/ 全选
//   只读模式：复制 /（分隔条）/ 全选
//
// 各项的可用规则见 DuiRichEditMenuCommand 里逐条的注释。
//
//   state：控件当前状态。
//   items：出参，默认项追加到这里。
inline void BuildDuiRichEditContextMenu(const DuiRichEditMenuState& state,
                                        std::vector<DuiRichEditMenuItem>& items)
{
    if (!state.m_readOnly)
    {
        // ---- 撤销 / 重做 ----
        //
        // 只读模式下这两项**整项不出现**而不是灰显：只读控件的内容不会变，
        // 摆一个永远点不动的撤销只会让菜单更长。
        items.push_back(MakeDuiRichEditMenuItem(kRichEditCmdUndo, nullptr,
                                                state.m_canUndo));
        items.push_back(MakeDuiRichEditMenuItem(kRichEditCmdRedo, nullptr,
                                                state.m_canRedo));
        items.push_back(MakeDuiRichEditMenuSeparator());

        // ---- 剪切 / 复制 / 两种粘贴 / 删除 ----
        items.push_back(MakeDuiRichEditMenuItem(kRichEditCmdCut, nullptr,
                                                state.m_hasSelection));
        items.push_back(MakeDuiRichEditMenuItem(kRichEditCmdCopy, nullptr,
                                                state.m_hasSelection));
        items.push_back(MakeDuiRichEditMenuItem(kRichEditCmdPaste, nullptr,
                                                state.m_clipboardHasText));
        items.push_back(MakeDuiRichEditMenuItem(kRichEditCmdPastePlain, nullptr,
                                                state.m_clipboardHasText));
        items.push_back(MakeDuiRichEditMenuItem(kRichEditCmdDelete, nullptr,
                                                state.m_hasSelection));
    }
    else
    {
        // ---- 只读：只保留复制 ----
        items.push_back(MakeDuiRichEditMenuItem(kRichEditCmdCopy, nullptr,
                                                state.m_hasSelection));
    }

    // 把编辑类命令与「全选」分开。
    items.push_back(MakeDuiRichEditMenuSeparator());
    items.push_back(MakeDuiRichEditMenuItem(kRichEditCmdSelectAll, nullptr,
                                            state.m_hasText));
}

// 规整分隔条：去掉开头的分隔条、结尾的分隔条，并把连续多条合并成一条。
//
// 为什么需要这一步：调用方可以在默认菜单的基础上删项（比如去掉整组粘贴命令），
// 删完就会留下开头悬空的分隔条、或者两条挨在一起的分隔条 —— 界面上表现为
// 菜单顶部莫名多出一条横线、或者中间出现一段过宽的空隙。规整放在这里统一做，
// 调用方增删项时不必自己操心分隔条。
//
//   items：入参兼出参，就地整理。
inline void NormalizeDuiRichEditContextMenu(std::vector<DuiRichEditMenuItem>& items)
{
    std::vector<DuiRichEditMenuItem> result;
    result.reserve(items.size());

    for (size_t i = 0; i < items.size(); ++i)
    {
        if (!items[i].m_separator)
        {
            result.push_back(items[i]);
            continue;
        }

        // 开头的分隔条直接丢弃 —— 它前面没有任何东西可分隔。
        if (result.empty())
        {
            continue;
        }
        // 上一项已经是分隔条了，本条与它重复，丢弃。
        if (result.back().m_separator)
        {
            continue;
        }
        result.push_back(items[i]);
    }

    // 循环结束后可能还剩一条结尾的分隔条（它后面没有内容了），去掉。
    if (!result.empty() && result.back().m_separator)
    {
        result.pop_back();
    }

    items.swap(result);
}

} // namespace balloonwjui

#endif // BUI_FEATURE_RICHTEXT
