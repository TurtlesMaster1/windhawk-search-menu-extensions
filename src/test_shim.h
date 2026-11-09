// Prepended to the mod source for the standalone test build only (see build.sh).
#include <windows.h>
// ---------------------------------------------------------------------------
// Standalone test build (injected DLL, no Windhawk engine).
// ---------------------------------------------------------------------------
#include <stdio.h>
#include <stdarg.h>
static void TestLog(const wchar_t* fmt, ...) {
    wchar_t buf[4096];
    va_list ap;
    va_start(ap, fmt);
    _vsnwprintf(buf, 4095, fmt, ap);
    va_end(ap);
    buf[4095] = 0;
    SYSTEMTIME st;
    GetLocalTime(&st);
    wchar_t logPath[MAX_PATH];
    GetTempPathW(MAX_PATH, logPath);
    wcscat_s(logPath, L"wse-test.log");
    FILE* f = _wfopen(logPath, L"a, ccs=UTF-8");
    if (f) {
        fwprintf(f, L"%02d:%02d:%02d.%03d [%lu] %s\n", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
                 GetCurrentThreadId(), buf);
        fclose(f);
    }
}
#define Wh_Log(fmt, ...) TestLog(fmt, ##__VA_ARGS__)
static int Wh_GetIntSetting(const wchar_t* name) {
    if (!wcscmp(name, L"queryTimeoutMs")) return 2500;
    if (!wcscmp(name, L"restartIfLate")) return 0;  // the harness injects late on purpose
    return 1;  // demoProvider, pipeServer
}

// Wh_SetFunctionHook stand-in. With no engine to patch code, it patches every pointer to the
// target found in OSClient.API.dll's non-executable sections (its vtables). The "original" is
// the target itself, which stays untouched.
#include <vector>
struct TestHook {
    void* target;
    void* hook;
    std::vector<void**> slots;
};
static std::vector<TestHook> g_testHooks;
static BOOL Wh_SetFunctionHook(void* target, void* hook, void** orig) {
    *orig = target;
    g_testHooks.push_back({target, hook, {}});
    return TRUE;
}
static void TestPatch(void** slot, void* value) {
    DWORD old;
    if (VirtualProtect(slot, sizeof(void*), PAGE_READWRITE, &old)) {
        *slot = value;
        VirtualProtect(slot, sizeof(void*), old, &old);
    }
}
static void TestApplyHooks() {
    auto base = (BYTE*)GetModuleHandleW(L"OSClient.API.dll");
    if (!base) return;
    auto nt = (IMAGE_NT_HEADERS*)(base + ((IMAGE_DOS_HEADER*)base)->e_lfanew);
    auto sec = IMAGE_FIRST_SECTION(nt);
    for (int i = 0; i < nt->FileHeader.NumberOfSections; i++, sec++) {
        if (sec->Characteristics & IMAGE_SCN_MEM_EXECUTE) continue;
        auto** p = (void**)(base + sec->VirtualAddress);
        auto** end = (void**)(base + sec->VirtualAddress + sec->Misc.VirtualSize) - 1;
        for (; p <= end; p++)
            for (auto& h : g_testHooks)
                if (*p == h.target) h.slots.push_back(p);
    }
    for (auto& h : g_testHooks) {
        for (auto** s : h.slots) TestPatch(s, h.hook);
        TestLog(L"[test] hooked %p in %zu slots", h.target, h.slots.size());
    }
}
static void TestRemoveHooks() {
    for (auto& h : g_testHooks)
        for (auto** s : h.slots) TestPatch(s, h.target);
    g_testHooks.clear();
}
