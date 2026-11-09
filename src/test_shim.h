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
    return 1;  // demoProvider, pipeServer, debugLog
}
