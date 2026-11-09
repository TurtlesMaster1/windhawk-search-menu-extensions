// ==WindhawkMod==
// @id              search-kill-process
// @name            Search: kill process
// @description     Type "kill <name>" in the new Windows search to end a running process. Example client of Search Menu Extensions.
// @version         0.1.0
// @author          TurtlesMaster1
// @github          https://github.com/TurtlesMaster1
// @include         shellhost.exe
// @compilerOptions -lpsapi -lshlwapi
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# Search: kill process

Needs the **New Windows Search Extensions (library)** mod.

Type `kill note` in the new search box. Each running process whose name matches
shows up with its PID and memory use. Press Enter on a row to end that process.
`kill` with no name lists the biggest processes by memory.

This mod is also a minimal example of using the library's in-process C API.
*/
// ==/WindhawkModReadme==

#include <windows.h>
#include <psapi.h>
#include <shlwapi.h>
#include <tlhelp32.h>

#include <algorithm>
#include <atomic>
#include <string>
#include <thread>
#include <vector>

#ifndef WH_MOD
#include <stdio.h>
static void TestLog(const wchar_t* fmt, ...) {
    wchar_t buf[2048];
    va_list ap;
    va_start(ap, fmt);
    _vsnwprintf(buf, 2047, fmt, ap);
    va_end(ap);
    wchar_t logPath[MAX_PATH];
    GetTempPathW(MAX_PATH, logPath);
    wcscat_s(logPath, L"wse-test.log");
    FILE* f = _wfopen(logPath, L"a, ccs=UTF-8");
    if (f) { fwprintf(f, L"[kill-client] %s\n", buf); fclose(f); }
}
#define Wh_Log(fmt, ...) TestLog(fmt, ##__VA_ARGS__)
#endif

// ---- Search Menu Extensions client glue (copy this block into your own mod) ----
struct WSE_ProviderCallbacks {
    UINT32 cbSize;
    void* ctx;
    void(WINAPI* onQuery)(void* ctx, UINT64 queryHandle, PCWSTR text, PCWSTR queryJson);
    void(WINAPI* onInvoke)(void* ctx, PCWSTR itemId, PCWSTR actionId, PCWSTR dataJson, PCWSTR inputsJson);
    void(WINAPI* onCancel)(void* ctx, UINT64 queryHandle);
    void(WINAPI* onDetach)(void* ctx);
};
struct WSE_Api {
    int(WINAPI* GetApiVersion)();
    UINT64(WINAPI* RegisterProvider)(PCWSTR, const WSE_ProviderCallbacks*);
    BOOL(WINAPI* SubmitResults)(UINT64, PCWSTR, BOOL);
    BOOL(WINAPI* UnregisterProvider)(UINT64);
};
// The library exports WSE_* from its mod DLL. More than one loaded module can have them (e.g. while
// Windhawk swaps in an update), so try each until one accepts the registration.
static UINT64 WSE_FindAndRegister(WSE_Api* api, PCWSTR providerJson, const WSE_ProviderCallbacks* cb) {
    HMODULE mods[1024];
    DWORD needed = 0;
    if (!EnumProcessModules(GetCurrentProcess(), mods, sizeof(mods), &needed)) return 0;
    for (DWORD i = 0; i < needed / sizeof(HMODULE); i++) {
        WSE_Api a{};
        a.GetApiVersion = (decltype(a.GetApiVersion))GetProcAddress(mods[i], "WSE_GetApiVersion");
        a.RegisterProvider = (decltype(a.RegisterProvider))GetProcAddress(mods[i], "WSE_RegisterProvider");
        a.SubmitResults = (decltype(a.SubmitResults))GetProcAddress(mods[i], "WSE_SubmitResults");
        a.UnregisterProvider = (decltype(a.UnregisterProvider))GetProcAddress(mods[i], "WSE_UnregisterProvider");
        if (!a.GetApiVersion || a.GetApiVersion() < 1 || !a.RegisterProvider || !a.SubmitResults || !a.UnregisterProvider)
            continue;
        if (UINT64 h = a.RegisterProvider(providerJson, cb)) {
            *api = a;
            return h;
        }
    }
    return 0;
}
// ---- end of glue ----

static WSE_Api g_api;
static std::atomic<UINT64> g_provider{0};
static std::atomic<bool> g_stop{false};
static std::thread g_finder;

static std::wstring Esc(std::wstring const& s) {
    std::wstring r;
    for (wchar_t c : s) {
        if (c == L'"' || c == L'\\') { r += L'\\'; r += c; }
        else if (c < 0x20) { wchar_t b[8]; swprintf(b, 8, L"\\u%04x", c); r += b; }
        else r += c;
    }
    return r;
}

