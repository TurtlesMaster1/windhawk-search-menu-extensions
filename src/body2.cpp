
// ---------------------------------------------------------------------------
// Exported C API (in-process clients)
// ---------------------------------------------------------------------------
extern "C" {
typedef struct WSE_ProviderCallbacks {
    UINT32 cbSize;
    void* ctx;
    void(WINAPI* onQuery)(void* ctx, UINT64 queryHandle, PCWSTR text, PCWSTR queryJson);
    void(WINAPI* onInvoke)(void* ctx, PCWSTR itemId, PCWSTR actionId, PCWSTR dataJson, PCWSTR inputsJson);
    void(WINAPI* onCancel)(void* ctx, UINT64 queryHandle);
    void(WINAPI* onDetach)(void* ctx);
} WSE_ProviderCallbacks;

__declspec(dllexport) int WINAPI WSE_GetApiVersion(void) { return 1; }

__declspec(dllexport) UINT64 WINAPI WSE_RegisterProvider(PCWSTR providerJson, const WSE_ProviderCallbacks* c) {
    if (!providerJson || !c || c->cbSize < offsetof(WSE_ProviderCallbacks, onCancel)) return 0;
    WSE_ProviderCallbacks cc{};
    memcpy(&cc, c, std::min((size_t)c->cbSize, sizeof(cc)));
    ProviderCallbacks cb;
    if (cc.onQuery)
        cb.onQuery = [cc](UINT64 q, std::wstring const& t, std::wstring const& j) { cc.onQuery(cc.ctx, q, t.c_str(), j.c_str()); };
    if (cc.onInvoke)
        cb.onInvoke = [cc](std::wstring const& i, std::wstring const& a, std::wstring const& d, std::wstring const& in) {
            cc.onInvoke(cc.ctx, i.c_str(), a.c_str(), d.c_str(), in.c_str());
        };
    if (cc.onCancel) cb.onCancel = [cc](UINT64 q) { cc.onCancel(cc.ctx, q); };
    if (cc.onDetach) cb.onDetach = [cc] { cc.onDetach(cc.ctx); };
    std::wstring err;
    UINT64 h = RegisterProvider(providerJson, std::move(cb), nullptr, &err);
    if (!h) LOG(L"WSE_RegisterProvider failed: %s", err.c_str());
    return h;
}
__declspec(dllexport) BOOL WINAPI WSE_SubmitResults(UINT64 q, PCWSTR itemsJson, BOOL done) {
    return SubmitResults(q, itemsJson ? itemsJson : L"[]", done);
}
__declspec(dllexport) BOOL WINAPI WSE_SetStaticItems(UINT64 p, PCWSTR itemsJson) {
    return SetStaticItems(p, itemsJson ? itemsJson : L"[]");
}
__declspec(dllexport) BOOL WINAPI WSE_UpdateCard(UINT64 p, PCWSTR itemId, PCWSTR dataJson) {
    return itemId && dataJson && UpdateCard(p, itemId, dataJson);
}
__declspec(dllexport) BOOL WINAPI WSE_NotifyChanged(UINT64 p) { return NotifyChanged(p); }
__declspec(dllexport) BOOL WINAPI WSE_UnregisterProvider(UINT64 p) { return UnregisterProvider(p); }
}

// ---------------------------------------------------------------------------
// Named pipe server (out-of-process clients)
// ---------------------------------------------------------------------------
static const wchar_t kPipeName[] = L"\\\\.\\pipe\\WindhawkSearchExtensions";

static std::string ToUtf8(std::wstring const& w) {
    if (w.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s(n, 0);
    WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), s.data(), n, nullptr, nullptr);
    return s;
}
static std::wstring FromUtf8(std::string const& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring w(n, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), w.data(), n);
    return w;
}

struct PipeClient : std::enable_shared_from_this<PipeClient> {
    HANDLE pipe = INVALID_HANDLE_VALUE;
    std::mutex writeLock;
    std::mutex provLock;
    std::map<std::wstring, UINT64> providers;  // id -> handle
    std::map<UINT64, std::wstring> queryProvider;
    std::atomic<bool> closed{false};

