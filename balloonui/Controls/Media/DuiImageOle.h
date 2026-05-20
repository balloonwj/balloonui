#pragma once

#include "../../BalloonUiFeatures.h"
#if BUI_FEATURE_IMAGEOLE

// .cpp 必须先 include stdafx.h（项目 PCH 约定）。

// =================================================================
// CDuiImageOle —— 把 HBITMAP 包成 OLE 对象嵌入 RichEdit
// =================================================================
//
// 用途：DuiRichEditHost::InsertImageFromFile / InsertImageFromBitmap
// 在富文本里插图所用的"图片对象"。比"CF_BITMAP 剪贴板 +
// EM_PASTESPECIAL 借道 hack"更干净 —— 用真 OLE 对象插，图片在选区 /
// 拖动 / 复制时不会污染用户剪贴板。
//
// 只实现 RichEdit 真正会调到的接口子集：
//   · IOleObject             —— 大多空 stub；SetClientSite / Close 实做
//   · IViewObject2::Draw     —— 把 bitmap 画到目标 HDC
//   · IViewObject2::GetExtent —— 返 himetric 范围
//   · IDataObject::GetData(CF_BITMAP) —— EM_PASTE / drag-out 能取到位图
//   · IPersistStorage stubs  —— Save/Load 空实现（RichEdit 用内存里的
//                                位图重画即可；RTF 序列化不在本次迭代
//                                范围）
//
// 代码用法：
//
//     // 单 STA 线程；ULONG 引用计数。
//     auto* obj = new CDuiImageOle(hbm, /*ownsHbm*/ true);
//     // 然后把 obj QueryInterface 成 IOleObject* 塞进 REOBJECT，调
//     // IRichEditOle::InsertObject 插入。RichEdit 会自己加 ref；caller
//     // InsertObject 后 Release 本地指针即可。
//     // ownsHbm=true 时 OLE 对象在 dtor 里 DeleteObject HBITMAP。
//
// 不是 DuiControl 子类，<u>不</u>参与 XML / DUI 树。完全是富文本图文
// 混排的 plumbing。

#include <windows.h>
#include <ole2.h>
#include <richole.h>

