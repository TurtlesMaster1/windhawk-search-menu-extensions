
#include <windows.h>
#include <inspectable.h>
#include <roapi.h>
#include <winstring.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <sddl.h>

#include <atomic>
#include <condition_variable>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Data.Json.h>

// combaseapi.h only declares these for NTDDI_VERSION >= Windows 8, and windows.h is already
// included (by windhawk_api.h) before this file could raise it.
#if !defined(NTDDI_VERSION) || NTDDI_VERSION < 0x06020000
DECLARE_HANDLE(CO_MTA_USAGE_COOKIE);
extern "C" DECLSPEC_IMPORT HRESULT WINAPI CoIncrementMTAUsage(CO_MTA_USAGE_COOKIE* cookie);
extern "C" DECLSPEC_IMPORT HRESULT WINAPI CoDecrementMTAUsage(CO_MTA_USAGE_COOKIE cookie);
#endif



namespace wf = winrt::Windows::Foundation;
namespace wfc = winrt::Windows::Foundation::Collections;
namespace wdj = winrt::Windows::Data::Json;

struct Settings {
    bool demoProvider = true;
    bool pipeServer = true;
    int queryTimeoutMs = 2500;
    bool restartIfLate = true;
} g_settings;

//@@ABI@@

// ---------------------------------------------------------------------------
// Generic WinRT ABI pieces we implement by hand.
// ---------------------------------------------------------------------------
constexpr GUID kIID_IInspectable = {0xAF86E2E0, 0xB12D, 0x4c6a, {0x9C, 0x5A, 0xD7, 0xAA, 0x65, 0x10, 0x1E, 0x90}};
constexpr GUID IID_IAgileObject_ = {0x94ea2b94, 0xe9cc, 0x49e0, {0xc0, 0xff, 0xee, 0x64, 0xca, 0x8f, 0x5b, 0x90}};
constexpr GUID IID_IAsyncInfo_ = {0x00000036, 0x0000, 0x0000, {0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}};
constexpr GUID IID_IAsyncAction_ = {0x5a648006, 0x843a, 0x4da9, {0x86, 0x5b, 0x9d, 0x26, 0xe5, 0xdf, 0xad, 0x7b}};
// IAsyncOperationWithProgress<PaletteQueryResultsView, PaletteQueryProgress>
constexpr GUID IID_AsyncOpResults = {0xcb62b7ba, 0x8e45, 0x55c1, {0xbd, 0x2e, 0x81, 0xb7, 0xf5, 0x13, 0xd1, 0x35}};
// IAsyncOperation<PaletteContent>
constexpr GUID IID_AsyncOpContent = {0xcb73ad0d, 0x69fd, 0x5ae3, {0x98, 0x8c, 0xad, 0x2b, 0xf6, 0x8a, 0x16, 0x60}};
constexpr GUID IID_IPaletteCatalogPrivate = {0xdcaa0cae, 0x03e9, 0x40b2, {0x94, 0x96, 0x67, 0x6c, 0xd0, 0x56, 0x82, 0x78}};
constexpr GUID IID_IValueUnmarshalByPropertySet = {0xecabe149, 0x4833, 0x452b, {0x81, 0x12, 0x3e, 0xee, 0xc6, 0xd2, 0x0c, 0xb0}};

struct IValueUnmarshalByPropertySet : ::IInspectable {
    virtual HRESULT STDMETHODCALLTYPE UnmarshalObjectFromPropertySet(void* propertySet, void** result) = 0;
};

#ifndef E_ILLEGAL_DELEGATE_ASSIGNMENT
#define E_ILLEGAL_DELEGATE_ASSIGNMENT ((HRESULT)0x80000018L)
#endif
#ifndef E_ILLEGAL_METHOD_CALL
#define E_ILLEGAL_METHOD_CALL ((HRESULT)0x8000000EL)
#endif
enum AsyncStatus_ : INT32 { AS_Started = 0, AS_Completed = 1, AS_Canceled = 2, AS_Error = 3 };

struct IAsyncInfoRaw : ::IInspectable {
    virtual HRESULT STDMETHODCALLTYPE get_Id(UINT32* id) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_Status(INT32* status) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_ErrorCode(HRESULT* code) = 0;
    virtual HRESULT STDMETHODCALLTYPE Cancel() = 0;
    virtual HRESULT STDMETHODCALLTYPE Close() = 0;
};
struct IAsyncOpWithProgressRaw : ::IInspectable {
    virtual HRESULT STDMETHODCALLTYPE put_Progress(::IUnknown* handler) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_Progress(::IUnknown** handler) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_Completed(::IUnknown* handler) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_Completed(::IUnknown** handler) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetResults(::IUnknown** results) = 0;
};
struct IAsyncOpRaw : ::IInspectable {
    virtual HRESULT STDMETHODCALLTYPE put_Completed(::IUnknown* handler) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_Completed(::IUnknown** handler) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetResults(::IUnknown** results) = 0;
};
struct IAsyncActionRaw : ::IInspectable {
    virtual HRESULT STDMETHODCALLTYPE put_Completed(::IUnknown* handler) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_Completed(::IUnknown** handler) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetResults() = 0;
};
// Every completed/progress handler and TypedEventHandler has Invoke(a, b) at slot 3.
struct IHandler2Ptr : ::IUnknown {
    virtual HRESULT STDMETHODCALLTYPE Invoke(void* a, void* b) = 0;
};
struct IHandler2Int : ::IUnknown {
    virtual HRESULT STDMETHODCALLTYPE Invoke(void* a, INT32 b) = 0;
};

struct IPaletteCatalogPrivate : ::IInspectable {
    virtual void STDMETHODCALLTYPE SetEnumerateDisabledSources(BOOL v) = 0;
    virtual void STDMETHODCALLTYPE GetOrderedSourceDefinitions(HSTRING kind, UINT32* n, void*** defs) = 0;
    virtual BOOL STDMETHODCALLTYPE IsEnabled(HSTRING id) = 0;
    virtual BOOL STDMETHODCALLTYPE IsUserDisabled(HSTRING id) = 0;
    virtual void STDMETHODCALLTYPE SetItemSourceEnabled(HSTRING id, BOOL enabled) = 0;
};

// Every object handed to Windows derives from LiveToken (as its first base, so it is destroyed last), so uninit knows whether Windows still holds
// code from this DLL (and must restart the composer instead of letting it unload).
static std::atomic<int> g_liveObjects{0};
struct LiveToken {
    LiveToken() { ++g_liveObjects; }
    LiveToken(LiveToken const&) = delete;
    ~LiveToken() { --g_liveObjects; }
};

#define IMPL_REFCOUNT                                                         \
    std::atomic<ULONG> m_ref{1};                                              \
    ULONG STDMETHODCALLTYPE AddRef() override { return ++m_ref; }             \
    ULONG STDMETHODCALLTYPE Release() override {                              \
        ULONG r = --m_ref;                                                    \
        if (!r) delete this;                                                  \
        return r;                                                             \
    }
#define IMPL_INSPECTABLE(cls)                                                 \
    HRESULT STDMETHODCALLTYPE GetIids(ULONG* c, IID** i) override {           \
        *c = 0;                                                               \
        *i = nullptr;                                                         \
        return S_OK;                                                          \
    }                                                                         \
    HRESULT STDMETHODCALLTYPE GetRuntimeClassName(HSTRING* n) override {      \
        return WindowsCreateString(cls, (UINT32)wcslen(cls), n);              \
    }                                                                         \
    HRESULT STDMETHODCALLTYPE GetTrustLevel(TrustLevel* t) override {         \
        *t = BaseTrust;                                                       \
        return S_OK;                                                          \
    }

static inline HSTRING H(winrt::hstring const& s) { return (HSTRING)winrt::get_abi(s); }
static inline winrt::hstring FromH(HSTRING h) {
    UINT32 n;
    const wchar_t* p = WindowsGetStringRawBuffer(h, &n);
    return winrt::hstring(p, n);
}
static HRESULT DupH(winrt::hstring const& s, HSTRING* out) {
    return WindowsDuplicateString(H(s), out);
}

static std::atomic<bool> g_alive{false};

// ---------------------------------------------------------------------------
// Worker pool. Everything that runs off Windows' threads goes through here, so uninit can drain
// it (CloseThreadpoolCleanupGroupMembers waits for every submitted callback and timer).
// ---------------------------------------------------------------------------
static TP_CALLBACK_ENVIRON g_tpEnv;
static PTP_CLEANUP_GROUP g_tpGroup;
static std::mutex g_tpLock;
static bool g_tpOpen;

static bool PoolOpen() {
    std::lock_guard l(g_tpLock);
    if (g_tpOpen) return true;
    g_tpGroup = CreateThreadpoolCleanupGroup();
    if (!g_tpGroup) return false;
    InitializeThreadpoolEnvironment(&g_tpEnv);
    SetThreadpoolCallbackCleanupGroup(&g_tpEnv, g_tpGroup, nullptr);
    g_tpOpen = true;
    return true;
}
static void PoolClose() {
    {
        std::lock_guard l(g_tpLock);
        if (!g_tpOpen) return;
        g_tpOpen = false;  // no new work from here on
    }
    CloseThreadpoolCleanupGroupMembers(g_tpGroup, FALSE, nullptr);  // waits for all callbacks
    CloseThreadpoolCleanupGroup(g_tpGroup);
    DestroyThreadpoolEnvironment(&g_tpEnv);
    g_tpGroup = nullptr;
}
static VOID CALLBACK PoolThunk(PTP_CALLBACK_INSTANCE, PVOID ctx) {
    std::unique_ptr<std::function<void()>> fn((std::function<void()>*)ctx);
    try {
        (*fn)();
    } catch (...) {
    }
}
static bool Submit(std::function<void()> fn) {
    auto* heap = new std::function<void()>(std::move(fn));
    std::lock_guard l(g_tpLock);
    if (!g_tpOpen || !TrySubmitThreadpoolCallback(PoolThunk, heap, &g_tpEnv)) {
        delete heap;
        return false;
    }
    return true;
}
// A pool timer (closed with the pool). Returns nullptr once the pool is closing.
static PTP_TIMER PoolTimer(PTP_TIMER_CALLBACK cb, void* ctx) {
    std::lock_guard l(g_tpLock);
    return g_tpOpen ? CreateThreadpoolTimer(cb, ctx, &g_tpEnv) : nullptr;
}
static void ArmTimer(PTP_TIMER t, int ms) {
    LARGE_INTEGER due;
    due.QuadPart = -10000LL * ms;
    FILETIME ft{due.LowPart, (DWORD)due.HighPart};
    SetThreadpoolTimer(t, &ft, 0, 0);
}

