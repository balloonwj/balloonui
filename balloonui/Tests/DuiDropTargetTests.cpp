#include "stdafx.h"
#include "DuiDropTargetTests.h"

namespace balloonwjui {

namespace DuiDropTargetTests {

namespace {

struct Result
{
    CString name;
    bool ok;
    CString detail;
};

static Result OK(const CString& n)
{
    Result r;
    r.name = n;
    r.ok = true;
    return r;
}

static Result Fail(const CString& n, const CString& d)
{
    Result r;
    r.name = n;
    r.ok = false;
    r.detail = d;
    return r;
}

#define EXPECT_INT(actual, expected, name) \
    do { int _a = (actual); int _e = (expected); \
         if (_a != _e) { CString _d; _d.Format(_T("expected=%d got=%d"), _e, _a); return Fail(name, _d); } \
    } while (0)
#define EXPECT_TRUE(cond, name) \
    do { if (!(cond)) return Fail(name, _T("condition false")); } while (0)
#define EXPECT_STR(actual, expected, name) \
    do { CString _a = (actual); CString _e = (expected); \
         if (_a != _e) { return Fail(name, _T("string mismatch")); } \
    } while (0)

// ----- Default state --------------------------------------------------

static Result Test_DefaultState()
{
    DuiDropTargetHelper h;
    EXPECT_TRUE(!h.IsRegistered(), _T("Def/notReg"));
    return OK(_T("DefaultState"));
}

// ----- Register / Unregister against a real HWND ----------------------

static Result Test_RegisterUnregister()
{
    // Need a real HWND for RegisterDragDrop to bind to.
    HINSTANCE hInst = ::GetModuleHandle(nullptr);
    HWND h = ::CreateWindowEx(0, _T("STATIC"), _T(""),
                              WS_OVERLAPPED | WS_POPUP,
                              0, 0, 1, 1, nullptr, nullptr, hInst, nullptr);
    if (!h)
    {
        return Fail(_T("Reg/createHwnd"), _T("CreateWindowEx failed"));
    }

    DuiDropTargetHelper helper;
    bool ok = helper.Register(h);
    EXPECT_TRUE(ok, _T("Reg/ok"));
    EXPECT_TRUE(helper.IsRegistered(), _T("Reg/isReg"));
    helper.Unregister();
    EXPECT_TRUE(!helper.IsRegistered(), _T("Reg/unreg"));

    ::DestroyWindow(h);
    return OK(_T("RegisterUnregister"));
}

// ----- Null HWND register fails gracefully ----------------------------

static Result Test_RegisterNullFails()
{
    DuiDropTargetHelper h;
    EXPECT_TRUE(!h.Register(nullptr), _T("Null/fail"));
    EXPECT_TRUE(!h.IsRegistered(), _T("Null/notReg"));
    return OK(_T("RegisterNullFails"));
}

// ----- ExtractFilesFromHDrop synthesizes a CF_HDROP block -------------

// Build a synthetic HDROP HGLOBAL with two file paths, then verify
// ExtractFilesFromHDrop returns both paths in order.
static Result Test_ExtractFromHDrop()
{
    // CF_HDROP layout: DROPFILES header, then concatenated null-
    // terminated file paths, ending with a double null. Wide format.
    LPCTSTR p0 = _T("C:\\foo\\bar.png");
    LPCTSTR p1 = _T("D:\\baz quux.txt");
    SIZE_T len0 = _tcslen(p0);
    SIZE_T len1 = _tcslen(p1);

    SIZE_T payloadChars = len0 + 1 + len1 + 1 + 1;     // +1 sentinel
    SIZE_T payloadBytes = payloadChars * sizeof(TCHAR);
    SIZE_T total = sizeof(DROPFILES) + payloadBytes;

    HGLOBAL h = ::GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, total);
    if (!h)
    {
        return Fail(_T("Hdrop/alloc"), _T("GlobalAlloc failed"));
    }
    DROPFILES* df = (DROPFILES*)::GlobalLock(h);
    df->pFiles = sizeof(DROPFILES);
#ifdef _UNICODE
    df->fWide = TRUE;
#else
    df->fWide = FALSE;
#endif
    TCHAR* paths = (TCHAR*)((BYTE*)df + sizeof(DROPFILES));
    _tcscpy_s(paths, len0 + 1, p0);
    _tcscpy_s(paths + len0 + 1, len1 + 1, p1);
    paths[len0 + 1 + len1 + 1] = 0;     // double-null terminator
    ::GlobalUnlock(h);

