#include "stdafx.h"
#include "DuiImageOle.h"

#if BUI_FEATURE_IMAGEOLE


namespace balloonwjui {

// =====================================================================
// CDuiImageOle: minimal OLE wrapper around an HBITMAP for embedding into
// a RichEdit. Only the methods RichEdit actually uses do work — the rest
// return E_NOTIMPL or S_OK to satisfy the interfaces. This is enough for
// in-memory render of the image; on-disk RTF persistence is out of scope.
// =====================================================================

CDuiImageOle::CDuiImageOle(HBITMAP hbm, bool ownsHbm)
    : m_ref(1)
    , m_hbm(hbm)
    , m_ownsHbm(ownsHbm)
    , m_pxSize{ 0, 0 }
    , m_himetric{ 0, 0 }
    , m_pSite(nullptr)
{
    ComputeSizes();
}

CDuiImageOle::~CDuiImageOle()
{
    if (m_pSite)
    {
        m_pSite->Release();
        m_pSite = nullptr;
    }
    if (m_ownsHbm && m_hbm)
    {
        ::DeleteObject(m_hbm);
        m_hbm = nullptr;
    }
}

void CDuiImageOle::ComputeSizes()
{
    BITMAP bm = {};
    if (m_hbm && ::GetObject(m_hbm, sizeof(bm), &bm) == sizeof(bm))
    {
        m_pxSize.cx = bm.bmWidth;
        m_pxSize.cy = bm.bmHeight;
    }
    else
    {
        m_pxSize.cx = m_pxSize.cy = 0;
    }
    // Convert px -> himetric (0.01mm). 1 inch = 96 px (DPI-agnostic
    // baseline), 1 inch = 2540 himetric units. RichEdit uses these for
    // layout — we use the OLE convention even though our actual paint
    // path is StretchBlt.
    m_himetric.cx = ::MulDiv(m_pxSize.cx, 2540, 96);
    m_himetric.cy = ::MulDiv(m_pxSize.cy, 2540, 96);
}

// ----- IUnknown -------------------------------------------------------

HRESULT STDMETHODCALLTYPE CDuiImageOle::QueryInterface(REFIID riid, void** ppv)
{
    if (!ppv)
    {
        return E_POINTER;
    }
    *ppv = nullptr;
    if (riid == IID_IUnknown)
    {
        *ppv = static_cast<IOleObject*>(this);
    }
    else if (riid == IID_IOleObject)
    {
        *ppv = static_cast<IOleObject*>(this);
    }
    else if (riid == IID_IViewObject)
    {
        *ppv = static_cast<IViewObject2*>(this);
    }
    else if (riid == IID_IViewObject2)
    {
        *ppv = static_cast<IViewObject2*>(this);
    }
    else if (riid == IID_IDataObject)
    {
        *ppv = static_cast<IDataObject*>(this);
    }
    else if (riid == IID_IPersist)
    {
        *ppv = static_cast<IPersistStorage*>(this);
    }
    else if (riid == IID_IPersistStorage)
    {
        *ppv = static_cast<IPersistStorage*>(this);
    }
    if (!*ppv)
    {
        return E_NOINTERFACE;
    }
    AddRef();
    return S_OK;
}

ULONG STDMETHODCALLTYPE CDuiImageOle::AddRef()
{
    return (ULONG)InterlockedIncrement(&m_ref);
}

ULONG STDMETHODCALLTYPE CDuiImageOle::Release()
{
    LONG r = InterlockedDecrement(&m_ref);
    if (r == 0)
    {
        delete this;
    }
    return (ULONG)r;
}

// ----- IOleObject (mostly stubs) --------------------------------------

HRESULT STDMETHODCALLTYPE CDuiImageOle::SetClientSite(IOleClientSite* pSite)
{
    if (m_pSite)
    {
        m_pSite->Release();
    }
    m_pSite = pSite;
    if (m_pSite)
    {
        m_pSite->AddRef();
    }
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CDuiImageOle::GetClientSite(IOleClientSite** ppSite)
{
    if (!ppSite)
    {
        return E_POINTER;
    }
    *ppSite = m_pSite;
    if (m_pSite)
    {
        m_pSite->AddRef();
    }
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CDuiImageOle::Close(DWORD)
{
    if (m_pSite)
    {
        m_pSite->Release();
        m_pSite = nullptr;
    }
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CDuiImageOle::SetHostNames(LPCOLESTR, LPCOLESTR)            { return S_OK; }
HRESULT STDMETHODCALLTYPE CDuiImageOle::SetMoniker(DWORD, IMoniker*)                  { return E_NOTIMPL; }
HRESULT STDMETHODCALLTYPE CDuiImageOle::GetMoniker(DWORD, DWORD, IMoniker**)          { return E_NOTIMPL; }
HRESULT STDMETHODCALLTYPE CDuiImageOle::InitFromData(IDataObject*, BOOL, DWORD)       { return E_NOTIMPL; }
HRESULT STDMETHODCALLTYPE CDuiImageOle::GetClipboardData(DWORD, IDataObject**)        { return E_NOTIMPL; }
HRESULT STDMETHODCALLTYPE CDuiImageOle::DoVerb(LONG, LPMSG, IOleClientSite*, LONG,
                                               HWND, LPCRECT)                          { return S_OK; }
HRESULT STDMETHODCALLTYPE CDuiImageOle::EnumVerbs(IEnumOLEVERB**)                     { return E_NOTIMPL; }
HRESULT STDMETHODCALLTYPE CDuiImageOle::Update()                                      { return S_OK; }
HRESULT STDMETHODCALLTYPE CDuiImageOle::IsUpToDate()                                  { return S_OK; }

HRESULT STDMETHODCALLTYPE CDuiImageOle::GetUserClassID(CLSID* pClsid)
{
    if (pClsid)
    {
        *pClsid = CLSID_NULL;
    }
    return S_OK;
}
HRESULT STDMETHODCALLTYPE CDuiImageOle::GetUserType(DWORD, LPOLESTR*)                 { return E_NOTIMPL; }

HRESULT STDMETHODCALLTYPE CDuiImageOle::SetExtent(DWORD, SIZEL* pSize)
{
    if (pSize)
    {
        m_himetric = *pSize;
    }
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CDuiImageOle::GetExtent(DWORD, SIZEL* pSize)
{
    if (!pSize)
    {
        return E_POINTER;
    }
    *pSize = m_himetric;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CDuiImageOle::Advise(IAdviseSink*, DWORD*)                  { return S_OK; }
HRESULT STDMETHODCALLTYPE CDuiImageOle::Unadvise(DWORD)                               { return S_OK; }
HRESULT STDMETHODCALLTYPE CDuiImageOle::EnumAdvise(IEnumSTATDATA**)                   { return E_NOTIMPL; }

HRESULT STDMETHODCALLTYPE CDuiImageOle::GetMiscStatus(DWORD, DWORD* pdw)
{
    if (pdw)
    {
        *pdw = OLEMISC_RECOMPOSEONRESIZE | OLEMISC_INSIDEOUT;
    }
    return S_OK;
}
HRESULT STDMETHODCALLTYPE CDuiImageOle::SetColorScheme(LOGPALETTE*)                   { return E_NOTIMPL; }

// ----- IViewObject2 ---------------------------------------------------

HRESULT STDMETHODCALLTYPE CDuiImageOle::Draw(
    DWORD /*dwAspect*/, LONG /*lindex*/, void* /*pvAspect*/,
    DVTARGETDEVICE* /*ptd*/,
    HDC /*hdcTargetDev*/, HDC hdcDraw,
    LPCRECTL lprcBounds, LPCRECTL /*lprcWBounds*/,
    BOOL (STDMETHODCALLTYPE* /*pfnContinue*/)(ULONG_PTR), ULONG_PTR /*dwContinue*/)
{
    if (!hdcDraw || !lprcBounds || !m_hbm)
    {
        return E_INVALIDARG;
    }
    HDC mem = ::CreateCompatibleDC(hdcDraw);
    if (!mem)
    {
        return E_FAIL;
    }
    HGDIOBJ old = ::SelectObject(mem, m_hbm);

    int x = lprcBounds->left;
    int y = lprcBounds->top;
    int w = lprcBounds->right  - lprcBounds->left;
    int h = lprcBounds->bottom - lprcBounds->top;
    int sw = m_pxSize.cx;
    int sh = m_pxSize.cy;
    if (sw > 0 && sh > 0 && w > 0 && h > 0)
    {
        ::SetStretchBltMode(hdcDraw, HALFTONE);
        ::SetBrushOrgEx(hdcDraw, 0, 0, nullptr);
        ::StretchBlt(hdcDraw, x, y, w, h, mem, 0, 0, sw, sh, SRCCOPY);
    }
    ::SelectObject(mem, old);
    ::DeleteDC(mem);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CDuiImageOle::GetColorSet(
    DWORD, LONG, void*, DVTARGETDEVICE*, HDC, LOGPALETTE**)
{
    return E_NOTIMPL;
}
HRESULT STDMETHODCALLTYPE CDuiImageOle::Freeze(DWORD, LONG, void*, DWORD*)            { return E_NOTIMPL; }
HRESULT STDMETHODCALLTYPE CDuiImageOle::Unfreeze(DWORD)                               { return S_OK; }
HRESULT STDMETHODCALLTYPE CDuiImageOle::SetAdvise(DWORD, DWORD, IAdviseSink*)         { return S_OK; }
HRESULT STDMETHODCALLTYPE CDuiImageOle::GetAdvise(DWORD* pa, DWORD* pf, IAdviseSink** ps)
{
    if (pa)
    {
        *pa = 0;
    }
    if (pf)
    {
        *pf = 0;
    }
    if (ps)
    {
        *ps = nullptr;
    }
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CDuiImageOle::GetExtent(DWORD, LONG, DVTARGETDEVICE*, LPSIZEL pSize)
{
    if (!pSize)
    {
        return E_POINTER;
    }
    *pSize = m_himetric;
    return S_OK;
}

// ----- IDataObject ----------------------------------------------------

HRESULT STDMETHODCALLTYPE CDuiImageOle::GetData(FORMATETC* pFmt, STGMEDIUM* pMed)
{
    if (!pFmt || !pMed)
    {
        return E_POINTER;
    }
    if (pFmt->cfFormat != CF_BITMAP)
    {
        return DV_E_FORMATETC;
    }
    if (!(pFmt->tymed & TYMED_GDI))
    {
        return DV_E_TYMED;
    }
    if (!m_hbm)
    {
        return E_FAIL;
    }

    pMed->tymed = TYMED_GDI;
    pMed->hBitmap = (HBITMAP)::CopyImage(m_hbm, IMAGE_BITMAP, 0, 0, LR_COPYRETURNORG);
    pMed->pUnkForRelease = nullptr;
    return pMed->hBitmap ? S_OK : E_FAIL;
}
HRESULT STDMETHODCALLTYPE CDuiImageOle::GetDataHere(FORMATETC*, STGMEDIUM*)           { return E_NOTIMPL; }

HRESULT STDMETHODCALLTYPE CDuiImageOle::QueryGetData(FORMATETC* pFmt)
{
    if (!pFmt)
    {
        return E_POINTER;
    }
    if (pFmt->cfFormat == CF_BITMAP && (pFmt->tymed & TYMED_GDI))
    {
        return S_OK;
    }
    return S_FALSE;
}

HRESULT STDMETHODCALLTYPE CDuiImageOle::GetCanonicalFormatEtc(FORMATETC*, FORMATETC* pOut)
{
    if (pOut)
    {
        ZeroMemory(pOut, sizeof(*pOut));
        pOut->cfFormat = CF_BITMAP;
        pOut->dwAspect = DVASPECT_CONTENT;
        pOut->lindex   = -1;
        pOut->tymed    = TYMED_GDI;
    }
    return DATA_S_SAMEFORMATETC;
}
HRESULT STDMETHODCALLTYPE CDuiImageOle::SetData(FORMATETC*, STGMEDIUM*, BOOL)         { return E_NOTIMPL; }
HRESULT STDMETHODCALLTYPE CDuiImageOle::EnumFormatEtc(DWORD, IEnumFORMATETC**)        { return E_NOTIMPL; }
HRESULT STDMETHODCALLTYPE CDuiImageOle::DAdvise(FORMATETC*, DWORD, IAdviseSink*, DWORD*)
{
    return OLE_E_ADVISENOTSUPPORTED;
}
HRESULT STDMETHODCALLTYPE CDuiImageOle::DUnadvise(DWORD)                              { return OLE_E_ADVISENOTSUPPORTED; }
HRESULT STDMETHODCALLTYPE CDuiImageOle::EnumDAdvise(IEnumSTATDATA**)                  { return OLE_E_ADVISENOTSUPPORTED; }

// ----- IPersist / IPersistStorage (no-op) -----------------------------

HRESULT STDMETHODCALLTYPE CDuiImageOle::GetClassID(CLSID* pClsid)
{
    if (pClsid)
    {
        *pClsid = CLSID_NULL;
    }
    return S_OK;
}
HRESULT STDMETHODCALLTYPE CDuiImageOle::IsDirty()                                     { return S_FALSE; }
HRESULT STDMETHODCALLTYPE CDuiImageOle::InitNew(IStorage*)                            { return S_OK; }
HRESULT STDMETHODCALLTYPE CDuiImageOle::Load(IStorage*)                               { return S_OK; }
HRESULT STDMETHODCALLTYPE CDuiImageOle::Save(IStorage*, BOOL)                         { return S_OK; }
HRESULT STDMETHODCALLTYPE CDuiImageOle::SaveCompleted(IStorage*)                      { return S_OK; }
HRESULT STDMETHODCALLTYPE CDuiImageOle::HandsOffStorage()                             { return S_OK; }

// =====================================================================
// InsertIntoRichEdit: full plumbing path. Returns true on success.
// =====================================================================

bool CDuiImageOle::InsertIntoRichEdit(HWND hRichEdit, HBITMAP hbm, bool ownsHbm)
{
    if (!hRichEdit || !hbm)
    {
        if (ownsHbm)
        {
            ::DeleteObject(hbm);
        }
        return false;
    }

    IRichEditOle* pRiche = nullptr;
    ::SendMessage(hRichEdit, EM_GETOLEINTERFACE, 0, (LPARAM)&pRiche);
    if (!pRiche)
    {
        if (ownsHbm)
        {
            ::DeleteObject(hbm);
        }
        return false;
    }

    IOleClientSite* pSite = nullptr;
    pRiche->GetClientSite(&pSite);

    IStorage* pStg = nullptr;
    LPLOCKBYTES pLock = nullptr;
    if (FAILED(::CreateILockBytesOnHGlobal(NULL, TRUE, &pLock)))
    {
        if (pSite)
        {
            pSite->Release();
        }
        if (pRiche)
        {
            pRiche->Release();
        }
        if (ownsHbm)
        {
            ::DeleteObject(hbm);
        }
        return false;
    }
    if (FAILED(::StgCreateDocfileOnILockBytes(pLock,
            STGM_SHARE_EXCLUSIVE | STGM_CREATE | STGM_READWRITE, 0, &pStg)))
    {
        pLock->Release();
        if (pSite)
        {
            pSite->Release();
        }
        if (pRiche)
        {
            pRiche->Release();
        }
        if (ownsHbm)
        {
            ::DeleteObject(hbm);
        }
        return false;
    }

    CDuiImageOle* obj = new CDuiImageOle(hbm, ownsHbm);   // ref=1
    obj->SetClientSite(pSite);
    ::OleSetContainedObject(static_cast<IOleObject*>(obj), TRUE);

    REOBJECT reo = {};
    reo.cbStruct = sizeof(REOBJECT);
    reo.clsid    = CLSID_NULL;
    reo.cp       = REO_CP_SELECTION;
    reo.dvaspect = DVASPECT_CONTENT;
    reo.dwFlags  = REO_BELOWBASELINE;
    reo.poleobj  = static_cast<IOleObject*>(obj);
    reo.pstg     = pStg;
    reo.polesite = pSite;
    SIZEL ext = obj->GetPixelSize().cx > 0
        ? SIZEL{ ::MulDiv(obj->GetPixelSize().cx, 2540, 96),
                 ::MulDiv(obj->GetPixelSize().cy, 2540, 96) }
        : SIZEL{ 0, 0 };
    reo.sizel    = ext;
    reo.dwUser   = 0;

    HRESULT hr = pRiche->InsertObject(&reo);

    obj->Release();   // RichEdit AddRef'd what it needs
    if (pStg)
    {
        pStg->Release();
    }
    if (pLock)
    {
        pLock->Release();
    }
    if (pSite)
    {
        pSite->Release();
    }
    pRiche->Release();
    return SUCCEEDED(hr);
}

} // namespace balloonwjui

#endif // BUI_FEATURE_IMAGEOLE
