#include "stdafx.h"
#include "DuiImageOleTests.h"

#if BUI_FEATURE_IMAGEOLE

#include <ole2.h>

namespace balloonwjui {

namespace DuiImageOleTests {

namespace {

struct Result { CString name; bool ok; CString detail; };
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
#define EXPECT_HR(hr, name) \
    do { HRESULT _h = (hr); \
         if (FAILED(_h)) { CString _d; _d.Format(_T("HRESULT=0x%08X"), (unsigned)_h); return Fail(name, _d); } \
    } while (0)

static HBITMAP MakeFlatBitmap(int w, int h, BYTE r, BYTE g, BYTE b)
{
    BITMAPINFO bi = {};
    bi.bmiHeader.biSize     = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth    = w;
    bi.bmiHeader.biHeight   = -h;
    bi.bmiHeader.biPlanes   = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HBITMAP hbm = ::CreateDIBSection(nullptr, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!hbm)
    {
        return nullptr;
    }
    BYTE* p = (BYTE*)bits;
    for (int i = 0; i < w * h; ++i)
    {
        p[i*4 + 0] = b;
        p[i*4 + 1] = g;
        p[i*4 + 2] = r;
        p[i*4 + 3] = 255;
    }
    return hbm;
}

// ----- ref counting ---------------------------------------------------

static Result Test_AddRefRelease()
{
    HBITMAP hbm = MakeFlatBitmap(8, 8, 200, 100, 50);
    EXPECT_TRUE(hbm != nullptr, _T("Ref/srcDib"));
    CDuiImageOle* p = new CDuiImageOle(hbm, /*ownsHbm=*/true);
    EXPECT_INT(p->GetRefCount(), 1, _T("Ref/init"));
    p->AddRef();
    EXPECT_INT(p->GetRefCount(), 2, _T("Ref/+1"));
    p->Release();
    EXPECT_INT(p->GetRefCount(), 1, _T("Ref/-1"));
    p->Release();   // deletes; we no longer touch p (and hbm is freed by ownsHbm)
    return OK(_T("AddRefRelease"));
}

// ----- QueryInterface lands on every advertised iface -----------------

static Result Test_QueryInterface()
{
    HBITMAP hbm = MakeFlatBitmap(4, 4, 0, 0, 0);
    CDuiImageOle* p = new CDuiImageOle(hbm, true);

    IOleObject*      o1 = nullptr;
    IViewObject2*    o2 = nullptr;
    IDataObject*     o3 = nullptr;
    IPersist*        o4 = nullptr;
    IPersistStorage* o5 = nullptr;
    IUnknown*        o6 = nullptr;
    EXPECT_HR(p->QueryInterface(IID_IOleObject,      (void**)&o1), _T("QI/Ole"));
    EXPECT_HR(p->QueryInterface(IID_IViewObject2,    (void**)&o2), _T("QI/View"));
    EXPECT_HR(p->QueryInterface(IID_IDataObject,     (void**)&o3), _T("QI/Data"));
    EXPECT_HR(p->QueryInterface(IID_IPersist,        (void**)&o4), _T("QI/Persist"));
    EXPECT_HR(p->QueryInterface(IID_IPersistStorage, (void**)&o5), _T("QI/PersistStg"));
    EXPECT_HR(p->QueryInterface(IID_IUnknown,        (void**)&o6), _T("QI/Unk"));
    o1->Release();
    o2->Release();
    o3->Release();
    o4->Release();
    o5->Release();
    o6->Release();

    // Bogus IID returns E_NOINTERFACE.
    void* nothing = nullptr;
    HRESULT hr = p->QueryInterface(IID_IClassFactory, &nothing);
    EXPECT_INT(hr, E_NOINTERFACE, _T("QI/none"));
    EXPECT_TRUE(nothing == nullptr, _T("QI/zeroOut"));
    p->Release();
    return OK(_T("QueryInterface"));
}

// ----- IViewObject2::GetExtent returns himetric size -----------------

static Result Test_GetExtent()
{
    HBITMAP hbm = MakeFlatBitmap(96, 48, 0, 0, 0);    // 1in × 0.5in @ 96dpi
    CDuiImageOle* p = new CDuiImageOle(hbm, true);

    // Both IOleObject and IViewObject2 expose a GetExtent method with
    // different arity. Go through QueryInterface so the call binds to
    // the right vtable slot without name-lookup ambiguity.
    IViewObject2* pView = nullptr;
    EXPECT_HR(p->QueryInterface(IID_IViewObject2, (void**)&pView), _T("Ext/qi"));
    SIZEL sz = {};
    EXPECT_HR(pView->GetExtent(DVASPECT_CONTENT, -1, nullptr, &sz), _T("Ext/hr"));
    pView->Release();

    // 96px @ 96dpi = 1 inch = 2540 himetric units.
    EXPECT_INT(sz.cx, 2540, _T("Ext/cx"));
    EXPECT_INT(sz.cy, 1270, _T("Ext/cy"));

    // Also exercise the IOleObject::GetExtent variant (2 args).
    SIZEL sz2 = {};
    IOleObject* pOle = nullptr;
    EXPECT_HR(p->QueryInterface(IID_IOleObject, (void**)&pOle), _T("Ext/qiOle"));
    EXPECT_HR(pOle->GetExtent(DVASPECT_CONTENT, &sz2), _T("Ext/oleHr"));
    pOle->Release();
    EXPECT_INT(sz2.cx, 2540, _T("Ext/oleCx"));
    EXPECT_INT(sz2.cy, 1270, _T("Ext/oleCy"));

    p->Release();
    return OK(_T("GetExtent"));
}

// ----- IDataObject::QueryGetData / GetData CF_BITMAP ------------------

static Result Test_DataObjectBitmap()
{
    HBITMAP hbm = MakeFlatBitmap(16, 16, 80, 160, 240);
    CDuiImageOle* p = new CDuiImageOle(hbm, true);

    FORMATETC fmt = {};
    fmt.cfFormat = CF_BITMAP;
    fmt.dwAspect = DVASPECT_CONTENT;
    fmt.lindex   = -1;
    fmt.tymed    = TYMED_GDI;
    EXPECT_HR(p->QueryGetData(&fmt), _T("Data/qOk"));

    fmt.cfFormat = CF_TEXT;
    EXPECT_INT(p->QueryGetData(&fmt), S_FALSE, _T("Data/qNotText"));

    fmt.cfFormat = CF_BITMAP;
    STGMEDIUM med = {};
    EXPECT_HR(p->GetData(&fmt, &med), _T("Data/get"));
    EXPECT_INT((int)med.tymed, (int)TYMED_GDI, _T("Data/tymed"));
    EXPECT_TRUE(med.hBitmap != nullptr, _T("Data/hbm"));
    EXPECT_TRUE(med.hBitmap != hbm, _T("Data/copy"));     // CopyImage returns a new HBITMAP
    if (med.hBitmap)
    {
        ::DeleteObject(med.hBitmap);
    }
    p->Release();
    return OK(_T("DataObjectBitmap"));
}

// ----- Draw smoke onto a memory DC ------------------------------------

static Result Test_DrawSmoke()
{
    HBITMAP hbm = MakeFlatBitmap(16, 16, 200, 50, 50);
    CDuiImageOle* p = new CDuiImageOle(hbm, true);

    BITMAPINFO bi = {};
    bi.bmiHeader.biSize     = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth    = 32;
    bi.bmiHeader.biHeight   = -32;
    bi.bmiHeader.biPlanes   = 1;
    bi.bmiHeader.biBitCount = 32;
    void* bits = nullptr;
    HBITMAP dst = ::CreateDIBSection(nullptr, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    EXPECT_TRUE(dst != nullptr, _T("Draw/dst"));
    HDC hdc = ::CreateCompatibleDC(nullptr);
    HGDIOBJ old = ::SelectObject(hdc, dst);

    RECTL rc = { 0, 0, 32, 32 };
    HRESULT hr = p->Draw(DVASPECT_CONTENT, -1, nullptr, nullptr,
                         hdc, hdc, &rc, nullptr, nullptr, 0);
    EXPECT_HR(hr, _T("Draw/hr"));

    ::SelectObject(hdc, old);
    ::DeleteDC(hdc);
    ::DeleteObject(dst);
    p->Release();
    return OK(_T("DrawSmoke"));
}

// ----- SetClientSite / GetClientSite ref counting --------------------

class FakeSite : public IOleClientSite
{
public:
    LONG ref = 1;
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override
    {
        if (riid == IID_IUnknown || riid == IID_IOleClientSite)
        {
            *ppv = this;
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef () override { return (ULONG)InterlockedIncrement(&ref); }
    ULONG STDMETHODCALLTYPE Release() override { return (ULONG)InterlockedDecrement(&ref); }
    HRESULT STDMETHODCALLTYPE SaveObject() override                                      { return S_OK; }
    HRESULT STDMETHODCALLTYPE GetMoniker(DWORD, DWORD, IMoniker**) override               { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetContainer(IOleContainer**) override                      { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE ShowObject() override                                       { return S_OK; }
    HRESULT STDMETHODCALLTYPE OnShowWindow(BOOL) override                                 { return S_OK; }
    HRESULT STDMETHODCALLTYPE RequestNewObjectLayout() override                           { return E_NOTIMPL; }
};

static Result Test_ClientSiteRef()
{
    HBITMAP hbm = MakeFlatBitmap(2, 2, 0, 0, 0);
    CDuiImageOle* p = new CDuiImageOle(hbm, true);
    FakeSite site;     // ref = 1 (initial)
    p->SetClientSite(&site);
    EXPECT_INT((int)site.ref, 2, _T("Site/+1"));
    p->Release();      // drops site ref
    EXPECT_INT((int)site.ref, 1, _T("Site/-1onClose"));
    return OK(_T("ClientSiteRef"));
}

#undef EXPECT_INT
#undef EXPECT_TRUE
#undef EXPECT_HR

} // anonymous

CString RunAll()
{
    typedef Result (*TestFn)();
    struct Entry { LPCTSTR name; TestFn fn; };
    Entry tests[] = {
        { _T("AddRefRelease"),   &Test_AddRefRelease   },
        { _T("QueryInterface"),  &Test_QueryInterface  },
        { _T("GetExtent"),       &Test_GetExtent       },
        { _T("DataObjectBitmap"),&Test_DataObjectBitmap},
        { _T("DrawSmoke"),       &Test_DrawSmoke       },
        { _T("ClientSiteRef"),   &Test_ClientSiteRef   },
    };

    CString out;
    int passed = 0, failed = 0;
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
    summary.Format(_T("[summary] DuiImageOleTests passed=%d failed=%d"), passed, failed);
    if (!out.IsEmpty())
    {
        out += _T("\r\n");
    }
    out += summary;
    return out;
}

} // namespace DuiImageOleTests

} // namespace balloonwjui

#endif // BUI_FEATURE_IMAGEOLE
