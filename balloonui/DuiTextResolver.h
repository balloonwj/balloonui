/**
 *  balloonui 内置文字的转换回调。
 *
 *  库里有少量自带的界面文字（目前是输入框与富文本框右键菜单的命令文字）。宿主程序在启动时登记一个回调，
 *  库取这些文字时经 ResolveText 交给回调换成宿主当前界面语言的文字；没有登记回调时原样返回，行为与改动之前相同。
 *
 *  只有头文件：登记的回调存放在函数内的静态变量里，exe 与各个静态链接了 balloonui 的 DLL 各有一份，互不影响。
 *
 *  典型用法（宿主程序启动时、创建任何窗口之前）：
 *      static LPCTSTR ResolveBalloonUiText(LPCTSTR text) { return TR(text); }
 *      balloonwjui::SetTextResolver(&ResolveBalloonUiText);
 *  库内取文字：
 *      return ResolveText(_T("剪切(&T)"));
 *
 *  balloonwj@qq.com   2026-10-04
 */
#pragma once

#include <windows.h>
#include <tchar.h>

namespace balloonwjui {

//转换回调的类型：传入库内置的原文，返回要显示的文字；返回的指针须在进程（或模块）生命周期内有效，返回 NULL 表示沿用原文
typedef LPCTSTR (*DuiTextResolverFn)(LPCTSTR text);

//登记的转换回调所在的存储；每个模块（exe、各 DLL）各有一份
inline DuiTextResolverFn& DuiTextResolverSlot()
{
    static DuiTextResolverFn s_fn = NULL;
    return s_fn;
}

/**
 *  登记转换回调。在创建任何窗口之前、单线程环境下调用一次。
 *  @param fn 转换回调；传 NULL 取消登记，此后 ResolveText 原样返回
 */
inline void SetTextResolver(DuiTextResolverFn fn)
{
    DuiTextResolverSlot() = fn;
}

/**
 *  转换一段库内置的文字。
 *  @param text 原文（字符串字面量）；为 NULL 时返回 NULL
 *  @return 登记了回调时返回回调的结果（回调返回 NULL 时仍返回原文）；没有登记回调时返回 text 本身
 */
inline LPCTSTR ResolveText(LPCTSTR text)
{
    DuiTextResolverFn fn = DuiTextResolverSlot();
    if (fn == NULL || text == NULL)
    {
        return text;
    }
    LPCTSTR resolved = fn(text);
    return (resolved != NULL) ? resolved : text;
}

} // namespace balloonwjui