    void Send(wdj::JsonObject const& o) {
        if (closed) return;
        std::string line = ToUtf8(std::wstring(o.Stringify())) + "\n";
        std::lock_guard l(writeLock);
        OVERLAPPED ov{};
        ov.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        DWORD w = 0;
        if (!WriteFile(pipe, line.data(), (DWORD)line.size(), &w, &ov) && GetLastError() == ERROR_IO_PENDING) {
            if (WaitForSingleObject(ov.hEvent, 5000) != WAIT_OBJECT_0) CancelIoEx(pipe, &ov);
            GetOverlappedResult(pipe, &ov, &w, TRUE);
        }
        CloseHandle(ov.hEvent);
    }
    void Error(std::wstring const& msg) {
        wdj::JsonObject o;
        o.SetNamedValue(L"op", wdj::JsonValue::CreateStringValue(L"error"));
        o.SetNamedValue(L"message", wdj::JsonValue::CreateStringValue(msg));
        Send(o);
    }
    UINT64 Lookup(wdj::JsonObject const& o) {
        std::lock_guard l(provLock);
        auto it = providers.find(JStr(o, L"provider"));
        return it == providers.end() ? 0 : it->second;
    }
    static std::wstring ItemsOf(wdj::JsonObject const& o) {
        if (!o.HasKey(L"items")) return L"[]";
        return std::wstring(o.GetNamedValue(L"items").Stringify());
    }