    auto v = DuiDropTargetHelper::ExtractFilesFromHDrop((HDROP)h);
    bool wasTwo = (v.size() == 2);
    if (!wasTwo)
    {
        ::GlobalFree(h);
        CString d;
        d.Format(_T("expected 2 got %d"), (int)v.size());
        return Fail(_T("Hdrop/count"), d);
    }
    EXPECT_STR(v[0], CString(p0), _T("Hdrop/p0"));
    EXPECT_STR(v[1], CString(p1), _T("Hdrop/p1"));
    ::GlobalFree(h);
    return OK(_T("ExtractFromHDrop"));
}

// Null HDROP returns empty vec.
static Result Test_ExtractFromNullDropEmpty()
{
    auto v = DuiDropTargetHelper::ExtractFilesFromHDrop(nullptr);
    EXPECT_INT((int)v.size(), 0, _T("Null/empty"));
    return OK(_T("ExtractFromNullDropEmpty"));
}

// SetCallbacks before Register: storing must not crash.
static Result Test_SetCallbacksBeforeRegister()
{
    DuiDropTargetHelper h;
    h.SetCallbacks(
        [](const std::vector<CString>&) {},
        [](HBITMAP) {});
    EXPECT_TRUE(!h.IsRegistered(), _T("Cb/notReg"));
    return OK(_T("SetCallbacksBeforeRegister"));
}

// ----- 拖动过程回调（DragEnter / DragOver / DragLeave / 带坐标的 Drop）-------
//
// 这一组不依赖真实的鼠标拖放：直接取出内部的 IDropTarget，喂一个只提供 CF_HDROP
// 的假 IDataObject，把 OLE 会调的四个方法自己调一遍。helper <u>不注册</u>到窗口，
// 这样宿主窗口为空、坐标原样透传，断言里的期望值是确定的；屏幕坐标到客户区坐标
// 的换算另由 ClientPointFromScreen 那两条用例单独覆盖。

// 只提供 CF_HDROP 的最小 IDataObject。offerFiles 为 false 时什么格式都不给，
// 用来构造"格式不支持"的场景。
class FakeFilesDataObject : public IDataObject
{
public:
    explicit FakeFilesDataObject(bool offerFiles)
        : m_ref(1)
        , m_offerFiles(offerFiles)
    {
    }
    virtual ~FakeFilesDataObject() = default;

