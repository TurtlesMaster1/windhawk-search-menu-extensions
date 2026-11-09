
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
    UINT64 h = RegisterProvider(providerJson, std::move(cb), &err);
    if (!h) Wh_Log(L"WSE_RegisterProvider failed: %s", err.c_str());
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
// Blocks until the provider's running callbacks return, so it's safe to unload right after.
__declspec(dllexport) BOOL WINAPI WSE_UnregisterProvider(UINT64 p) { return UnregisterProvider(p); }
}

// ---------------------------------------------------------------------------
// Named pipe server (out-of-process clients)
// ---------------------------------------------------------------------------
// One pipe per session: \\.\pipe\WindhawkSearchExtensions-<session id>, current user only.
static std::wstring PipeName() {
    DWORD session = 0;
    ProcessIdToSessionId(GetCurrentProcessId(), &session);
    return L"\\\\.\\pipe\\WindhawkSearchExtensions-" + std::to_wstring(session);
}

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
    std::atomic<bool> closed{false};

    void Send(wdj::JsonObject const& o) {
        if (closed) return;
        std::string line = ToUtf8(std::wstring(o.Stringify())) + "\n";
        std::lock_guard l(writeLock);
        if (pipe == INVALID_HANDLE_VALUE) return;
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
            UINT64 h = RegisterProvider(std::wstring(po.Stringify()), std::move(cb), &err);
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
        // Unregistering waits for this client's callbacks (which write to the pipe) to finish.
        std::map<std::wstring, UINT64> ps;
        {
            std::lock_guard l(provLock);
            ps.swap(providers);
        }
        for (auto& [id, h] : ps) UnregisterProvider(h);
        closed = true;
        std::lock_guard l(writeLock);
        DisconnectNamedPipe(pipe);
        CloseHandle(pipe);
        pipe = INVALID_HANDLE_VALUE;
    }
};

// Raw handles only: nothing here has a destructor that could run at process exit.
static HANDLE g_pipeStop;
static HANDLE g_pipeThread;
static std::mutex g_clientThreadsLock;
static std::vector<HANDLE> g_clientThreads;  // plain handles

static DWORD WINAPI ClientThread(void* ctx) {
    std::unique_ptr<std::shared_ptr<PipeClient>> c((std::shared_ptr<PipeClient>*)ctx);
    (*c)->Run(g_pipeStop);
    return 0;
}

// Security descriptor for the pipe: the current user and SYSTEM only.
static bool PipeSecurity(SECURITY_ATTRIBUTES* sa) {
    HANDLE token;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) return false;
    BYTE buf[256];
    DWORD len = 0;
    bool ok = GetTokenInformation(token, TokenUser, buf, sizeof(buf), &len);
    CloseHandle(token);
    if (!ok) return false;
    LPWSTR sid = nullptr;
    if (!ConvertSidToStringSidW(((TOKEN_USER*)buf)->User.Sid, &sid)) return false;
    std::wstring sddl = L"D:P(A;;GA;;;" + std::wstring(sid) + L")(A;;GA;;;SY)";
    LocalFree(sid);
    PSECURITY_DESCRIPTOR sd = nullptr;
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl.c_str(), SDDL_REVISION_1, &sd, nullptr)) return false;
    sa->nLength = sizeof(*sa);
    sa->lpSecurityDescriptor = sd;  // freed by the listener
    sa->bInheritHandle = FALSE;
    return true;
}