namespace balloonwjui {

class CDuiImageOle : public IOleObject,
                     public IViewObject2,
                     public IDataObject,
                     public IPersistStorage
{
public:
    // hbm: bitmap to display. ownsHbm=true transfers ownership.
    CDuiImageOle(HBITMAP hbm, bool ownsHbm);
    virtual ~CDuiImageOle();

    // Helper that walks the full plumbing: lazy create RichEdit storage,
    // build the REOBJECT, insert at REO_CP_SELECTION. Returns true on
    // success. The caller still owns the HBITMAP if it passed ownsHbm
    // = false to the constructor.
    static bool InsertIntoRichEdit(HWND hRichEdit, HBITMAP hbm, bool ownsHbm);

    // ---- IUnknown ----
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override;
    ULONG   STDMETHODCALLTYPE AddRef () override;
    ULONG   STDMETHODCALLTYPE Release() override;

    // ---- IOleObject ----
    HRESULT STDMETHODCALLTYPE SetClientSite(IOleClientSite* pSite) override;
    HRESULT STDMETHODCALLTYPE GetClientSite(IOleClientSite** ppSite) override;
    HRESULT STDMETHODCALLTYPE SetHostNames(LPCOLESTR, LPCOLESTR) override;
    HRESULT STDMETHODCALLTYPE Close(DWORD dwSaveOption) override;
    HRESULT STDMETHODCALLTYPE SetMoniker(DWORD, IMoniker*) override;
    HRESULT STDMETHODCALLTYPE GetMoniker(DWORD, DWORD, IMoniker**) override;
    HRESULT STDMETHODCALLTYPE InitFromData(IDataObject*, BOOL, DWORD) override;
    HRESULT STDMETHODCALLTYPE GetClipboardData(DWORD, IDataObject**) override;
    HRESULT STDMETHODCALLTYPE DoVerb(LONG, LPMSG, IOleClientSite*, LONG, HWND, LPCRECT) override;
    HRESULT STDMETHODCALLTYPE EnumVerbs(IEnumOLEVERB**) override;
    HRESULT STDMETHODCALLTYPE Update() override;
    HRESULT STDMETHODCALLTYPE IsUpToDate() override;
    HRESULT STDMETHODCALLTYPE GetUserClassID(CLSID*) override;
    HRESULT STDMETHODCALLTYPE GetUserType(DWORD, LPOLESTR*) override;
    HRESULT STDMETHODCALLTYPE SetExtent(DWORD, SIZEL*) override;
    HRESULT STDMETHODCALLTYPE GetExtent(DWORD, SIZEL*) override;
    HRESULT STDMETHODCALLTYPE Advise(IAdviseSink*, DWORD*) override;
    HRESULT STDMETHODCALLTYPE Unadvise(DWORD) override;
    HRESULT STDMETHODCALLTYPE EnumAdvise(IEnumSTATDATA**) override;
    HRESULT STDMETHODCALLTYPE GetMiscStatus(DWORD, DWORD*) override;
    HRESULT STDMETHODCALLTYPE SetColorScheme(LOGPALETTE*) override;

    // ---- IViewObject2 ----
    HRESULT STDMETHODCALLTYPE Draw(DWORD dwDrawAspect, LONG lindex,
                                   void* pvAspect, DVTARGETDEVICE* ptd,
                                   HDC hdcTargetDev, HDC hdcDraw,
                                   LPCRECTL lprcBounds, LPCRECTL lprcWBounds,
                                   BOOL (STDMETHODCALLTYPE* pfnContinue)(ULONG_PTR),
                                   ULONG_PTR dwContinue) override;
    HRESULT STDMETHODCALLTYPE GetColorSet(DWORD, LONG, void*, DVTARGETDEVICE*, HDC, LOGPALETTE**) override;
    HRESULT STDMETHODCALLTYPE Freeze(DWORD, LONG, void*, DWORD*) override;
    HRESULT STDMETHODCALLTYPE Unfreeze(DWORD) override;
    HRESULT STDMETHODCALLTYPE SetAdvise(DWORD, DWORD, IAdviseSink*) override;
    HRESULT STDMETHODCALLTYPE GetAdvise(DWORD*, DWORD*, IAdviseSink**) override;
    HRESULT STDMETHODCALLTYPE GetExtent(DWORD, LONG, DVTARGETDEVICE*, LPSIZEL) override;

    // ---- IDataObject ----
    HRESULT STDMETHODCALLTYPE GetData(FORMATETC*, STGMEDIUM*) override;
    HRESULT STDMETHODCALLTYPE GetDataHere(FORMATETC*, STGMEDIUM*) override;
    HRESULT STDMETHODCALLTYPE QueryGetData(FORMATETC*) override;
    HRESULT STDMETHODCALLTYPE GetCanonicalFormatEtc(FORMATETC*, FORMATETC*) override;
    HRESULT STDMETHODCALLTYPE SetData(FORMATETC*, STGMEDIUM*, BOOL) override;
    HRESULT STDMETHODCALLTYPE EnumFormatEtc(DWORD, IEnumFORMATETC**) override;
    HRESULT STDMETHODCALLTYPE DAdvise(FORMATETC*, DWORD, IAdviseSink*, DWORD*) override;
    HRESULT STDMETHODCALLTYPE DUnadvise(DWORD) override;
    HRESULT STDMETHODCALLTYPE EnumDAdvise(IEnumSTATDATA**) override;

    // ---- IPersist / IPersistStorage ----
    HRESULT STDMETHODCALLTYPE GetClassID(CLSID*) override;
    HRESULT STDMETHODCALLTYPE IsDirty() override;
    HRESULT STDMETHODCALLTYPE InitNew(IStorage*) override;
    HRESULT STDMETHODCALLTYPE Load(IStorage*) override;
    HRESULT STDMETHODCALLTYPE Save(IStorage*, BOOL) override;
    HRESULT STDMETHODCALLTYPE SaveCompleted(IStorage*) override;
    HRESULT STDMETHODCALLTYPE HandsOffStorage() override;

    // ---- accessors / test helpers ----
    HBITMAP GetBitmap() const { return m_hbm; }
    SIZE    GetPixelSize() const { return m_pxSize; }
    LONG    GetRefCount() const { return m_ref; }

private:
    void    ComputeSizes();   // fills m_pxSize and m_himetric

private:
    LONG             m_ref;
    HBITMAP          m_hbm;
    bool             m_ownsHbm;
    SIZE             m_pxSize;       // pixels
    SIZEL            m_himetric;     // 0.01mm units
    IOleClientSite*  m_pSite;
};

} // namespace balloonwjui

#endif // BUI_FEATURE_IMAGEOLE