    // ---- IUnknown ----
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override
    {
        if (!ppv)
        {
            return E_POINTER;
        }
        if (riid == IID_IUnknown || riid == IID_IDataObject)
        {
            *ppv = static_cast<IDataObject*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override
    {
        return (ULONG)InterlockedIncrement(&m_ref);
    }
    ULONG STDMETHODCALLTYPE Release() override
    {
        LONG r = InterlockedDecrement(&m_ref);
        if (r == 0)
        {
            delete this;
        }
        return (ULONG)r;
    }

    // ---- IDataObject：只实现 QueryGetData / GetData，其余一律不支持 ----
    HRESULT STDMETHODCALLTYPE QueryGetData(FORMATETC* pFmt) override
    {
        if (!pFmt)
        {
            return E_POINTER;
        }
        if (m_offerFiles && pFmt->cfFormat == CF_HDROP && (pFmt->tymed & TYMED_HGLOBAL))
        {
            return S_OK;
        }
        return DV_E_FORMATETC;
    }
    HRESULT STDMETHODCALLTYPE GetData(FORMATETC* pFmt, STGMEDIUM* pMed) override
    {
        if (!pFmt || !pMed)
        {
            return E_POINTER;
        }
        if (!m_offerFiles || pFmt->cfFormat != CF_HDROP || !(pFmt->tymed & TYMED_HGLOBAL))
        {
            return DV_E_FORMATETC;
        }

        HGLOBAL h = BuildHDrop();
        if (!h)
        {
            return E_OUTOFMEMORY;
        }
        ZeroMemory(pMed, sizeof(*pMed));
        pMed->tymed          = TYMED_HGLOBAL;
        pMed->hGlobal        = h;
        pMed->pUnkForRelease = nullptr;   // 由调用方 ReleaseStgMedium 负责释放
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetDataHere(FORMATETC*, STGMEDIUM*) override
    {
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE GetCanonicalFormatEtc(FORMATETC*, FORMATETC*) override
    {
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE SetData(FORMATETC*, STGMEDIUM*, BOOL) override
    {
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE EnumFormatEtc(DWORD, IEnumFORMATETC**) override
    {
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE DAdvise(FORMATETC*, DWORD, IAdviseSink*, DWORD*) override
    {
        return OLE_E_ADVISENOTSUPPORTED;
    }
    HRESULT STDMETHODCALLTYPE DUnadvise(DWORD) override
    {
        return OLE_E_ADVISENOTSUPPORTED;
    }
    HRESULT STDMETHODCALLTYPE EnumDAdvise(IEnumSTATDATA**) override
    {
        return OLE_E_ADVISENOTSUPPORTED;
    }

    // 本假对象每次 GetData 都给的那两个路径，断言里要对照。
    static LPCTSTR Path0()
    {
        return _T("C:\\drop\\a.txt");
    }
    static LPCTSTR Path1()
    {
        return _T("C:\\drop\\b.png");
    }

private:
    // 现搓一块 CF_HDROP 内存：DROPFILES 头 + 两个以 '\0' 结尾的路径 + 一个结尾哨兵。
    static HGLOBAL BuildHDrop()
    {
        LPCTSTR p0 = Path0();
        LPCTSTR p1 = Path1();
        SIZE_T len0 = _tcslen(p0);
        SIZE_T len1 = _tcslen(p1);
        SIZE_T payloadChars = len0 + 1 + len1 + 1 + 1;
        SIZE_T total = sizeof(DROPFILES) + payloadChars * sizeof(TCHAR);

        HGLOBAL h = ::GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, total);
        if (!h)
        {
            return nullptr;
        }
        DROPFILES* df = (DROPFILES*)::GlobalLock(h);
        df->pFiles = sizeof(DROPFILES);
#ifdef _UNICODE
        df->fWide = TRUE;
#else
        df->fWide = FALSE;
#endif
        TCHAR* paths = (TCHAR*)((BYTE*)df + sizeof(DROPFILES));
        _tcscpy_s(paths, len0 + 1, p0);
        _tcscpy_s(paths + len0 + 1, len1 + 1, p1);
        paths[len0 + 1 + len1 + 1] = 0;
        ::GlobalUnlock(h);
        return h;
    }

    LONG m_ref;
    bool m_offerFiles;
};

// 回调探针：记下各回调被调了几次、拿到了什么。用静态函数而不是 lambda，与仓库
// "尽量不使用 lambda" 的约定一致。
struct DragProbe
{
    int   enterCount;       // DragEnter 回调次数
    int   overCount;        // DragOver 回调次数
    int   leaveCount;       // DragLeave 回调次数（含 Drop 之后补的那次）
    int   filesCount;       // 旧的无坐标文件回调次数
    int   filesAtPtCount;   // 新的带坐标文件回调次数
    bool  lastHasFiles;     // 最近一次 DragEnter 拿到的 hasFiles
    POINT lastEnterPt;      // 最近一次 DragEnter 拿到的坐标
    POINT lastOverPt;       // 最近一次 DragOver 拿到的坐标
    POINT lastDropPt;       // 最近一次带坐标文件回调拿到的落点
    int   lastFileCount;    // 最近一次文件回调拿到几个路径
    bool  enterReturn;      // DragEnter 回调的返回值（测试用例事先设定）
    bool  overReturn;       // DragOver 回调的返回值（同上；false = 当前位置不能放）
};

static DragProbe g_probe;

static void ResetProbe(bool enterReturn)
{
    ZeroMemory(&g_probe, sizeof(g_probe));
    g_probe.enterReturn = enterReturn;
    g_probe.overReturn  = true;   // 默认"当前位置能放"，只有专门测改判的用例才置 false
}

static bool OnEnterProbe(const POINT& pt, bool hasFiles)
{
    ++g_probe.enterCount;
    g_probe.lastEnterPt  = pt;
    g_probe.lastHasFiles = hasFiles;
    return g_probe.enterReturn;
}

static bool OnOverProbe(const POINT& pt)
{
    ++g_probe.overCount;
    g_probe.lastOverPt = pt;
    return g_probe.overReturn;
}

static void OnLeaveProbe()
{
    ++g_probe.leaveCount;
}

static void OnFilesProbe(const std::vector<CString>& files)
{
    ++g_probe.filesCount;
    g_probe.lastFileCount = (int)files.size();
}

static void OnFilesAtPointProbe(const std::vector<CString>& files, const POINT& pt)
{
    ++g_probe.filesAtPtCount;
    g_probe.lastFileCount = (int)files.size();
    g_probe.lastDropPt    = pt;
}

// 造一个屏幕坐标点，供各拖放用例复用。
static POINTL MakeScreenPt(LONG x, LONG y)
{
    POINTL pt;
    pt.x = x;
    pt.y = y;
    return pt;
}

// 拖入 → 拖动 → 落手：三个回调都收到，坐标透传正确，hasFiles 为真。
static Result Test_DragCallbacksFullFlow()
{
    ResetProbe(true);

    DuiDropTargetHelper h;
    h.SetDragCallbacks(&OnEnterProbe, &OnOverProbe, &OnLeaveProbe);
    h.SetFilesAtPointCallback(&OnFilesAtPointProbe);

    IDropTarget* dt = h.GetDropTarget();
    EXPECT_TRUE(dt != nullptr, _T("Flow/dt"));

    FakeFilesDataObject* pDO = new FakeFilesDataObject(true);
    DWORD effect = DROPEFFECT_NONE;

    dt->DragEnter(pDO, MK_LBUTTON, MakeScreenPt(11, 22), &effect);
    EXPECT_INT(g_probe.enterCount, 1, _T("Flow/enterCount"));
    EXPECT_TRUE(g_probe.lastHasFiles, _T("Flow/hasFiles"));
    EXPECT_INT((int)g_probe.lastEnterPt.x, 11, _T("Flow/enterX"));
    EXPECT_INT((int)g_probe.lastEnterPt.y, 22, _T("Flow/enterY"));
    EXPECT_INT((int)effect, (int)DROPEFFECT_COPY, _T("Flow/enterEffect"));

    dt->DragOver(MK_LBUTTON, MakeScreenPt(33, 44), &effect);
    EXPECT_INT(g_probe.overCount, 1, _T("Flow/overCount"));
    EXPECT_INT((int)g_probe.lastOverPt.x, 33, _T("Flow/overX"));
    EXPECT_INT((int)g_probe.lastOverPt.y, 44, _T("Flow/overY"));
    EXPECT_INT((int)effect, (int)DROPEFFECT_COPY, _T("Flow/overEffect"));

    dt->Drop(pDO, MK_LBUTTON, MakeScreenPt(55, 66), &effect);
    EXPECT_INT(g_probe.filesAtPtCount, 1, _T("Flow/dropCount"));
    EXPECT_INT(g_probe.lastFileCount, 2, _T("Flow/dropFiles"));
    EXPECT_INT((int)g_probe.lastDropPt.x, 55, _T("Flow/dropX"));
    EXPECT_INT((int)g_probe.lastDropPt.y, 66, _T("Flow/dropY"));
    EXPECT_INT((int)effect, (int)DROPEFFECT_COPY, _T("Flow/dropEffect"));
    // 落手之后 OLE 不再调 DragLeave，本类补一次，调用方才能在一处收起提示。
    EXPECT_INT(g_probe.leaveCount, 1, _T("Flow/leaveAfterDrop"));
    // 装了带坐标的回调之后，旧的无坐标回调不应再被调（这里根本没装）。
    EXPECT_INT(g_probe.filesCount, 0, _T("Flow/noLegacyCb"));

    pDO->Release();
    return OK(_T("DragCallbacksFullFlow"));
}

// DragEnter 回调返回 false：光标显示禁止符，落手时什么都不做，但提示仍要收起来。
static Result Test_DragEnterReject()
{
    ResetProbe(false);

    DuiDropTargetHelper h;
    h.SetDragCallbacks(&OnEnterProbe, &OnOverProbe, &OnLeaveProbe);
    h.SetFilesAtPointCallback(&OnFilesAtPointProbe);

    IDropTarget* dt = h.GetDropTarget();
    FakeFilesDataObject* pDO = new FakeFilesDataObject(true);
    DWORD effect = DROPEFFECT_COPY;

    dt->DragEnter(pDO, MK_LBUTTON, MakeScreenPt(1, 2), &effect);
    EXPECT_INT((int)effect, (int)DROPEFFECT_NONE, _T("Reject/enterEffect"));

    dt->DragOver(MK_LBUTTON, MakeScreenPt(3, 4), &effect);
    EXPECT_INT((int)effect, (int)DROPEFFECT_NONE, _T("Reject/overEffect"));
    // 拒收之后不再回调 DragOver，免得调用方给一次收不下的拖动更新提示。
    EXPECT_INT(g_probe.overCount, 0, _T("Reject/noOver"));

    dt->Drop(pDO, MK_LBUTTON, MakeScreenPt(5, 6), &effect);
    EXPECT_INT(g_probe.filesAtPtCount, 0, _T("Reject/noDrop"));
    EXPECT_INT((int)effect, (int)DROPEFFECT_NONE, _T("Reject/dropEffect"));
    EXPECT_INT(g_probe.leaveCount, 1, _T("Reject/leaveAfterDrop"));

    pDO->Release();
    return OK(_T("DragEnterReject"));
}

// 拖离（没有落手）：canDrop 复位，onLeave 回调一次。
static Result Test_DragLeaveNotifies()
{
    ResetProbe(true);

    DuiDropTargetHelper h;
    h.SetDragCallbacks(&OnEnterProbe, &OnOverProbe, &OnLeaveProbe);

    IDropTarget* dt = h.GetDropTarget();
    FakeFilesDataObject* pDO = new FakeFilesDataObject(true);
    DWORD effect = DROPEFFECT_NONE;

    dt->DragEnter(pDO, MK_LBUTTON, MakeScreenPt(7, 8), &effect);
    dt->DragLeave();
    EXPECT_INT(g_probe.leaveCount, 1, _T("Leave/count"));

    // 拖离之后再来一次 DragOver（正常流程里不会发生，这里验证状态确实复位了）：
    // 应按拒收处理，不回调、不给 COPY。
    effect = DROPEFFECT_COPY;
    dt->DragOver(MK_LBUTTON, MakeScreenPt(9, 10), &effect);
    EXPECT_INT((int)effect, (int)DROPEFFECT_NONE, _T("Leave/overAfter"));
    EXPECT_INT(g_probe.overCount, 0, _T("Leave/noOverAfter"));

    pDO->Release();
    return OK(_T("DragLeaveNotifies"));
}

// 只装旧的无坐标文件回调（老调用方的写法）：行为不变，照样收到文件。
static Result Test_LegacyFilesCallbackStillWorks()
{
    ResetProbe(true);

    DuiDropTargetHelper h;
    h.SetCallbacks(&OnFilesProbe, nullptr);

    IDropTarget* dt = h.GetDropTarget();
    FakeFilesDataObject* pDO = new FakeFilesDataObject(true);
    DWORD effect = DROPEFFECT_NONE;

    dt->DragEnter(pDO, MK_LBUTTON, MakeScreenPt(0, 0), &effect);
    dt->Drop(pDO, MK_LBUTTON, MakeScreenPt(0, 0), &effect);
    EXPECT_INT(g_probe.filesCount, 1, _T("Legacy/count"));
    EXPECT_INT(g_probe.lastFileCount, 2, _T("Legacy/files"));
    EXPECT_INT(g_probe.filesAtPtCount, 0, _T("Legacy/noAtPt"));
    // 没装拖动过程回调时不该崩，也不该有任何 leave 计数。
    EXPECT_INT(g_probe.leaveCount, 0, _T("Legacy/noLeaveCb"));

    pDO->Release();
    return OK(_T("LegacyFilesCallbackStillWorks"));
}

// 两个文件回调都装：只走带坐标的那个，避免同一次拖放被处理两遍。
static Result Test_FilesAtPointWinsOverLegacy()
{
    ResetProbe(true);

    DuiDropTargetHelper h;
    h.SetCallbacks(&OnFilesProbe, nullptr);
    h.SetFilesAtPointCallback(&OnFilesAtPointProbe);

    IDropTarget* dt = h.GetDropTarget();
    FakeFilesDataObject* pDO = new FakeFilesDataObject(true);
    DWORD effect = DROPEFFECT_NONE;

    dt->DragEnter(pDO, MK_LBUTTON, MakeScreenPt(0, 0), &effect);
    dt->Drop(pDO, MK_LBUTTON, MakeScreenPt(12, 34), &effect);
    EXPECT_INT(g_probe.filesAtPtCount, 1, _T("Both/atPt"));
    EXPECT_INT(g_probe.filesCount, 0, _T("Both/legacyNotCalled"));
    EXPECT_INT((int)g_probe.lastDropPt.x, 12, _T("Both/x"));
    EXPECT_INT((int)g_probe.lastDropPt.y, 34, _T("Both/y"));

    pDO->Release();
    return OK(_T("FilesAtPointWinsOverLegacy"));
}

// 数据格式不支持（既没有 CF_HDROP 也没有 CF_BITMAP）：不回调拖入，effect 为 NONE。
static Result Test_UnsupportedFormatRejected()
{
    ResetProbe(true);

    DuiDropTargetHelper h;
    h.SetDragCallbacks(&OnEnterProbe, &OnOverProbe, &OnLeaveProbe);

    IDropTarget* dt = h.GetDropTarget();
    FakeFilesDataObject* pDO = new FakeFilesDataObject(false);
    DWORD effect = DROPEFFECT_COPY;

    dt->DragEnter(pDO, MK_LBUTTON, MakeScreenPt(0, 0), &effect);
    EXPECT_INT((int)effect, (int)DROPEFFECT_NONE, _T("Unsup/effect"));
    // 格式就不支持，连问都不必问调用方。
    EXPECT_INT(g_probe.enterCount, 0, _T("Unsup/noEnterCb"));

    pDO->Release();
    return OK(_T("UnsupportedFormatRejected"));
}

// ----- 屏幕坐标 → 客户区坐标 ------------------------------------------

// 窗口为空时原样返回屏幕坐标（换算不了也不能返回 (0,0)，那会被误当成左上角）。
static Result Test_ClientPointFromScreenNullHwnd()
{
    POINT pt = DuiDropTargetHelper::ClientPointFromScreen(nullptr, MakeScreenPt(640, 480));
    EXPECT_INT((int)pt.x, 640, _T("Cvt/nullX"));
    EXPECT_INT((int)pt.y, 480, _T("Cvt/nullY"));
    return OK(_T("ClientPointFromScreenNullHwnd"));
}

// 有真实窗口时按客户区原点换算。
static Result Test_ClientPointFromScreenRealHwnd()
{
    HINSTANCE hInst = ::GetModuleHandle(nullptr);
    HWND hwnd = ::CreateWindowEx(0, _T("STATIC"), _T(""),
                                 WS_OVERLAPPED | WS_POPUP,
                                 100, 50, 200, 200, nullptr, nullptr, hInst, nullptr);
    if (!hwnd)
    {
        return Fail(_T("Cvt/createHwnd"), _T("CreateWindowEx failed"));
    }

    // 先问一次客户区原点在屏幕上的位置，再据它造一个"客户区 (37, 23)"的屏幕坐标。
    POINT origin;
    origin.x = 0;
    origin.y = 0;
    ::ClientToScreen(hwnd, &origin);

    const LONG kOffsetX = 37;   // 期望换算出来的客户区 x
    const LONG kOffsetY = 23;   // 期望换算出来的客户区 y
    POINT pt = DuiDropTargetHelper::ClientPointFromScreen(
        hwnd, MakeScreenPt(origin.x + kOffsetX, origin.y + kOffsetY));

    bool okX = (pt.x == kOffsetX);
    bool okY = (pt.y == kOffsetY);
    ::DestroyWindow(hwnd);

    EXPECT_TRUE(okX, _T("Cvt/realX"));
    EXPECT_TRUE(okY, _T("Cvt/realY"));
    return OK(_T("ClientPointFromScreenRealHwnd"));
}

// DragOver 回调返回 false：光标当前位置不能放（窗口内只有一块区域收拖放时靠它逐帧
// 改判 —— 光标在同一个窗口里移动是不会再触发 DragEnter 的）。
static Result Test_DragOverRejectsThisSpot()
{
    ResetProbe(true);

    DuiDropTargetHelper h;
    h.SetDragCallbacks(&OnEnterProbe, &OnOverProbe, &OnLeaveProbe);

    IDropTarget* dt = h.GetDropTarget();
    FakeFilesDataObject* pDO = new FakeFilesDataObject(true);
    DWORD effect = DROPEFFECT_NONE;

    // 拖入时是"能收"的
    dt->DragEnter(pDO, MK_LBUTTON, MakeScreenPt(1, 2), &effect);
    EXPECT_INT((int)effect, (int)DROPEFFECT_COPY, _T("OverRej/enter"));

    // 移到不能放的位置：effect 变成 NONE
    g_probe.overReturn = false;
    dt->DragOver(MK_LBUTTON, MakeScreenPt(3, 4), &effect);
    EXPECT_INT((int)effect, (int)DROPEFFECT_NONE, _T("OverRej/no"));
    EXPECT_INT(g_probe.overCount, 1, _T("OverRej/count1"));

    // 再移回能放的位置：effect 恢复成 COPY
    g_probe.overReturn = true;
    dt->DragOver(MK_LBUTTON, MakeScreenPt(5, 6), &effect);
    EXPECT_INT((int)effect, (int)DROPEFFECT_COPY, _T("OverRej/yes"));
    EXPECT_INT(g_probe.overCount, 2, _T("OverRej/count2"));

    pDO->Release();
    return OK(_T("DragOverRejectsThisSpot"));
}

// 未 Register 就装拖动回调：不该崩，也不该被当成已注册。
static Result Test_SetDragCallbacksBeforeRegister()
{
    DuiDropTargetHelper h;
    h.SetDragCallbacks(&OnEnterProbe, &OnOverProbe, &OnLeaveProbe);
    h.SetFilesAtPointCallback(&OnFilesAtPointProbe);
    EXPECT_TRUE(!h.IsRegistered(), _T("PreReg/notReg"));
    EXPECT_TRUE(h.GetDropTarget() != nullptr, _T("PreReg/dt"));
    return OK(_T("SetDragCallbacksBeforeRegister"));
}

// ----- 位图拖入（CF_BITMAP）-------------------------------------------------
//
// 位图回调拿到的句柄只在回调执行期间有效：回调返回后，库会释放系统交来的这块数据。
// 要保留就得在回调里自己复制一份（约定见 DuiDropTarget.h 的 BitmapCallback）。
// 库内曾用 CopyImage 加 LR_COPYRETURNORG 复制后交给回调，副本不会被释放，每次拖放
// 泄漏一个 GDI 对象；BitmapDropLeavesNoGdiObjects 即为此而设。

// 测试位图的尺寸（像素），断言里据此确认回调拿到的就是这张图。
static const int kFakeBitmapWidth  = 13;
static const int kFakeBitmapHeight = 7;

// 只提供 CF_BITMAP 的最小 IDataObject。每次 GetData 都新建一张位图，pUnkForRelease 为空，
// 由取数据的一方 ReleaseStgMedium 负责释放 —— 与真实拖放源交来位图的方式相同。
class FakeBitmapDataObject : public IDataObject
{
public:
    FakeBitmapDataObject()
        : m_ref(1)
    {
    }
    virtual ~FakeBitmapDataObject() = default;

    // ---- IUnknown ----
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override
    {
        if (!ppv)
        {
            return E_POINTER;
        }
        if (riid == IID_IUnknown || riid == IID_IDataObject)
        {
            *ppv = static_cast<IDataObject*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override
    {
        return (ULONG)InterlockedIncrement(&m_ref);
    }
    ULONG STDMETHODCALLTYPE Release() override
    {
        LONG r = InterlockedDecrement(&m_ref);
        if (r == 0)
        {
            delete this;
        }
        return (ULONG)r;
    }

    // ---- IDataObject：只实现 QueryGetData / GetData，其余一律不支持 ----
    HRESULT STDMETHODCALLTYPE QueryGetData(FORMATETC* pFmt) override
    {
        if (!pFmt)
        {
            return E_POINTER;
        }
        if (pFmt->cfFormat == CF_BITMAP && (pFmt->tymed & TYMED_GDI))
        {
            return S_OK;
        }
        return DV_E_FORMATETC;
    }
    HRESULT STDMETHODCALLTYPE GetData(FORMATETC* pFmt, STGMEDIUM* pMed) override
    {
        if (!pFmt || !pMed)
        {
            return E_POINTER;
        }
        if (pFmt->cfFormat != CF_BITMAP || !(pFmt->tymed & TYMED_GDI))
        {
            return DV_E_FORMATETC;
        }
        HDC screen = ::GetDC(nullptr);
        HBITMAP hbm = ::CreateCompatibleBitmap(screen, kFakeBitmapWidth, kFakeBitmapHeight);
        ::ReleaseDC(nullptr, screen);
        if (!hbm)
        {
            return E_OUTOFMEMORY;
        }
        ZeroMemory(pMed, sizeof(*pMed));
        pMed->tymed          = TYMED_GDI;
        pMed->hBitmap        = hbm;
        pMed->pUnkForRelease = nullptr;   // 由调用方 ReleaseStgMedium 负责释放
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetDataHere(FORMATETC*, STGMEDIUM*) override
    {
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE GetCanonicalFormatEtc(FORMATETC*, FORMATETC*) override
    {
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE SetData(FORMATETC*, STGMEDIUM*, BOOL) override
    {
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE EnumFormatEtc(DWORD, IEnumFORMATETC**) override
    {
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE DAdvise(FORMATETC*, DWORD, IAdviseSink*, DWORD*) override
    {
        return OLE_E_ADVISENOTSUPPORTED;
    }
    HRESULT STDMETHODCALLTYPE DUnadvise(DWORD) override
    {
        return OLE_E_ADVISENOTSUPPORTED;
    }
    HRESULT STDMETHODCALLTYPE EnumDAdvise(IEnumSTATDATA**) override
    {
        return OLE_E_ADVISENOTSUPPORTED;
    }

private:
    LONG m_ref;   // 引用计数
};

// 位图回调探针：在回调<u>执行期间</u>检查句柄，记下结果。
struct BitmapProbe
{
    int  calls;        // 位图回调被调用的次数
    bool validInCb;    // 回调期间 GetObject 能否取到位图信息
    int  width;        // 回调期间取到的位图宽（像素）
    int  height;       // 回调期间取到的位图高（像素）
};

static BitmapProbe g_bmpProbe;

static void OnBitmapProbe(HBITMAP hbm)
{
    ++g_bmpProbe.calls;
    BITMAP bm;
    ZeroMemory(&bm, sizeof(bm));
    g_bmpProbe.validInCb = (hbm != nullptr) && (::GetObject(hbm, sizeof(bm), &bm) == (int)sizeof(bm));
    g_bmpProbe.width  = bm.bmWidth;
    g_bmpProbe.height = bm.bmHeight;
}

// 把一次完整的位图拖放（拖入、落手）喂给 helper。
static void DropFakeBitmap(DuiDropTargetHelper& h)
{
    IDropTarget* dt = h.GetDropTarget();
    FakeBitmapDataObject* pDO = new FakeBitmapDataObject();
    DWORD effect = DROPEFFECT_NONE;
    dt->DragEnter(pDO, MK_LBUTTON, MakeScreenPt(1, 1), &effect);
    dt->Drop(pDO, MK_LBUTTON, MakeScreenPt(2, 2), &effect);
    pDO->Release();
}

// DTB1：拖入位图时回调恰好被调用一次，回调期间拿到的是一张尺寸正确的有效位图。
static Result Test_BitmapDropCallbackGetsValidBitmap()
{
    ZeroMemory(&g_bmpProbe, sizeof(g_bmpProbe));
    DuiDropTargetHelper h;
    h.SetCallbacks(DuiDropTargetHelper::FilesCallback(), &OnBitmapProbe);
    EXPECT_TRUE(h.GetDropTarget() != nullptr, _T("DTB1/dt"));

    DropFakeBitmap(h);

    EXPECT_INT(g_bmpProbe.calls, 1, _T("DTB1/calls"));
    EXPECT_TRUE(g_bmpProbe.validInCb, _T("DTB1/validInCallback"));
    EXPECT_INT(g_bmpProbe.width, kFakeBitmapWidth, _T("DTB1/width"));
    EXPECT_INT(g_bmpProbe.height, kFakeBitmapHeight, _T("DTB1/height"));
    return OK(_T("BitmapDropCallbackGetsValidBitmap"));
}

// DTB2：位图拖放结束后进程里不多出 GDI 对象 —— 库既不能把系统交来的位图漏掉不释放，
// 也不能自己另外复制一份却没人释放。
static Result Test_BitmapDropLeavesNoGdiObjects()
{
    ZeroMemory(&g_bmpProbe, sizeof(g_bmpProbe));
    DuiDropTargetHelper h;
    h.SetCallbacks(DuiDropTargetHelper::FilesCallback(), &OnBitmapProbe);
    EXPECT_TRUE(h.GetDropTarget() != nullptr, _T("DTB2/dt"));

    const DWORD before = ::GetGuiResources(::GetCurrentProcess(), GR_GDIOBJECTS);
    DropFakeBitmap(h);
    const DWORD after = ::GetGuiResources(::GetCurrentProcess(), GR_GDIOBJECTS);

    EXPECT_INT(g_bmpProbe.calls, 1, _T("DTB2/calls"));
    EXPECT_INT((int)after, (int)before, _T("DTB2/gdiObjects"));
    return OK(_T("BitmapDropLeavesNoGdiObjects"));
}

#undef EXPECT_INT
#undef EXPECT_TRUE
#undef EXPECT_STR

} // anonymous

CString RunAll()
{
    typedef Result (*TestFn)();
    struct Entry
    {
        LPCTSTR name;
        TestFn fn;
    };
    Entry tests[] = {
        { _T("DefaultState"),               &Test_DefaultState               },
        { _T("RegisterUnregister"),         &Test_RegisterUnregister         },
        { _T("RegisterNullFails"),          &Test_RegisterNullFails          },
        { _T("ExtractFromHDrop"),           &Test_ExtractFromHDrop           },
        { _T("ExtractFromNullDropEmpty"),   &Test_ExtractFromNullDropEmpty   },
        { _T("SetCallbacksBeforeRegister"), &Test_SetCallbacksBeforeRegister },
        { _T("DragCallbacksFullFlow"),         &Test_DragCallbacksFullFlow         },
        { _T("DragEnterReject"),               &Test_DragEnterReject               },
        { _T("DragLeaveNotifies"),             &Test_DragLeaveNotifies             },
        { _T("DragOverRejectsThisSpot"),       &Test_DragOverRejectsThisSpot       },
        { _T("LegacyFilesCallbackStillWorks"), &Test_LegacyFilesCallbackStillWorks },
        { _T("FilesAtPointWinsOverLegacy"),    &Test_FilesAtPointWinsOverLegacy    },
        { _T("UnsupportedFormatRejected"),     &Test_UnsupportedFormatRejected     },
        { _T("ClientPointFromScreenNullHwnd"), &Test_ClientPointFromScreenNullHwnd },
        { _T("ClientPointFromScreenRealHwnd"), &Test_ClientPointFromScreenRealHwnd },
        { _T("SetDragCallbacksBeforeRegister"),&Test_SetDragCallbacksBeforeRegister},
        { _T("BitmapDropCallbackGetsValidBitmap"), &Test_BitmapDropCallbackGetsValidBitmap },
        { _T("BitmapDropLeavesNoGdiObjects"),      &Test_BitmapDropLeavesNoGdiObjects      },
    };

    CString out;
    int passed = 0;
    int failed = 0;
    for (auto& e : tests)
    {
        Result r = e.fn();
        CString line;
        if (r.ok)
        {
            ++passed;
            line.Format(_T("[ok]   %s"), e.name);
        }
        else
        {
            ++failed;
            line.Format(_T("[FAIL] %s : %s"), e.name, (LPCTSTR)r.detail);
        }
        if (!out.IsEmpty())
        {
            out += _T("\r\n");
        }
        out += line;
    }
    CString summary;
    summary.Format(_T("[summary] DuiDropTargetTests passed=%d failed=%d"), passed, failed);
    if (!out.IsEmpty())
    {
        out += _T("\r\n");
    }
    out += summary;
    return out;
}

} // namespace DuiDropTargetTests

} // namespace balloonwjui
