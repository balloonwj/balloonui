#include "stdafx.h"
#include "DuiMnemonic.h"

namespace balloonwjui {

namespace DuiMnemonic {

TCHAR FindChar(LPCTSTR text)
{
    if (!text)
    {
        return 0;
    }
    for (LPCTSTR p = text; *p; ++p)
    {
        if (*p != _T('&'))
        {
            continue;
        }
        TCHAR next = *(p + 1);
        if (next == 0)
        {
            return 0;
        }
        if (next == _T('&'))
        {
            // Escaped literal '&' — skip the second '&' and keep scanning.
            ++p;
            continue;
        }
        return (TCHAR)_totlower(next);
    }
    return 0;
}

CString StripPrefix(LPCTSTR text)
{
    CString out;
    if (!text)
    {
        return out;
    }
    for (LPCTSTR p = text; *p; ++p)
    {
        if (*p == _T('&'))
        {
            TCHAR next = *(p + 1);
            if (next == _T('&'))
            {
                // Escaped literal '&' — emit one '&' and skip ahead.
                out += _T('&');
                ++p;
                continue;
            }
            // Drop the prefix marker; the next char is rendered normally.
            continue;
        }
        out += *p;
    }
    return out;
}

} // namespace DuiMnemonic

} // namespace balloonwjui