// Activation factories are agile; cache them.
template <class T>
static winrt::com_ptr<T> Factory(const wchar_t* cls, const GUID& iid) {
    winrt::com_ptr<T> out;
    try {
        auto f = winrt::get_activation_factory<wf::IInspectable>(cls);
        ((::IUnknown*)winrt::get_abi(f))->QueryInterface(iid, out.put_void());
    } catch (...) {
    }
    if (!out) Wh_Log(L"activation factory %s unavailable", cls);
    return out;
}
// Cached in trivially destructible slots and released by ReleaseFactories() at uninit, never by a
// static destructor (that would run at process exit, after COM is gone).
static std::mutex g_factoriesLock;
static std::vector<std::atomic<::IUnknown*>*> g_factorySlots;  // plain pointers
template <class T>
static T* CachedFactory(std::atomic<::IUnknown*>& slot, const wchar_t* cls, const GUID& iid) {
    if (auto* f = slot.load()) return (T*)f;
    auto f = Factory<T>(cls, iid);
    if (!f) return nullptr;
    std::lock_guard l(g_factoriesLock);
    if (auto* cur = slot.load()) return (T*)cur;
    slot = (::IUnknown*)f.detach();
    g_factorySlots.push_back(&slot);
    return (T*)slot.load();
}
static void ReleaseFactories() {
    std::lock_guard l(g_factoriesLock);
    for (auto* s : g_factorySlots)
        if (auto* f = s->exchange(nullptr)) f->Release();
    g_factorySlots.clear();
}
#define CACHED_FACTORY(T, cls)                                                \
    ([]() -> T* {                                                             \
        static std::atomic<::IUnknown*> slot{nullptr};                        \
        return CachedFactory<T>(slot, cls, IID_##T);                          \
    }())
#define IID_IPaletteKindsFactory Pal::IID_IPaletteKindsFactory
#define IID_IPaletteIconDataStatics Pal::IID_IPaletteIconDataStatics
#define IID_IPaletteTagFactory Pal::IID_IPaletteTagFactory
#define IID_IPaletteQueryResultsFactory Pal::IID_IPaletteQueryResultsFactory
#define IID_IPaletteDelegateInvokerFactory Pal::IID_IPaletteDelegateInvokerFactory
#define IID_IPaletteToggleInvokerFactory Pal::IID_IPaletteToggleInvokerFactory
#define IID_IPaletteUriInvokerFactory Pal::IID_IPaletteUriInvokerFactory
#define IID_IPaletteTemplateContentFactory Pal::IID_IPaletteTemplateContentFactory
#define IID_IPaletteQueryProgressFactory Pal::IID_IPaletteQueryProgressFactory
using Pal::IPaletteKindsFactory;
using Pal::IPaletteIconDataStatics;
using Pal::IPaletteTagFactory;
using Pal::IPaletteQueryResultsFactory;
using Pal::IPaletteDelegateInvokerFactory;
using Pal::IPaletteToggleInvokerFactory;
using Pal::IPaletteUriInvokerFactory;
using Pal::IPaletteTemplateContentFactory;
using Pal::IPaletteQueryProgressFactory;

static winrt::com_ptr<Pal::IPaletteKinds> MakeKinds(std::vector<std::wstring> const& v) {
    winrt::com_ptr<Pal::IPaletteKinds> k;
    auto* f = CACHED_FACTORY(IPaletteKindsFactory, L"Windows.UI.Palettes.PaletteKinds");
    if (!f) return k;
    std::vector<winrt::hstring> hs(v.begin(), v.end());
    std::vector<HSTRING> raw;
    for (auto& h : hs) raw.push_back(H(h));
    f->CreateInstance((UINT32)raw.size(), raw.data(), k.put());
    return k;
}

static bool KindMatches(std::wstring const& a, std::wstring const& b) {
    if (!_wcsicmp(a.c_str(), b.c_str())) return true;
    auto pref = [](std::wstring const& w, std::wstring const& x) {
        if (w.size() < 2 || w.compare(w.size() - 2, 2, L".*")) return false;
        std::wstring p = w.substr(0, w.size() - 1);  // keep the dot
        return x.size() >= p.size() && !_wcsnicmp(x.c_str(), p.c_str(), p.size());
    };
    return pref(a, b) || pref(b, a) || a == L"*" || b == L"*";
}

// ---------------------------------------------------------------------------
// Async operations
// ---------------------------------------------------------------------------
struct AsyncBase : IAsyncInfoRaw {
    std::mutex m;
    INT32 status = AS_Started;
    HRESULT error = S_OK;
    winrt::com_ptr<::IUnknown> completed;
    winrt::com_ptr<::IUnknown> result;
    UINT32 id;
    AsyncBase() {
        static std::atomic<UINT32> next{1};
        id = next++;
    }
    virtual ~AsyncBase() = default;
    virtual void* OpPtr() = 0;  // interface pointer passed to handlers

    HRESULT STDMETHODCALLTYPE get_Id(UINT32* v) override { *v = id; return S_OK; }
    HRESULT STDMETHODCALLTYPE get_Status(INT32* v) override { std::lock_guard l(m); *v = status; return S_OK; }
    HRESULT STDMETHODCALLTYPE get_ErrorCode(HRESULT* v) override { std::lock_guard l(m); *v = error; return S_OK; }
    HRESULT STDMETHODCALLTYPE Cancel() override {
        OnCancel();
        Finish(AS_Canceled, nullptr, HRESULT_FROM_WIN32(ERROR_CANCELLED));
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE Close() override { return S_OK; }
    virtual void OnCancel() {}

    HRESULT SetCompleted(::IUnknown* h) {
        INT32 st;
        {
            std::lock_guard l(m);
            if (completed) return E_ILLEGAL_DELEGATE_ASSIGNMENT;
            if (!h) return S_OK;
            st = status;
            if (st == AS_Started) {
                completed.copy_from(h);
                return S_OK;
            }
        }
        ((IHandler2Int*)h)->Invoke(OpPtr(), st);
        return S_OK;
    }
    HRESULT GetCompleted(::IUnknown** h) {
        std::lock_guard l(m);
        completed.copy_to(h);
        return S_OK;
    }
    HRESULT Results(::IUnknown** r) {
        std::lock_guard l(m);
        if (status == AS_Completed) {
            result.copy_to(r);
            return S_OK;
        }
        if (status == AS_Error) return error;
        if (status == AS_Canceled) return HRESULT_FROM_WIN32(ERROR_CANCELLED);
        return E_ILLEGAL_METHOD_CALL;
    }
    // Returns false if already finished.
    bool Finish(INT32 st, ::IUnknown* res, HRESULT err = S_OK) {
        winrt::com_ptr<::IUnknown> h;
        {
            std::lock_guard l(m);
            if (status != AS_Started) return false;
            status = st;
            error = err;
            result.copy_from(res);
            h = std::move(completed);
        }
        if (h) ((IHandler2Int*)h.get())->Invoke(OpPtr(), st);
        return true;
    }
};

struct ResultsOp final : LiveToken, IAsyncOpWithProgressRaw, AsyncBase {
    IMPL_REFCOUNT
    IMPL_INSPECTABLE(L"Windows.Foundation.IAsyncOperationWithProgress`2<Windows.UI.Palettes.PaletteQueryResultsView, Windows.UI.Palettes.PaletteQueryProgress>")
    std::function<void()> onCancel;
    winrt::com_ptr<::IUnknown> progress;

    void* OpPtr() override { return static_cast<IAsyncOpWithProgressRaw*>(this); }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if (riid == IID_IUnknown || riid == kIID_IInspectable || riid == IID_AsyncOpResults || riid == IID_IAgileObject_)
            *ppv = static_cast<IAsyncOpWithProgressRaw*>(this);
        else if (riid == IID_IAsyncInfo_)
            *ppv = static_cast<IAsyncInfoRaw*>(this);
        else {
            *ppv = nullptr;
            return E_NOINTERFACE;
        }
        AddRef();
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE put_Progress(::IUnknown* h) override { std::lock_guard l(m); progress.copy_from(h); return S_OK; }
    HRESULT STDMETHODCALLTYPE get_Progress(::IUnknown** h) override { std::lock_guard l(m); progress.copy_to(h); return S_OK; }
    HRESULT STDMETHODCALLTYPE put_Completed(::IUnknown* h) override { return SetCompleted(h); }
    HRESULT STDMETHODCALLTYPE get_Completed(::IUnknown** h) override { return GetCompleted(h); }
    HRESULT STDMETHODCALLTYPE GetResults(::IUnknown** r) override { return Results(r); }
    void OnCancel() override { if (onCancel) onCancel(); }
    void ReportProgress(::IUnknown* p) {
        winrt::com_ptr<::IUnknown> h;
        {
            std::lock_guard l(m);
            if (status != AS_Started) return;
            h = progress;
        }
        if (h) ((IHandler2Ptr*)h.get())->Invoke(OpPtr(), p);
    }
};

struct ContentOp final : LiveToken, IAsyncOpRaw, AsyncBase {
    IMPL_REFCOUNT
    IMPL_INSPECTABLE(L"Windows.Foundation.IAsyncOperation`1<Windows.UI.Palettes.PaletteContent>")
    void* OpPtr() override { return static_cast<IAsyncOpRaw*>(this); }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if (riid == IID_IUnknown || riid == kIID_IInspectable || riid == IID_AsyncOpContent || riid == IID_IAgileObject_)
            *ppv = static_cast<IAsyncOpRaw*>(this);
        else if (riid == IID_IAsyncInfo_)
            *ppv = static_cast<IAsyncInfoRaw*>(this);
        else {
            *ppv = nullptr;
            return E_NOINTERFACE;
        }
        AddRef();
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE put_Completed(::IUnknown* h) override { return SetCompleted(h); }
    HRESULT STDMETHODCALLTYPE get_Completed(::IUnknown** h) override { return GetCompleted(h); }
    HRESULT STDMETHODCALLTYPE GetResults(::IUnknown** r) override { return Results(r); }
};

struct DoneAction final : LiveToken, IAsyncActionRaw, AsyncBase {
    IMPL_REFCOUNT
    IMPL_INSPECTABLE(L"Windows.Foundation.IAsyncAction")
    DoneAction() { status = AS_Completed; }
    void* OpPtr() override { return static_cast<IAsyncActionRaw*>(this); }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if (riid == IID_IUnknown || riid == kIID_IInspectable || riid == IID_IAsyncAction_ || riid == IID_IAgileObject_)
            *ppv = static_cast<IAsyncActionRaw*>(this);
        else if (riid == IID_IAsyncInfo_)
            *ppv = static_cast<IAsyncInfoRaw*>(this);
        else {
            *ppv = nullptr;
            return E_NOINTERFACE;
        }
        AddRef();
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE put_Completed(::IUnknown* h) override { return SetCompleted(h); }
    HRESULT STDMETHODCALLTYPE get_Completed(::IUnknown** h) override { return GetCompleted(h); }
    HRESULT STDMETHODCALLTYPE GetResults() override { return S_OK; }
};

struct InvokeDelegate final : LiveToken, Pal::PaletteItemInvokeDelegate {
    IMPL_REFCOUNT
    std::function<void(Pal::IPaletteQuery*)> fn;
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if (riid == IID_IUnknown || riid == Pal::IID_PaletteItemInvokeDelegate || riid == IID_IAgileObject_) {
            *ppv = static_cast<Pal::PaletteItemInvokeDelegate*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    HRESULT STDMETHODCALLTYPE Invoke(Pal::IPaletteQuery* q) override {
        try {
            if (fn && g_alive) fn(q);
        } catch (...) {
        }
        return S_OK;
    }
};

struct TemplateActionDelegate final : LiveToken, Pal::PaletteTemplateActionInvokeDelegate {
    IMPL_REFCOUNT
    std::function<void(HSTRING, HSTRING, HSTRING)> fn;
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if (riid == IID_IUnknown || riid == Pal::IID_PaletteTemplateActionInvokeDelegate || riid == IID_IAgileObject_) {
            *ppv = static_cast<Pal::PaletteTemplateActionInvokeDelegate*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    HRESULT STDMETHODCALLTYPE Invoke(Pal::IPaletteTemplateContent*, HSTRING actionId, HSTRING data, HSTRING inputs) override {
        try {
            if (fn && g_alive) fn(actionId, data, inputs);
        } catch (...) {
        }
        return S_OK;
    }
};

struct ContentDelegate final : LiveToken, Pal::PaletteContentDelegate {
    IMPL_REFCOUNT
    std::function<winrt::com_ptr<::IUnknown>(winrt::hstring const&)> fn;
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if (riid == IID_IUnknown || riid == Pal::IID_PaletteContentDelegate || riid == IID_IAgileObject_) {
            *ppv = static_cast<Pal::PaletteContentDelegate*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    HRESULT STDMETHODCALLTYPE Invoke(HSTRING itemId, IInspectable** operation) override {
        auto* op = new ContentOp();
        winrt::com_ptr<::IUnknown> content;
        try {
            if (fn && g_alive) content = fn(FromH(itemId));
        } catch (...) {
        }
        if (content)
            op->Finish(AS_Completed, content.get());
        else
            op->Finish(AS_Error, nullptr, E_FAIL);
        *operation = static_cast<IAsyncOpRaw*>(op);
        return S_OK;
    }
};

// ---------------------------------------------------------------------------
// Providers
// ---------------------------------------------------------------------------
struct ProviderCallbacks {
    std::function<void(UINT64 q, std::wstring const& text, std::wstring const& json)> onQuery;
    std::function<void(std::wstring const& item, std::wstring const& action, std::wstring const& data,
                       std::wstring const& inputs)>
        onInvoke;
    std::function<void(UINT64 q)> onCancel;
    std::function<void()> onDetach;
};

struct Extension;
struct Definition;

struct CardState {
    winrt::com_ptr<Pal::IPaletteTemplateContent> content;
    std::wstring templateJson, dataJson;
};

struct Provider {
    UINT64 handle = 0;
    std::wstring id, name, description, extId;
    std::wstring glyph, font, iconPath;
    bool zeroState = false;
    std::vector<std::wstring> kinds, containedKinds, itemKinds;
    ProviderCallbacks cb;

    winrt::com_ptr<Pal::IPaletteItemFactory> factory;
    winrt::com_ptr<Definition> def;

    std::mutex m;
    std::vector<winrt::com_ptr<Pal::IPaletteItemView>> staticItems;
    // Item lookup (GetItem/TryGetItem): the static items plus the latest query's results only.
    std::map<std::wstring, winrt::com_ptr<Pal::IPaletteItemView>> staticCache, resultCache;
    UINT64 resultQuery = 0;  // query whose results are in resultCache
    std::map<std::wstring, CardState> cards;
    std::vector<Extension*> extensions;  // weak, removed in ~Extension

    // Calls into the provider's callbacks, so unregistering can wait for them to finish.
    std::mutex cbLock;
    std::condition_variable cbIdle;
    int inFlight = 0;
    bool closed = false;
};

static thread_local Provider* t_inCallbackOf = nullptr;

// Run f (which calls p->cb.*) unless the provider is being unregistered.
template <class F>
static void CallProvider(Provider* p, F&& f) {
    {
        std::lock_guard l(p->cbLock);
        if (p->closed) return;
        p->inFlight++;
    }
    Provider* prev = t_inCallbackOf;
    t_inCallbackOf = p;
    try {
        f();
    } catch (...) {
    }
    t_inCallbackOf = prev;
    {
        std::lock_guard l(p->cbLock);
        p->inFlight--;
    }
    p->cbIdle.notify_all();
}
// Stop new callbacks and wait for running ones. Called from inside one of its own callbacks, it
// waits for the others only.
static void CloseProviderCallbacks(Provider* p) {
    std::unique_lock l(p->cbLock);
    p->closed = true;
    int self = t_inCallbackOf == p ? 1 : 0;
    p->cbIdle.wait(l, [&] { return p->inFlight <= self; });
}

static std::mutex g_providersLock;
[[clang::no_destroy]] static std::vector<std::shared_ptr<Provider>> g_providers;  // emptied at uninit
static std::atomic<UINT64> g_nextHandle{0x5E00};

static std::shared_ptr<Provider> FindProvider(UINT64 h) {
    std::lock_guard l(g_providersLock);
    for (auto& p : g_providers)
        if (p->handle == h) return p;
    return nullptr;
}
static std::shared_ptr<Provider> FindProviderByExtId(winrt::hstring const& ext) {
    std::lock_guard l(g_providersLock);
    for (auto& p : g_providers)
        if (ext == p->extId) return p;
    return nullptr;
}
static std::vector<std::shared_ptr<Provider>> ProvidersSnapshot() {
    std::lock_guard l(g_providersLock);
    return g_providers;
}

// ---------------------------------------------------------------------------
// JSON -> native PaletteItem
// ---------------------------------------------------------------------------
static std::wstring JStr(wdj::JsonObject const& o, const wchar_t* k, std::wstring def = L"") {
    if (!o.HasKey(k)) return def;
    auto v = o.GetNamedValue(k);
    switch (v.ValueType()) {
    case wdj::JsonValueType::String: return std::wstring(v.GetString());
    case wdj::JsonValueType::Null: return def;
    default: return std::wstring(v.Stringify());
    }
}
static std::vector<std::wstring> JStrArr(wdj::JsonObject const& o, const wchar_t* k) {
    std::vector<std::wstring> r;
    if (!o.HasKey(k)) return r;
    auto v = o.GetNamedValue(k);
    if (v.ValueType() == wdj::JsonValueType::String) {
        r.emplace_back(v.GetString());
    } else if (v.ValueType() == wdj::JsonValueType::Array) {
        for (auto e : v.GetArray())
            if (e.ValueType() == wdj::JsonValueType::String) r.emplace_back(e.GetString());
    }
    return r;
}
static wf::IInspectable JsonToBoxed(wdj::IJsonValue const& v) {
    switch (v.ValueType()) {
    case wdj::JsonValueType::String: return winrt::box_value(v.GetString());
    case wdj::JsonValueType::Number: return winrt::box_value(v.GetNumber());
    case wdj::JsonValueType::Boolean: return winrt::box_value(v.GetBoolean());
    case wdj::JsonValueType::Null: return nullptr;
    default: return winrt::box_value(v.Stringify());
    }
}

static winrt::com_ptr<Pal::IPaletteIconData> MakeIcon(std::wstring const& glyph, std::wstring const& font,
                                                      std::wstring const& uriOrPath) {
    winrt::com_ptr<Pal::IPaletteIconData> icon;
    auto* st = CACHED_FACTORY(IPaletteIconDataStatics, L"Windows.UI.Palettes.PaletteIconData");
    if (!st) return icon;
    try {
        if (!glyph.empty()) {
            winrt::hstring g(glyph), f(font.empty() ? L"Segoe Fluent Icons" : font);
            st->FromGlyph2(H(g), H(f), icon.put());
        } else if (!uriOrPath.empty()) {
            std::wstring u = uriOrPath;
            if (u.find(L"://") == std::wstring::npos && u.find(L":") == 1) {  // C:\path
                wchar_t buf[2048];
                DWORD n = ARRAYSIZE(buf);
                if (SUCCEEDED(UrlCreateFromPathW(u.c_str(), buf, &n, 0))) u = buf;
            }
            wf::Uri uri(u);
            st->FromUri((IInspectable*)winrt::get_abi(uri), icon.put());
        }
    } catch (...) {
        Wh_Log(L"bad icon '%s'", uriOrPath.c_str());
    }
    return icon;
}
static winrt::com_ptr<Pal::IPaletteIconData> MakeIconJson(wdj::IJsonValue const& v) {
    if (v.ValueType() == wdj::JsonValueType::String) {
        std::wstring s(v.GetString());
        if (s.size() <= 2) return MakeIcon(s, L"", L"");
        return MakeIcon(L"", L"", s);
    }
    if (v.ValueType() == wdj::JsonValueType::Object) {
        auto o = v.GetObject();
        std::wstring uri = JStr(o, L"uri");
        if (uri.empty()) uri = JStr(o, L"path");
        return MakeIcon(JStr(o, L"glyph"), JStr(o, L"font"), uri);
    }
    return nullptr;
}

// Runs on a pool thread. The clipboard needs an owner window for SetClipboardData to stick.
static void CopyToClipboard(std::wstring const& text) {
    HWND owner = CreateWindowExW(0, L"STATIC", L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, nullptr, nullptr);
    bool opened = false;
    for (int i = 0; i < 10 && !(opened = OpenClipboard(owner)); i++) Sleep(20);  // another app may hold it
    if (opened) {
        EmptyClipboard();
        size_t bytes = (text.size() + 1) * sizeof(wchar_t);
        HGLOBAL g = GlobalAlloc(GMEM_MOVEABLE, bytes);
        if (g) {
            memcpy(GlobalLock(g), text.c_str(), bytes);
            GlobalUnlock(g);
            if (!SetClipboardData(CF_UNICODETEXT, g)) GlobalFree(g);
        }
        CloseClipboard();
    }
    if (owner) DestroyWindow(owner);
}

// Runs on a pool thread, in a temporary STA as ShellExecute expects.
static void RunCommand(std::wstring const& cmd, std::wstring const& args, std::wstring const& dir) {
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    ShellExecuteW(nullptr, L"open", cmd.c_str(), args.empty() ? nullptr : args.c_str(),
                  dir.empty() ? nullptr : dir.c_str(), SW_SHOWNORMAL);
    if (SUCCEEDED(hr)) CoUninitialize();
}

static void DispatchInvoke(std::shared_ptr<Provider> const& p, std::wstring const& item, std::wstring const& action,
                           std::wstring const& data, std::wstring const& inputs) {
    Wh_Log(L"[%s] invoke item=%s action=%s data=%s inputs=%s", p->id.c_str(), item.c_str(), action.c_str(),
         data.c_str(), inputs.c_str());
    if (!p->cb.onInvoke) return;
    Submit([p, item, action, data, inputs] {
        CallProvider(p.get(), [&] { p->cb.onInvoke(item, action, data, inputs); });
    });
}

static winrt::com_ptr<::IUnknown> MakeCardContent(std::shared_ptr<Provider> const& p, std::wstring const& itemId) {
    std::wstring tmpl, data;
    {
        std::lock_guard l(p->m);
        auto it = p->cards.find(itemId);
        if (it == p->cards.end()) return nullptr;
        tmpl = it->second.templateJson;
        data = it->second.dataJson;
    }
    auto* f = CACHED_FACTORY(IPaletteTemplateContentFactory, L"Windows.UI.Palettes.PaletteTemplateContent");
    if (!f) return nullptr;
    winrt::com_ptr<TemplateActionDelegate> act;
    act.attach(new TemplateActionDelegate());
    std::weak_ptr<Provider> wp = p;
    act->fn = [wp, itemId](HSTRING a, HSTRING d, HSTRING in) {
        if (auto p = wp.lock())
            DispatchInvoke(p, itemId, std::wstring(FromH(a)), std::wstring(FromH(d)), std::wstring(FromH(in)));
    };
    winrt::com_ptr<Pal::IPaletteTemplateContent> content;
    if (FAILED(f->CreateInstance(act.get(), content.put())) || !content) return nullptr;
    winrt::hstring ht(tmpl), hd(data);
    content->put_TemplateJson(H(ht));
    content->put_DataJson(H(hd));
    {
        std::lock_guard l(p->m);
        p->cards[itemId].content = content;
    }
    winrt::com_ptr<::IUnknown> asContent;
    content->QueryInterface(Pal::IID_IPaletteContent, asContent.put_void());
    Wh_Log(L"[%s] card content created for %s", p->id.c_str(), itemId.c_str());
    return asContent;
}

static winrt::com_ptr<Pal::IPaletteItemView> BuildItem(std::shared_ptr<Provider> const& p, wdj::JsonObject const& o,
                                                       int index) {
    winrt::com_ptr<Pal::IPaletteItemView> view;
    if (!p->factory) return view;
    std::wstring title = JStr(o, L"title");
    std::wstring id = JStr(o, L"id");
    if (id.empty()) id = L"item" + std::to_wstring(index) + L"-" + title;

    auto kindsV = JStrArr(o, L"kinds");
    if (kindsV.empty()) kindsV = p->itemKinds;
    auto kinds = MakeKinds(kindsV);

    winrt::com_ptr<Pal::IPaletteItem> item;
    winrt::hstring hid(id);
    HRESULT hr = kinds ? p->factory->Create2(H(hid), kinds.get(), item.put()) : p->factory->Create(H(hid), item.put());
    if (FAILED(hr) || !item) {
        Wh_Log(L"[%s] PaletteItemFactory.Create failed %08lx", p->id.c_str(), hr);
        return view;
    }

    winrt::hstring ht(title);
    item->put_Title(H(ht));

    auto subs = JStrArr(o, L"subtitle");
    if (!subs.empty()) {
        std::vector<wf::IInspectable> boxes;
        std::vector<IInspectable*> raw;
        for (auto& s : subs) boxes.push_back(winrt::box_value(winrt::hstring(s)));
        for (auto& b : boxes) raw.push_back((IInspectable*)winrt::get_abi(b));
        item->put_Subtitle((UINT32)raw.size(), raw.data());
    }

    if (o.HasKey(L"icon")) {
        if (auto icon = MakeIconJson(o.GetNamedValue(L"icon"))) item->put_Icon(icon.get());
    } else if (auto icon = MakeIcon(p->glyph, p->font, p->iconPath)) {
        item->put_Icon(icon.get());
    }
    if (o.HasKey(L"iconOverlay"))
        if (auto icon = MakeIconJson(o.GetNamedValue(L"iconOverlay"))) item->put_IconOverlay(icon.get());

    if (o.HasKey(L"tags") && o.GetNamedValue(L"tags").ValueType() == wdj::JsonValueType::Array) {
        auto* tf = CACHED_FACTORY(IPaletteTagFactory, L"Windows.UI.Palettes.PaletteTag");
        std::vector<winrt::com_ptr<Pal::IPaletteTag>> tags;
        for (auto tv : o.GetNamedArray(L"tags")) {
            if (!tf || tv.ValueType() != wdj::JsonValueType::Object) continue;
            auto to = tv.GetObject();
            winrt::com_ptr<Pal::IPaletteIconData> ti;
            if (to.HasKey(L"icon")) ti = MakeIconJson(to.GetNamedValue(L"icon"));
            winrt::hstring tt(JStr(to, L"text")), tip(JStr(to, L"tooltip"));
            winrt::com_ptr<Pal::IPaletteTag> tag;
            if (SUCCEEDED(tf->CreateInstance(ti.get(), H(tt), H(tip), tag.put()))) tags.push_back(tag);
        }
        std::vector<Pal::IPaletteTag*> raw;
        for (auto& t : tags) raw.push_back(t.get());
        if (!raw.empty()) item->put_Tags((UINT32)raw.size(), raw.data());
    }

    // Properties (score + arbitrary).
    winrt::com_ptr<IInspectable> propsRaw;
    if (SUCCEEDED(item->get_Properties(propsRaw.put())) && propsRaw) {
        wfc::IMap<winrt::hstring, wf::IInspectable> props{nullptr};
        winrt::copy_from_abi(props, propsRaw.get());
        try {
            if (o.HasKey(L"score")) props.Insert(L"Score", winrt::box_value(o.GetNamedNumber(L"score")));
            if (o.HasKey(L"properties") && o.GetNamedValue(L"properties").ValueType() == wdj::JsonValueType::Object)
                for (auto kv : o.GetNamedObject(L"properties")) props.Insert(kv.Key(), JsonToBoxed(kv.Value()));
        } catch (...) {
            Wh_Log(L"[%s] failed to set properties on %s", p->id.c_str(), id.c_str());
        }
    }

    // Action.
    std::wstring type = L"callback";
    wdj::JsonObject action{nullptr};
    if (o.HasKey(L"action") && o.GetNamedValue(L"action").ValueType() == wdj::JsonValueType::Object) {
        action = o.GetNamedObject(L"action");
        type = JStr(action, L"type", L"callback");
    }
    std::weak_ptr<Provider> wp = p;
    auto makeDelegateInvoker = [&](std::function<void(Pal::IPaletteQuery*)> fn) {
        winrt::com_ptr<Pal::IPaletteInvoker> inv;
        auto* df = CACHED_FACTORY(IPaletteDelegateInvokerFactory, L"Windows.UI.Palettes.PaletteDelegateInvoker");
        if (!df) return inv;
        winrt::com_ptr<InvokeDelegate> d;
        d.attach(new InvokeDelegate());
        d->fn = std::move(fn);
        winrt::com_ptr<IInspectable> di;
        if (SUCCEEDED(df->CreateInstance(d.get(), (Pal::IPaletteDelegateInvoker**)di.put())) && di)
            di->QueryInterface(Pal::IID_IPaletteInvoker, inv.put_void());
        return inv;
    };
    winrt::com_ptr<Pal::IPaletteInvoker> invoker;
    if (type == L"uri") {
        auto* uf = CACHED_FACTORY(IPaletteUriInvokerFactory, L"Windows.UI.Palettes.PaletteUriInvoker");
        try {
            wf::Uri uri(JStr(action, L"uri"));
            winrt::com_ptr<IInspectable> ui;
            if (uf && SUCCEEDED(uf->CreateInstance((IInspectable*)winrt::get_abi(uri), (Pal::IPaletteUriInvoker**)ui.put())) && ui)
                ui->QueryInterface(Pal::IID_IPaletteInvoker, invoker.put_void());
        } catch (...) {
            Wh_Log(L"[%s] bad uri on %s", p->id.c_str(), id.c_str());
        }
    } else if (type == L"run") {
        std::wstring cmd = JStr(action, L"command"), args = JStr(action, L"args"), dir = JStr(action, L"cwd");
        invoker = makeDelegateInvoker([cmd, args, dir](Pal::IPaletteQuery*) {
            Submit([cmd, args, dir] { RunCommand(cmd, args, dir); });
        });
    } else if (type == L"copy") {
        std::wstring text = JStr(action, L"text", title);
        invoker = makeDelegateInvoker([text](Pal::IPaletteQuery*) { Submit([text] { CopyToClipboard(text); }); });
    } else if (type == L"toggle") {
        bool on = action.HasKey(L"isToggled") && action.GetNamedBoolean(L"isToggled");
        auto inner = makeDelegateInvoker([wp, id, on](Pal::IPaletteQuery*) {
            if (auto p = wp.lock()) DispatchInvoke(p, id, L"toggle", on ? L"false" : L"true", L"");
        });
        auto* tf = CACHED_FACTORY(IPaletteToggleInvokerFactory, L"Windows.UI.Palettes.PaletteToggleInvoker");
        winrt::com_ptr<IInspectable> ti;
        if (tf && inner && SUCCEEDED(tf->CreateInstance(inner.get(), on, (Pal::IPaletteToggleInvoker**)ti.put())) && ti)
            ti->QueryInterface(Pal::IID_IPaletteInvoker, invoker.put_void());
    } else if (type != L"none") {
        invoker = makeDelegateInvoker([wp, id](Pal::IPaletteQuery*) {
            if (auto p = wp.lock()) DispatchInvoke(p, id, L"invoke", L"", L"");
        });
    }
    if (invoker) item->put_Invoker(invoker.get());

    // Card.
    if (o.HasKey(L"card") && o.GetNamedValue(L"card").ValueType() == wdj::JsonValueType::Object) {
        auto card = o.GetNamedObject(L"card");
        std::wstring tmpl = JStr(card, L"template"), data = JStr(card, L"data", L"{}");
        {
            std::lock_guard l(p->m);
            auto& cs = p->cards[id];
            cs.templateJson = tmpl;
            cs.dataJson = data;
            cs.content = nullptr;
        }
        winrt::com_ptr<ContentDelegate> cd;
        cd.attach(new ContentDelegate());
        cd->fn = [wp](winrt::hstring const& itemId) -> winrt::com_ptr<::IUnknown> {
            if (auto p = wp.lock()) return MakeCardContent(p, std::wstring(itemId));
            return nullptr;
        };
        item->put_ContentDelegate(cd.get());
    }

    if (FAILED(item->GetView(view.put()))) view = nullptr;
    return view;
}

struct BuiltItem {
    std::wstring id;
    winrt::com_ptr<Pal::IPaletteItemView> view;
};

static winrt::com_ptr<Pal::IPaletteItemView> FindItemView(Provider* p, std::wstring const& id) {
    std::lock_guard l(p->m);
    if (auto it = p->resultCache.find(id); it != p->resultCache.end()) return it->second;
    if (auto it = p->staticCache.find(id); it != p->staticCache.end()) return it->second;
    return nullptr;
}

// Drop card state for items that are in neither cache any more. Caller holds p->m.
static void PruneCards(Provider* p) {
    std::erase_if(p->cards, [&](auto const& kv) {
        return !p->resultCache.contains(kv.first) && !p->staticCache.contains(kv.first);
    });
}

static std::vector<BuiltItem> BuildItems(std::shared_ptr<Provider> const& p, std::wstring const& json) {
    std::vector<BuiltItem> out;
    try {
        auto v = wdj::JsonValue::Parse(json);
        wdj::JsonArray arr{nullptr};
        if (v.ValueType() == wdj::JsonValueType::Array)
            arr = v.GetArray();
        else if (v.ValueType() == wdj::JsonValueType::Object && v.GetObject().HasKey(L"items"))
            arr = v.GetObject().GetNamedArray(L"items");
        else if (v.ValueType() == wdj::JsonValueType::Object) {
            arr = wdj::JsonArray();
            arr.Append(v);
        }
        int i = 0;
        if (arr)
            for (auto e : arr) {
                if (e.ValueType() == wdj::JsonValueType::Object) {
                    auto o = e.GetObject();
                    std::wstring id = JStr(o, L"id");
                    if (id.empty()) id = L"item" + std::to_wstring(i) + L"-" + JStr(o, L"title");
                    if (auto view = BuildItem(p, o, i)) out.push_back({id, view});
                }
                i++;
            }
    } catch (winrt::hresult_error const& e) {
        Wh_Log(L"[%s] bad items JSON: %s", p->id.c_str(), e.message().c_str());
    } catch (...) {
        Wh_Log(L"[%s] bad items JSON", p->id.c_str());
    }
    return out;
}

// ---------------------------------------------------------------------------
// Queries
// ---------------------------------------------------------------------------
struct Query {
    UINT64 handle;
    std::weak_ptr<Provider> provider;
    winrt::com_ptr<Pal::IPaletteQuery> query;
    winrt::com_ptr<ResultsOp> op;
    std::mutex m;
    std::vector<winrt::com_ptr<Pal::IPaletteItemView>> items;
    std::atomic<PTP_TIMER> timer{nullptr};
    std::atomic<bool> finished{false};
};
static std::mutex g_queriesLock;
[[clang::no_destroy]] static std::map<UINT64, std::shared_ptr<Query>> g_queries;  // emptied at uninit
static std::atomic<UINT64> g_nextQuery{1};

// Remove the query from the table and stop its timeout. Exactly one caller gets true.
static bool RetireQuery(std::shared_ptr<Query> const& q) {
    if (q->finished.exchange(true)) return false;
    {
        std::lock_guard l(g_queriesLock);
        g_queries.erase(q->handle);
    }
    if (PTP_TIMER t = q->timer.exchange(nullptr)) {
        SetThreadpoolTimer(t, nullptr, 0, 0);
        CloseThreadpoolTimer(t);  // safe from inside its own callback too
    }
    return true;
}

static void FinishQuery(std::shared_ptr<Query> const& q, const wchar_t* why) {
    if (!RetireQuery(q)) return;
    std::vector<winrt::com_ptr<Pal::IPaletteItemView>> items;
    {
        std::lock_guard l(q->m);
        items = q->items;
    }
    winrt::com_ptr<Pal::IPaletteQueryResults> results;
    auto* rf = CACHED_FACTORY(IPaletteQueryResultsFactory, L"Windows.UI.Palettes.PaletteQueryResults");
    HRESULT hr = rf ? rf->CreateInstance(q->query.get(), results.put()) : E_FAIL;
    winrt::com_ptr<Pal::IPaletteQueryResultsView> view;
    if (SUCCEEDED(hr) && results) {
        std::vector<Pal::IPaletteItemView*> raw;
        for (auto& i : items) raw.push_back(i.get());
        results->put_Items((UINT32)raw.size(), raw.data());
        results->put_HasMoreItems(false);
        hr = results->GetView(view.put());
    }
    auto p = q->provider.lock();
    Wh_Log(L"[%s] query %llu finished (%s): %zu items, hr=%08lx", p ? p->id.c_str() : L"?", q->handle, why,
        items.size(), hr);
    if (view)
        q->op->Finish(AS_Completed, view.get());
    else
        q->op->Finish(AS_Error, nullptr, FAILED(hr) ? hr : E_FAIL);
}

static std::shared_ptr<Query> FindQuery(UINT64 h) {
    std::lock_guard l(g_queriesLock);
    auto it = g_queries.find(h);
    return it == g_queries.end() ? nullptr : it->second;
}

static bool SubmitResults(UINT64 qh, std::wstring const& json, bool done) {
    auto q = FindQuery(qh);
    if (!q) return false;
    auto p = q->provider.lock();
    if (!p) return false;
    auto built = BuildItems(p, json);
    {
        // Keep only the newest query's items for lookups (and their cards).
        std::lock_guard l(p->m);
        if (q->handle > p->resultQuery) {
            p->resultQuery = q->handle;
            p->resultCache.clear();
        }
        if (q->handle == p->resultQuery)
            for (auto& b : built) p->resultCache[b.id] = b.view;
        PruneCards(p.get());
    }
    std::vector<winrt::com_ptr<Pal::IPaletteItemView>> items;
    for (auto& b : built) items.push_back(b.view);
    {
        std::lock_guard l(q->m);
        for (auto& i : items) q->items.push_back(i);
    }
    if (done) {
        FinishQuery(q, L"done");
    } else if (!items.empty()) {
        // Stream partial results as progress.
        auto* pf = CACHED_FACTORY(IPaletteQueryProgressFactory, L"Windows.UI.Palettes.PaletteQueryProgress");
        if (pf) {
            std::vector<Pal::IPaletteItemView*> raw;
            for (auto& i : items) raw.push_back(i.get());
            winrt::com_ptr<IInspectable> prog;
            if (SUCCEEDED(pf->CreateInstance((UINT32)raw.size(), raw.data(), nullptr, (Pal::IPaletteQueryProgress**)prog.put())) && prog)
                q->op->ReportProgress(prog.get());
        }
    }
    return true;
}

static std::wstring QueryText(Pal::IPaletteQuery* q, std::wstring& jsonOut) {
    std::wstring text;
    winrt::hstring json;
    if (SUCCEEDED(q->ToJson((HSTRING*)winrt::put_abi(json)))) jsonOut = std::wstring(json);
    UINT32 n = 0;
    IInspectable** args = nullptr;
    if (SUCCEEDED(q->get_Arguments(&n, &args)) && args) {
        for (UINT32 i = 0; i < n; i++) {
            if (!args[i]) continue;
            if (text.empty()) {
                wf::IInspectable a{nullptr};
                winrt::copy_from_abi(a, args[i]);
                if (auto pv = a.try_as<wf::IPropertyValue>(); pv && pv.Type() == wf::PropertyType::String)
                    text = std::wstring(pv.GetString());
            }
            args[i]->Release();
        }
        CoTaskMemFree(args);
    }
    if (text.empty() && !jsonOut.empty()) {
        // Fall back to the JSON form: first string inside "Arguments"/"arguments", else "Query"/"text".
        try {
            auto o = wdj::JsonObject::Parse(jsonOut);
            for (auto k : {L"Arguments", L"arguments"}) {
                if (o.HasKey(k) && o.GetNamedValue(k).ValueType() == wdj::JsonValueType::Array)
                    for (auto e : o.GetNamedArray(k)) {
                        if (e.ValueType() == wdj::JsonValueType::String) return std::wstring(e.GetString());
                        if (e.ValueType() == wdj::JsonValueType::Object) {
                            auto eo = e.GetObject();
                            for (auto kk : {L"Value", L"value", L"Text", L"text"})
                                if (eo.HasKey(kk) && eo.GetNamedValue(kk).ValueType() == wdj::JsonValueType::String)
                                    return std::wstring(eo.GetNamedString(kk));
                        }
                    }
            }
            for (auto k : {L"Query", L"query", L"Text", L"text"})
                if (o.HasKey(k) && o.GetNamedValue(k).ValueType() == wdj::JsonValueType::String)
                    return std::wstring(o.GetNamedString(k));
        } catch (...) {
        }
    }
    return text;
}

static VOID CALLBACK QueryTimeout(PTP_CALLBACK_INSTANCE, PVOID ctx, PTP_TIMER) {
    if (auto q = FindQuery((UINT64)(UINT_PTR)ctx)) FinishQuery(q, L"timeout");
}

static IAsyncOpWithProgressRaw* StartQuery(std::shared_ptr<Provider> const& p, Pal::IPaletteQuery* pq) {
    auto q = std::make_shared<Query>();
    q->handle = g_nextQuery++;
    q->provider = p;
    q->query.copy_from(pq);
    q->op.attach(new ResultsOp());
    std::weak_ptr<Query> wq = q;
    q->op->onCancel = [wq] {
        // Called on Windows' thread: retire here, tell the provider from the pool.
        auto q = wq.lock();
        if (!q || !RetireQuery(q)) return;
        if (auto p = q->provider.lock(); p && p->cb.onCancel) {
            UINT64 h = q->handle;
            Submit([p, h] { CallProvider(p.get(), [&] { p->cb.onCancel(h); }); });
        }
    };
    std::wstring json;
    std::wstring text = QueryText(pq, json);
    INT32 mode = 0;
    pq->get_Mode(&mode);
    Wh_Log(L"[%s] query %llu mode=%d text='%s' json=%s", p->id.c_str(), q->handle, mode, text.c_str(), json.c_str());

    IAsyncOpWithProgressRaw* ret = static_cast<IAsyncOpWithProgressRaw*>(q->op.get());
    ret->AddRef();

    bool started = false;
    if (p->cb.onQuery && (!text.empty() || p->zeroState)) {
        // Arm the timeout before the query is visible to anything that could retire it.
        if (PTP_TIMER t = PoolTimer(QueryTimeout, (void*)(UINT_PTR)q->handle)) {
            q->timer = t;
            ArmTimer(t, g_settings.queryTimeoutMs);
        }
        std::lock_guard l(g_queriesLock);
        if (g_alive) {  // checked under the lock uninit takes before draining the table
            g_queries[q->handle] = q;
            started = true;
        }
    }
    if (!started) {
        FinishQuery(q, L"skipped");  // also closes the timer
        return ret;
    }
    // Never run provider code on Windows' thread.
    UINT64 h = q->handle;
    if (!Submit([p, h, text, json] { CallProvider(p.get(), [&] { p->cb.onQuery(h, text, json); }); }))
        FinishQuery(q, L"shutting down");
    return ret;
}

// ---------------------------------------------------------------------------
// IPaletteItemSourceExtension
// ---------------------------------------------------------------------------
struct Extension final : LiveToken, Pal::IPaletteItemSourceExtension {
    IMPL_REFCOUNT
    IMPL_INSPECTABLE(L"Windhawk.SearchMenuExtensions.ItemSource")
    std::weak_ptr<Provider> provider;
    winrt::com_ptr<Pal::IPaletteExtensionHost> host;
    std::mutex m;
    std::map<INT64, winrt::com_ptr<::IUnknown>> handlers;
    INT64 nextToken = 1;

    explicit Extension(std::shared_ptr<Provider> const& p) : provider(p) {
        std::lock_guard l(p->m);
        p->extensions.push_back(this);
    }
    ~Extension() {
        if (auto p = provider.lock()) {
            std::lock_guard l(p->m);
            std::erase(p->extensions, this);
        }
    }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if (riid == IID_IUnknown || riid == kIID_IInspectable || riid == Pal::IID_IPaletteItemSourceExtension ||
            riid == IID_IAgileObject_) {
            *ppv = static_cast<Pal::IPaletteItemSourceExtension*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    HRESULT STDMETHODCALLTYPE Initialize(Pal::IPaletteItemFactory*, Pal::IPaletteExtensionHost* h) override {
        host.copy_from(h);
        auto p = provider.lock();
        Wh_Log(L"[%s] Initialize", p ? p->id.c_str() : L"?");
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetItem(HSTRING id, Pal::IPaletteItemView** result) override {
        *result = nullptr;
        auto p = provider.lock();
        if (!p) return S_OK;
        std::wstring k(FromH(id));
        Wh_Log(L"[%s] GetItem %s", p->id.c_str(), k.c_str());
        if (auto v = FindItemView(p.get(), k)) v.copy_to(result);
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetItems(UINT32* n, Pal::IPaletteItemView*** result) override {
        *n = 0;
        *result = nullptr;
        auto p = provider.lock();
        if (!p || !g_alive) return S_OK;
        std::lock_guard l(p->m);
        Wh_Log(L"[%s] GetItems -> %zu", p->id.c_str(), p->staticItems.size());
        if (p->staticItems.empty()) return S_OK;
        auto** arr = (Pal::IPaletteItemView**)CoTaskMemAlloc(sizeof(void*) * p->staticItems.size());
        if (!arr) return E_OUTOFMEMORY;
        for (size_t i = 0; i < p->staticItems.size(); i++) p->staticItems[i].copy_to(&arr[i]);
        *n = (UINT32)p->staticItems.size();
        *result = arr;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE ExecuteQueryAsync(Pal::IPaletteQuery* query, IInspectable** operation) override {
        auto p = provider.lock();
        if (!p) {
            auto* op = new ResultsOp();
            op->Finish(AS_Error, nullptr, E_ABORT);
            *operation = static_cast<IAsyncOpWithProgressRaw*>(op);
            return S_OK;
        }
        *operation = StartQuery(p, query);
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE ReportItemAnalyticsAsync(UINT32, Pal::IPaletteItemAnalytics**, IInspectable** op) override {
        *op = static_cast<IAsyncActionRaw*>(new DoneAction());
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE ReportConversationAnalyticsAsync(UINT32, Pal::IPaletteItemAnalytics**, IInspectable** op) override {
        *op = static_cast<IAsyncActionRaw*>(new DoneAction());
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE add_ItemsChanged(IInspectable* handler, INT64* token) override {
        std::lock_guard l(m);
        *token = nextToken++;
        handlers[*token].copy_from((::IUnknown*)handler);
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE remove_ItemsChanged(INT64 token) override {
        std::lock_guard l(m);
        handlers.erase(token);
        return S_OK;
    }
    void RaiseItemsChanged() {
        std::vector<winrt::com_ptr<::IUnknown>> hs;
        {
            std::lock_guard l(m);
            for (auto& [k, h] : handlers) hs.push_back(h);
        }
        for (auto& h : hs) ((IHandler2Ptr*)h.get())->Invoke(static_cast<Pal::IPaletteItemSourceExtension*>(this), nullptr);
    }
};

// ---------------------------------------------------------------------------
// IPaletteExtensionDefinition
// ---------------------------------------------------------------------------
struct Definition final : LiveToken, Pal::IPaletteExtensionDefinition {
    IMPL_REFCOUNT
    IMPL_INSPECTABLE(L"Windows.UI.Palettes.PaletteExtensionDefinition")
    std::weak_ptr<Provider> provider;
    winrt::hstring extId, name, description;
    winrt::com_ptr<Pal::IPaletteItemFactory> factory;
    winrt::com_ptr<Pal::IPaletteKinds> kinds, contained;
    winrt::com_ptr<Pal::IPaletteIconData> icon;
    wfc::IMap<winrt::hstring, wf::IInspectable> props = wfc::PropertySet();  // Windows-implemented, no code of ours

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if (riid == IID_IUnknown || riid == kIID_IInspectable || riid == Pal::IID_IPaletteExtensionDefinition ||
            riid == IID_IAgileObject_) {
            *ppv = static_cast<Pal::IPaletteExtensionDefinition*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    HRESULT STDMETHODCALLTYPE CreateExtension(Pal::IPaletteExtensionHost* host, Pal::IPaletteItemSourceExtension** result) override {
        *result = nullptr;
        auto p = provider.lock();
        Wh_Log(L"CreateExtension for %s (provider %s)", extId.c_str(), p ? L"alive" : L"gone");
        if (!p || !g_alive) return E_ABORT;
        auto* e = new Extension(p);
        e->host.copy_from(host);
        *result = e;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE get_ItemFactory(Pal::IPaletteItemFactory** v) override { factory.copy_to(v); return S_OK; }
    HRESULT STDMETHODCALLTYPE get_ExtensionId(HSTRING* v) override { return DupH(extId, v); }
    HRESULT STDMETHODCALLTYPE get_PackageFamilyName(HSTRING* v) override { return DupH(L"Windhawk.SearchMenuExtensions_windhawk", v); }
    HRESULT STDMETHODCALLTYPE get_ApplicationId(HSTRING* v) override { return DupH(L"Windhawk.SearchMenuExtensions_windhawk!App", v); }
    HRESULT STDMETHODCALLTYPE get_Icon(Pal::IPaletteIconData** v) override { icon.copy_to(v); return S_OK; }
    HRESULT STDMETHODCALLTYPE get_DisplayName(HSTRING* v) override { return DupH(name, v); }
    HRESULT STDMETHODCALLTYPE get_Description(HSTRING* v) override { return DupH(description, v); }
    HRESULT STDMETHODCALLTYPE get_AcceptedKindSpecs(UINT32* n, HSTRING** v) override { *n = 0; *v = nullptr; return S_OK; }
    HRESULT STDMETHODCALLTYPE get_Publisher(HSTRING* v) override { return DupH(L"windhawk", v); }
    HRESULT STDMETHODCALLTYPE get_Properties(IInspectable** v) override {
        *v = (IInspectable*)winrt::detach_abi(wfc::IMap<winrt::hstring, wf::IInspectable>(props));
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE get_Kinds(Pal::IPaletteKinds** v) override { kinds.copy_to(v); return S_OK; }
    HRESULT STDMETHODCALLTYPE get_ContainedKinds(Pal::IPaletteKinds** v) override { contained.copy_to(v); return S_OK; }
};

// ---------------------------------------------------------------------------
// Catalog hooks. The functions behind the vtable slots of PaletteCatalog (in OSClient.API.dll)
// are hooked through Windhawk. Each hook checks that `self` really is the catalog class it was
// set up for, so a shared (identical-code-folded) function on another class is passed through.
// ---------------------------------------------------------------------------
using GetPalette_t = HRESULT(STDMETHODCALLTYPE*)(void*, HSTRING, void**);
using GetPalettes_t = HRESULT(STDMETHODCALLTYPE*)(void*, UINT32*, void***);
using AddChanged_t = HRESULT(STDMETHODCALLTYPE*)(void*, ::IUnknown*, INT64*);
using RemoveChanged_t = HRESULT(STDMETHODCALLTYPE*)(void*, INT64);
using OpenDefault_t = HRESULT(STDMETHODCALLTYPE*)(void*, void**);
using TryGetItem_t = HRESULT(STDMETHODCALLTYPE*)(void*, HSTRING, HSTRING, void**);
using GetOrdered_t = void(STDMETHODCALLTYPE*)(void*, HSTRING, UINT32*, void***);
using IsEnabled_t = BOOL(STDMETHODCALLTYPE*)(void*, HSTRING);

static GetPalette_t o_GetPalette;
static GetPalettes_t o_GetPalettes;
static AddChanged_t o_AddChanged;
static RemoveChanged_t o_RemoveChanged;
static TryGetItem_t o_TryGetItem;
static GetOrdered_t o_GetOrdered;
static IsEnabled_t o_IsEnabled;
static IsEnabled_t o_IsUserDisabled;

// vtables of the objects the hooks were set up for
static void* g_staticsVt;
static void* g_catalogVt;
static void* g_privateVt;
static inline bool Is(void* self, void* vt) { return vt && self && *(void**)self == vt; }

struct ChangedSub {
    winrt::com_ptr<::IUnknown> catalog;  // IPaletteCatalog*
    winrt::com_ptr<::IUnknown> handler;
    INT64 token;
};
static std::mutex g_subsLock;
[[clang::no_destroy]] static std::vector<ChangedSub> g_subs;  // emptied at uninit

static std::vector<winrt::com_ptr<Definition>> DefinitionsFor(const std::wstring* kind) {
    std::vector<winrt::com_ptr<Definition>> r;
    if (!g_alive) return r;
    for (auto& p : ProvidersSnapshot()) {
        if (!p->def) continue;
        if (kind) {
            bool ok = false;
            for (auto& k : p->kinds)
                if (KindMatches(k, *kind)) ok = true;
            if (!ok) continue;
        }
        r.push_back(p->def);
    }
    return r;
}

static void AppendDefs(UINT32* n, void*** arr, std::vector<winrt::com_ptr<Definition>> const& defs) {
    if (defs.empty()) return;
    UINT32 old = *n;
    void** na = (void**)CoTaskMemAlloc(sizeof(void*) * (old + defs.size()));
    if (!na) return;
    if (old && *arr) memcpy(na, *arr, sizeof(void*) * old);
    for (size_t i = 0; i < defs.size(); i++) {
        defs[i]->AddRef();
        na[old + i] = static_cast<Pal::IPaletteExtensionDefinition*>(defs[i].get());
    }
    if (*arr) CoTaskMemFree(*arr);
    *arr = na;
    *n = old + (UINT32)defs.size();
}

static HRESULT STDMETHODCALLTYPE h_GetPalettes(void* self, UINT32* n, void*** arr) {
    HRESULT hr = o_GetPalettes(self, n, arr);
    if (SUCCEEDED(hr) && Is(self, g_catalogVt)) {
        UINT32 before = *n;
        AppendDefs(n, arr, DefinitionsFor(nullptr));
        Wh_Log(L"GetPalettes: %u native + %u ours", before, *n - before);
    }
    return hr;
}
static HRESULT STDMETHODCALLTYPE h_GetPalette(void* self, HSTRING id, void** result) {
    if (Is(self, g_catalogVt) && g_alive)
        if (auto p = FindProviderByExtId(FromH(id)); p && p->def) {
            Wh_Log(L"GetPalette(%s) -> ours", p->extId.c_str());
            p->def->AddRef();
            *result = static_cast<Pal::IPaletteExtensionDefinition*>(p->def.get());
            return S_OK;
        }
    return o_GetPalette(self, id, result);
}
static HRESULT STDMETHODCALLTYPE h_AddChanged(void* self, ::IUnknown* handler, INT64* token) {
    HRESULT hr = o_AddChanged(self, handler, token);
    if (SUCCEEDED(hr) && handler && g_alive && Is(self, g_catalogVt)) {
        ChangedSub s;
        s.catalog.copy_from((::IUnknown*)self);
        s.handler.copy_from(handler);
        s.token = *token;
        std::lock_guard l(g_subsLock);
        g_subs.push_back(std::move(s));
    }
    return hr;
}
static HRESULT STDMETHODCALLTYPE h_RemoveChanged(void* self, INT64 token) {
    if (Is(self, g_catalogVt)) {
        std::lock_guard l(g_subsLock);
        std::erase_if(g_subs, [&](ChangedSub const& s) { return s.catalog.get() == self && s.token == token; });
    }
    return o_RemoveChanged(self, token);
}
static void STDMETHODCALLTYPE h_GetOrdered(void* self, HSTRING kind, UINT32* n, void*** arr) {
    o_GetOrdered(self, kind, n, arr);
    if (!Is(self, g_privateVt)) return;
    std::wstring k(FromH(kind));
    UINT32 before = *n;
    AppendDefs(n, arr, DefinitionsFor(&k));
    Wh_Log(L"GetOrderedSourceDefinitions(%s): %u native + %u ours", k.c_str(), before, *n - before);
}
static BOOL STDMETHODCALLTYPE h_IsEnabled(void* self, HSTRING id) {
    if (Is(self, g_privateVt) && g_alive && FindProviderByExtId(FromH(id))) return TRUE;
    return o_IsEnabled(self, id);
}
static BOOL STDMETHODCALLTYPE h_IsUserDisabled(void* self, HSTRING id) {
    if (Is(self, g_privateVt) && g_alive && FindProviderByExtId(FromH(id))) return FALSE;
    return o_IsUserDisabled(self, id);
}
static HRESULT STDMETHODCALLTYPE h_TryGetItem(void* self, HSTRING ext, HSTRING item, void** result) {
    if (Is(self, g_staticsVt) && g_alive)
        if (auto p = FindProviderByExtId(FromH(ext))) {
            *result = nullptr;
            if (auto v = FindItemView(p.get(), std::wstring(FromH(item)))) v.copy_to((Pal::IPaletteItemView**)result);
            return S_OK;
        }
    return o_TryGetItem(self, ext, item, result);
}

// Typed so the hook, the original and the target must share one signature.
template <class F>
static bool Hook(void* target, F hook, F* orig, const wchar_t* name) {
    if (!target || !Wh_SetFunctionHook(target, (void*)hook, (void**)orig)) {
        Wh_Log(L"failed to hook %s", name);
        return false;
    }
    return true;
}

// Called from Wh_ModInit: everything is in place when Windhawk applies the hooks after it returns.
static bool SetCatalogHooks() {
    HMODULE mod = LoadLibraryExW(L"OSClient.API.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!mod) {
        Wh_Log(L"OSClient.API.dll not found - this build has no Windows.UI.Palettes");
        return false;
    }
    using DGAF = HRESULT(WINAPI*)(HSTRING, ::IUnknown**);
    auto dgaf = (DGAF)GetProcAddress(mod, "DllGetActivationFactory");
    if (!dgaf) return false;
    winrt::hstring cls = L"Windows.UI.Palettes.PaletteCatalog";
    winrt::com_ptr<::IUnknown> f;
    HRESULT hr = dgaf(H(cls), f.put());
    winrt::com_ptr<Pal::IPaletteCatalogStatics> statics;
    if (FAILED(hr) || !f || FAILED(f->QueryInterface(Pal::IID_IPaletteCatalogStatics, statics.put_void())) || !statics) {
        Wh_Log(L"PaletteCatalog statics unavailable (%08lx)", hr);
        return false;
    }
    // One catalog instance, to find the catalog class's methods.
    void** svt = *(void***)statics.get();
    winrt::com_ptr<::IUnknown> catalog, priv;
    hr = ((OpenDefault_t)svt[10])(statics.get(), catalog.put_void());
    if (FAILED(hr) || !catalog) {
        Wh_Log(L"PaletteCatalog.OpenDefault failed %08lx", hr);
        return false;
    }
    if (FAILED(catalog->QueryInterface(IID_IPaletteCatalogPrivate, priv.put_void())) || !priv) {
        Wh_Log(L"IPaletteCatalogPrivate missing - unsupported build");
        return false;
    }
    void** cvt = *(void***)catalog.get();
    void** pvt = *(void***)priv.get();
    g_staticsVt = svt;
    g_catalogVt = cvt;
    g_privateVt = pvt;
    bool ok = Hook(svt[14], h_TryGetItem, &o_TryGetItem, L"TryGetItem") &&
              Hook(cvt[6], h_GetPalette, &o_GetPalette, L"GetPalette") &&
              Hook(cvt[7], h_GetPalettes, &o_GetPalettes, L"GetPalettes") &&
              Hook(cvt[8], h_AddChanged, &o_AddChanged, L"add_Changed") &&
              Hook(cvt[9], h_RemoveChanged, &o_RemoveChanged, L"remove_Changed") &&
              Hook(pvt[7], h_GetOrdered, &o_GetOrdered, L"GetOrderedSourceDefinitions") &&
              Hook(pvt[8], h_IsEnabled, &o_IsEnabled, L"IsEnabled") &&
              Hook(pvt[9], h_IsUserDisabled, &o_IsUserDisabled, L"IsUserDisabled");
    if (ok) Wh_Log(L"catalog hooks set");
    return ok;
}

// Ask Windows to re-read the catalog (raises PaletteCatalog.Changed on every subscriber we saw).
static void RaiseCatalogChanged() {
    std::vector<ChangedSub> subs;
    {
        std::lock_guard l(g_subsLock);
        subs = g_subs;
    }
    Wh_Log(L"raising PaletteCatalog.Changed to %zu subscribers", subs.size());
    for (auto& s : subs) {
        try {
            ((IHandler2Ptr*)s.handler.get())->Invoke(s.catalog.get(), nullptr);
        } catch (...) {
        }
    }
}
static std::atomic<PTP_TIMER> g_changeTimer{nullptr};  // debounce; a pool timer, closed with the pool
static VOID CALLBACK ChangeTimerCb(PTP_CALLBACK_INSTANCE, PVOID, PTP_TIMER) { RaiseCatalogChanged(); }
static void ScheduleCatalogChanged() {
    if (PTP_TIMER t = g_changeTimer.load(); t && g_alive) ArmTimer(t, 300);  // re-arming restarts the delay
}

// ---------------------------------------------------------------------------
// Provider registration
// ---------------------------------------------------------------------------
static winrt::com_ptr<Pal::IPaletteItemFactory> MakeFactory(winrt::hstring const& extId) {
    winrt::com_ptr<Pal::IPaletteItemFactory> out;
    auto* um = CACHED_FACTORY(IValueUnmarshalByPropertySet, L"Windows.UI.Palettes.PaletteItemFactory");
    if (!um) return out;
    wfc::PropertySet ps;
    ps.Insert(L"ExtensionId", winrt::box_value(extId));
    HRESULT hr = um->UnmarshalObjectFromPropertySet(winrt::get_abi(ps), out.put_void());
    if (FAILED(hr)) Wh_Log(L"PaletteItemFactory unmarshal failed %08lx", hr);
    return out;
}

static bool ValidId(std::wstring const& id) {
    if (id.empty() || id.size() > 64) return false;
    for (wchar_t c : id)
        if (!(iswalnum(c) || c == L'.' || c == L'_' || c == L'-')) return false;
    return true;
}

static UINT64 RegisterProvider(std::wstring const& json, ProviderCallbacks cb, std::wstring* err) {
    if (!g_alive) {
        if (err) *err = L"library not running";
        return 0;
    }
    auto p = std::make_shared<Provider>();
    try {
        auto o = wdj::JsonObject::Parse(json);
        p->id = JStr(o, L"id");
        p->name = JStr(o, L"name", p->id);
        p->description = JStr(o, L"description");
        p->glyph = JStr(o, L"glyph");
        p->font = JStr(o, L"font");
        p->iconPath = JStr(o, L"icon");
        p->zeroState = o.HasKey(L"zeroState") && o.GetNamedValue(L"zeroState").ValueType() == wdj::JsonValueType::Boolean &&
                       o.GetNamedBoolean(L"zeroState");
        p->kinds = JStrArr(o, L"kinds");
        p->containedKinds = JStrArr(o, L"containedKinds");
        p->itemKinds = JStrArr(o, L"itemKinds");
    } catch (...) {
        if (err) *err = L"provider JSON is invalid";
        return 0;
    }
    if (!ValidId(p->id)) {
        if (err) *err = L"provider id must be 1-64 chars of [A-Za-z0-9._-]";
        return 0;
    }
    if (p->kinds.empty()) p->kinds = {L"QueryResult.SystemComposer"};
    if (p->zeroState) p->kinds.push_back(L"ZeroState.SystemComposer");
    if (p->itemKinds.empty()) p->itemKinds = {L"Command"};
    if (p->containedKinds.empty()) p->containedKinds = p->itemKinds;
    p->extId = L"Windhawk." + p->id;
    p->cb = std::move(cb);

    {
        std::lock_guard l(g_providersLock);
        for (auto& q : g_providers)
            if (q->id == p->id) {
                if (err) *err = L"provider id already registered";
                return 0;
            }
    }

    winrt::hstring ext(p->extId);
    p->factory = MakeFactory(ext);
    if (!p->factory) {
        if (err) *err = L"could not create PaletteItemFactory";
        return 0;
    }
    winrt::com_ptr<Definition> def;
    def.attach(new Definition());
    def->provider = p;
    def->extId = ext;
    def->name = winrt::hstring(p->name);
    def->description = winrt::hstring(p->description);
    def->factory = p->factory;
    std::vector<std::wstring> kinds = p->kinds;
    kinds.push_back(L"BuiltIn");  // skips the "new provider" delayed-enablement gate
    def->kinds = MakeKinds(kinds);
    def->contained = MakeKinds(p->containedKinds);
    def->icon = MakeIcon(p->glyph.empty() && p->iconPath.empty() ? std::wstring(L"") : p->glyph, p->font,
                         p->iconPath);
    p->def = def;
    p->handle = g_nextHandle++;
    {
        std::lock_guard l(g_providersLock);
        if (!g_alive) {  // unloading started after the check above
            if (err) *err = L"library not running";
            return 0;
        }
        g_providers.push_back(p);
    }
    Wh_Log(L"provider registered: %s (%s) handle=%llu", p->id.c_str(), p->extId.c_str(), p->handle);
    ScheduleCatalogChanged();
    return p->handle;
}

// Waits for the provider's callbacks that are already running (see CloseProviderCallbacks), so a
// client may unload right after this returns.
static bool UnregisterProvider(UINT64 h, bool notifyDetach = false) {
    std::shared_ptr<Provider> p;
    {
        std::lock_guard l(g_providersLock);
        for (auto it = g_providers.begin(); it != g_providers.end(); ++it)
            if ((*it)->handle == h) {
                p = *it;
                g_providers.erase(it);
                break;
            }
    }
    if (!p) return false;
    // Fail any of its pending queries.
    std::vector<std::shared_ptr<Query>> qs;
    {
        std::lock_guard l(g_queriesLock);
        for (auto& [k, q] : g_queries)
            if (q->provider.lock() == p) qs.push_back(q);
    }
    for (auto& q : qs) FinishQuery(q, L"unregistered");
    CloseProviderCallbacks(p.get());
    Wh_Log(L"provider unregistered: %s", p->id.c_str());
    if (notifyDetach && p->cb.onDetach) p->cb.onDetach();
    {
        // Let go of the native items (and the delegates of ours they hold) right away.
        std::lock_guard l(p->m);
        p->staticItems.clear();
        p->staticCache.clear();
        p->resultCache.clear();
        p->cards.clear();
    }
    ScheduleCatalogChanged();
    return true;
}

static bool NotifyChanged(UINT64 h) {
    auto p = FindProvider(h);
    if (!p) return false;
    std::vector<winrt::com_ptr<Extension>> exts;
    {
        std::lock_guard l(p->m);
        for (auto* e : p->extensions) {
            // Only take a reference while Windows still holds one: an Extension at 0 is already in
            // its destructor (waiting for p->m) and must not be revived.
            ULONG r = e->m_ref.load();
            while (r && !e->m_ref.compare_exchange_weak(r, r + 1)) {
            }
            if (r) {
                winrt::com_ptr<Extension> c;
                c.attach(e);
                exts.push_back(std::move(c));
            }
        }
    }
    for (auto& e : exts) e->RaiseItemsChanged();
    return true;
}

static bool SetStaticItems(UINT64 h, std::wstring const& json) {
    auto p = FindProvider(h);
    if (!p) return false;
    auto built = BuildItems(p, json);
    {
        std::lock_guard l(p->m);
        p->staticItems.clear();
        p->staticCache.clear();
        for (auto& b : built) {
            p->staticItems.push_back(b.view);
            p->staticCache[b.id] = b.view;
        }
        PruneCards(p.get());
    }
    return NotifyChanged(h);
}

static bool UpdateCard(UINT64 h, std::wstring const& itemId, std::wstring const& dataJson) {
    auto p = FindProvider(h);
    if (!p) return false;
    winrt::com_ptr<Pal::IPaletteTemplateContent> content;
    {
        std::lock_guard l(p->m);
        auto it = p->cards.find(itemId);
        if (it == p->cards.end()) return false;
        it->second.dataJson = dataJson;
        content = it->second.content;
    }
    if (content) {
        winrt::hstring d(dataJson);
        content->put_DataJson(H(d));
    }
    return true;
}
