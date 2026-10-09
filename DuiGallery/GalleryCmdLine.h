/**
 *  DuiGallery 的命令行解析。语法：DuiGallery.exe [--lang en|zh] [--capture-all <目录>]，
 *  两个参数顺序不限，取值不区分大小写，目录含空格时要加双引号。解析写成纯函数，
 *  main.cpp 据结果决定界面语言、是否进入截图模式；GalleryTests 直接测它。
 *  典型用法：
 *      Gallery::GalleryCmdLine cmd = Gallery::ParseGalleryCmdLine(lpCmdLine);
 *      if (cmd.m_langGiven) { Gallery::SetCurrentLanguage(cmd.m_lang); }
 *  balloonwj@qq.com   2026-10-09
 */
#pragma once

#include "GalleryText.h"

namespace Gallery {

// 命令行解析结果。各字段在解析失败时仍按已经读到的部分填写，调用方据 m_valid 决定怎么处理：
// 截图模式下无效即退出，正常启动时忽略无效参数照常启动。
struct GalleryCmdLine
{
    bool     m_valid;        // 命令行是否完全有效；false 时 m_error 说明第一处错误
    bool     m_captureAll;   // 是否给了 --capture-all（进入截图模式）
    CString  m_captureDir;   // --capture-all 的输出目录（已去掉双引号）；没给目录时为空
    bool     m_langGiven;    // 是否给了合法的 --lang
    Language m_lang;         // 界面语言；没给或不合法时为 LangChinese（与不带参数启动时相同）
    CString  m_error;        // 第一处错误的说明（英文，写进调试输出）；有效时为空

    GalleryCmdLine()
        : m_valid(true), m_captureAll(false), m_langGiven(false), m_lang(LangChinese)
    {
    }
};

// 解析命令行（不含程序名，即 WinMain 的 lpCmdLine）。
//   cmdLine：命令行文本，可以为 NULL 或空串（表示没有参数）。
//   返回：解析结果。下列情况 m_valid 为 false：--lang 缺值或值不是 en / zh；
//         --capture-all 缺目录；出现不认识的参数。
GalleryCmdLine ParseGalleryCmdLine(LPCTSTR cmdLine);

} // namespace Gallery
