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

    // 装拖动过程回调（进入 / 移动 / 离开），三个都可为空。
    void   SetDragCallbacks(const DuiDropTargetHelper::DragEnterCallback& ecb,
                            const DuiDropTargetHelper::DragOverCallback& ocb,
                            const DuiDropTargetHelper::DragLeaveCallback& lcb)
    {
        m_dragEnterCb = ecb;
        m_dragOverCb  = ocb;
        m_dragLeaveCb = lcb;
    }

    // 装带落点坐标的文件回调；装了它就不再回调 m_filesCb。
    void   SetFilesAtPointCallback(const DuiDropTargetHelper::FilesAtPointCallback& cb)
    {
        m_filesAtPtCb = cb;
    }

    // 记下宿主窗口，用于把 OLE 给的屏幕坐标换算成客户区坐标。注册成功后由
    // DuiDropTargetHelper::Register 调；未注册时为空，此时坐标原样透传。
    void   SetHostHwnd(HWND hwnd)
    {
        m_hostHwnd = hwnd;
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
                                       POINTL pt, DWORD* pEffect) override
    {
        m_canDrop = SupportsFormats(pDO);

        // 数据格式本身支持时，再问一次调用方要不要收（不装回调即默认要收）。
        // 调用方返回 false 表示本次拒收，后面的 DragOver / Drop 都按拒收处理。
        if (m_canDrop && m_dragEnterCb)
        {
            const POINT ptClient = DuiDropTargetHelper::ClientPointFromScreen(m_hostHwnd, pt);
            if (!m_dragEnterCb(ptClient, HasFiles(pDO)))
            {
                m_canDrop = false;
            }
        }

        if (pEffect)
        {
            *pEffect = m_canDrop ? DROPEFFECT_COPY : DROPEFFECT_NONE;
        }
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE DragOver(DWORD /*grfKeyState*/, POINTL pt, DWORD* pEffect) override
    {
        // 只有本次拖放是"能收"的才回调，免得调用方为拒收的拖动也去更新提示。
        // 回调的返回值决定<u>当前这个位置</u>能不能放 —— 窗口内只有一块区域收拖放时
        // 靠它逐帧改判（光标在同一个窗口里移动不会再触发 DragEnter）。
        bool allowHere = m_canDrop;
        if (m_canDrop && m_dragOverCb)
        {
            const POINT ptClient = DuiDropTargetHelper::ClientPointFromScreen(m_hostHwnd, pt);
            allowHere = m_dragOverCb(ptClient);
        }

        if (pEffect)
        {
            *pEffect = allowHere ? DROPEFFECT_COPY : DROPEFFECT_NONE;
        }
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE DragLeave() override
    {
        m_canDrop = false;
        NotifyDragEnd();
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE Drop(IDataObject* pDO, DWORD /*grfKeyState*/,
                                  POINTL pt, DWORD* pEffect) override
    {
        if (pEffect)
        {
            *pEffect = DROPEFFECT_NONE;
        }
        if (!pDO)
        {
            NotifyDragEnd();
            return E_POINTER;
        }

        // DragEnter 阶段判定为拒收（格式不支持，或调用方主动拒绝）：什么都不做，
        // 只把悬停提示收起来。
        if (!m_canDrop)
        {
            NotifyDragEnd();
            return S_OK;
        }

        // 落点坐标换算一次，文件回调与位图回调都用它。
        const POINT ptClient = DuiDropTargetHelper::ClientPointFromScreen(m_hostHwnd, pt);

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
            // 带坐标的回调优先：装了它就只走它，免得同一次拖放被处理两遍。
            if (!files.empty())
            {
                if (m_filesAtPtCb)
                {
                    m_filesAtPtCb(files, ptClient);
                }
                else if (m_filesCb)
                {
                    m_filesCb(files);
                }
            }
            ::ReleaseStgMedium(&med);
            if (pEffect)
            {
                *pEffect = DROPEFFECT_COPY;
            }
            NotifyDragEnd();
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
                // 直接把拖放源交来的位图交给回调，它只在回调执行期间有效（约定见
                // DuiDropTarget.h 的 BitmapCallback）。这里原先用 CopyImage 加
                // LR_COPYRETURNORG "复制"一份：尺寸色深一致时该标志直接返回原句柄，
                // 等于没复制，回调存下的句柄在下面释放后失效；偶尔真复制出来的那份
                // 又不会被释放，造成 GDI 对象泄漏（单测 BitmapDropLeavesNoGdiObjects
                // 在旧写法下实测每次拖放多出一个 GDI 对象）。
                m_bmpCb(med.hBitmap);
            }
            ::ReleaseStgMedium(&med);
            if (pEffect)
            {
                *pEffect = DROPEFFECT_COPY;
            }
            NotifyDragEnd();
            return S_OK;
        }

        NotifyDragEnd();
        return S_OK;
    }

private:
    // 一次拖放结束（拖离 / 落手 / 落手时取不到数据）后统一回调 onLeave，让调用方
    // 只在一个地方收起悬停提示 —— OLE 在 Drop 之后是<u>不会</u>再调 DragLeave 的。
    void NotifyDragEnd()
    {
        if (m_dragLeaveCb)
        {
            m_dragLeaveCb();
        }
    }

    // 本次拖的数据里是否含文件列表（CF_HDROP）。DragEnter 用它告诉调用方拖来的
    // 是文件还是位图，好让提示文案有区别。
    static bool HasFiles(IDataObject* pDO)
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
        return pDO->QueryGetData(&fmt) == S_OK;
    }

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
    HWND  m_hostHwnd = nullptr;         // 宿主窗口，屏幕坐标换算成客户区坐标要用；未注册时为空
    DuiDropTargetHelper::FilesCallback  m_filesCb;
    DuiDropTargetHelper::BitmapCallback m_bmpCb;
    DuiDropTargetHelper::FilesAtPointCallback m_filesAtPtCb;   // 带落点坐标的文件回调，优先于 m_filesCb
    DuiDropTargetHelper::DragEnterCallback    m_dragEnterCb;   // 拖入回调，返回 false 表示本次拒收
    DuiDropTargetHelper::DragOverCallback     m_dragOverCb;    // 拖动中光标移动回调
    DuiDropTargetHelper::DragLeaveCallback    m_dragLeaveCb;   // 拖离 / 落手后回调，用于收起提示
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
    // 注册成功后把宿主窗口交给实现对象，它据此把 OLE 给的屏幕坐标换算成客户区坐标。
    m_impl->SetHostHwnd(hwnd);
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

