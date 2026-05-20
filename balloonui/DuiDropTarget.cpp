#include "stdafx.h"
#include "DuiDropTarget.h"

namespace balloonwjui {

// =====================================================================
// DuiDropTargetImpl: minimal IDropTarget. Accepts a drop iff the
// IDataObject offers CF_HDROP or CF_BITMAP. On Drop, extracts the
// payload + invokes the helper's callbacks.
// =====================================================================

class DuiDropTargetImpl : public IDropTarget
{
public:
    DuiDropTargetImpl(DuiDropTargetHelper* owner)
        : m_ref(1), m_owner(owner)
    {
    }
    virtual ~DuiDropTargetImpl() = default;

    void   SetCallbacks(const DuiDropTargetHelper::FilesCallback& fcb,
                        const DuiDropTargetHelper::BitmapCallback& bcb)
    {
        m_filesCb = fcb;
        m_bmpCb   = bcb;
    }

    // ---- IUnknown ----
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override
    {
        if (!ppv)
        {
            return E_POINTER;
        }
        if (riid == IID_IUnknown || riid == IID_IDropTarget)
        {
            *ppv = static_cast<IDropTarget*>(this);
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

    // ---- IDropTarget ----
    HRESULT STDMETHODCALLTYPE DragEnter(IDataObject* pDO, DWORD /*grfKeyState*/,
                                       POINTL /*pt*/, DWORD* pEffect) override
    {
        m_canDrop = SupportsFormats(pDO);
        if (pEffect)
        {
            *pEffect = m_canDrop ? DROPEFFECT_COPY : DROPEFFECT_NONE;
        }
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE DragOver(DWORD /*grfKeyState*/, POINTL /*pt*/, DWORD* pEffect) override
    {
        if (pEffect)
        {
            *pEffect = m_canDrop ? DROPEFFECT_COPY : DROPEFFECT_NONE;
        }
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE DragLeave() override
    {
        m_canDrop = false;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE Drop(IDataObject* pDO, DWORD /*grfKeyState*/,
                                  POINTL /*pt*/, DWORD* pEffect) override
    {
        if (pEffect)
        {
            *pEffect = DROPEFFECT_NONE;
        }
        if (!pDO)
        {
            return E_POINTER;
        }

        // Try files first (more common in chat composers).
        FORMATETC fmt = {};
        STGMEDIUM med = {};
        fmt.cfFormat = CF_HDROP;
        fmt.dwAspect = DVASPECT_CONTENT;
        fmt.lindex   = -1;
        fmt.tymed    = TYMED_HGLOBAL;
        if (pDO->GetData(&fmt, &med) == S_OK)
        {
            HDROP hDrop = (HDROP)::GlobalLock(med.hGlobal);
            std::vector<CString> files;
            if (hDrop)
            {
                files = DuiDropTargetHelper::ExtractFilesFromHDrop(hDrop);
            }
            if (med.hGlobal)
            {
                ::GlobalUnlock(med.hGlobal);
            }
            if (m_filesCb && !files.empty())
            {
                m_filesCb(files);
            }
            ::ReleaseStgMedium(&med);
            if (pEffect)
            {
                *pEffect = DROPEFFECT_COPY;
            }
            return S_OK;
        }

        // Fall back to bitmap.
        ZeroMemory(&fmt, sizeof(fmt));
        ZeroMemory(&med, sizeof(med));
        fmt.cfFormat = CF_BITMAP;
        fmt.dwAspect = DVASPECT_CONTENT;
        fmt.lindex   = -1;
        fmt.tymed    = TYMED_GDI;
        if (pDO->GetData(&fmt, &med) == S_OK)
        {
            if (m_bmpCb && med.hBitmap)
            {
                HBITMAP copy = (HBITMAP)::CopyImage(med.hBitmap, IMAGE_BITMAP, 0, 0,
                                                    LR_COPYRETURNORG);
                m_bmpCb(copy ? copy : med.hBitmap);
            }
            ::ReleaseStgMedium(&med);
            if (pEffect)
            {
                *pEffect = DROPEFFECT_COPY;
            }
            return S_OK;
        }
        return S_OK;
    }

private:
    static bool SupportsFormats(IDataObject* pDO)
    {
        if (!pDO)
        {
            return false;
        }
        FORMATETC fmt = {};
        fmt.cfFormat = CF_HDROP;
        fmt.dwAspect = DVASPECT_CONTENT;
        fmt.lindex   = -1;
        fmt.tymed    = TYMED_HGLOBAL;
        if (pDO->QueryGetData(&fmt) == S_OK)
        {
            return true;
        }

        fmt.cfFormat = CF_BITMAP;
        fmt.tymed    = TYMED_GDI;
        if (pDO->QueryGetData(&fmt) == S_OK)
        {
            return true;
        }
        return false;
    }

    LONG  m_ref;
    DuiDropTargetHelper* m_owner;
    bool  m_canDrop = false;
    DuiDropTargetHelper::FilesCallback  m_filesCb;
    DuiDropTargetHelper::BitmapCallback m_bmpCb;
};

// =====================================================================
// DuiDropTargetHelper
// =====================================================================

DuiDropTargetHelper::DuiDropTargetHelper() = default;
DuiDropTargetHelper::~DuiDropTargetHelper()
{
    Unregister();
}

bool DuiDropTargetHelper::Register(HWND hwnd)
{
    if (m_hwnd)
    {
        Unregister();
    }
    if (!hwnd)
    {
        return false;
    }

    if (!m_impl)
    {
        m_impl = new DuiDropTargetImpl(this);
    }
    HRESULT hr = ::RegisterDragDrop(hwnd, m_impl);
    if (FAILED(hr))
    {
        m_impl->Release();
        m_impl = nullptr;
        return false;
    }
    m_hwnd = hwnd;
    return true;
}

void DuiDropTargetHelper::Unregister()
{
    if (m_hwnd)
    {
        ::RevokeDragDrop(m_hwnd);
    }
    m_hwnd = nullptr;
    if (m_impl)
    {
        m_impl->Release();
        m_impl = nullptr;
    }
}

void DuiDropTargetHelper::SetCallbacks(FilesCallback onFiles, BitmapCallback onBitmap)
{
    if (!m_impl)
    {
        m_impl = new DuiDropTargetImpl(this);
    }
    m_impl->SetCallbacks(onFiles, onBitmap);
}

std::vector<CString> DuiDropTargetHelper::ExtractFilesFromHDrop(HDROP hDrop)
{
    std::vector<CString> out;
    if (!hDrop)
    {
        return out;
    }
    UINT n = ::DragQueryFile(hDrop, 0xFFFFFFFF, nullptr, 0);
    out.reserve(n);
    for (UINT i = 0; i < n; ++i)
    {
        UINT len = ::DragQueryFile(hDrop, i, nullptr, 0);
        if (len == 0)
        {
            continue;
        }
        CString path;
        TCHAR* buf = path.GetBufferSetLength((int)len);
        ::DragQueryFile(hDrop, i, buf, len + 1);
        path.ReleaseBuffer();
        out.push_back(path);
    }
    return out;
}

} // namespace balloonwjui