static DWORD WINAPI PipeListener(void*) {
    std::wstring name = PipeName();
    SECURITY_ATTRIBUTES sa{};
    if (!PipeSecurity(&sa)) {
        Wh_Log(L"pipe: could not build its security descriptor (%lu), not starting", GetLastError());
        return 0;
    }
    HANDLE ev = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    bool first = true;
    while (WaitForSingleObject(g_pipeStop, 0) != WAIT_OBJECT_0) {
        DWORD openMode = PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED | (first ? FILE_FLAG_FIRST_PIPE_INSTANCE : 0);
        HANDLE pipe = CreateNamedPipeW(name.c_str(), openMode,
                                       PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS,
                                       PIPE_UNLIMITED_INSTANCES, 64 * 1024, 64 * 1024, 0, &sa);
        if (pipe == INVALID_HANDLE_VALUE) {
            // ACCESS_DENIED on the first instance: someone else already owns this name.
            Wh_Log(L"CreateNamedPipe(%s) failed: %lu", name.c_str(), GetLastError());
            if (WaitForSingleObject(g_pipeStop, 5000) == WAIT_OBJECT_0) break;
            continue;
        }
        if (first) Wh_Log(L"pipe server listening on %s", name.c_str());
        first = false;
        OVERLAPPED ov{};
        ov.hEvent = ev;
        ResetEvent(ev);
        BOOL connected = ConnectNamedPipe(pipe, &ov);
        DWORD err = GetLastError();
        if (!connected && err == ERROR_IO_PENDING) {
            HANDLE hs[2] = {ev, g_pipeStop};
            DWORD dummy;
            if (WaitForMultipleObjects(2, hs, FALSE, INFINITE) != WAIT_OBJECT_0) {
                CancelIoEx(pipe, &ov);
                GetOverlappedResult(pipe, &ov, &dummy, TRUE);  // the kernel is done with ov after this
                CloseHandle(pipe);
                break;
            }
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
        Wh_Log(L"pipe client connected");
        std::lock_guard l(g_clientThreadsLock);
        // Forget clients that have already disconnected.
        std::erase_if(g_clientThreads, [](HANDLE t) {
            if (WaitForSingleObject(t, 0) != WAIT_OBJECT_0) return false;
            CloseHandle(t);
            return true;
        });
        auto* ctx = new std::shared_ptr<PipeClient>(c);
        if (HANDLE t = CreateThread(nullptr, 0, ClientThread, ctx, 0, nullptr)) {
            g_clientThreads.push_back(t);
        } else {
            delete ctx;
            DisconnectNamedPipe(pipe);
            CloseHandle(pipe);
        }
    }
    CloseHandle(ev);
    LocalFree(sa.lpSecurityDescriptor);
    return 0;
}

static void StartPipe() {
    if (g_pipeThread) return;
    g_pipeStop = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    g_pipeThread = CreateThread(nullptr, 0, PipeListener, nullptr, 0, nullptr);
}
static void StopPipe() {
    if (!g_pipeStop) return;
    SetEvent(g_pipeStop);
    if (g_pipeThread) {
        WaitForSingleObject(g_pipeThread, INFINITE);
        CloseHandle(g_pipeThread);
        g_pipeThread = nullptr;
    }
    std::vector<HANDLE> ts;
    {
        std::lock_guard l(g_clientThreadsLock);
        ts.swap(g_clientThreads);
    }
    for (HANDLE t : ts) {
        WaitForSingleObject(t, INFINITE);
        CloseHandle(t);
    }
    CloseHandle(g_pipeStop);
    g_pipeStop = nullptr;
}

// ---------------------------------------------------------------------------
// Demo provider
// ---------------------------------------------------------------------------
static std::atomic<UINT64> g_demo{0};
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
    json += L"{\"id\":\"notepad\",\"title\":\"Open Notepad\",\"subtitle\":\"A \\\"run\\\" action\","
            L"\"icon\":{\"glyph\":\"\\uE70B\"},"
            L"\"action\":{\"type\":\"run\",\"command\":\"notepad.exe\"}},";
    json += L"{\"id\":\"card\",\"title\":\"Demo card\",\"subtitle\":\"Adaptive Card content from a mod\","
            L"\"icon\":{\"glyph\":\"\\uE9E9\"},"
            L"\"card\":{\"template\":{\"type\":\"AdaptiveCard\",\"version\":\"1.5\",\"body\":["
            L"{\"type\":\"TextBlock\",\"text\":\"${title}\",\"weight\":\"Bolder\",\"size\":\"Large\"},"
            L"{\"type\":\"TextBlock\",\"text\":\"Invoked ${clicks} times\",\"wrap\":true}]},"
            L"\"data\":{\"title\":\"Hello from Windhawk\",\"clicks\":" + std::to_wstring(g_demoClicks.load()) + L"}}}";
    json += L"]";
    SubmitResults(q, json, TRUE);
}

static void RegisterDemo() {
    if (g_demo) return;
    ProviderCallbacks cb;
    cb.onQuery = [](UINT64 q, std::wstring const& text, std::wstring const&) { DemoQuery(q, text); };
    cb.onInvoke = [](std::wstring const& item, std::wstring const& action, std::wstring const& data, std::wstring const&) {
        if (item == L"toggle") g_demoToggle = (data == L"true");
        if (item == L"card") {
            int n = ++g_demoClicks;
            UpdateCard(g_demo, L"card", L"{\"title\":\"Hello from Windhawk\",\"clicks\":" + std::to_wstring(n) + L"}");
        }
        Wh_Log(L"demo invoke %s/%s", item.c_str(), action.c_str());
    };
    std::wstring err;
    UINT64 h = RegisterProvider(
        L"{\"id\":\"demo\",\"name\":\"Windhawk demo\",\"description\":\"Search Menu Extensions sample items\",\"glyph\":\"\\uE8D4\"}",
        std::move(cb), &err);
    if (!h) Wh_Log(L"demo registration failed: %s", err.c_str());
    g_demo = h;
}
static void UnregisterDemo() {
    if (UINT64 h = g_demo.exchange(0)) UnregisterProvider(h);
}

// ---------------------------------------------------------------------------
// Lifetime
// ---------------------------------------------------------------------------
static void LoadSettings() {
    g_settings.demoProvider = Wh_GetIntSetting(L"demoProvider") != 0;
    g_settings.pipeServer = Wh_GetIntSetting(L"pipeServer") != 0;
    g_settings.queryTimeoutMs = Wh_GetIntSetting(L"queryTimeoutMs");
    if (g_settings.queryTimeoutMs < 100 || g_settings.queryTimeoutMs > 60000) g_settings.queryTimeoutMs = 2500;
    g_settings.restartIfLate = Wh_GetIntSetting(L"restartIfLate") != 0;
}

