/**
 *  DuiGallery 命令行解析的实现：按空白切分参数（双引号括起的部分算一个参数、引号本身去掉），
 *  再逐个识别 --lang 与 --capture-all。语法与结果字段见 GalleryCmdLine.h。
 *  balloonwj@qq.com   2026-10-09
 */
#include "stdafx.h"
#include "GalleryCmdLine.h"

#include <vector>

namespace Gallery {

namespace {

// 指定界面语言的参数名。
const TCHAR kOptLang[] = _T("--lang");
// 进入截图模式的参数名，其后跟输出目录。
const TCHAR kOptCaptureAll[] = _T("--capture-all");
// --lang 的两种取值。
const TCHAR kLangEnglish[] = _T("en");
const TCHAR kLangChinese[] = _T("zh");
// 参数名的前缀：以它开头的参数不会被当成上一个参数的取值。
const TCHAR kOptPrefix[] = _T("--");

// 按空白切分命令行；双引号括起的部分（可以含空格）算同一个参数，引号本身不保留。
std::vector<CString> SplitArgs(LPCTSTR cmdLine)
{
    std::vector<CString> args;
    if (cmdLine == NULL)
    {
        return args;
    }
    CString cur;
    bool inQuotes = false;
    bool hasToken = false;   // 当前参数是否已经开始（"" 这样的空引号也算一个参数）
    for (LPCTSTR p = cmdLine; *p != _T('\0'); ++p)
    {
        const TCHAR ch = *p;
        if (ch == _T('"'))
        {
            inQuotes = !inQuotes;
            hasToken = true;
        }
        else if (!inQuotes && (ch == _T(' ') || ch == _T('\t')))
        {
            if (hasToken)
            {
                args.push_back(cur);
                cur.Empty();
                hasToken = false;
            }
        }
        else
        {
            cur += ch;
            hasToken = true;
        }
    }
    if (hasToken)
    {
        args.push_back(cur);
    }
    return args;
}

// 是否像一个参数名（以 "--" 开头）。
bool IsOption(const CString& arg)
{
    return arg.Left((int)_tcslen(kOptPrefix)) == kOptPrefix;
}

// 记下第一处错误；已经记过错误时保留最早的那一条。
void SetError(GalleryCmdLine& out, LPCTSTR message)
{
    if (out.m_valid)
    {
        out.m_valid = false;
        out.m_error = message;
    }
}

} // namespace

GalleryCmdLine ParseGalleryCmdLine(LPCTSTR cmdLine)
{
    GalleryCmdLine out;
    std::vector<CString> args = SplitArgs(cmdLine);
    const size_t count = args.size();
    for (size_t i = 0; i < count; ++i)
    {
        const CString& arg = args[i];
        if (arg.CompareNoCase(kOptLang) == 0)
        {
            if (i + 1 >= count || IsOption(args[i + 1]))
            {
                SetError(out, _T("--lang needs a value: en or zh"));
                continue;
            }
            const CString& value = args[++i];
            if (value.CompareNoCase(kLangEnglish) == 0)
            {
                out.m_lang = LangEnglish;
                out.m_langGiven = true;
            }
            else if (value.CompareNoCase(kLangChinese) == 0)
            {
                out.m_lang = LangChinese;
                out.m_langGiven = true;
            }
            else
            {
                SetError(out, _T("--lang accepts only en or zh"));
            }
        }
        else if (arg.CompareNoCase(kOptCaptureAll) == 0)
        {
            out.m_captureAll = true;
            if (i + 1 >= count || IsOption(args[i + 1]) || args[i + 1].IsEmpty())
            {
                SetError(out, _T("--capture-all needs an output directory"));
                continue;
            }
            out.m_captureDir = args[++i];
        }
        else
        {
            CString message;
            message.Format(_T("unknown argument: %s"), (LPCTSTR)arg);
            SetError(out, message);
        }
    }
    return out;
}

} // namespace Gallery