    void Handle(std::wstring const& line) {
        wdj::JsonObject o{nullptr};
        if (!wdj::JsonObject::TryParse(line, o)) {
            Error(L"invalid JSON");
            return;
        }
        std::wstring op = JStr(o, L"op");
        if (op == L"register") {
            if (!o.HasKey(L"provider") || o.GetNamedValue(L"provider").ValueType() != wdj::JsonValueType::Object) {
                Error(L"register needs a provider object");
                return;
            }
            auto po = o.GetNamedObject(L"provider");
            std::wstring id = JStr(po, L"id");
            std::weak_ptr<PipeClient> self = shared_from_this();
            ProviderCallbacks cb;
            cb.onQuery = [self, id](UINT64 q, std::wstring const& text, std::wstring const& json) {
                auto c = self.lock();
                if (!c) return;
                {
                    std::lock_guard l(c->provLock);
                    c->queryProvider[q] = id;
                }
                wdj::JsonObject m;
                m.SetNamedValue(L"op", wdj::JsonValue::CreateStringValue(L"query"));
                m.SetNamedValue(L"provider", wdj::JsonValue::CreateStringValue(id));
                m.SetNamedValue(L"queryId", wdj::JsonValue::CreateStringValue(std::to_wstring(q)));
                m.SetNamedValue(L"text", wdj::JsonValue::CreateStringValue(text));
                wdj::JsonObject qj{nullptr};
                if (!json.empty() && wdj::JsonObject::TryParse(json, qj)) m.SetNamedValue(L"query", qj);
                c->Send(m);
            };
            cb.onInvoke = [self, id](std::wstring const& item, std::wstring const& action, std::wstring const& data,
                                     std::wstring const& inputs) {
                auto c = self.lock();
                if (!c) return;
                wdj::JsonObject m;
                m.SetNamedValue(L"op", wdj::JsonValue::CreateStringValue(L"invoke"));
                m.SetNamedValue(L"provider", wdj::JsonValue::CreateStringValue(id));
                m.SetNamedValue(L"itemId", wdj::JsonValue::CreateStringValue(item));
                m.SetNamedValue(L"actionId", wdj::JsonValue::CreateStringValue(action));
                m.SetNamedValue(L"data", wdj::JsonValue::CreateStringValue(data));
                m.SetNamedValue(L"inputs", wdj::JsonValue::CreateStringValue(inputs));
                c->Send(m);
            };
            cb.onCancel = [self, id](UINT64 q) {
                auto c = self.lock();
                if (!c) return;
                wdj::JsonObject m;
                m.SetNamedValue(L"op", wdj::JsonValue::CreateStringValue(L"cancel"));
                m.SetNamedValue(L"provider", wdj::JsonValue::CreateStringValue(id));
                m.SetNamedValue(L"queryId", wdj::JsonValue::CreateStringValue(std::to_wstring(q)));
                c->Send(m);
            };
            std::wstring err;
            UINT64 h = RegisterProvider(std::wstring(po.Stringify()), std::move(cb), this, &err);
            if (!h) {
                Error(L"register failed: " + err);
                return;
            }
            {
                std::lock_guard l(provLock);
                providers[id] = h;
            }
            wdj::JsonObject m;
            m.SetNamedValue(L"op", wdj::JsonValue::CreateStringValue(L"registered"));
            m.SetNamedValue(L"provider", wdj::JsonValue::CreateStringValue(id));
            m.SetNamedValue(L"extensionId", wdj::JsonValue::CreateStringValue(L"Windhawk." + id));
            Send(m);
        } else if (op == L"results") {
            UINT64 q = _wcstoui64(JStr(o, L"queryId").c_str(), nullptr, 10);
            bool done = !o.HasKey(L"done") || o.GetNamedValue(L"done").ValueType() != wdj::JsonValueType::Boolean ||
                        o.GetNamedBoolean(L"done");
            SubmitResults(q, ItemsOf(o), done);
        } else if (op == L"setItems") {
            if (UINT64 h = Lookup(o)) SetStaticItems(h, ItemsOf(o));
            else Error(L"unknown provider");
        } else if (op == L"updateCard") {
            UINT64 h = Lookup(o);
            std::wstring data = o.HasKey(L"data") ? std::wstring(o.GetNamedValue(L"data").Stringify()) : L"{}";
            if (o.HasKey(L"data") && o.GetNamedValue(L"data").ValueType() == wdj::JsonValueType::String)
                data = o.GetNamedString(L"data");
            if (!h || !UpdateCard(h, JStr(o, L"itemId"), data)) Error(L"updateCard: unknown provider or item");
        } else if (op == L"refresh") {
            if (UINT64 h = Lookup(o)) NotifyChanged(h);
        } else if (op == L"unregister") {
            UINT64 h = Lookup(o);
            if (h) {
                UnregisterProvider(h);
                std::lock_guard l(provLock);
                providers.erase(JStr(o, L"provider"));
            }
        } else if (op == L"ping") {
            wdj::JsonObject m;
            m.SetNamedValue(L"op", wdj::JsonValue::CreateStringValue(L"pong"));
            Send(m);
        } else {
            Error(L"unknown op '" + op + L"'");
        }
    }

    void Run(HANDLE stopEvent) {
        winrt::init_apartment(winrt::apartment_type::multi_threaded);
        std::string buf;
        char chunk[8192];
        HANDLE ev = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        while (!closed) {
            OVERLAPPED ov{};
            ov.hEvent = ev;
            ResetEvent(ev);
            DWORD n = 0;
            BOOL ok = ReadFile(pipe, chunk, sizeof(chunk), &n, &ov);
            if (!ok && GetLastError() == ERROR_IO_PENDING) {
                HANDLE hs[2] = {ev, stopEvent};
                DWORD w = WaitForMultipleObjects(2, hs, FALSE, INFINITE);
                if (w != WAIT_OBJECT_0) {
                    CancelIoEx(pipe, &ov);
                    GetOverlappedResult(pipe, &ov, &n, TRUE);
                    break;
                }
                ok = GetOverlappedResult(pipe, &ov, &n, FALSE);
            }
            if (!ok && GetLastError() != ERROR_MORE_DATA) break;
            if (n == 0) continue;
            buf.append(chunk, n);
            if (buf.size() > 16 * 1024 * 1024) break;  // runaway client
            size_t pos;
            while ((pos = buf.find('\n')) != std::string::npos) {
                std::string line = buf.substr(0, pos);
                buf.erase(0, pos + 1);
                if (!line.empty() && line.back() == '\r') line.pop_back();
                if (line.empty()) continue;
                try {
                    Handle(FromUtf8(line));
                } catch (winrt::hresult_error const& e) {
                    Error(std::wstring(L"error: ") + e.message().c_str());
                } catch (...) {
                    Error(L"internal error");
                }
            }
        }
        CloseHandle(ev);
        closed = true;
        std::map<std::wstring, UINT64> ps;
        {
            std::lock_guard l(provLock);
            ps.swap(providers);
        }
        for (auto& [id, h] : ps) UnregisterProvider(h);
        std::lock_guard l(writeLock);
        DisconnectNamedPipe(pipe);
        CloseHandle(pipe);
        pipe = INVALID_HANDLE_VALUE;
        winrt::uninit_apartment();
    }
};