void DuiDropTargetHelper::SetDragCallbacks(DragEnterCallback onEnter, DragOverCallback onOver,
                                           DragLeaveCallback onLeave)
{
    if (!m_impl)
    {
        m_impl = new DuiDropTargetImpl(this);
    }
    m_impl->SetDragCallbacks(onEnter, onOver, onLeave);
    // 先装回调、后 Register 时，实现对象还不知道宿主窗口；已注册则补上，避免
    // 装回调的顺序影响坐标换算。
    m_impl->SetHostHwnd(m_hwnd);
}

void DuiDropTargetHelper::SetFilesAtPointCallback(FilesAtPointCallback onFilesAtPoint)
{
    if (!m_impl)
    {
        m_impl = new DuiDropTargetImpl(this);
    }
    m_impl->SetFilesAtPointCallback(onFilesAtPoint);
    m_impl->SetHostHwnd(m_hwnd);
}

IDropTarget* DuiDropTargetHelper::GetDropTarget()
{
    if (!m_impl)
    {
        m_impl = new DuiDropTargetImpl(this);
    }
    return m_impl;
}

POINT DuiDropTargetHelper::ClientPointFromScreen(HWND hwnd, const POINTL& ptScreen)
{
    POINT pt;
    pt.x = (LONG)ptScreen.x;
    pt.y = (LONG)ptScreen.y;
    // 窗口为空或已销毁时没法换算，原样返回屏幕坐标 —— 这比返回 (0,0) 好，调用方
    // 至少还能看出这是一个真实坐标而不是"落在左上角"。
    if (hwnd == nullptr || !::IsWindow(hwnd))
    {
        return pt;
    }
    ::ScreenToClient(hwnd, &pt);
    return pt;
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