struct Proc { DWORD pid; std::wstring name, path; SIZE_T mem; };

static std::vector<Proc> ListProcesses(std::wstring const& filter) {
    std::vector<Proc> out;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    PROCESSENTRY32W pe{sizeof(pe)};
    for (BOOL ok = Process32FirstW(snap, &pe); ok; ok = Process32NextW(snap, &pe)) {
        if (pe.th32ProcessID <= 4) continue;
        if (!filter.empty() && !StrStrIW(pe.szExeFile, filter.c_str())) continue;
        Proc p{pe.th32ProcessID, pe.szExeFile, L"", 0};
        if (HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, p.pid)) {
            PROCESS_MEMORY_COUNTERS pmc{sizeof(pmc)};
            if (GetProcessMemoryInfo(h, &pmc, sizeof(pmc))) p.mem = pmc.WorkingSetSize;
            wchar_t path[MAX_PATH];
            DWORD n = MAX_PATH;
            if (QueryFullProcessImageNameW(h, 0, path, &n)) p.path = path;
            CloseHandle(h);
        }
        out.push_back(p);
    }
    CloseHandle(snap);
    std::sort(out.begin(), out.end(), [](Proc const& a, Proc const& b) { return a.mem > b.mem; });
    if (out.size() > 8) out.resize(8);
    return out;
}

static void WINAPI OnQuery(void*, UINT64 q, PCWSTR text, PCWSTR) {
    std::wstring t = text;
    if (_wcsnicmp(t.c_str(), L"kill", 4) != 0 || (t.size() > 4 && t[4] != L' ')) {
        g_api.SubmitResults(q, L"[]", TRUE);
        return;
    }
    std::wstring filter = t.size() > 5 ? t.substr(5) : L"";
    std::wstring json = L"[";
    for (auto& p : ListProcesses(filter)) {
        wchar_t sub[256];
        swprintf(sub, 256, L"PID %lu \u00b7 %.0f MB \u00b7 Enter to end it", p.pid, p.mem / 1048576.0);
        if (json.size() > 1) json += L",";
        json += L"{\"id\":\"" + std::to_wstring(p.pid) + L"\",\"title\":\"End " + Esc(p.name) +
                L"\",\"subtitle\":\"" + Esc(sub) + L"\",";
        json += p.path.empty() ? L"\"icon\":{\"glyph\":\"\\uE71A\"}," : L"\"icon\":{\"path\":\"" + Esc(p.path) + L"\"},";
        json += L"\"score\":0.9}";
    }
    json += L"]";
    g_api.SubmitResults(q, json.c_str(), TRUE);
}

static void WINAPI OnInvoke(void*, PCWSTR itemId, PCWSTR action, PCWSTR, PCWSTR) {
    DWORD pid = wcstoul(itemId, nullptr, 10);
    HANDLE h = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
    BOOL ok = h && TerminateProcess(h, 1);
    if (h) CloseHandle(h);
    Wh_Log(L"kill %lu (%s): %s", pid, action, ok ? L"ok" : L"failed");
}

static void WINAPI OnDetach(void*) { g_provider = 0; }

static void FinderThread() {
    // Mods load in any order; wait for the library, and re-register if it reloads.
    while (!g_stop) {
        if (!g_provider) {
            static WSE_ProviderCallbacks cb{sizeof(cb), nullptr, OnQuery, OnInvoke, nullptr, OnDetach};
            g_provider = WSE_FindAndRegister(
                &g_api, L"{\"id\":\"kill-process\",\"name\":\"Kill process\",\"glyph\":\"\\uE71A\"}", &cb);
            if (g_provider) Wh_Log(L"registered provider: %llu", (unsigned long long)g_provider.load());
        }
        for (int i = 0; i < 20 && !g_stop; i++) Sleep(100);
    }
}

static void Start() { g_finder = std::thread(FinderThread); }
static void Stop() {
    g_stop = true;
    if (g_finder.joinable()) g_finder.join();
    if (UINT64 p = g_provider.exchange(0)) g_api.UnregisterProvider(p);
}

#ifdef WH_MOD
BOOL Wh_ModInit() {
    if (!StrStrIW(GetCommandLineW(), L"SystemComposerControl")) return TRUE;
    Start();
    return TRUE;
}
void Wh_ModUninit() { Stop(); }
#else
extern "C" __declspec(dllexport) DWORD WINAPI TestInit(void*) { Start(); return 0; }
extern "C" __declspec(dllexport) DWORD WINAPI TestUninit(void*) { Stop(); return 0; }
#endif