static HANDLE g_pipeStop;
static std::thread g_pipeThread;
static std::mutex g_clientThreadsLock;
static std::vector<std::thread> g_clientThreads;

static void PipeListener() {
    HANDLE ev = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    while (WaitForSingleObject(g_pipeStop, 0) != WAIT_OBJECT_0) {
        HANDLE pipe = CreateNamedPipeW(kPipeName, PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
                                       PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS,
                                       PIPE_UNLIMITED_INSTANCES, 64 * 1024, 64 * 1024, 0, nullptr);
        if (pipe == INVALID_HANDLE_VALUE) {
            LOG(L"CreateNamedPipe failed: %lu", GetLastError());
            if (WaitForSingleObject(g_pipeStop, 5000) == WAIT_OBJECT_0) break;
            continue;
        }
        OVERLAPPED ov{};
        ov.hEvent = ev;
        ResetEvent(ev);
        BOOL connected = ConnectNamedPipe(pipe, &ov);
        DWORD err = GetLastError();
        if (!connected && err == ERROR_IO_PENDING) {
            HANDLE hs[2] = {ev, g_pipeStop};
            if (WaitForMultipleObjects(2, hs, FALSE, INFINITE) != WAIT_OBJECT_0) {
                CancelIoEx(pipe, &ov);
                CloseHandle(pipe);
                break;
            }
            DWORD dummy;
            connected = GetOverlappedResult(pipe, &ov, &dummy, FALSE);
        } else if (!connected && err == ERROR_PIPE_CONNECTED) {
            connected = TRUE;
        }
        if (!connected) {
            CloseHandle(pipe);
            continue;
        }
        auto c = std::make_shared<PipeClient>();
        c->pipe = pipe;
        DLOG(L"pipe client connected");
        std::lock_guard l(g_clientThreadsLock);
        g_clientThreads.emplace_back([c] { c->Run(g_pipeStop); });
    }
    CloseHandle(ev);
}

static void StartPipe() {
    g_pipeStop = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    g_pipeThread = std::thread(PipeListener);
}
static void StopPipe() {
    if (!g_pipeStop) return;
    SetEvent(g_pipeStop);
    if (g_pipeThread.joinable()) g_pipeThread.join();
    std::vector<std::thread> ts;
    {
        std::lock_guard l(g_clientThreadsLock);
        ts.swap(g_clientThreads);
    }
    for (auto& t : ts)
        if (t.joinable()) t.join();
    CloseHandle(g_pipeStop);
    g_pipeStop = nullptr;
}

// ---------------------------------------------------------------------------
// Demo provider
// ---------------------------------------------------------------------------
static UINT64 g_demo;
static std::atomic<bool> g_demoToggle{false};
static std::atomic<int> g_demoClicks{0};

static std::wstring JsonEscape(std::wstring const& s) {
    return std::wstring(wdj::JsonValue::CreateStringValue(s).Stringify());
}

