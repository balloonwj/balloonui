// balloonui.dll entry point. The kernel sources expect a process-wide
// CAppModule named _Module (WTL convention); we host one here so any
// consumer (NewChatDemo, future demos) doesn't need to provide its
// own. Consumers that already define _Module (e.g. when they instantiate
// DuiHost via SubclassWindow on an existing dialog) are unaffected
// because each module gets its own copy of static globals.
#include "stdafx.h"

CAppModule _Module;

BOOL APIENTRY DllMain(HMODULE hModule,
                      DWORD   ulReason,
                      LPVOID  /*lpReserved*/)
{
    switch (ulReason)
    {
    case DLL_PROCESS_ATTACH:
        _Module.Init(NULL, hModule);
        break;
    case DLL_PROCESS_DETACH:
        _Module.Term();
        break;
    }
    return TRUE;
}