static bool IsComposerProcess() {
    const wchar_t* cmd = GetCommandLineW();
    return cmd && StrStrIW(cmd, L"SystemComposerControl");
}

static CO_MTA_USAGE_COOKIE g_mtaCookie;
static bool g_started;
static bool g_stopped;

// Enabled while search was already running: Windows has read its provider list and won't re-read
// it (we never saw its Changed subscription), so restart the composer. It relaunches the next
// time search opens, with us in it.
static VOID CALLBACK LateLoadCheck(PTP_CALLBACK_INSTANCE, PVOID, PTP_TIMER) {
    bool haveSubs;
    {
        std::lock_guard l(g_subsLock);
        haveSubs = !g_subs.empty();
    }
    if (!haveSubs && g_alive) {
        Wh_Log(L"loaded after search started; restarting the composer so it picks up our providers");
        TerminateProcess(GetCurrentProcess(), 0);
    }
}

BOOL Wh_ModInit() {
    g_stopped = false;
    LoadSettings();
    if (!IsComposerProcess()) return FALSE;  // not the search composer: nothing to do here
    if (FAILED(CoIncrementMTAUsage(&g_mtaCookie))) g_mtaCookie = nullptr;  // lets any thread use WinRT
    if (!PoolOpen()) return FALSE;
    g_alive = true;
    if (!SetCatalogHooks()) {
        g_alive = false;
        PoolClose();
        if (g_mtaCookie) CoDecrementMTAUsage(g_mtaCookie);
        return FALSE;
    }
    g_changeTimer = PoolTimer(ChangeTimerCb, nullptr);
    bool lateLoad = GetModuleHandleW(L"RunUX.AOT.dll") != nullptr;
    if (g_settings.demoProvider) RegisterDemo();
    if (g_settings.pipeServer) StartPipe();
    if (lateLoad && g_settings.restartIfLate)
        if (PTP_TIMER t = PoolTimer(LateLoadCheck, nullptr)) ArmTimer(t, 1500);
    g_started = true;
    Wh_Log(L"Search Menu Extensions running (pid %lu)", GetCurrentProcessId());
    return TRUE;
}

// Hooks are still active here: take our providers out and tell Windows to re-read the catalog.
void Wh_ModBeforeUninit() {
    if (!g_started || g_stopped) return;
    g_stopped = true;
    {
        std::lock_guard l(g_queriesLock);
        g_alive = false;  // no new providers or queries from here on (both re-check it under a lock)
    }
    StopPipe();  // pipe clients unregister their providers as they disconnect
    std::vector<UINT64> hs;
    for (auto& p : ProvidersSnapshot()) hs.push_back(p->handle);
    for (auto h : hs) UnregisterProvider(h, true);  // in-process clients get onDetach
    g_demo = 0;
    std::vector<std::shared_ptr<Query>> qs;
    {
        std::lock_guard l(g_queriesLock);
        for (auto& [k, q] : g_queries) qs.push_back(q);
    }
    for (auto& q : qs) FinishQuery(q, L"unloading");
    RaiseCatalogChanged();
}

void Wh_ModUninit() {
    if (!g_started) return;
    Wh_ModBeforeUninit();
    g_changeTimer = nullptr;
    PoolClose();  // waits for every worker callback and timer
    {
        std::lock_guard l(g_subsLock);
        std::vector<ChangedSub>().swap(g_subs);  // frees the buffer too
    }
    {
        std::lock_guard l(g_providersLock);
        std::vector<std::shared_ptr<Provider>>().swap(g_providers);
    }
    {
        std::lock_guard l(g_queriesLock);
        g_queries.clear();
    }
    ReleaseFactories();
    // Windows may still hold objects whose code lives in this DLL (definitions, items' delegates,
    // finished async operations). Give it a moment to drop them after the Changed event; if it
    // doesn't, restart the composer rather than let the DLL unload under them. Search relaunches
    // it on next use.
    for (int i = 0; i < 40 && g_liveObjects > 0; i++) Sleep(50);
    if (g_liveObjects == 0) Sleep(50);  // let the last Release return out of our code
    if (g_liveObjects > 0) {
        Wh_Log(L"Windows still holds %d of our objects; restarting the composer", g_liveObjects.load());
        TerminateProcess(GetCurrentProcess(), 0);
    }
    if (g_mtaCookie) CoDecrementMTAUsage(g_mtaCookie);
    g_mtaCookie = nullptr;
    g_started = false;
    Wh_Log(L"Search Menu Extensions stopped");
}

void Wh_ModSettingsChanged() {
    bool oldDemo = g_settings.demoProvider, oldPipe = g_settings.pipeServer;
    LoadSettings();
    if (!g_started || g_stopped) return;
    if (g_settings.demoProvider && !oldDemo) RegisterDemo();
    if (!g_settings.demoProvider && oldDemo) UnregisterDemo();
    if (g_settings.pipeServer && !oldPipe) StartPipe();
    if (!g_settings.pipeServer && oldPipe) StopPipe();
}