static void DemoQuery(UINT64 q, std::wstring const& text) {
    std::wstring t = text;
    if (_wcsnicmp(t.c_str(), L"wh", 2) != 0) {
        SubmitResults(q, L"[]", TRUE);
        return;
    }
    std::wstring rest = t.size() > 2 ? t.substr(2) : L"";
    while (!rest.empty() && rest[0] == L' ') rest.erase(0, 1);
    std::wstring json = L"[";
    json += L"{\"id\":\"echo\",\"title\":" + JsonEscape(L"Windhawk heard: " + (rest.empty() ? std::wstring(L"(nothing yet)") : rest)) +
            L",\"subtitle\":\"Search Menu Extensions demo. Press Enter to copy\",\"score\":1.0,"
            L"\"icon\":{\"glyph\":\"\\uE8D4\"},\"tags\":[{\"text\":\"Windhawk\"}],"
            L"\"action\":{\"type\":\"copy\",\"text\":" + JsonEscape(rest) + L"}},";
    json += L"{\"id\":\"toggle\",\"title\":\"Demo toggle\",\"subtitle\":\"Flips a value inside the library\","
            L"\"icon\":{\"glyph\":\"\\uE7E8\"},\"kinds\":[\"Toggle\",\"Command\"],"
            L"\"action\":{\"type\":\"toggle\",\"isToggled\":" + std::wstring(g_demoToggle ? L"true" : L"false") + L"}},";
    json += L"{\"id\":\"open\",\"title\":\"Open Windhawk\",\"subtitle\":\"Runs windhawk.exe\","
            L"\"icon\":{\"glyph\":\"\\uE8A7\"},"
            L"\"action\":{\"type\":\"run\",\"command\":\"C:\\\\Program Files\\\\Windhawk\\\\windhawk.exe\"}},";
    json += L"{\"id\":\"card\",\"title\":\"Demo card\",\"subtitle\":\"Adaptive Card content from a mod\","
            L"\"icon\":{\"glyph\":\"\\uE9E9\"},"
            L"\"card\":{\"template\":{\"type\":\"AdaptiveCard\",\"version\":\"1.5\",\"body\":["
            L"{\"type\":\"TextBlock\",\"text\":\"${title}\",\"weight\":\"Bolder\",\"size\":\"Large\"},"
            L"{\"type\":\"TextBlock\",\"text\":\"Clicked ${clicks} times\",\"wrap\":true}],"
            L"\"actions\":[{\"type\":\"Action.Submit\",\"id\":\"click\",\"title\":\"Click me\",\"data\":{\"x\":1}}]},"
            L"\"data\":{\"title\":\"Hello from Windhawk\",\"clicks\":" + std::to_wstring(g_demoClicks.load()) + L"}}}";
    json += L"]";
    SubmitResults(q, json, TRUE);
}

static void RegisterDemo() {
    ProviderCallbacks cb;
    cb.onQuery = [](UINT64 q, std::wstring const& text, std::wstring const&) { DemoQuery(q, text); };
    cb.onInvoke = [](std::wstring const& item, std::wstring const& action, std::wstring const& data, std::wstring const&) {
        if (item == L"toggle") g_demoToggle = (data == L"true");
        if (item == L"card") {
            int n = ++g_demoClicks;
            UpdateCard(g_demo, L"card", L"{\"title\":\"Hello from Windhawk\",\"clicks\":" + std::to_wstring(n) + L"}");
        }
        LOG(L"demo invoke %s/%s", item.c_str(), action.c_str());
    };
    std::wstring err;
    g_demo = RegisterProvider(
        L"{\"id\":\"demo\",\"name\":\"Windhawk demo\",\"description\":\"Search Menu Extensions sample items\",\"glyph\":\"\\uE8D4\"}",
        std::move(cb), nullptr, &err);
    if (!g_demo) LOG(L"demo registration failed: %s", err.c_str());
}

// ---------------------------------------------------------------------------
// Lifetime
// ---------------------------------------------------------------------------
static void LoadSettings() {
    g_settings.demoProvider = Wh_GetIntSetting(L"demoProvider") != 0;
    g_settings.pipeServer = Wh_GetIntSetting(L"pipeServer") != 0;
    g_settings.queryTimeoutMs = Wh_GetIntSetting(L"queryTimeoutMs");
    if (g_settings.queryTimeoutMs < 100 || g_settings.queryTimeoutMs > 60000) g_settings.queryTimeoutMs = 2500;
    g_settings.debugLog = Wh_GetIntSetting(L"debugLog") != 0;
    g_settings.restartIfLate = Wh_GetIntSetting(L"restartIfLate") != 0;
}

static bool IsComposerProcess() {
    const wchar_t* cmd = GetCommandLineW();
    return cmd && StrStrIW(cmd, L"SystemComposerControl");
}

static bool g_started;

static bool LibInit() {
    LoadSettings();
    if (!IsComposerProcess()) {
        DLOG(L"not the composer shellhost, idle");
        return true;
    }
    {
        using IncMta_t = HRESULT(WINAPI*)(void**);
        static void* cookie;
        if (auto f = (IncMta_t)GetProcAddress(GetModuleHandleW(L"combase.dll"), "CoIncrementMTAUsage")) f(&cookie);
    }  // so any thread can use WinRT objects
    bool lateLoad = GetModuleHandleW(L"RunUX.AOT.dll") != nullptr;
    (void)lateLoad;
    g_alive = true;
    if (!InstallCatalogHooks()) {
        g_alive = false;
        return true;  // stay loaded but inactive
    }
    g_started = true;
    // Patch the catalog instance vtables now in case Windows already opened its catalogs.
    std::thread([] {
        winrt::init_apartment(winrt::apartment_type::multi_threaded);
        winrt::com_ptr<Pal::IPaletteCatalog> cat;
        HRESULT hr = g_statics->OpenDefault(cat.put());
        LOG(L"background OpenDefault hr=%08lx", hr);
        if (g_settings.demoProvider) RegisterDemo();
        winrt::uninit_apartment();
    }).detach();
    if (g_settings.pipeServer) StartPipe();
    LOG(L"Search Menu Extensions running (pid %lu)", GetCurrentProcessId());
#ifdef WH_MOD
    // Enabled while search was already running: Windows has read its provider list and won't
    // re-read it, so restart the composer (it relaunches the next time search opens, with us in it).
    if (lateLoad && g_settings.restartIfLate) {
        std::thread([] {
            Sleep(1500);
            bool haveSubs;
            {
                std::lock_guard l(g_subsLock);
                haveSubs = !g_subs.empty();
            }
            if (!haveSubs && g_alive) {
                LOG(L"loaded after search started; restarting the composer so it picks up our providers");
                TerminateProcess(GetCurrentProcess(), 0);
            }
        }).detach();
    }
#endif
    return true;
}

static void LibUninit() {
    if (!g_started) return;
    StopPipe();
    std::vector<UINT64> hs;
    for (auto& p : ProvidersSnapshot()) hs.push_back(p->handle);
    for (auto h : hs) UnregisterProvider(h, true);
    g_alive = false;
    UnpatchAll();
    RaiseCatalogChanged();
    // Keep g_subs: if we're re-initialised in this process we can refresh search without a restart.
    g_started = false;
    LOG(L"Search Menu Extensions stopped");
}

#ifdef WH_MOD
BOOL Wh_ModInit() {
    LOG(L"Init");
    return LibInit();
}
void Wh_ModUninit() { LibUninit(); }
void Wh_ModSettingsChanged() {
    bool oldDemo = g_settings.demoProvider;
    LoadSettings();
    if (!g_started) return;
    if (g_settings.demoProvider && !g_demo) RegisterDemo();
    if (!g_settings.demoProvider && oldDemo && g_demo) {
        UnregisterProvider(g_demo);
        g_demo = 0;
    }
}
#else
extern "C" __declspec(dllexport) DWORD WINAPI TestInit(void*) {
    LibInit();
    return 0;
}
extern "C" __declspec(dllexport) DWORD WINAPI TestUninit(void*) {
    LibUninit();
    return 0;
}
BOOL WINAPI DllMain(HINSTANCE h, DWORD r, void*) {
    if (r == DLL_PROCESS_ATTACH) g_module = h;
    return TRUE;
}
#endif
